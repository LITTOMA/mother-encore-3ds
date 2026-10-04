#!/usr/bin/env python3
"""Build and run a checked, partial production-flow selection, never a full pass."""
from __future__ import annotations
import argparse
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
REQUIRED = {
    'playable_opening': 'encore_playable_opening_tests',
    'room_data': 'encore_room_data_tests',
    'battle_data': 'encore_battle_data_tests',
    'audio_data': 'encore_audio_data_tests',
    'native_session': 'encore_native_session_tests',
    'new_game_setup': 'encore_new_game_setup_tests',
    'resource_catalog': 'encore_resource_catalog_tests',
}
OPTIONAL = {'original_introduction': 'encore_introduction_tests'}


def select(metadata):
    if not isinstance(metadata, dict) or metadata.get('kind') != 'ctestInfo' or metadata.get('version') != {'major': 1, 'minor': 0}:
        raise ValueError('Unsupported CTest metadata')
    tests = metadata.get('tests')
    if not isinstance(tests, list):
        raise ValueError('Missing CTest test list')
    names = [test.get('name') for test in tests if isinstance(test, dict)]
    if len(names) != len(tests) or any(not isinstance(name, str) for name in names) or len(set(names)) != len(names):
        raise ValueError('Invalid or duplicate test registration')
    missing = set(REQUIRED) - set(names)
    if missing:
        raise ValueError('Missing production tests: ' + ', '.join(sorted(missing)))
    selected = dict(REQUIRED)
    selected.update({name: target for name, target in OPTIONAL.items() if name in names})
    return selected


def expression(names):
    if not names:
        raise ValueError('Empty test selection')
    return '^(' + '|'.join(re.escape(name) for name in sorted(names)) + ')$'


def read_metadata(build_dir, *args):
    return json.loads(subprocess.check_output(['ctest', '--test-dir', str(build_dir), '--show-only=json-v1', *args], text=True))


def run(build_dir):
    subprocess.run(['cmake', '-S', str(ROOT), '-B', str(build_dir), '-DCMAKE_BUILD_TYPE=Release',
                    '-DENCORE_REGENERATE_NATIVE_CONTENT=OFF', '-DBUILD_TESTING=ON', '-DENCORE_TEST_PARALLEL=ON'], check=True)
    selected = select(read_metadata(build_dir))
    subprocess.run(['cmake', '--build', str(build_dir), '--parallel', '4', '--target', *selected.values()], check=True)
    regex = expression(selected)
    filtered = read_metadata(build_dir, '-R', regex)
    actual = [test['name'] for test in filtered['tests']]
    if len(actual) != len(selected) or set(actual) != set(selected):
        raise ValueError('CTest selection differs from the reviewed production set')
    for test in filtered['tests']:
        command = test.get('command')
        if not command or not Path(command[0]).is_file():
            raise ValueError('Production test executable missing: ' + test['name'])
        if Path(command[0]).stem != selected[test['name']]:
            raise ValueError('Production test command changed: ' + test['name'])
    (build_dir / 'selection.json').write_text(json.dumps({'scope': 'partial-production-flow', 'tests': sorted(selected)}, indent=2) + '\n', encoding='utf-8')
    subprocess.run(['ctest', '--test-dir', str(build_dir), '--output-on-failure', '--parallel', '4', '--no-tests=error', '-R', regex], check=True)
    print('Partial production-flow checks passed; full compiler/sanitizer/source/console verification is separate.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path, required=True)
    args = parser.parse_args()
    run(args.build_dir.resolve())
