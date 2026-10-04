#!/usr/bin/env python3
"""Compile the ORIGINAL sandbox IR to a bounded, little-endian ENCPAK01 file.
This does NOT import arbitrary Godot scenes or EncoreScript. Unknown fields/opcodes fail.
Only Python's standard library is required. See docs/CONTENT_FORMAT.md.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys
import zlib

OP = {"END": 0, "SAY": 1, "SET_FLAG": 2, "IF_FLAG": 3,
      "JUMP": 4, "WAIT": 5, "TELEPORT": 6, "BATTLE": 7}
KIND = {"guide": 0, "door": 1, "terminal": 2, "sign": 3}
FAMILY = 0x454e0001

class ContentError(ValueError):
    pass

def check(condition: bool, message: str) -> None:
    if not condition:
        raise ContentError(message)

def integer(value, low: int, high: int, context: str) -> int:
    check(type(value) is int and low <= value <= high, f"{context}: expected integer {low}..{high}")
    return value

def fields(obj: dict, required: set[str], context: str) -> None:
    check(isinstance(obj, dict), f"{context}: expected object")
    check(set(obj) == required, f"{context}: missing {required-set(obj)}, unknown {set(obj)-required}")

def table(items: list, context: str) -> dict[str, int]:
    check(isinstance(items, list), f"{context}: expected array")
    keys, ids = {}, set()
    for index, item in enumerate(items):
        check(isinstance(item, dict), f"{context}[{index}]: expected object")
        key = item.get("key")
        check(isinstance(key, str) and key and key not in keys, f"{context}: missing/duplicate key")
        ident = integer(item.get("id"), 1, 0xffffffff, f"{context}.{key}.id")
        check(ident not in ids, f"{context}: duplicate stable ID {ident}")
        keys[key] = index
        ids.add(ident)
    return keys

def compile_content(data: dict) -> tuple[bytes, dict]:
    fields(data, {"format", "rules", "family", "required_capabilities", "start", "flags", "maps", "programs", "enemies"}, "root")
    check(type(data["format"]) is int and type(data["rules"]) is int and type(data["family"]) is int
          and data["format"] == 1 and data["rules"] == 1 and data["family"] == FAMILY, "Unsupported format/rules/family")
    caps = integer(data["required_capabilities"], 0, 15, "required_capabilities")
    # This compiler emits all four capability categories even if a particular pack uses a subset.
    check(caps == 15, "This compiler requires the complete M0 capability set (15)")
    mi, pi, ei, fi = (table(data[k], k) for k in ("maps", "programs", "enemies", "flags"))
    check(1 <= len(mi) <= 64 and 1 <= len(pi) <= 512 and len(ei) <= 128 and len(fi) <= 32, "Table limits exceeded")
    strings: list[str] = []
    string_ids: dict[str, int] = {}
    def text(s) -> int:
        check(isinstance(s, str) and '\0' not in s and len(s.encode('utf-8')) <= 1024, "Invalid/oversized text")
        if s not in string_ids:
            string_ids[s] = len(strings)
            strings.append(s)
        return string_ids[s]
    def ref(index, key, context):
        check(isinstance(key, str) and key in index, f"Unknown {context}: {key!r}")
        return index[key]
    def position(m, x, y, context):
        x = integer(x, 0, len(m['tiles'][0])*16-1, context+'.x')
        y = integer(y, 0, len(m['tiles'])*16-1, context+'.y')
        check(m['tiles'][y//16][x//16] == '.', f"{context}: position must be walkable")
        return x, y
    maps_bin, programs_bin, enemies_bin = bytearray(), bytearray(), bytearray()
    object_ids = set()
    for m in data['maps']:
        fields(m, {'key', 'id', 'title', 'tiles', 'objects'}, 'map')
        rows = m['tiles']
        check(isinstance(rows, list) and 3 <= len(rows) <= 64 and all(isinstance(r, str) for r in rows), 'Invalid tile rows')
        w = len(rows[0])
        check(3 <= w <= 64 and all(len(r) == w and set(r) <= set('.#~') for r in rows), 'Ragged/unknown tiles')
        check(isinstance(m['objects'], list) and len(m['objects']) <= 128, 'Too many objects')
        maps_bin += struct.pack('<IIHHHH', m['id'], text(m['title']), w, len(rows), len(m['objects']), 0)
        maps_bin += bytes('.#~'.index(c) for row in rows for c in row)
        for o in m['objects']:
            fields(o, {'id', 'kind', 'name', 'x', 'y', 'program'}, 'object')
            ident = integer(o['id'], 1, 0xffffffff, 'object.id')
            check(ident not in object_ids, 'Duplicate global object ID')
            object_ids.add(ident)
            check(o['kind'] in KIND, f"Unsupported native object kind: {o['kind']}")
            x, y = position(m, o['x'], o['y'], 'object')
            maps_bin += struct.pack('<IB3xhhII', ident, KIND[o['kind']], x, y, ref(pi, o['program'], 'program'), text(o['name']))
    source_map = {}
    for p in data['programs']:
        fields(p, {'key', 'id', 'code'}, 'program')
        check(isinstance(p['code'], list) and p['code'], 'Empty program')
        labels, instructions, paths = {}, [], []
        for source_index, item in enumerate(p['code']):
            check(isinstance(item, dict), 'Instruction must be object')
            if 'label' in item:
                fields(item, {'label'}, 'label')
                label = item['label']
                check(isinstance(label, str) and label and label not in labels, 'Invalid/duplicate label')
                labels[label] = len(instructions)
            else:
                instructions.append(item)
                paths.append(f"/programs/{pi[p['key']]}/code/{source_index}")
        check(1 <= len(instructions) <= 2048 and instructions[-1].get('op') == 'END', 'Program must end with END; max 2048 instructions')
        programs_bin += struct.pack('<II', p['id'], len(instructions))
        def jump(label):
            check(isinstance(label, str) and label in labels and labels[label] < len(instructions), 'Unknown/out-of-range label')
            return labels[label]
        for ix, item in enumerate(instructions):
            op = item.get('op')
            check(op in OP, f"Unsupported opcode {op!r} in {p['key']} at {paths[ix]}")
            a = b = c = 0
            if op == 'END':
                fields(item, {'op'}, op)
            elif op == 'SAY':
                fields(item, {'op', 'text'}, op); a = text(item['text'])
            elif op in ('SET_FLAG', 'IF_FLAG'):
                fields(item, {'op', 'flag', 'value'} | ({'target'} if op == 'IF_FLAG' else set()), op)
                a = ref(fi, item['flag'], 'flag')
                check(type(item['value']) is bool, 'Flag value must be boolean')
                b = int(item['value'])
                if op == 'IF_FLAG': c = jump(item['target'])
            elif op == 'JUMP':
                fields(item, {'op', 'target'}, op); a = jump(item['target'])
            elif op == 'WAIT':
                fields(item, {'op', 'ticks'}, op); a = integer(item['ticks'], 0, 36000, 'wait ticks')
            elif op == 'TELEPORT':
                fields(item, {'op', 'map', 'x', 'y'}, op)
                a = ref(mi, item['map'], 'map')
                b, c = position(data['maps'][a], item['x'], item['y'], 'teleport')
            elif op == 'BATTLE':
                fields(item, {'op', 'enemy'}, op); a = ref(ei, item['enemy'], 'enemy')
            programs_bin += struct.pack('<B3xiii', OP[op], a, b, c)
            source_map[f"{p['id']}:{ix}"] = {'json_pointer': paths[ix], 'program': p['key'], 'op': op}
    for e in data['enemies']:
        fields(e, {'key', 'id', 'name', 'hp', 'attack', 'defense', 'reward'}, 'enemy')
        enemies_bin += struct.pack('<IIiiii', e['id'], text(e['name']),
            integer(e['hp'], 1, 9999, 'hp'), integer(e['attack'], 0, 999, 'attack'),
            integer(e['defense'], 0, 999, 'defense'), integer(e['reward'], 0, 9999, 'reward'))
    flags_bin = bytearray()
    for f in data['flags']:
        fields(f, {'key', 'id'}, 'flag'); flags_bin += struct.pack('<I', f['id'])
    check(len(strings) <= 1024, 'Too many strings')
    strings_bin = b''.join(struct.pack('<H', len(s.encode('utf-8'))) + s.encode('utf-8') for s in strings)
    payload = strings_bin + maps_bin + programs_bin + enemies_bin + flags_bin
    start = data['start']; fields(start, {'map', 'x', 'y'}, 'start')
    sm = ref(mi, start['map'], 'start map'); sx, sy = position(data['maps'][sm], start['x'], start['y'], 'spawn')
    header = struct.pack('<8s14I', b'ENCPAK01', 1, 1, len(payload), zlib.crc32(payload), caps, FAMILY,
        len(strings), len(mi), len(pi), len(ei), len(fi), sm, sx, sy)
    check(len(header+payload) <= 4*1024*1024, 'Pack exceeds 4 MiB limit')
    metadata = {'format': 1, 'rules': 1, 'family': FAMILY, 'capabilities': caps,
        'maps': len(mi), 'programs': len(pi), 'enemies': len(ei), 'flags': len(fi),
        'strings': len(strings), 'bytes': len(header+payload), 'source_map': source_map}
    return header+payload, metadata

def write(path: Path, data: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix+'.tmp')
    temporary.write_bytes(data)
    temporary.replace(path)

def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input', type=Path, default=Path('content/sandbox.json'))
    parser.add_argument('--out', type=Path, default=Path('build/fixtures/sandbox.encpak'))
    parser.add_argument('--manifest', type=Path, default=Path('build/content-manifest.json'))
    parser.add_argument('--source-map', type=Path, default=Path('build/story-source-map.json'))
    a = parser.parse_args()
    try:
        source = a.input.read_bytes()
        data = json.loads(source)
        blob, meta = compile_content(data)
        source_map = meta.pop('source_map')
        meta.update({'source': a.input.as_posix(), 'source_sha256': hashlib.sha256(source).hexdigest(),
            'compiler_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
            'pack_sha256': hashlib.sha256(blob).hexdigest(), 'upstream_commit': None,
            'content_origin': 'original integration fixture, NOT Mother: Encore'})
        write(a.out, blob)
        write(a.manifest, (json.dumps(meta, indent=2, sort_keys=True)+'\n').encode())
        write(a.source_map, (json.dumps(source_map, indent=2, sort_keys=True)+'\n').encode())
        print(f"Content: {len(blob)} bytes; {meta['maps']} maps; {meta['programs']} programs; {meta['pack_sha256']}")
        return 0
    except (OSError, ValueError, KeyError, TypeError, struct.error) as error:
        print(f'CONTENT ERROR: {error}', file=sys.stderr)
        return 1

if __name__ == '__main__':
    raise SystemExit(main())
