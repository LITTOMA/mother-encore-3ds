#!/usr/bin/env python3
"""Compile and validate the bounded, independently loadable ENCITM01 resource."""
import argparse
import hashlib
import json
import math
import struct
import sys
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
PIN = '7d9246600fffe518408f5830d4848635019005a3'
NO_INDEX = 0xffffffff
SECTION_NAMES = ['Strings', 'Metadata', 'Definitions', 'Instances', 'Resources',
                 'Layouts', 'Parameters', 'Clips', 'Tracks', 'Keys', 'Sounds']
FORMATS = [None, '<4I', '<6I4i2I', '<4I', '<7I32s', '<7I10f4I',
           '<I4f', '<4IfI', '<5I', '<6f', '<4I']
STRIDES = [1] + [struct.calcsize(f) for f in FORMATS[1:]]
HEADER = 64 + 16 * len(STRIDES)
PARAMETERS = ['SourceViewport', 'PlatformViewport', 'GridShape', 'LabelSize',
              'CursorOffset', 'CursorMotion', 'InfoMotion', 'DisabledColor',
              'NormalColor', 'ScrollColor', 'InputBinding', 'InputRepeat']
LAYOUT_ROLES = ['Container', 'Panel', 'Grid', 'ItemLabel', 'ItemIcon', 'Equipped',
                'Cursor', 'InfoPanel', 'Description', 'Scrollbar',
                'ScrollBackground', 'ScrollThumb', 'Hint']
DRAW_KINDS = ['Container', 'Sprite', 'Rectangle', 'NinePatch', 'Text']
CLIP_ROLES = ['Open', 'Close', 'CursorIdle']
PROPERTIES = ['Position', 'PositionX', 'PositionY', 'Scale', 'Alpha', 'Visible',
              'Rect', 'Frame', 'Offset']
SOUND_EVENTS = ['Open', 'Move', 'Close', 'Disabled', 'Confirm']


class ContentError(ValueError):
    pass


def require(ok, message):
    if not ok:
        raise ContentError(message)


def fields(obj, names, label):
    require(isinstance(obj, dict) and set(obj) == set(names),
            'Unknown/missing ' + label + ' fields')


def finite(v):
    return isinstance(v, (float, int)) and not isinstance(v, bool) and math.isfinite(v)


def uint(v, maximum=NO_INDEX):
    return isinstance(v, int) and not isinstance(v, bool) and 0 <= v <= maximum


def vector(v, n):
    require(isinstance(v, list) and len(v) == n and all(finite(x) for x in v), 'Items vector')
    return v


def safe_path(value):
    return (isinstance(value, str) and bool(value) and ':' not in value and '\\' not in value
            and all(ord(c) >= 32 and ord(c) != 127 for c in value)
            and all(s not in ('', '.', '..') for s in value.split('/')))


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def verify_sources(ir, root=ROOT):
    require(ir.get('schema') == 1 and ir.get('kind') == 'encore.native-items.source-ir'
            and ir.get('commit') == PIN, 'Unreviewed Items schema/pin')
    inventory = json.loads((root / 'compatibility/upstream-inventory.json').read_text())
    require(inventory['commit'] == PIN, 'Items inventory pin')
    report = json.loads((root / 'reports/items-menu-source/source-review.json').read_text())
    require(report['commit'] == PIN and report['sources'] == ir['sources']
            and report['dependencies'] == ir['dependencies'], 'Items source review mismatch')
    require(isinstance(ir['sources'], dict) and ir['sources'], 'Missing Items sources')
    for path, sha in ir['sources'].items():
        require(safe_path(path) and path in inventory['files']
                and inventory['files'][path]['sha256'] == sha
                and digest(root / 'upstream/MOTHER-Encore' / path) == sha,
                'Changed Items source ' + str(path))
    require(isinstance(ir['dependencies'], dict), 'Items dependencies')
    for path, sha in ir['dependencies'].items():
        require(safe_path(path) and path.startswith(('content/', 'romfs/'))
                and digest(root / path) == sha, 'Changed Items dependency ' + str(path))

    from tools.items_presentation_bindings import check_round
    check_round(ir, root)


