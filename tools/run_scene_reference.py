#!/usr/bin/env python3
"""Run the bounded official-engine scene reference and retain actual evidence."""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from tools.scene_reference import prepare, SCENE
from tools.scene_data import validate, require_gameplay
from tools.upstream import read_json, write_json


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def invoke(engine: Path, project: Path, arguments: list[str], log: Path, expected_error: str | None = None) -> None:
    environment = dict(os.environ)
    for key, child in [('APPDATA', 'user-roaming'), ('LOCALAPPDATA', 'user-local'),
                       ('XDG_DATA_HOME', 'user-data'), ('XDG_CONFIG_HOME', 'user-config')]:
        directory = project / child
        directory.mkdir(exist_ok=True)
        environment[key] = str(directory)
    command = [str(engine), '--path', str(project)]
    if os.name == 'nt':
        command += ['--no-window', '--audio-driver', 'Dummy']
    log.parent.mkdir(parents=True, exist_ok=True)
    # Direct redirection retains diagnostics if the engine crashes or times out.
    with log.open('wb') as stream:
        result = subprocess.run(command + arguments, stdout=stream, stderr=subprocess.STDOUT,
                                env=environment, timeout=55)
    output = log.read_text(encoding='utf-8', errors='replace')
    if expected_error is not None:
        if result.returncode == 0 or expected_error not in output:
            raise ValueError('Expected negative reference was not rejected: ' + log.name)
    elif result.returncode != 0 or any(marker in output for marker in ('ERROR:', 'WARNING:')):
        raise ValueError('Engine reference failed; inspect ' + str(log))


