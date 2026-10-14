#!/usr/bin/env python3
"""Run bounded semantic scenarios against a genuine 3DSX in an isolated CDB Azahar.

This is Windows portable-profile automation, not a substitute for hardware or
an original Godot oracle. Scenarios carry their own reviewed reference scope.
"""
import math
import argparse
import base64
import hashlib
import json
import os
import re
from pathlib import Path
import secrets
import shutil
import socket
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from tools.cdb_client import CdbClient
from tools.semantic_probe import MAGIC, WIRE, FIELDS, decode, projection


def sha(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as f:
        for block in iter(lambda: f.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def validate_scenario(scenario):
    if set(scenario) != {'schema', 'name', 'reference', 'projection', 'steps'} or type(scenario['schema']) is not int or scenario['schema'] != 1:
        raise ValueError('Unknown scenario schema/fields')
    if not isinstance(scenario['steps'], list) or not 1 <= len(scenario['steps']) <= 256:
        raise ValueError('Scenario step budget exceeded')
    if not scenario['reference'].get('commit') or not scenario['reference'].get('scope'):
        raise ValueError('Scenario lacks source scope')
    known = set(decode(WIRE.pack(MAGIC, 3, WIRE.size, *([0]*(len(FIELDS)-2)), *([0.0]*4), *(b'' for _ in range(4)))))
    projection({k: 0 for k in known}, scenario['projection'])
    for step in scenario['steps']:
        if not isinstance(step, dict) or set(step) - {'name', 'buttons', 'circle', 'frames', 'wait', 'capture', 'same_as', 'audio_progress', 'expected_music', 'wait_audio', 'hold_until', 'repeat_until', 'max_attempts'}:
            raise ValueError('Unknown scenario step field')
        if not isinstance(step.get('name'), str) or not step['name'].replace('_', '').isalnum():
            raise ValueError('Unsafe scenario checkpoint name')
        if any(not isinstance(step.get(key, {}), dict) or any(k not in known for k in step.get(key, {})) for key in ('wait', 'wait_audio', 'hold_until', 'repeat_until')):
            raise ValueError('Unknown wait observation')
        for key in ('wait', 'wait_audio', 'hold_until', 'repeat_until'):
            for value in step.get(key, {}).values():
                if isinstance(value, dict):
                    if set(value) != {'min','max'} or any(type(v) not in (int,float) or not math.isfinite(v) for v in value.values()) or value['min'] > value['max']:
                        raise ValueError('Invalid bounded observation interval')
                elif type(value) not in (int,float,str) or (isinstance(value,float) and not math.isfinite(value)):
                    raise ValueError('Invalid expected observation')
        if 'repeat_until' in step or 'max_attempts' in step:
            if not step.get('repeat_until') or 'buttons' not in step or 'hold_until' in step or type(step.get('max_attempts')) is not int or not 1 <= step['max_attempts'] <= 64:
                raise ValueError('Invalid bounded input repetition')
        if type(step.get('frames', 4)) is not int or not 1 <= step.get('frames', 4) <= 600:
            raise ValueError('Input frame budget exceeded')
    if len({s['name'] for s in scenario['steps']}) != len(scenario['steps']):
        raise ValueError('Duplicate checkpoint identity')


def validate_reference(reference):
    from tools.extract_battle_entry import Extractor
    ex = Extractor(ROOT)
    if reference['commit'] != ex.lock['commit'] or not reference.get('sources'):
        raise ValueError('Scenario must identify the pinned original sources')
    for source, digest in reference['sources'].items():
        if type(digest) is not str or not re.fullmatch('[0-9a-f]{64}', digest):
            raise ValueError('Invalid reviewed source digest')
        ex.data(source)  # checks the readonly full inventory before returning bytes
        if ex.sources[source] != digest:
            raise ValueError('Original source changed; semantic review required')


def compare_original(checkpoints, original):
    cases = original['cases']
    for name, case in [('close_pause', 'pause_select_close'), ('cancel_pause', 'pause_cancel_close')]:
        state = checkpoints[name]
        expected = cases[case]
        if state['paused'] != int(expected['paused']) or (state['menu_phase'] != 0) != expected['active']:
            raise AssertionError('Original Pause close result differs')
    state = checkpoints['psi_back']
    expected = cases['psi_back']
    if state['paused'] != int(expected['paused']) or (state['menu_phase'] == 2) != expected['pause_active']:
        raise AssertionError('Original PSI back result differs')
    state = checkpoints['psi_unlearned_confirm']
    expected = cases['psi_unlearned']
    if state['pp'] != expected['pp'] or state['menu_phase'] != 9 or state['psi_learned'] != expected['skills']:
        raise AssertionError('Original unlearned PSI result differs')
    return ['pause_select_close', 'pause_cancel_close', 'psi_unlearned', 'psi_back']


CONFIG = '''[UI]
firstStart=false
firstStart\\default=false
confirmClose=false
confirmClose\\default=false
calloutFlags=4294967295
calloutFlags\\default=false
pause_when_in_background=false
pause_when_in_background\\default=false
mute_when_in_background=false
mute_when_in_background\\default=false
[Miscellaneous]
log_filter=*:Warning
log_filter\\default=false
[Renderer]
graphics_api=1
graphics_api\\default=false
resolution_factor=1
resolution_factor\\default=false
[WebService]
enable_telemetry=false
enable_telemetry\\default=false
'''


def portable(source, target):
    target.mkdir()
    for item in source.parent.rglob('*'):
        relative = item.relative_to(source.parent)
        if 'user' in relative.parts or item.is_symlink():
            continue
        if item.is_file() and (item.suffix.lower() in ('.exe', '.dll') or item.name == 'qt.conf'):
            out = target / relative
            out.parent.mkdir(parents=True, exist_ok=True)
            try:
                os.link(item, out)
            except OSError:
                shutil.copyfile(item, out)
    user = target / 'user'
    (user / 'config').mkdir(parents=True)
    (user / 'config/qt-config.ini').write_text(CONFIG, encoding='utf-8')
    return target / source.name


def matches(state, expected):
    return all(v['min'] <= state[k] <= v['max'] if isinstance(v,dict) else state[k] == v for k,v in expected.items())


class Session:
    def __init__(self, client):
        self.client, self.address = client, None
        candidates = client.call('process.list')
        if len(candidates) != 1:
            raise RuntimeError('Expected exactly one guest process in the fresh profile')
        self.pid = candidates[0]['processId']
        client.call('process.attach', {'processId': self.pid, 'pause': True})
        result = client.call('memory.search', {'text': MAGIC.decode(), 'maxResults': 32, 'maxBytes': 64*1024*1024})
        maps = client.call('memory.maps')
        for match in result['matches']:
            address = match['address']
            if not any(m['base'] <= address and address+WIRE.size <= m['end'] and 'w' in m['permissions'] for m in maps):
                continue
            try:
                decode(self.read(address))
            except ValueError:
                continue
            if self.address is not None:
                raise RuntimeError('Ambiguous semantic probe; refusing arbitrary guest memory')
            self.address = address
        if self.address is None:
            raise RuntimeError('Genuine build does not expose a supported semantic probe')

    def read(self, address=None):
        result = self.client.call('memory.read', {'address': self.address if address is None else address, 'size': WIRE.size})
        if result['encoding'] != 'hex' or result['size'] != WIRE.size:
            raise ValueError('Unknown CDB memory encoding/size')
        return bytes.fromhex(result['data'])

    def state(self):
        return decode(self.read())

    def advance(self, frames):
        result = self.client.call('frame.step', {'frames': frames})
        self.client.event('frame.stepComplete', processId=self.pid)
        return self.state()

    def wait(self, expected, timeout=45):
        deadline = time.monotonic() + timeout
        state = self.state()
        while not matches(state, expected) or not state['frame']:
            if time.monotonic() >= deadline:
                raise TimeoutError(f'Semantic wait failed: expected {expected}, observed {state}')
            state = self.advance(1 if any(isinstance(v,dict) for v in expected.values()) else 8)
        return state

    def repeat_input(self, params, frames, expected, attempts):
        state=self.state()
        for _ in range(attempts):
            if matches(state, expected):return state
            self.client.call('input.set',params)
            try:state=self.advance(frames)
            finally:self.client.call('input.clear')
            state=self.advance(4)
        if not matches(state, expected):raise TimeoutError('Bounded input repetition did not reach expected state')
        return state

    def capture(self, path):
        result = self.client.call('screen.capture', {'screen': 'combined'})
        # Renderer may require a new guest swap. Capture is evidence only; its
        # completion never determines the behavioral comparison.
        self.advance(2)
        event = self.client.event('screen.captured', jobId=result['jobId'])
        if event['mediaType'] != 'image/png' or event['encoding'] != 'base64':
            raise ValueError('Unknown screenshot encoding')
        png = base64.b64decode(event['data'], validate=True)
        if len(png) > 2 * 1024 * 1024 or not png.startswith(b'\x89PNG\r\n\x1a\n'):
            raise ValueError('Invalid or oversized screenshot')
        path.write_bytes(png)


def run_once(args, scenario, index):
    folder = args.out / f'run-{index}'
    folder.mkdir()
    work = folder / 'temporary'
    work.mkdir()
    (work / '.owned-emulator-workspace').write_text('encore-cdb-v1\n')
    exe = portable(args.emulator, work / 'emulator')
    if args.dsp_component:
        component = exe.parent / 'user/sdmc/3ds/dspfirm.cdc'
        component.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(args.dsp_component, component)
    with socket.socket() as sock:
        sock.bind(('127.0.0.1', 0))
        port = sock.getsockname()[1]
    token = secrets.token_hex(24)
    command = [str(exe), '--cdb-port', str(port), '--cdb-token', token, '--cdb-allow-input', str(args.game)]
    startup = subprocess.STARTUPINFO()
    startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = 0
    client = process = None
    checkpoints, captures = {}, 0
    state = step = None
    result = {'status': 'failed', 'checkpoints': checkpoints}
    try:
        with (folder / 'emulator-stdout.log').open('wb') as log:
            process = subprocess.Popen(command, cwd=exe.parent, stdout=log, stderr=subprocess.STDOUT, startupinfo=startup)
            deadline = time.monotonic() + 60
            while client is None:
                if process.poll() is not None or time.monotonic() >= deadline:
                    raise RuntimeError('CDB emulator did not boot; inspect retained log')
                try:
                    client = CdbClient(token, port)
                except (OSError, TimeoutError):
                    time.sleep(.1)
            capabilities = client.call('cdb.capabilities')
            if capabilities.get('protocolVersion') != 1 or capabilities.get('transport') != 'tcp-line-json-rpc':
                raise RuntimeError('Unsupported CDB protocol/transport')
            if capabilities.get('memoryWriteEnabled') is not False or capabilities.get('inputEnabled') is not True:
                raise RuntimeError('Unexpected CDB write/input configuration')
            session = Session(client)
            for step in scenario['steps']:
                if (folder / 'emulator-stdout.log').stat().st_size > 8*1024*1024:
                    raise RuntimeError('Emulator diagnostic log budget exceeded')
                if 'buttons' in step or 'circle' in step:
                    params = {'buttons': step.get('buttons', []), 'circle': step.get('circle', {'x': 0, 'y': 0, 'override': True})}
                    if 'repeat_until' in step:
                        session.repeat_input(params,step.get('frames',4),step['repeat_until'],step['max_attempts'])
                    else:
                        client.call('input.set', params)
                    if 'repeat_until' in step:
                        pass
                    elif 'hold_until' in step:
                        session.wait(step['hold_until'])
                    else:
                        session.advance(step.get('frames', 4))
                    client.call('input.clear')
                    session.advance(4)  # sample release before the next fresh press
                state = session.wait(step.get('wait', {}))
                if args.require_audio and 'wait_audio' in step:
                    state = session.wait(step['wait_audio'])
                if args.require_audio and step.get('audio_progress'):
                    before = (state['dsp_music_sample_pos'], state['dsp_music_wave_seq'])
                    state = session.advance(32)
                    if state['music_asset'] != step.get('expected_music', state['music_asset']) or not state['music_playing'] or not state['dsp_music_playing'] or before == (state['dsp_music_sample_pos'], state['dsp_music_wave_seq']):
                        raise AssertionError('BGM is not being consumed by DSP')
                if args.require_audio and not state['audio_available']:
                    raise AssertionError('NDSP unavailable in audio-required acceptance')
                selected = projection(state, scenario['projection'])
                if 'same_as' in step and selected != checkpoints[step['same_as']]['semantic']:
                    raise AssertionError(f"{step['name']} changed state compared with {step['same_as']}")
                checkpoints[step['name']] = {'semantic': selected, 'observed': state}
                if step.get('capture'):
                    captures += 1
                    if captures > 8:
                        raise RuntimeError('Screenshot count budget exceeded')
                    session.capture(folder / (step['name'] + '.png'))
                print(f"run {index}: {step['name']} accepted", flush=True)
            result['status'] = 'passed'
    except Exception as error:
        result['error'] = str(error)
        if state is not None:
            result['failed_observation'] = {'checkpoint': step['name'], 'observed': state}
        raise
    finally:
        if client:
            try:
                client.call('input.clear')
            except Exception:
                pass
            client.close()
        if process and process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait(timeout=10)
        # Keep bounded original logs; delete only this runner's marked temporary
        # profile/cache/hardlinks after the owned process has actually stopped.
        source_log = work / 'emulator/user/log/azahar_log.txt'
        if source_log.exists():
            with source_log.open('rb') as src, (folder / 'emulator.log').open('wb') as dst:
                dst.write(src.read(8*1024*1024))
                if src.read(1):
                    result['log_truncated'] = True
        result['temporary_bytes'] = sum(p.stat().st_size for p in work.rglob('*') if p.is_file())
        (folder / 'result.json').write_text(json.dumps(result, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
        if work.resolve().parent != folder.resolve() or not (work / '.owned-emulator-workspace').is_file():
            raise RuntimeError('Refusing cleanup outside owned workspace')
        shutil.rmtree(work)
    return {k: v['semantic'] for k, v in checkpoints.items()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--emulator', type=Path, required=True)
    parser.add_argument('--game', type=Path, required=True)
    parser.add_argument('--receipt', type=Path, required=True)
    parser.add_argument('--scenario', type=Path, required=True)
    parser.add_argument('--original-reference', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--repetitions', type=int, default=2)
    parser.add_argument('--dsp-component', type=Path, help='Existing private local DSP component, test profile only; never packaged')
    parser.add_argument('--require-audio', action='store_true')
    args = parser.parse_args()
    if args.dsp_component:
        args.dsp_component = args.dsp_component.resolve()
        if not args.dsp_component.is_file() or not 0 < args.dsp_component.stat().st_size <= 1024*1024:
            parser.error('DSP component must be an existing bounded private local file')
    for name in ('emulator', 'game', 'receipt', 'scenario', 'original_reference', 'out'):
        setattr(args, name, getattr(args, name).resolve())
    if os.name != 'nt' or not 2 <= args.repetitions <= 4:
        parser.error('Windows portable runner requires 2–4 fresh repetitions')
    if args.out.exists() or not args.out.is_relative_to(ROOT / 'build/emulator-regression'):
        parser.error('Use a fresh directory inside build/emulator-regression')
    scenario = json.loads(args.scenario.read_text(encoding='utf-8'))
    validate_scenario(scenario)
    validate_reference(scenario['reference'])
    original = json.loads(args.original_reference.read_text(encoding='utf-8'))
    original_receipt_path = args.original_reference.parent/'receipt.json'
    original_receipt = json.loads(original_receipt_path.read_text(encoding='utf-8'))
    if original_receipt['commit'] != scenario['reference']['commit'] or original_receipt['reference_sha256'] != sha(args.original_reference):
        raise ValueError('Original oracle provenance mismatch')
    if original['engine']['string'] != '3.6.2-stable (official)':
        raise ValueError('Wrong original oracle engine')
    for source, digest in original_receipt['sources'].items():
        validate_reference({'commit': original_receipt['commit'], 'sources': {source: digest}})
    receipt = json.loads(args.receipt.read_text(encoding='utf-8'))
    identity = receipt['artifacts']['encore-native.3dsx']
    if args.game.stat().st_size != identity['bytes'] or sha(args.game) != identity['sha256']:
        raise ValueError('Game does not match the genuine cross-build receipt')
    if shutil.disk_usage(ROOT).free < 2*1024**3:
        raise RuntimeError('Insufficient free disk for bounded emulator run')
    args.out.mkdir(parents=True)
    report = {'status': 'failed', 'source_commit': receipt['source_commit'], 'game_sha256': identity['sha256'],
              'private_dsp_sha256': sha(args.dsp_component) if args.dsp_component else None,
              'audio_required': args.require_audio, 'emulator_sha256': sha(args.emulator), 'scenario_sha256': sha(args.scenario), 'original_reference_sha256': sha(args.original_reference), 'reference': scenario['reference'],
              'scope': 'Fresh 3DSX emulator semantic repetitions; no pixel equivalence, CIA installation or hardware claim'}
    try:
        baseline = run_once(args, scenario, 1)
        for index in range(2, args.repetitions+1):
            current = run_once(args, scenario, index)
            if current != baseline:
                raise AssertionError('Fresh repetitions produced different semantic outcomes')
        report.update(status='passed', repetitions=args.repetitions, semantic=baseline, compared_original_cases=compare_original(baseline, original), unverified=['learned PSI use/targeting in emulator', 'CIA installation', 'Old/New 3DS hardware', 'perceptual audio quality'])
    except Exception as error:
        report['error'] = str(error)
        raise
    finally:
        (args.out / 'receipt.json').write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')


if __name__ == '__main__':
    main()