def lower(ir, root=ROOT, verify_assets=True):
    fields(ir, ['schema', 'kind', 'commit', 'sources', 'dependencies', 'scope', 'capacity',
                'owner', 'definitions', 'initial_inventory', 'resources', 'layouts',
                'parameters', 'clips', 'sounds'], 'Items IR')
    require(ir['schema'] == 1 and ir['kind'] == 'encore.native-items.source-ir'
            and ir['commit'] == PIN, 'Unreviewed Items IR')
    require(isinstance(ir['scope'], str) and ir['scope'], 'Items scope')
    pool = bytearray(b'\0')
    strings = {'': 0}

    def string(s):
        require(isinstance(s, str) and '\0' not in s, 'Items string')
        encoded = s.encode('utf-8')
        require(len(encoded) <= 4096, 'Items string length')
        if s not in strings:
            strings[s] = len(pool)
            pool.extend(encoded + b'\0')
        return strings[s]

    def enum(s, names, label):
        require(s in names, 'Unknown Items ' + label)
        return names.index(s) + 1

    def index(v):
        return NO_INDEX if v is None else v

    t = {name: [] for name in SECTION_NAMES}
    require(uint(ir['capacity'], 64) and uint(ir['owner']), 'Items metadata integer')
    t['Metadata'] = [[ir['capacity'], ir['owner'], 0, 0]]
    for r in ir['resources']:
        fields(r, ['id', 'path', 'kind', 'width', 'height', 'columns', 'rows', 'sha256'], 'resource')
        require(safe_path(r['path']), 'Items resource path')
        sha = bytes.fromhex(r['sha256'])
        require(len(sha) == 32, 'Items resource SHA')
        if verify_assets:
            require(digest(root / 'romfs' / r['path']) == r['sha256'], 'Changed Items resource ' + r['path'])
        t['Resources'].append([r['id'], string(r['path']), r['kind'], r['width'], r['height'], r['columns'], r['rows'], sha])
    for d in ir['definitions']:
        fields(d, ['id', 'source', 'name', 'description', 'icon', 'equipment_slot', 'heal_hp',
                   'heal_pp', 'max_hp_boost', 'max_pp_boost', 'flags', 'can_use'], 'definition')
        t['Definitions'].append([d['id'], string(d['source']), string(d['name']), string(d['description']),
                                 index(d['icon']), index(d['equipment_slot']), d['heal_hp'], d['heal_pp'],
                                 d['max_hp_boost'], d['max_pp_boost'], d['flags'], d['can_use']])
    for i in ir['initial_inventory']:
        fields(i, ['id', 'definition', 'equipped', 'doses'], 'initial instance')
        t['Instances'].append([i['id'], i['definition'], i['equipped'], i['doses']])
    for l in ir['layouts']:
        fields(l, ['id', 'role', 'parent', 'kind', 'resource', 'frame', 'flags', 'anchor', 'rect', 'color', 'patch'], 'layout')
        require(isinstance(l['patch'], list) and len(l['patch']) == 4 and all(uint(x) for x in l['patch']), 'Items patch')
        t['Layouts'].append([l['id'], enum(l['role'], LAYOUT_ROLES, 'layout role'), index(l['parent']),
                              enum(l['kind'], DRAW_KINDS, 'draw kind'), index(l['resource']), l['frame'], l['flags'],
                              *vector(l['anchor'], 2), *vector(l['rect'], 4), *vector(l['color'], 4), *l['patch']])
    fields(ir['parameters'], PARAMETERS, 'parameter')
    t['Parameters'] = [[i + 1, *vector(ir['parameters'][name], 4)] for i, name in enumerate(PARAMETERS)]
    for c in ir['clips']:
        fields(c, ['id', 'role', 'duration', 'loop', 'tracks'], 'clip')
        first_track = len(t['Tracks'])
        for a in c['tracks']:
            fields(a, ['target', 'property', 'interpolation', 'keys'], 'track')
            first_key = len(t['Keys'])
            for k in a['keys']:
                fields(k, ['time', 'ease', 'value'], 'key')
                t['Keys'].append([k['time'], k['ease'], *vector(k['value'], 4)])
            t['Tracks'].append([a['target'], enum(a['property'], PROPERTIES, 'property'), a['interpolation'], first_key, len(a['keys'])])
        t['Clips'].append([c['id'], enum(c['role'], CLIP_ROLES, 'clip role'), first_track, len(c['tracks']), c['duration'], c['loop']])
    for s in ir['sounds']:
        fields(s, ['event', 'path', 'audio_id'], 'sound')
        t['Sounds'].append([enum(s['event'], SOUND_EVENTS, 'sound event'), string(s['path']), s['audio_id'], 0])
    t['Strings'] = bytes(pool)
    validate(t)
    return t


