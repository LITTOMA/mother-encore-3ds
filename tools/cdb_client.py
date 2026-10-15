"""Bounded, dependency-free client for the user's loopback Citra Debug Bridge.

No memory writes, save-state injection, debugger patching, or remote connections.
The caller owns lifecycle, source receipts and semantic assertions.
"""
from collections import deque
import json
import socket
import time


class CdbError(RuntimeError):
    pass


class CdbClient:
    MAX_LINE = 8 * 1024 * 1024
    MAX_EVENTS = 128

    def __init__(self, token, port, timeout=20):
        self.socket = socket.create_connection(('127.0.0.1', port), timeout=timeout)
        self.timeout, self.buffer, self.events, self.next_id = timeout, b'', deque(), 1
        try:
            self.call('cdb.hello', {'token': token, 'mode': 'control'})
        except Exception:
            self.close()
            raise

    def close(self):
        self.socket.close()

    def _read(self, deadline):
        while b'\n' not in self.buffer:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise TimeoutError('CDB response deadline expired')
            self.socket.settimeout(remaining)
            block = self.socket.recv(65536)
            if not block:
                raise CdbError('CDB disconnected before completion')
            self.buffer += block
            if len(self.buffer) > self.MAX_LINE:
                raise CdbError('CDB message exceeds bounded line size')
        line, self.buffer = self.buffer.split(b'\n', 1)
        if len(line) > self.MAX_LINE:
            raise CdbError('CDB message exceeds bounded line size')
        message = json.loads(line)
        if not isinstance(message, dict) or message.get('jsonrpc') != '2.0':
            raise CdbError('Invalid CDB JSON-RPC envelope')
        return message

    def _event(self, message):
        if 'method' not in message or 'params' not in message:
            raise CdbError('Invalid CDB event')
        if len(self.events) >= self.MAX_EVENTS:
            raise CdbError('CDB event queue overflow; events were not silently discarded')
        self.events.append(message)

    def call(self, method, params=None):
        # Deny write APIs even if a server is accidentally configured to permit them.
        if method in ('memory.write', 'registers.write'):
            raise CdbError('This acceptance client does not mutate guest memory')
        request_id = self.next_id
        self.next_id += 1
        request = {'jsonrpc': '2.0', 'id': request_id, 'method': method}
        if params is not None:
            request['params'] = params
        self.socket.sendall((json.dumps(request, separators=(',', ':')) + '\n').encode())
        deadline = time.monotonic() + self.timeout
        while True:
            message = self._read(deadline)
            if 'id' not in message:
                self._event(message)
                continue
            if message['id'] != request_id:
                raise CdbError('Unexpected CDB response identity')
            if 'error' in message:
                raise CdbError(str(message['error']))
            if 'result' not in message:
                raise CdbError('CDB response lacks result')
            return message['result']

    def event(self, method, **identity):
        deadline = time.monotonic() + self.timeout
        while True:
            for message in tuple(self.events):
                if message['method'] == method and all(message['params'].get(k) == v for k, v in identity.items()):
                    self.events.remove(message)
                    return message['params']
                if message['method'] == 'screen.captureFailed':
                    raise CdbError(str(message['params']))
            self._event(self._read(deadline))
