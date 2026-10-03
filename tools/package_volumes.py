#!/usr/bin/env python3
"""Losslessly split a validated ZIP into numbered 7-Zip-compatible byte volumes."""
import argparse
import hashlib
import json
from pathlib import Path


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('archive', type=Path)
    ap.add_argument('destination', type=Path)
    ap.add_argument('--name')
    args = ap.parse_args()
    source = args.archive.read_bytes()
    name = args.name or args.archive.name
    if Path(name).name != name or not name.endswith('.zip'):
        raise SystemExit('A plain .zip filename is required')
    args.destination.mkdir(parents=True, exist_ok=True)
    limit = 17 * 1024 * 1024
    records = []
    for i, offset in enumerate(range(0, len(source), limit), 1):
        data = source[offset:offset + limit]
        p = args.destination / (name + '.' + str(i).zfill(3))
        p.write_bytes(data)
        records.append({'name': p.name, 'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()})
    rebuilt = b''.join((args.destination / r['name']).read_bytes() for r in records)
    if rebuilt != source:
        raise SystemExit('Volume round-trip failed')
    manifest = {'schema': 1, 'format': 'sequential byte volumes; open .001 with 7-Zip or concatenate in listed order',
                'archive': name, 'bytes': len(source), 'sha256': hashlib.sha256(source).hexdigest(), 'volumes': records}
    (args.destination / (name + '.volumes.json')).write_text(json.dumps(manifest, indent=2) + '\n')
    print(json.dumps(manifest, indent=2))


if __name__ == '__main__':
    main()
