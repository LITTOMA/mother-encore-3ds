"""Require a positive replay, then checked rejection of seven malformed fixtures."""
import argparse
import hashlib
import json
import shutil
import struct
import subprocess
import tempfile
from pathlib import Path


def check(executable, fixture):
    executable, fixture = Path(executable).resolve(), Path(fixture).resolve()
    root = Path(__file__).resolve().parents[1]
    if not executable.is_file() or root / 'build' not in fixture.parents:
        raise ValueError('Prepared dropped executable/fixture required')
    raw = (fixture / 'native.bin').read_bytes()
    meta = json.loads((fixture / 'source.json').read_bytes())
    if meta['bytes'] != len(raw) or meta['sha256'] != hashlib.sha256(raw).hexdigest():
        raise ValueError('Dropped fixture fingerprint changed')
    result = subprocess.run([str(executable), str(fixture)], capture_output=True, timeout=30)
    if result.returncode:
        raise ValueError('Positive dropped replay failed: ' + repr((result.returncode, result.stdout, result.stderr)))
    negatives = [('truncated', raw[:8]), ('unknown-magic', b'UNKNOWN!' + raw[8:]),
                 ('unowned-tail', raw + b'\0'), ('old-version', b'ENCDRPB1' + raw[8:])]
    for key in ['first_alive', 'first_cover_animation', 'first_initial_count']:
        at = meta['test_offsets'][key]
        if type(at) is not int or not 8 <= at <= len(raw) - 4:
            raise ValueError('Dropped negative fixture field offset')
        changed = bytearray(raw)
        struct.pack_into('<I', changed, at, 99)
        negatives.append((key, bytes(changed)))
    receipts = []
    with tempfile.TemporaryDirectory(prefix='dropped-negative-', dir=fixture.parent) as name:
        target = Path(name)
        for file in ['source.encdrp', 'breakables.encbrk']:
            shutil.copyfile(fixture / file, target / file)
        for label, value in negatives:
            (target / 'native.bin').write_bytes(value)
            result = subprocess.run([str(executable), str(target)], capture_output=True, timeout=30)
            if result.returncode != 1 or not result.stderr.startswith(b'Dropped source failure ') or any(
                    marker in result.stderr for marker in [b'AddressSanitizer', b'runtime error:']):
                raise ValueError('Malformed dropped fixture did not fail checked: ' + repr((label, result.returncode, result.stdout, result.stderr)))
            receipts.append(dict(case=label, sha256=hashlib.sha256(value).hexdigest(), return_code=result.returncode,
                                 stdout=result.stdout.decode('utf-8'), stderr=result.stderr.decode('utf-8')))
    print(json.dumps(dict(executable_sha256=hashlib.sha256(executable.read_bytes()).hexdigest(),
                          fixture_sha256=meta['sha256'], results=receipts), indent=2))
    return receipts


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--fixture', type=Path, required=True)
    args = parser.parse_args()
    check(args.executable, args.fixture)
