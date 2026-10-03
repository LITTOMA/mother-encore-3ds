#!/usr/bin/env python3
"""Build official, fixed Project_CTR sources against the local host libc.

The source archive hash was observed from the official commit archive on
2026-10-01. It pins repeat downloads; it is not an upstream signed checksum.
Tool binaries stay in tools/bin and are never included in the SD release.
"""
import hashlib
import io
import json
from pathlib import Path, PurePosixPath
import platform
import shutil
import subprocess
import tarfile
import tempfile
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
COMMIT = 'e8f5f529c54ff9b22a2491a480ffa69206bf7b19'
URL = f'https://codeload.github.com/3DSGuy/Project_CTR/tar.gz/{COMMIT}'
ARCHIVE_SHA256 = '6b757ab8b8e4715047db9ddeb91f89e10e7165c3e803315d9d131a9297ca4fe0'
MAX_SOURCE_BYTES = 64 * 1024 * 1024


def extract_source(data, destination):
    """Accept only regular files/directories under the expected archive root."""
    destination = Path(destination).resolve()
    prefix = f'Project_CTR-{COMMIT}'
    with tarfile.open(fileobj=io.BytesIO(data), mode='r:gz') as archive:
        members = archive.getmembers()
        total = 0
        paths = set()
        # Validate the complete archive before writing any file.
        for member in members:
            path = PurePosixPath(member.name)
            if (path.is_absolute() or not path.parts or path.parts[0] != prefix
                    or '..' in path.parts or '\\' in member.name or ':' in member.name
                    or not (member.isdir() or member.isfile())):
                raise ValueError(f'Unsupported source archive entry: {member.name}')
            if path in paths:
                raise ValueError(f'Duplicate source archive entry: {member.name}')
            paths.add(path)
            total += member.size
            if member.size < 0 or total > MAX_SOURCE_BYTES:
                raise ValueError('Source archive exceeds extraction limit')
        if not members:
            raise ValueError('Empty source archive')
        for member in members:
            relative = PurePosixPath(member.name).parts[1:]
            target = destination.joinpath(*relative)
            if destination not in (target.resolve(), *target.resolve().parents):
                raise ValueError('Source archive escapes destination')
            if member.isdir():
                target.mkdir(parents=True, exist_ok=True)
            else:
                target.parent.mkdir(parents=True, exist_ok=True)
                with archive.extractfile(member) as source, target.open('wb') as output:
                    shutil.copyfileobj(source, output)


def main():
    if platform.system() != 'Linux' or platform.machine() != 'x86_64':
        raise SystemExit('This verified source-build route requires Linux x86_64.')
    vendor = ROOT / 'build/vendor'
    vendor.mkdir(parents=True, exist_ok=True)
    archive_path = vendor / 'project-ctr-makerom.tar.gz'
    if not archive_path.is_file():
        request = urllib.request.Request(URL, headers={'User-Agent': 'encore-native-build/0.1'})
        with urllib.request.urlopen(request, timeout=60) as response:
            data = response.read(16 * 1024 * 1024 + 1)
        if hashlib.sha256(data).hexdigest() != ARCHIVE_SHA256:
            raise SystemExit('Project_CTR source SHA256 mismatch; refusing to build')
        archive_path.write_bytes(data)
    data = archive_path.read_bytes()
    if hashlib.sha256(data).hexdigest() != ARCHIVE_SHA256:
        raise SystemExit('Cached Project_CTR source SHA256 mismatch; refusing to build')
    destination = ROOT / 'tools/bin'
    destination.mkdir(parents=True, exist_ok=True)
    records = []
    with tempfile.TemporaryDirectory(prefix='project-ctr-', dir=vendor) as temporary:
        source_root = Path(temporary)
        extract_source(data, source_root)
        for name in ('makerom', 'ctrtool'):
            source = source_root / name
            subprocess.run(['make', 'deps', '-j4'], cwd=source, check=True)
            subprocess.run(['make', '-j4'], cwd=source, check=True)
            binary = source / 'bin' / name
            shutil.copy2(binary, destination / name)
            (destination / name).chmod(0o755)
            records.append({'tool': name, 'source_commit': COMMIT,
                            'source_url': URL, 'archive_sha256': ARCHIVE_SHA256,
                            'checksum_origin': 'observed official commit archive, 2026-10-01',
                            'binary_sha256': hashlib.sha256(binary.read_bytes()).hexdigest()})
    (destination / 'source-build-manifest.json').write_text(json.dumps(records, indent=2) + '\n')
    print('Built makerom and ctrtool from fixed official sources for this host libc.')


if __name__ == '__main__':
    main()