def micro(engine: Path, work: Path, reports: Path) -> dict:
    project = work / 'micro'
    shutil.copytree(ROOT / 'tests/godot_scene_data', project, ignore=shutil.ignore_patterns('.import', '*.import', '.godot'))
    script = ROOT / 'tools/godot_exporter/scene_data.gd'
    output = reports / 'instances.json'
    invoke(engine, project, ['--script', str(script), '--encore-scene=res://instances.tscn', '--encore-out=' + str(output)], reports / 'micro-positive.txt')
    document = read_json(output)
    validate(document)
    nodes = {entry['path']: entry for entry in document['nodes']}
    if len(nodes) != 9 or len(document['scene_states']) != 3:
        raise ValueError('Inherited/instanced node loss')
    first = nodes['First']['properties']
    second = nodes['Second']['properties']
    if first['speed'] != {'type': 'int64', 'value': '128'} or second['speed'] != {'type': 'int64', 'value': '96'}:
        raise ValueError('Instance/parent override precedence mismatch')
    if first['from_script'] != 'default from script' or second['from_script'] != 'default from script':
        raise ValueError('Script default lost in isolated fixture')
    shape = nodes['First/Body/Shape']
    if shape['world_transform']['origin'] != {'type': 'Vector2', 'x': 117, 'y': 218} or shape['properties']['disabled'] is not True:
        raise ValueError('Child transform/override mismatch')
    if nodes['Second/Body/Shape']['properties']['disabled'] is not False:
        raise ValueError('Native default/instance isolation mismatch')
    owners = nodes['First/Body']['physics_shape_owners']
    if len(owners) != 1 or owners[0]['disabled'] is not True or owners[0]['owner'] != {'type': 'NodeReference', 'path': 'First/Body/Shape'}:
        raise ValueError('Native physics shape ownership mismatch')
    if owners[0]['transform']['origin'] != {'type': 'Vector2', 'x': 2, 'y': -1} or len(owners[0]['shapes']) != 1:
        raise ValueError('Native collision shape transform mismatch')
    if owners[0]['cached_transform_before_enter_tree']['origin'] != {'type': 'Vector2', 'x': 1, 'y': -2}:
        raise ValueError('Godot pre-tree shape cache behavior changed; review required')
    metadata = dict(first['__meta__']['pairs'])
    if metadata['identity'] != {'type': 'int64', 'value': '9223372036854775807'} or metadata['negative'] != {'type': 'int64', 'value': '-9223372036854775808'}:
        raise ValueError('64-bit source identity lost through JSON')
    if sum(len(state['connections']) for state in document['scene_states']) != 1:
        raise ValueError('Serialized signal connection lost')
    for name, marker in [('unsupported', 'Unsupported Variant type'), ('placeholder', 'InstancePlaceholder requires')]:
        negative_output = reports / (name + '.json')
        if negative_output.exists():
            raise ValueError('Negative output path already exists')
        invoke(engine, project, ['--script', str(script), '--encore-scene=res://' + name + '.tscn',
                                '--encore-out=' + str(negative_output)], reports / ('micro-' + name + '.txt'), marker)
        if negative_output.exists():
            raise ValueError('Rejected scene wrote a partial output')
    try:
        require_gameplay(document)
    except ValueError:
        pass
    else:
        raise ValueError('Data-only output passed gameplay gate')
    return {'positive': 'native defaults, script defaults, inheritance, instance overrides, transforms, int64 and signals verified',
            'negative': 'unknown Variant and deferred instance rejected without partial output',
            'gameplay_gate': 'blocked as required'}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--godot', type=Path, required=True)
    parser.add_argument('--work', type=Path, required=True)
    parser.add_argument('--reports', type=Path, required=True)
    parser.add_argument('--micro-only', action='store_true')
    args = parser.parse_args()
    try:
        engine = args.godot.resolve()
        work = args.work.resolve()
        reports = args.reports.resolve()
        if not work.is_relative_to(ROOT / 'build') or work == ROOT / 'build' or work.exists():
            raise ValueError('Use a fresh work directory inside build/')
        if not reports.is_relative_to(ROOT / 'reports') or reports.exists():
            raise ValueError('Use a fresh report directory inside reports/')
        work.mkdir(parents=True)
        reports.mkdir(parents=True)
        result = {'schema': 1, 'engine_sha256': digest(engine), 'micro': micro(engine, work, reports),
                  'exporter_sha256': digest(ROOT / 'tools/godot_exporter/scene_data.gd'),
                  'verification': 'official engine data reference; not full original game execution',
                  'hardware': 'not run', 'emulator': 'not run'}
        result['tools'] = {name: digest(ROOT / name) for name in
            ('tools/run_scene_reference.py', 'tools/scene_reference.py', 'tools/scene_data.py',
             'tools/godot_exporter/scene_data.gd', 'tools/godot_exporter/import_plugin.gd')}
        if not args.micro_only:
            project = work / 'house'
            source = prepare(ROOT / 'upstream/MOTHER-Encore', project, SCENE)
            invoke(engine, project, ['--editor'], reports / 'house-import.txt')
            if 'ENCORE_IMPORT_COMPLETE' not in (reports / 'house-import.txt').read_text(encoding='utf-8'):
                raise ValueError('Editor did not confirm completed resource scan')
            output = reports / 'house-data.json'
            invoke(engine, project, ['--script', str(project / 'scene_data.gd'), '--encore-scene=res://' + SCENE,
                                    '--encore-out=' + str(output)], reports / 'house-export.txt')
            document = read_json(output)
            validate(document)
            if any(node['properties'].get('script') is not None for node in document['nodes']):
                raise ValueError('Original script unexpectedly executed')
            write_json(reports / 'house-source.json', source)
            result['house'] = {'commit': source['commit'], 'game_version': source['game_version'],
                'nodes': len(document['nodes']), 'resources': len(document['resources']), 'scene_states': len(document['scene_states']),
                'quarantined_attachments': len(source['script_attachments']), 'quarantined_connections': len(source['signal_connections']),
                'source_receipt_sha256': digest(reports / 'house-source.json'), 'data_sha256': digest(output),
                'tilemaps': {node['path']: len(node['cells']) for node in document['nodes'] if node['class'] == 'TileMap'},
                'native_compatible': False}
        write_json(reports / 'receipt.json', result)
        print(json.dumps(result, indent=2))
        return 0
    except (OSError, ValueError, KeyError, subprocess.SubprocessError) as error:
        print('SCENE REFERENCE ERROR: ' + str(error), file=sys.stderr)
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
