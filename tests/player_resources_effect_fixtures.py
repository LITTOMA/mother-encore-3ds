"""Manual corrupt format2 fixtures submitted to the actual C++ reader CLI.

This is never an automatic test or a GPU/source lifecycle admission.
"""
import argparse
from pathlib import Path
import struct
import subprocess
import tempfile
import zlib


def cases(raw):
    for label, offset, value in [('version', 8, 3), ('capability', 28, 3),
                                 ('rules', 32, 2)]:
        data = bytearray(raw)
        struct.pack_into('<I', data, offset, value)
        yield label, data
    data = bytearray(raw)
    data[-1] ^= 1
    yield 'crc', data
    # Format2's independent effect-resource mapping follows three SHA fields.
    data = bytearray(raw)
    count = struct.unpack_from('<I', data, 224)[0]
    assert count
    struct.pack_into('<I', data, 228, 2)  # Unknown effect type.
    struct.pack_into('<I', data, 20, zlib.crc32(data[128:]))
    yield 'unknown_effect', data
    data = bytearray(raw)
    # Alias two actual source mapping keys while retaining a valid checksum.
    assert count > 1
    data[240:248] = data[228:236]
    struct.pack_into('<I', data, 20, zlib.crc32(data[128:]))
    yield 'duplicate_source_mapping', data
    data = bytearray(raw)
    data[192:224] = bytes(32)
    struct.pack_into('<I', data, 20, zlib.crc32(data[128:]))
    yield 'missing_effect_ir', data


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--loader', type=Path, required=True)
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--pack', type=Path, required=True)
    parser.add_argument('--global-id', required=True)
    parser.add_argument('--pin', required=True)
    parser.add_argument('--global-sha', required=True)
    args = parser.parse_args()
    tail = [args.global_id, args.pin, args.global_sha]
    subprocess.run([str(args.loader), '--reader', str(args.root),
                    str(args.pack), *tail], check=True)
    with tempfile.TemporaryDirectory(prefix='player-effect-resources-') as tmp:
        for label, data in cases(args.pack.read_bytes()):
            file = Path(tmp) / (label + '.encresources')
            file.write_bytes(data)
            subprocess.run([str(args.loader), '--expect-reject', str(args.root),
                            str(file), *tail], check=True)
