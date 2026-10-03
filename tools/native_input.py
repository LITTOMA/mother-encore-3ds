#!/usr/bin/env python3
"""Strict compiler for the user-authorized native input tuning (not game data)."""
import argparse
import json
import math
from pathlib import Path
import struct
import zlib

ROOT = Path(__file__).resolve().parents[1]
IR = ROOT / 'content/native-input.json'
OUT = ROOT / 'romfs/input/native.encinput'
MAGIC = b'ENCINP01'
HEADER = struct.Struct('<8s6I')
PAYLOAD = struct.Struct('<10f4I')
SIZE = HEADER.size + PAYLOAD.size


def require(value, message):
    if not value:
        raise ValueError(message)


def fields(value, expected, label):
    require(type(value) is dict and set(value) == set(expected), 'Unknown/missing ' + label + ' fields')


def number(value):
    return type(value) in (int, float) and math.isfinite(value)


def validate(ir):
    fields(ir, ('schema', 'reason', 'circle_pad', 'touch', 'visual'), 'input')
    require(type(ir['schema']) is int and ir['schema'] == 1, 'Unsupported native input schema')
    require(type(ir['reason']) is str and 1 <= len(ir['reason']) <= 2048, 'Missing native adaptation reason')
    p, t, v = ir['circle_pad'], ir['touch'], ir['visual']
    fields(p, ('nominal_radius', 'activate_radius', 'release_radius', 'angular_hysteresis_degrees'), 'circle pad')
    fields(t, ('width', 'height', 'activate_radius', 'release_radius', 'tap_max_travel', 'tap_max_seconds'), 'touch')
    fields(v, ('base_radius', 'thumb_radius', 'base_rgba', 'thumb_rgba'), 'visual')
    floats = [p['nominal_radius'], p['activate_radius'], p['release_radius'], p['angular_hysteresis_degrees'],
              t['activate_radius'], t['release_radius'], t['tap_max_travel'], t['tap_max_seconds'],
              v['base_radius'], v['thumb_radius']]
    require(all(number(x) for x in floats), 'Non-finite or nonnumeric input tuning')
    require(0 < p['release_radius'] < p['activate_radius'] <= p['nominal_radius'] <= 32767, 'Invalid Circle Pad radii')
    require(0 <= p['angular_hysteresis_degrees'] < 22.5, 'Invalid angular hysteresis')
    require(type(t['width']) is int and type(t['height']) is int and (t['width'], t['height']) == (320, 240), 'Unsupported lower screen geometry')
    require(0 < t['release_radius'] < t['activate_radius'] <= 120, 'Invalid touch radii')
    require(0 < t['tap_max_travel'] <= t['activate_radius'] and 0 < t['tap_max_seconds'] <= 2, 'Invalid tap limits')
    require(0 < v['thumb_radius'] < v['base_radius'] <= 120, 'Invalid stick visual radii')
    colors = []
    for key in ('base_rgba', 'thumb_rgba'):
        rgba = v[key]
        require(type(rgba) is list and len(rgba) == 4 and all(type(x) is int and 0 <= x <= 255 for x in rgba), 'Invalid ' + key)
        require(0 < rgba[3] < 128, 'Stick overlay must remain translucent')
        colors.append(sum(x << (8*i) for i, x in enumerate(rgba)))
    # Reject inputs that become invalid/non-finite after binary32 conversion.
    try:
        rounded = PAYLOAD.unpack(PAYLOAD.pack(*floats, t['width'], t['height'], *colors))
    except (OverflowError, struct.error) as exc:
        raise ValueError('Unrepresentable tuning') from exc
    require(all(math.isfinite(x) for x in rounded[:10]), 'Unrepresentable tuning')
    require(0 < rounded[2] < rounded[1] <= rounded[0] <= 32767 and
            0 <= rounded[3] < 22.5 and 0 < rounded[5] < rounded[4] <= 120 and
            0 < rounded[6] <= rounded[4] and 0 < rounded[7] <= 2 and
            0 < rounded[9] < rounded[8] <= 120, 'Tuning collapses at binary32 precision')
    return rounded


def encode(ir):
    payload = PAYLOAD.pack(*validate(ir))
    blob = bytearray(HEADER.pack(MAGIC, 1, SIZE, 0, PAYLOAD.size, 0, 0) + payload)
    struct.pack_into('<I', blob, 16, zlib.crc32(blob))
    return bytes(blob)


def decode(blob):
    require(len(blob) == SIZE, 'Invalid native input size')
    magic, version, size, crc, payload_size, reserved0, reserved1 = HEADER.unpack_from(blob)
    require(magic == MAGIC and version == 1, 'Unsupported native input version')
    require(size == SIZE and payload_size == PAYLOAD.size and not reserved0 and not reserved1, 'Invalid native input header')
    checked = bytearray(blob)
    struct.pack_into('<I', checked, 16, 0)
    require(zlib.crc32(checked) == crc, 'Native input checksum mismatch')
    p = PAYLOAD.unpack_from(blob, HEADER.size)
    color = lambda value: [(value >> (8*i)) & 255 for i in range(4)]
    ir = dict(schema=1, reason='Decoded native input',
              circle_pad=dict(nominal_radius=p[0], activate_radius=p[1], release_radius=p[2], angular_hysteresis_degrees=p[3]),
              touch=dict(width=p[10], height=p[11], activate_radius=p[4], release_radius=p[5], tap_max_travel=p[6], tap_max_seconds=p[7]),
              visual=dict(base_radius=p[8], thumb_radius=p[9], base_rgba=color(p[12]), thumb_rgba=color(p[13])))
    validate(ir)
    return ir


def load_ir(path=IR):
    def unique_object(pairs):
        result = {}
        for key, value in pairs:
            require(key not in result, 'Duplicate native input JSON field: ' + key)
            result[key] = value
        return result
    return json.loads(Path(path).read_text(), object_pairs_hook=unique_object)


def compile_file(source=IR, output=OUT):
    blob = encode(load_ir(source))
    decode(blob)
    output = Path(output)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(blob)
    return blob


def stage_files(source_root):
    """Stage a RomFS-relative resource only after source and binary validation."""
    source_root = Path(source_root)
    relative = Path('input/native.encinput')
    path = (source_root / relative).resolve()
    require(path.is_relative_to(source_root.resolve()), 'Native input staged path escape')
    blob = path.read_bytes()
    decode(blob)
    require(blob == encode(load_ir()), 'Native input resource is stale')
    return {relative: blob}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command', choices=('compile', 'verify'))
    parser.add_argument('--source', type=Path, default=IR)
    parser.add_argument('--out', type=Path, default=OUT)
    args = parser.parse_args()
    if args.command == 'compile':
        blob = compile_file(args.source, args.out)
    else:
        blob = args.out.read_bytes()
        decode(blob)
        require(blob == encode(load_ir(args.source)), 'Native input resource is stale')
    print('Native input tuning: %d checked bytes' % len(blob))


if __name__ == '__main__':
    main()
