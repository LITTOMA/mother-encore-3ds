#!/usr/bin/env python3
"""Prepare a source-tracked, data-only Godot scene reference in build/.

This is deliberately NOT a gameplay exporter. Scripts and signal connections
are quarantined with their original bytes/locations, not approved or ignored.
Godot resolves native inheritance, transforms, TileSets and collision resources.
The untouched SceneState properties are exported alongside the native tree so
script-owned overrides remain available for subsequent mechanism adapters.
"""
from __future__ import annotations
import argparse
import hashlib
from pathlib import Path
import re
import shutil
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from tools.upstream import git, read_json, safe_path, write_json

SCENE = 'Maps/podunk/Nintens House.tscn'
ATTRIBUTE = re.compile(r'\s+(\w+)=("(?:\\.|[^"\\])*"|[0-9]+)')
EXTERNAL = re.compile(r'^\[ext_resource(.*)\]\s*$')


def sha(raw: bytes) -> str:
    return hashlib.sha256(raw).hexdigest()


def unquote(value: str) -> str:
    """Godot string escapes used by resource paths; reject unknown escapes."""
    if not value.startswith('"') or not value.endswith('"'):
        raise ValueError('Resource path/type must be quoted')
    body = value[1:-1]
    result = []
    i = 0
    while i < len(body):
        if body[i] == '\\':
            i += 1
            if i == len(body) or body[i] not in ('\\', '"', "'"):
                raise ValueError('Unsupported path escape')
        result.append(body[i])
        i += 1
    return ''.join(result)


def external(line: str) -> dict | None:
    match = EXTERNAL.fullmatch(line.rstrip('\r\n'))
    if not match:
        if line.startswith('[ext_resource'):
            raise ValueError('Malformed external resource declaration')
        return None
    body = match[1]
    cursor = 0
    fields = {}
    for field in ATTRIBUTE.finditer(body):
        if field.start() != cursor or field[1] in fields:
            raise ValueError('Ambiguous external resource declaration')
        fields[field[1]] = field[2]
        cursor = field.end()
    if body[cursor:].strip() or set(fields) != {'path', 'type', 'id'}:
        raise ValueError('Unsupported external resource fields')
    path = unquote(fields['path'])
    if not path.startswith('res://') or not path[6:] or '\\' in path[6:]:
        raise ValueError('External resource must have a canonical res:// path')
    relative = path[6:]
    if any(part in ('', '.', '..') or ':' in part for part in relative.split('/')):
        raise ValueError('Unsafe/noncanonical resource path')
    safe_path(ROOT, relative)
    identity = int(fields['id'])
    if identity < 1:
        raise ValueError('Invalid external resource ID')
    return {'path': path[6:], 'type': unquote(fields['type']), 'id': identity}


