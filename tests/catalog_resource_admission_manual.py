#!/usr/bin/env python3
"""Manual all-family positive and negative admission; not run by normal builds.
Usage: python tests/catalog_resource_admission_manual.py HARNESS ROOT BUILD_FIXTURE
The supplied C++ harness uses the production admission consumer. Only a dedicated
build directory is written. Real pack bytes are copied, never synthesized as game
content. Each malformed magic is re-fingerprinted in the copied directory so that
catalog.verify_files succeeds and the actual typed loader must reject it.
"""
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import zlib


def u32(raw, offset):
    return struct.unpack_from('<I', raw, offset)[0]


def put(raw, offset, value):
    struct.pack_into('<I', raw, offset, value)


def main():
    if len(sys.argv) != 4:
        raise SystemExit(__doc__)
    harness, root, fixture = map(lambda value: Path(value).resolve(), sys.argv[1:])
    if 'build' not in fixture.parts or fixture == root or root in fixture.parents:
        raise SystemExit('Use an independent fixture directory under build')
    catalog_path = root / 'data/native.encresources'
    blob = catalog_path.read_bytes()
    subprocess.run([str(harness), str(catalog_path), str(root)], check=True)
    if fixture.exists():
        raise SystemExit('Fixture directory must be new; existing data is preserved')
    shutil.copytree(root, fixture)
    at = 60
    rows = []
    for _ in range(u32(blob, 52)):
        length = u32(blob, at + 16)
        path = blob[at + 20:at + 20 + length].decode('ascii')
        rows.append((at, path))
        at += 20 + length
    target_catalog = fixture / 'data/native.encresources'
    for offset, name in rows:
        pack = fixture / name
        original = pack.read_bytes()
        broken = bytearray(original)
        broken[0] ^= 1  # reject typed family identity, not just catalog fingerprint
        pack.write_bytes(broken)
        candidate = bytearray(blob)
        put(candidate, offset + 12, zlib.crc32(broken))
        put(candidate, 16, zlib.crc32(candidate[32:]))
        target_catalog.write_bytes(candidate)
        subprocess.run([str(harness), str(target_catalog), str(fixture), name], check=True)
        pack.write_bytes(original)
        target_catalog.write_bytes(blob)
    # Unknown next family remains rejected at the directory layer itself.
    candidate = bytearray(blob)
    put(candidate, rows[-1][0] + 4, 0xffffffff)
    put(candidate, 16, zlib.crc32(candidate[32:]))
    target_catalog.write_bytes(candidate)
    result = subprocess.run([str(harness), str(target_catalog), str(fixture)])
    if result.returncode != 3:
        raise SystemExit('Unknown catalog family was not rejected')
    target_catalog.write_bytes(blob)


if __name__ == '__main__':
    main()
