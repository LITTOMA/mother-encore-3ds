#!/usr/bin/env python3
"""Checked native resource wiring adapter; does not extract gameplay semantics.

Ordinary compilation preserves independent reviewed IR and fingerprints the
already compiled resources. Encounter companions are explicit typed bindings.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import zlib

ROOT = Path(__file__).resolve().parents[1]
IR = ROOT / 'content/native-resource-catalog.json'
OUT = ROOT / 'romfs/data/native.encresources'
PIN = '7d9246600fffe518408f5830d4848635019005a3'
MAGIC = b'ENCRSC01'
HEADER = struct.Struct('<8s6I')
CATALOG_LIMIT = 16384
RESOURCE_LIMIT = 16 * 1024 * 1024
# Schema types and stable identities only; resource filenames live in the IR.
ROLES = {name: index + 1 for index, name in enumerate((
    'Room', 'Blackbars', 'Battle', 'Round', 'House', 'Items', 'Audio', 'Phone',
    'Choices', 'SaveMenu', 'Session', 'Settings', 'Prompts', 'Continue', 'Restore',
    'SessionMigration', 'NewGame', 'Localization', 'TitleLocale', 'SourceFonts',
    'Input', 'LoadingIndicator', 'EncounterBattle', 'EncounterRound', 'Introduction', 'HouseInspections', 'DrawerProgram', 'Storage', 'ItemDetails', 'FieldEquipment', 'ItemUse'))}
SUFFIXES = dict(zip(ROLES, ('.encroom', '.encbars', '.encbattle', '.encround',
    '.enchouse', '.encitems', '.encaudio', '.encphone', '.encchoices', '.encsavemenu',
    '.encsession', '.encsettings', '.encprompts', '.enccontinue', '.encrestore',
    '.encmigration', '.encnewgame', '.enclocale', '.enctitlelocale', '.encfont',
    '.encinput', '.encload', '.encbattle', '.encround', '.encintro', '.encinspect', '.encdrawer', '.encstorage', '.encdetails', '.encfield', '.encuse')))
BATTLE_ROLES = {'Battle', 'EncounterBattle'}
ROUND_ROLES = {'Round', 'EncounterRound'}


def require(value, message):
    if not value:
        raise ValueError(message)


def fields(value, expected, label):
    require(type(value) is dict and set(value) == set(expected), 'Unknown/missing ' + label + ' fields')


def canonical(path):
    return (type(path) is str and 0 < len(path) <= 256 and
            all(33 <= ord(c) <= 126 and c not in '\\:?#' for c in path) and
            all(part not in ('', '.', '..') for part in path.split('/')))


def load_ir(path=IR):
    def unique(pairs):
        result = {}
        for key, value in pairs:
            require(key not in result, 'Duplicate resource catalog JSON field: ' + key)
            result[key] = value
        return result
    return json.loads(Path(path).read_text(encoding='utf-8'), object_pairs_hook=unique)


def validate(ir):
    fields(ir, ('schema', 'kind', 'commit', 'scope', 'sources', 'bindings', 'encounters'), 'resource catalog')
    require(type(ir['schema']) is int and ir['schema'] == 1 and
            ir['kind'] == 'encore.native-resource-catalog.source-ir', 'Unsupported resource catalog schema/kind')
    require(ir['commit'] == PIN, 'Resource catalog source pin mismatch')
    require(type(ir['scope']) is str and 0 < len(ir['scope']) <= 2048, 'Missing resource catalog scope')
    sources = ir['sources']
    require(type(sources) is dict and 0 < len(sources) <= 1024, 'Invalid catalog source coverage')
    for path, digest in sources.items():
        # Source paths may contain spaces; canonical components remain required.
        require(type(path) is str and 0 < len(path) <= 512 and all(32 <= ord(c) <= 126 and c not in '\\:?#' for c in path) and
                all(part not in ('', '.', '..') for part in path.split('/')), 'Invalid catalog source path')
        require(type(digest) is str and len(digest) == 64 and all(c in '0123456789abcdef' for c in digest), 'Invalid source digest')
    validate_bindings(ir['bindings'], ir['encounters'])
    return ir


def validate_bindings(bindings, encounters):
    require(type(bindings) is list and 22 <= len(bindings) <= 128, 'Incomplete catalog role coverage')
    names, ids, paths = set(), set(), set()
    for row in bindings:
        fields(row, ('id', 'role', 'path'), 'binding')
        role = row['role']
        require(type(role) is str and role in ROLES and type(row['id']) is int and
                (row['id'] == ROLES[role] if ROLES[role] <= 22 or ROLES[role] >= 25 else 256 <= row['id'] <= 0xffffffff), 'Unknown binding role/stable ID')
        require(canonical(row['path']) and row['path'].endswith(SUFFIXES[role]) and len(row['path']) > len(SUFFIXES[role]), 'Unsafe or wrong-type binding path')
        require((ROLES[role] > 22 or role not in names) and row['id'] not in ids and row['path'] not in paths, 'Duplicate binding role/ID/path')
        names.add(role); ids.add(row['id']); paths.add(row['path'])
    require({name for name in names if ROLES[name] <= 22} == {name for name in ROLES if ROLES[name] <= 22}, 'Incomplete catalog roles')
    require(type(encounters) is list and 1 <= len(encounters) <= 32, 'Incomplete encounter coverage')
    binding_roles = {row['id']: row['role'] for row in bindings}
    battles, rounds = set(), set()
    for row in encounters:
        fields(row, ('battle_id', 'round_id'), 'encounter')
        b, r = row['battle_id'], row['round_id']
        require(type(b) is int and type(r) is int and b in binding_roles and r in binding_roles and
                binding_roles[b] in BATTLE_ROLES and binding_roles[r] in ROUND_ROLES, 'Wrong encounter binding type/reference')
        require(b not in battles and r not in rounds, 'Duplicate encounter binding')
        battles.add(b); rounds.add(r)
    require(battles == {key for key, value in binding_roles.items() if value in BATTLE_ROLES} and
            rounds == {key for key, value in binding_roles.items() if value in ROUND_ROLES}, 'Incomplete encounter roles')


def verify_sources(ir):
    lock = load_ir(ROOT / 'upstream.lock')
    inventory = load_ir(ROOT / 'compatibility/upstream-inventory.json')
    require(lock['commit'] == inventory['commit'] == ir['commit'] == PIN, 'Catalog lock/inventory/source mismatch')
    upstream = ROOT / 'upstream/MOTHER-Encore'
    head = subprocess.check_output(['git', '-C', str(upstream), 'rev-parse', 'HEAD'], text=True).strip()
    require(head == PIN, 'Catalog upstream checkout mismatch')
    for path, digest in ir['sources'].items():
        require(path in inventory['files'] and inventory['files'][path]['sha256'] == digest, 'Catalog source not in pinned inventory: ' + path)
        target = (upstream / path).resolve()
        require(target.is_relative_to(upstream.resolve()), 'Catalog source path escape')
        require(hashlib.sha256(target.read_bytes()).hexdigest() == digest, 'Catalog source bytes changed: ' + path)


def checked_file(root, path):
    root = Path(root).resolve()
    target = (root / path).resolve()
    require(target.is_relative_to(root), 'Catalog resource path escape')
    require(target.is_file() and 0 < target.stat().st_size <= RESOURCE_LIMIT, 'Missing/oversized catalog resource: ' + str(path))
    with target.open('rb') as stream:
        blob = stream.read(RESOURCE_LIMIT + 1)
    require(0 < len(blob) <= RESOURCE_LIMIT, 'Resource grew beyond catalog bounds')
    return blob


def encode(ir, romfs_root=ROOT / 'romfs'):
    validate(ir)
    verify_sources(ir)
    payload = bytearray(bytes.fromhex(ir['commit']) + struct.pack('<2I', len(ir['bindings']), len(ir['encounters'])))
    for row in sorted(ir['bindings'], key=lambda row: row['id']):
        path = row['path'].encode('ascii')
        blob = checked_file(romfs_root, row['path'])
        payload += struct.pack('<5I', row['id'], ROLES[row['role']], len(blob), zlib.crc32(blob), len(path)) + path
    for row in ir['encounters']:
        payload += struct.pack('<2I', row['battle_id'], row['round_id'])
    require(HEADER.size + len(payload) <= CATALOG_LIMIT, 'Catalog exceeds format size bound')
    return HEADER.pack(MAGIC, 1, HEADER.size + len(payload), zlib.crc32(payload), 1, 0, 0) + payload


def decode(blob):
    require(type(blob) in (bytes, bytearray) and 60 <= len(blob) <= CATALOG_LIMIT, 'Catalog size rejected')
    magic, version, total, checksum, capability, reserved0, reserved1 = HEADER.unpack_from(blob)
    require(magic == MAGIC and version == 1 and total == len(blob) and capability == 1 and not reserved0 and not reserved1, 'Catalog header rejected')
    require(zlib.crc32(blob[HEADER.size:]) == checksum, 'Catalog checksum mismatch')
    require(blob[32:52] == bytes.fromhex(PIN), 'Catalog source pin rejected')
    count, pairs = struct.unpack_from('<2I', blob, 52)
    require(22 <= count <= 128 and 1 <= pairs <= 32, 'Catalog role/encounter count rejected')
    offset = 60
    names = {v: k for k, v in ROLES.items()}
    bindings, encounters, fingerprints = [], [], {}
    for _ in range(count):
        require(offset + 20 <= len(blob), 'Truncated catalog binding')
        identity, role, size, crc, length = struct.unpack_from('<5I', blob, offset); offset += 20
        require(role in names and 0 < size <= RESOURCE_LIMIT and 0 < length <= 256 and offset + length <= len(blob), 'Invalid catalog binding')
        try:
            path = blob[offset:offset + length].decode('ascii')
        except UnicodeDecodeError as exc:
            raise ValueError('Non-ASCII catalog path') from exc
        offset += length
        bindings.append(dict(id=identity, role=names[role], path=path))
        fingerprints[identity] = dict(size=size, crc32=crc)
    for _ in range(pairs):
        require(offset + 8 <= len(blob), 'Truncated catalog encounter')
        b, r = struct.unpack_from('<2I', blob, offset); offset += 8
        encounters.append(dict(battle_id=b, round_id=r))
    require(offset == len(blob), 'Trailing catalog bytes')
    validate_bindings(bindings, encounters)
    return dict(bindings=bindings, encounters=encounters, fingerprints=fingerprints)


def compile_file(source=IR, output=OUT, romfs_root=ROOT / 'romfs'):
    blob = encode(load_ir(source), romfs_root)
    decode(blob)
    output = Path(output); output.parent.mkdir(parents=True, exist_ok=True); output.write_bytes(blob)
    return blob


def stage_files(source_root, files=None):
    relative = Path('data/native.encresources')
    blob = checked_file(source_root, relative)
    decoded = decode(blob)
    require(blob == encode(load_ir(), source_root), 'Resource catalog is stale')
    if files is not None:
        for row in decoded['bindings']:
            path = Path(row['path'])
            require(path in files, 'Catalog-bound resource was not staged: ' + row['path'])
            value = files[path]; fingerprint = decoded['fingerprints'][row['id']]
            require(len(value) == fingerprint['size'] and zlib.crc32(value) == fingerprint['crc32'], 'Catalog-bound staged bytes differ: ' + row['path'])
    return {relative: blob}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command', choices=('compile', 'verify'))
    parser.add_argument('--source', type=Path, default=IR)
    parser.add_argument('--out', type=Path, default=OUT)
    parser.add_argument('--romfs', type=Path, default=ROOT / 'romfs')
    args = parser.parse_args()
    if args.command == 'compile':
        blob = compile_file(args.source, args.out, args.romfs)
    else:
        blob = args.out.read_bytes(); decode(blob)
        require(blob == encode(load_ir(args.source), args.romfs), 'Resource catalog is stale')
    print('Native resource catalog: %d checked bytes, %d typed bindings' % (len(blob), len(decode(blob)['bindings'])))


if __name__ == '__main__':
    main()