def encode(t, commit=PIN):
    require(commit == PIN, 'Items compiler pin')
    validate(t)
    data = bytearray(HEADER)
    for i, name in enumerate(SECTION_NAMES):
        block = t[name] if i == 0 else b''.join(struct.pack(FORMATS[i], *r) for r in t[name])
        if block:
            while len(data) % 4:
                data.append(0)
        off = len(data) if block else 0
        struct.pack_into('<HHIII', data, 64 + i * 16, i + 1, STRIDES[i], off, len(block) // STRIDES[i], len(block))
        data.extend(block)
    struct.pack_into('<8s6I20s12x', data, 0, b'ENCITM01', 1, len(data), 0, len(STRIDES), 1, 1, bytes.fromhex(commit))
    struct.pack_into('<I', data, 16, zlib.crc32(data))
    return bytes(data)


def parse_pack(blob):
    require(HEADER <= len(blob) <= 1024 * 1024, 'Items size')
    magic, version, size, crc, n, caps, rules, commit = struct.unpack_from('<8s6I20s', blob)
    require(magic == b'ENCITM01' and version == caps == rules == 1 and size == len(blob)
            and n == len(STRIDES) and commit.hex() == PIN and not any(blob[52:64]), 'Items header/pin')
    copy = bytearray(blob)
    struct.pack_into('<I', copy, 16, 0)
    require(zlib.crc32(copy) == crc, 'Items CRC')
    t = {}
    end = HEADER
    for i, name in enumerate(SECTION_NAMES):
        kind, stride, off, count, amount = struct.unpack_from('<HHIII', blob, 64 + i * 16)
        require(kind == i + 1 and stride == STRIDES[i] and amount == count * stride, 'Items directory')
        if count:
            require(off % 4 == 0 and off >= end and off + amount <= len(blob)
                    and not any(blob[end:off]), 'Items section span/padding')
            block = blob[off:off + amount]
            end = off + amount
        else:
            require(off == amount == 0, 'Items empty section')
            block = b''
        t[name] = bytes(block) if i == 0 else list(struct.iter_unpack(FORMATS[i], block))
    require(end == len(blob), 'Items trailing bytes')
    validate(t)
    return t


def validate(t):
    require(set(t) == set(SECTION_NAMES), 'Items sections')
    p = t['Strings']
    require(isinstance(p, bytes) and 1 <= len(p) <= 65536 and p[0] == p[-1] == 0, 'Items strings')
    p.decode('utf-8')
    require(all(len(s) <= 4096 for s in p.split(b'\0')), 'Items string length')

    def string(o):
        require(uint(o) and o < len(p) and (o == 0 or p[o - 1] == 0), 'Items string offset')
        return p[o:p.index(0, o)].decode('utf-8')

    limits = {'Metadata': (1, 1), 'Definitions': (1, 256), 'Instances': (0, 64), 'Resources': (1, 64),
              'Layouts': (1, 128), 'Parameters': (len(PARAMETERS), len(PARAMETERS)), 'Clips': (3, 3),
              'Tracks': (1, 256), 'Keys': (1, 2048), 'Sounds': (len(SOUND_EVENTS), len(SOUND_EVENTS))}
    for name, (lo, hi) in limits.items():
        require(lo <= len(t[name]) <= hi, 'Items ' + name + ' capacity')
        for row in t[name]:
            try:
                struct.pack(FORMATS[SECTION_NAMES.index(name)], *row)
            except (struct.error, OverflowError, TypeError) as ex:
                raise ContentError('Items ' + name + ' record') from ex
    for name in ['Definitions', 'Instances', 'Resources', 'Layouts', 'Clips']:
        ids = [r[0] for r in t[name]]
        require(all(uint(i) and i > 0 for i in ids) and len(ids) == len(set(ids)), 'Items duplicate/zero ID')
    meta = t['Metadata'][0]
    require(all(uint(x) for x in meta) and 1 <= meta[0] <= 64 and meta[1] > 0
            and list(meta[2:]) == [0, 0], 'Items metadata')
    require(len(t['Instances']) <= meta[0], 'Items inventory capacity')
    resource_paths = set()
    for r in t['Resources']:
        require(safe_path(string(r[1])) and r[2] in (1, 2, 3) and all(1 <= x <= 8192 for x in r[3:5])
                and all(1 <= x <= 256 for x in r[5:7]) and any(r[7])
                and (r[2] != 1 or (r[3] % r[5] == r[4] % r[6] == 0)), 'Items resource')
        require(string(r[1]) not in resource_paths, 'Items duplicate resource path')
        resource_paths.add(string(r[1]))
    sources = set()
    for d in t['Definitions']:
        require(safe_path(string(d[1])) and string(d[1]) not in sources and string(d[2]) and string(d[3]), 'Items definition strings')
        sources.add(string(d[1]))
        require((d[4] == NO_INDEX or (d[4] < len(t['Resources']) and t['Resources'][d[4]][2] == 1)) and d[10] & ~1 == 0 and d[11] in (0, 1)
                and all(-65535 <= v <= 65535 for v in d[6:10])
                and ((d[10] & 1 and d[5] < 4) or (not d[10] & 1 and d[5] == NO_INDEX)), 'Items definition')
    equipped_slots = set()
    for i in t['Instances']:
        require(i[1] < len(t['Definitions']) and i[2] in (0, 1) and 1 <= i[3] <= 65535, 'Items instance')
        if i[2]:
            d = t['Definitions'][i[1]]
            require(d[10] & 1 and d[5] not in equipped_slots, 'Items equipped instance')
            equipped_slots.add(d[5])
    for i, l in enumerate(t['Layouts']):
        require(1 <= l[1] <= len(LAYOUT_ROLES) and (l[2] == NO_INDEX or l[2] < i)
                and 1 <= l[3] <= len(DRAW_KINDS) and l[6] & ~15 == 0
                and (not l[6] & 4 or l[3] == 2) and (not l[6] & 8 or l[3] != 1)
                and all(finite(x) and abs(x) <= 8192 for x in l[7:17])
                and all(0 <= x <= 1 for x in l[7:9]) and all(0 <= x <= 8192 for x in l[11:13])
                and all(0 <= x <= 1 for x in l[13:17]) and all(0 <= x <= 8192 for x in l[17:21]), 'Items layout')
        if l[3] in (2, 4):
            require(l[4] < len(t['Resources']), 'Items layout resource')
            r = t['Resources'][l[4]]
            require(r[2] == 1 and l[5] < r[5] * r[6] and l[11] > 0 and l[12] > 0, 'Items sprite layout')
            if l[3] == 4:
                require(l[17] + l[19] <= r[3] // r[5] and l[18] + l[20] <= r[4] // r[6], 'Items patch margins')
        else:
            require(l[4] == NO_INDEX or (l[3] == 5 and l[4] < len(t['Resources'])), 'Items unused layout resource')
        require(l[3] == 4 or not any(l[17:21]), 'Items unused patch')
    roles = [l[1] for l in t['Layouts']]
    require(roles.count(3) == roles.count(8) == 1 and all(r in roles for r in (2, 4, 9))
            and any(l[1] == 7 and l[3] == 2 for l in t['Layouts']), 'Items required layout bindings')
    params = set()
    for row in t['Parameters']:
        key, *v = row
        require(1 <= key <= len(PARAMETERS) and key not in params and all(finite(x) for x in v), 'Items parameter')
        params.add(key)
        if key in (1, 2, 4):
            require(all(0 < x <= 8192 for x in v[:2]) and v[2:] == [0, 0], 'Items dimensions')
        elif key == 3:
            require(all(1 <= x <= 64 and x == math.floor(x) for x in v[:2])
                    and all(0 < x <= 8192 for x in v[2:]), 'Items grid')
        elif key in (8, 9, 10):
            require(all(0 <= x <= 1 for x in v), 'Items parameter color')
        elif key == 11:
            require(all(0 <= x <= 0xffffffff and x == math.floor(x) for x in v)
                    and 0 < v[2] <= 0x80000000 and int(v[2]) & (int(v[2]) - 1) == 0
                    and v[3] == 0, 'Items input binding')
        elif key == 12:
            require(0 < v[1] <= v[0] <= 10 and v[2:] == [0, 0], 'Items input repeat timing')
        else:
            require(all(abs(x) <= 8192 for x in v), 'Items parameter range')
            if key == 5:
                require(v[2] > 0 and v[3] > 0, 'Items cursor dimensions')
            if key == 6:
                require(all(0 < x <= 120 for x in v[:3]) and v[3] == 0, 'Items cursor timing')
            if key == 7:
                require(0 < v[0] <= 120 and v[1] >= 0 and v[2:] == [0, 0], 'Items info motion')
    owned_tracks = set()
    owned_keys = set()
    roles = set()
    for c in t['Clips']:
        require(1 <= c[1] <= len(CLIP_ROLES) and c[1] not in roles and c[3] > 0
                and c[2] + c[3] <= len(t['Tracks']) and finite(c[4]) and 0 < c[4] <= 120
                and c[5] in (0, 1), 'Items clip')
        roles.add(c[1])
        targets = set()
        for n in range(c[2], c[2] + c[3]):
            require(n not in owned_tracks, 'Items track ownership')
            owned_tracks.add(n)
            a = t['Tracks'][n]
            require(a[0] < len(t['Layouts']) and 1 <= a[1] <= len(PROPERTIES) and a[2] in (0, 1)
                    and a[4] > 0 and a[3] + a[4] <= len(t['Keys']) and (a[0], a[1]) not in targets, 'Items track')
            targets.add((a[0], a[1]))
            previous = -1
            for j in range(a[3], a[3] + a[4]):
                k = t['Keys'][j]
                require(j not in owned_keys and all(finite(x) for x in k) and 0 <= k[0] <= c[4]
                        and k[0] > previous and abs(k[1]) <= 100 and all(abs(x) <= 8192 for x in k[2:]), 'Items key')
                owned_keys.add(j)
                previous = k[0]
                if a[1] in (5, 6):
                    require(0 <= k[2] <= 1 and (a[1] != 6 or k[2] in (0, 1)), 'Items alpha/visibility')
                if a[1] == 4:
                    require(all(0 < x <= 16 for x in k[2:4]), 'Items scale')
                if a[1] == 7:
                    require(k[4] >= 0 and k[5] >= 0, 'Items animated rectangle')
                if a[1] == 8:
                    l = t['Layouts'][a[0]]
                    require(l[3] in (2, 4) and a[2] == 1 and k[2] == math.floor(k[2])
                            and 0 <= k[2] < t['Resources'][l[4]][5] * t['Resources'][l[4]][6], 'Items animated frame')
    require(len(owned_tracks) == len(t['Tracks']) and len(owned_keys) == len(t['Keys']), 'Items orphan animation')
    events = set()
    for s in t['Sounds']:
        path = string(s[1])
        require(1 <= s[0] <= len(SOUND_EVENTS) and s[0] not in events and uint(s[2]) and s[2] > 0 and s[3] == 0
                and (safe_path(path) or (path.startswith('res://') and safe_path(path[6:]))), 'Items sound')
        events.add(s[0])


def stage_files(source):
    blob = (source / 'data/opening.encitems').read_bytes()
    t = parse_pack(blob)
    p = t['Strings']
    out = {Path('data/opening.encitems'): blob}
    for r in t['Resources']:
        path = p[r[1]:p.index(0, r[1])].decode()
        full = (source / path).resolve()
        require(full.is_relative_to(source.resolve()), 'Items staged escape')
        data = full.read_bytes()
        require(hashlib.sha256(data).digest() == r[7], 'Items staged hash')
        out[Path(path)] = data
    return out


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('action', nargs='?', choices=['compile', 'verify'], default='compile')
    parser.add_argument('--input', type=Path, default=ROOT / 'content/native-items.json')
    parser.add_argument('--out', type=Path, default=ROOT / 'romfs/data/opening.encitems')
    args = parser.parse_args()
    try:
        ir = json.loads(args.input.read_text())
        verify_sources(ir)
        blob = encode(lower(ir), ir['commit'])
        parse_pack(blob)
        if args.action == 'compile':
            args.out.parent.mkdir(parents=True, exist_ok=True)
            args.out.write_bytes(blob)
        else:
            require(args.out.read_bytes() == blob, 'Items binary is stale')
        print('Items resource:', len(blob), 'bytes;', len(ir['definitions']), 'definitions;', len(ir['initial_inventory']), 'initial instances')
    except (ValueError, KeyError, TypeError, OSError, struct.error, OverflowError) as ex:
        print('Items resource rejected:', ex, file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