def quarantine(raw: bytes, source: str) -> tuple[bytes, list[dict], list[dict], list[dict]]:
    text = raw.decode('utf-8')
    lines = text.splitlines(keepends=True)
    if not lines or not re.fullmatch(r'\[gd_(scene|resource)\b[^\r\n]*\bformat=2\]\s*', lines[0]):
        raise ValueError('Only reviewed Godot text resource format 2 is supported')
    # Locate headers only outside multiline Godot strings. Never mistake a
    # header-looking line inside shader/script source for a resource section.
    sections = []
    quoted = False
    escaped = False
    for i, line in enumerate(lines):
        if not quoted and line.startswith('['):
            sections.append(i)
        for char in line:
            if escaped:
                escaped = False
            elif quoted and char == '\\':
                escaped = True
            elif char == '"':
                quoted = not quoted
    if quoted:
        raise ValueError('Unterminated resource string')
    inline = {}
    for start, end in zip(sections, sections[1:] + [len(lines)]):
        header = lines[start].rstrip('\r\n')
        if re.match(r'\[sub_resource[^\n]*type="(CSharpScript|VisualScript)"', header):
            raise ValueError('Unknown embedded script language: ' + source)
        if re.match(r'\[sub_resource[^\n]*type="GDScript"', header):
            match = re.fullmatch(r'\[sub_resource type="GDScript" id=([1-9][0-9]*)\]', header)
            if not match or int(match[1]) in inline:
                raise ValueError('Ambiguous embedded script declaration')
            block = ''.join(lines[start:end])
            if not block.startswith(header + '\n') and not block.startswith(header + '\r\n'):
                raise ValueError('Invalid embedded script section')
            inline[int(match[1])] = {'path': source + '::' + match[1], 'source': source, 'line': start + 1,
                'embedded_declaration': block, 'sha256': sha(block.encode('utf-8'))}
            for i in range(start, end):
                lines[i] = ''
    resources = []
    identities = set()
    scripts = {}
    for number, line in enumerate(lines, 1):
        record = external(line)
        if record is not None:
            if record['id'] in identities:
                raise ValueError('Duplicate external resource ID: ' + source)
            identities.add(record['id'])
            record.update({'source': source, 'line': number})
            resources.append(record)
            if record['type'] == 'Script':
                if not record['path'].endswith('.gd'):
                    raise ValueError('Unknown script language: ' + record['path'])
                scripts[('ExtResource', record['id'])] = record
    scripts.update({('SubResource', identity): record for identity, record in inline.items()})
    output = []
    attachments = []
    connections = []
    used_scripts = set()
    node = ''
    for number, line in enumerate(lines, 1):
        if line.startswith('[node '):
            node = line.rstrip('\r\n')
        record = external(line)
        if record is not None and ('ExtResource', record['id']) in scripts:
            continue
        if line.startswith('[connection '):
            if not line.rstrip().endswith(']'):
                raise ValueError('Multiline signal declaration requires review')
            connections.append({'source': source, 'line': number, 'declaration': line.rstrip('\r\n')})
            continue
        for (kind, identity), script in scripts.items():
            token = kind + r'\(\s*' + str(identity) + r'\s*\)'
            if re.search(token, line):
                if not re.fullmatch(r'script\s*=\s*' + token + r'\s*', line):
                    raise ValueError('Script resource used outside node attachment: ' + source)
                attachment = {'source': source, 'line': number, 'node_declaration': node,
                              'script': script['path'], 'declaration': line.rstrip('\r\n')}
                if 'embedded_declaration' in script:
                    attachment['embedded_script'] = script
                attachments.append(attachment)
                used_scripts.add((kind, identity))
                line = 'script = null\n'
        output.append(line)
    if set(scripts) != used_scripts:
        raise ValueError('Unattached script declaration requires explicit handling: ' + source)
    return ''.join(output).encode('utf-8'), resources, attachments, connections


