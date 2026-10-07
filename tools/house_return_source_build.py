#!/usr/bin/env python3
"""Produce the exact House Restore extension and its dependent resource closure.

Manual authoring on the same pinned Linux tool versions as console builds.
It does not run tests, an SDK build, source lifecycle, or change upstream.
"""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build/cloud-house-return'
OUTPUTS = (
    'content/native-house-inspection-restore.json',
    'reports/house-inspection-restore/source-review.json',
    'romfs/data/house-return.encrestore',
    'content/native-podunk-bundle.json',
    'reports/podunk-bundle/source-review.json',
    'romfs/data/podunk.encbundle',
    'romfs/data/native.encresources',
    'content/encounter-dependencies.json',
)
COMMANDS = (
    ('tools/house_inspection_restore.py', 'extract'),
    ('tools/house_inspection_restore.py', 'compile'),
    ('tools/podunk_bundle.py', 'extract'),
    ('tools/resource_catalog.py', 'compile'),
    ('tools/encounter_dependencies.py', 'compile'),
)


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    head = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT).decode().strip()
    names = subprocess.check_output(['git', 'ls-files', '-z'], cwd=ROOT).split(b'\0')
    originals = {}
    for raw in names:
        if not raw:
            continue
        name = raw.decode('utf-8')
        path = ROOT / name
        if name not in OUTPUTS and path.is_file():
            originals[name] = sha(path)
    for index, args in enumerate(COMMANDS):
        with (OUT / ('command-%d.log' % index)).open('wb') as log:
            subprocess.run([sys.executable, *args], cwd=ROOT, stdout=log,
                           stderr=subprocess.STDOUT, check=True)
    changed = [name for name, value in originals.items() if sha(ROOT / name) != value]
    if changed:
        raise ValueError('Producer changed an unrelated input: ' + ', '.join(changed))
    rows = [{ 'path': name, 'bytes': (ROOT / name).stat().st_size,
              'sha256': sha(ROOT / name)} for name in OUTPUTS]
    receipt = dict(schema=1, source_commit=head, outputs=rows,
                   source_workers=4, tests_run=False, sdk_run=False,
                   emulator_run=False, hardware_run=False)
    data = (json.dumps(receipt, indent=2, sort_keys=True) + '\n').encode()
    (OUT / 'receipt.json').write_bytes(data)
    with zipfile.ZipFile(OUT / 'house-return-source.zip', 'w', zipfile.ZIP_DEFLATED) as archive:
        for row in rows:
            archive.write(ROOT / row['path'], row['path'])
        archive.writestr('receipt.json', data)
    print(json.dumps(receipt, indent=2))


if __name__ == '__main__':
    main()
