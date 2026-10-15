"""Requested manual checks for the CDB acceptance boundary; no emulator double pass."""
import copy
import json
from pathlib import Path
import sys
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from tools.cdb_client import CdbClient, CdbError
from tools.semantic_probe import MAGIC, WIRE, FIELDS, decode, projection
from tools.run_emulator_semantics import validate_scenario, validate_reference, compare_original, matches, Session


def wire(**changes):
    words = {k: 0 for k in FIELDS}
    words.update(version=3, byte_size=WIRE.size)
    words.update(changes)
    return WIRE.pack(MAGIC, *(words[k] for k in FIELDS), *([0.0]*4), *(b'' for _ in range(4)))


class WireBoundary(unittest.TestCase):
    def test_exact_wire_and_projection(self):
        state = decode(wire(width=400, height=240))
        self.assertEqual(projection(state, ['width', 'height']), {'width': 400, 'height': 240})
        self.assertEqual(WIRE.size, 716)

    def test_truncated_extra_magic_and_versions(self):
        good = wire()
        for data in (good[:-1], good+b'\0', b'BADMAGIC'+good[8:], wire(version=1), wire(version=2), wire(version=4), wire(byte_size=717)):
            with self.subTest(data=data[:20]), self.assertRaises(ValueError):
                decode(data)

    def test_torn_overflow_boolean_utf8_and_nonfinite(self):
        cases = [wire(sequence=1), wire(overflow=1), wire(paused=2)]
        good = wire()
        cases += [good[:188]+b'\0\0\x80\x7f'+good[192:], good[:204]+b'\xff'+good[205:], good[:204]+b'x'*128+good[332:]]
        for data in cases:
            with self.subTest(data=data[:20]), self.assertRaises(ValueError):
                decode(data)

    def test_projection_unknown_duplicate_empty(self):
        for fields in ([], ['width', 'width'], ['unknown']):
            with self.assertRaises(ValueError):
                projection(decode(wire()), fields)

    def test_real_scenario_and_unknown_paths(self):
        scenario = json.loads((ROOT/'tests/emulator/opening-menu.json').read_text())
        validate_scenario(scenario)
        for change in (lambda s: s.update(schema=2), lambda s: s.update(schema=True), lambda s: s.update(unknown=True),
                       lambda s: s['steps'][0].update(wait={'invented': 1}),
                       lambda s: s['steps'][0].update(wait_audio={'invented': 1}),
                       lambda s: s['steps'][0].update(hold_until={'invented': 1}),
                       lambda s: s['steps'][0].update(frames=601),
                       lambda s: s['steps'][0].update(name='../escape'),
                       lambda s: s['projection'].append(s['projection'][0])):
            candidate = copy.deepcopy(scenario)
            change(candidate)
            with self.assertRaises(ValueError):
                validate_scenario(candidate)

    def test_bounded_intervals_and_input_repetition(self):
        scenario=json.loads((ROOT/'tests/emulator/opening-menu.json').read_text())
        step={'name':'bounded','buttons':['A'],'frames':120,'repeat_until':{'battle_active':0},'max_attempts':32,'wait':{'player_x':{'min':95,'max':97}}}
        scenario['steps'].append(step);validate_scenario(scenario)
        self.assertTrue(matches({'player_x':96},step['wait']))
        self.assertFalse(matches({'player_x':98},step['wait']))
        for change in ({'max_attempts':65},{'max_attempts':True},{'max_attempts':0},{'repeat_until':{}},{'hold_until':{'battle_active':0}},{'wait':{'player_x':{'min':98,'max':97}}},{'wait':{'player_x':{'min':False,'max':97}}},{'wait':{'player_x':{'min':0,'max':float('inf')}}},{'wait':{'player_x':{'min':0,'extra':1}}}):
            candidate=copy.deepcopy(scenario);candidate['steps'][-1].update(change)
            with self.assertRaises(ValueError):validate_scenario(candidate)
        s=object.__new__(Session);s.client=mock.Mock();s.state=lambda:{'battle_active':1}
        s.advance=mock.Mock(side_effect=[{'battle_active':1},{'battle_active':0}])
        self.assertEqual(s.repeat_input({'buttons':['A']},4,{'battle_active':0},2),{'battle_active':0})
        self.assertEqual([x.args[0] for x in s.client.call.call_args_list],['input.set','input.clear'])
        s.client.reset_mock();s.advance=mock.Mock(return_value={'battle_active':1})
        with self.assertRaises(TimeoutError):s.repeat_input({'buttons':['A']},4,{'battle_active':0},2)
        self.assertEqual(s.advance.call_count,4)
        self.assertEqual(s.client.call.call_count,4)

    def test_original_comparison_rejects_result_drift(self):
        original={'cases': {'pause_select_close': {'paused': False,'active': False}, 'pause_cancel_close': {'paused': False,'active': False}, 'psi_back': {'paused': True,'pause_active': True}, 'psi_unlearned': {'pp': 26,'skills': 0}}}
        baseline={'close_pause': {'paused': 0,'menu_phase': 0}, 'cancel_pause': {'paused': 0,'menu_phase': 0}, 'psi_back': {'paused': 1,'menu_phase': 2}, 'psi_unlearned_confirm': {'pp': 26,'menu_phase': 9,'psi_learned': 0}}
        self.assertEqual(len(compare_original(baseline,original)),4)
        for key,field in [('close_pause','paused'),('psi_back','menu_phase'),('psi_unlearned_confirm','pp')]:
            changed=copy.deepcopy(baseline);changed[key][field]=999
            with self.assertRaises(AssertionError):compare_original(changed,original)

    def test_pinned_original_and_missing_changed_unknown_source(self):
        scenario=json.loads((ROOT/'tests/emulator/opening-menu.json').read_text())
        validate_reference(scenario['reference'])
        for field,value in [('commit','0'*40),('sources',{}),('sources',{'../outside':'0'*64}),('sources',{'Scripts/UI/Pausemenu.gd':'0'*64})]:
            changed=copy.deepcopy(scenario['reference']);changed[field]=value
            with self.assertRaises((ValueError,KeyError)):validate_reference(changed)