def prepare(root: Path, out: Path, scene: str) -> dict:
    root = root.resolve()
    out = out.resolve()
    if not out.is_relative_to((ROOT / 'build').resolve()) or out == (ROOT / 'build').resolve():
        raise ValueError('Reference output must be a subdirectory of build/')
    if out.exists():
        raise ValueError('Use a fresh reference directory; stale imports are not reused')
    lock = read_json(ROOT / 'upstream.lock')
    inventory = read_json(ROOT / 'compatibility/upstream-inventory.json')
    if lock.get('schema') != 1 or inventory.get('schema') != 1 or lock.get('game_version') != '0.4.1.0':
        raise ValueError('Unreviewed source lock/inventory schema or game version')
    if git(root, 'rev-parse', 'HEAD') != lock['commit'] or git(root, 'status', '--porcelain'):
        raise ValueError('Reference requires pristine pinned upstream')
    if inventory['commit'] != lock['commit']:
        raise ValueError('Stale upstream byte inventory')
    indexed = inventory['files']
    pending = [scene]
    files = {}
    attachments = []
    connections = []
    payloads = {}
    while pending:
        name = pending.pop()
        if name in files:
            continue
        path = safe_path(root, name)
        if name not in indexed:
            raise ValueError('Dependency absent from pinned inventory: ' + name)
        raw = path.read_bytes()
        if sha(raw) != indexed[name]['sha256']:
            raise ValueError('Source differs from pinned inventory: ' + name)
        record = {'sha256': sha(raw), 'bytes': len(raw)}
        if path.suffix in ('.tscn', '.tres'):
            transformed, refs, scripts, signals = quarantine(raw, name)
            record['external_resources'] = refs
            # DynamicFontData stores its payload path as a property rather
            # than ext_resource. It is a native loader dependency, not a
            # permission to release upstream fonts.
            font_paths = []
            for number, line in enumerate(transformed.decode('utf-8').splitlines(), 1):
                if line.startswith('font_path = '):
                    path_value = unquote(line[len('font_path = '):])
                    if not path_value.startswith('res://'):
                        raise ValueError('Non-source font payload path')
                    safe_path(root, path_value[6:])
                    font_paths.append({'path': path_value[6:], 'line': number})
            record['native_font_payloads'] = font_paths
            record['reference_sha256'] = sha(transformed)
            attachments.extend(scripts)
            connections.extend(signals)
            pending.extend(ref['path'] for ref in refs)
            pending.extend(ref['path'] for ref in font_paths)
            payloads[name] = transformed
        elif path.suffix == '.gd':
            record['quarantined'] = 'not executed; defaults and behavior require mechanism adapter'
        elif path.suffix.lower() in ('.png', '.shader', '.wav', '.mp3', '.ogg', '.ttf', '.otf'):
            payloads[name] = raw
        else:
            raise ValueError('Unreviewed static resource codec: ' + name)
        files[name] = record
    document = {'schema': 1, 'commit': lock['commit'], 'game_version': lock['game_version'],
                'scene': scene, 'scope': 'native serialized scene data only; script behavior NOT executed',
                'native_compatible': False, 'files': files, 'script_attachments': attachments,
                'signal_connections': connections,
                'blockers': ['script defaults and runtime factories', 'script behavior and signals',
                             'dynamic resource paths', 'animation method calls', 'native mechanism coverage']}
    document['tools'] = {name: sha((ROOT / name).read_bytes()) for name in
        ('tools/scene_reference.py', 'tools/godot_exporter/scene_data.gd', 'tools/godot_exporter/import_plugin.gd')}
    # Validate the complete closure before making a partial reference project.
    out.mkdir(parents=True)
    for name, raw in payloads.items():
        target = safe_path(out, name)
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(raw)
    (out / 'project.godot').write_text('config_version=4\n[application]\nconfig/name="Encore scene data reference"\n[logging]\nfile_logging/enable_logging=false\n[editor_plugins]\nenabled=PoolStringArray("res://addons/encore_import/plugin.cfg")\n', encoding='utf-8')
    plugin_root = out / 'addons/encore_import'
    plugin_root.mkdir(parents=True)
    (plugin_root / 'plugin.cfg').write_text('[plugin]\nname="Encore import completion"\ndescription="Bounded resource scan"\nauthor="Encore Native"\nversion="1"\nscript="import_plugin.gd"\n', encoding='utf-8')
    shutil.copyfile(ROOT / 'tools/godot_exporter/import_plugin.gd', plugin_root / 'import_plugin.gd')
    shutil.copyfile(ROOT / 'tools/godot_exporter/scene_data.gd', out / 'scene_data.gd')
    write_json(out / 'source.json', document)
    print(f'Prepared {len(files)} source dependencies, {len(attachments)} quarantined attachments and {len(connections)} signal declarations.')
    return document


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=ROOT / 'upstream/MOTHER-Encore')
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--scene', default=SCENE)
    args = parser.parse_args()
    try:
        prepare(args.root, args.out, args.scene)
        return 0
    except (OSError, ValueError, KeyError) as error:
        print('SCENE REFERENCE ERROR: ' + str(error), file=sys.stderr)
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