class FakeSocket:
    def __init__(self, messages):
        self.data = b''.join(json.dumps(m).encode()+b'\n' for m in messages)
        self.sent = []
    def settimeout(self, timeout): pass
    def sendall(self, data): self.sent.append(json.loads(data))
    def recv(self, count):
        block, self.data = self.data[:count], self.data[count:]
        return block
    def close(self): pass


def response(identity, **extra):
    return dict(jsonrpc='2.0', id=identity, **extra)


class RpcBoundary(unittest.TestCase):
    def client(self, messages):
        sock = FakeSocket([response(1, result={'ok': True})]+messages)
        with mock.patch('socket.create_connection', return_value=sock):
            client = CdbClient('ephemeral-test-token', 12345)
        return client, sock

    def test_event_before_result_is_preserved(self):
        client, sock = self.client([{'jsonrpc': '2.0', 'method': 'frame.stepComplete', 'params': {'processId': 7}}, response(2, result={'frame': 9})])
        self.assertEqual(client.call('frame.step', {'frames': 2}), {'frame': 9})
        self.assertEqual(client.event('frame.stepComplete', processId=7), {'processId': 7})
        self.assertEqual(sock.sent[-1]['id'], 2)

    def test_error_wrong_response_disconnection_and_invalid_envelope(self):
        for messages in ([response(2, error={'code': -32000})], [response(99, result={})], [], [{'id': 2, 'result': {}}]):
            client, _ = self.client(messages)
            with self.assertRaises(CdbError):
                client.call('system.info')

    def test_guest_memory_write_is_denied_locally(self):
        client, sock = self.client([])
        for method in ('memory.write', 'registers.write'):
            with self.assertRaises(CdbError):
                client.call(method, {'address': 1})
        self.assertEqual(len(sock.sent), 1)

    def test_line_and_event_budgets_fail_closed(self):
        client, _ = self.client([response(2, result='x'*256)])
        client.MAX_LINE = 64
        with self.assertRaises(CdbError):
            client.call('system.info')
        event = {'jsonrpc': '2.0', 'method': 'input.changed', 'params': {}}
        client, _ = self.client([event, event, response(2, result={})])
        client.MAX_EVENTS = 1
        with self.assertRaises(CdbError):
            client.call('system.info')


if __name__ == '__main__':
    unittest.main()
