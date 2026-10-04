#!/usr/bin/env python3
"""Record observed build/tool bytes; makes no claim of device verification."""
import hashlib
import json
from pathlib import Path
import subprocess
from datetime import datetime, timezone

ROOT = Path(__file__).resolve().parents[1]


def describe(path):
    data = path.read_bytes()
    return {'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()}


def main():
    image = json.loads((ROOT / 'reports/local-docker-image.json').read_text(encoding='utf-8-sig'))
    packages = subprocess.check_output(['dkp-pacman', '-Q'], text=True)
    (ROOT / 'reports/local-dkp-packages.txt').write_text(packages)
    source_tools = json.loads((ROOT / 'tools/bin/source-build-manifest.json').read_text())
    for tool in source_tools:
        if describe(ROOT / 'tools/bin' / tool['tool'])['sha256'] != tool['binary_sha256']:
            raise SystemExit(f"Tool changed since source build: {tool['tool']}")
    commit = subprocess.run(['git', 'rev-parse', 'HEAD'], cwd=ROOT, capture_output=True, text=True)
    content = json.loads((ROOT / 'build/content-manifest.json').read_text())
    sources = {}
    for directory in ('runtime', 'platform', 'include', 'content', 'tools', 'tests'):
        for path in sorted((ROOT / directory).rglob('*')):
            if path.is_file() and (path.suffix in ('.cpp', '.hpp', '.py', '.json', '.sh', '.ps1') or path.name == 'Makefile') and 'bin' not in path.parts and '__pycache__' not in path.parts:
                sources[str(path.relative_to(ROOT))] = describe(path)['sha256']
    for name in ('Makefile', 'CMakeLists.txt', 'Dockerfile', 'assets/cia.rsf'):
        sources[name] = describe(ROOT / name)['sha256']
    result = {
        'recorded_at_utc': datetime.now(timezone.utc).isoformat(),
        'scope': 'M0 fixture and optional real opening-map background preview; cross-build and local packaging only',
        'source_commit': commit.stdout.strip() if commit.returncode == 0 else None,
        'upstream_commit': content['upstream_commit'],
        'upstream_source_lock': json.loads((ROOT / 'upstream.lock').read_text()),
        'audited_native_rule_reviews': {
            str(p.relative_to(ROOT)): json.loads(p.read_text())
            for p in sorted((ROOT / 'compatibility/reviews').glob('*.json'))
        },
        'native_rule_integration': 'progression, scoped opening movement and discrete animation compiled for ARM; current background viewer and M0 fixture do not call them; unused sections may be discarded',
        'upstream_visual_assets': json.loads((ROOT / 'content/asset-receipts/graphics/world/house-map/source.json').read_text())
            if (ROOT / 'content/asset-receipts/graphics/world/house-map/source.json').is_file() else None,
        'content_manifest': content,
        'source_sha256': sources,
        'docker_image_id': image['Id'], 'docker_repo_digests': image['RepoDigests'],
        'dkp_packages': packages.splitlines(),
        'host_compiler': subprocess.check_output(['g++', '--version'], text=True).splitlines()[0],
        'arm_compiler': subprocess.check_output(['/opt/devkitpro/devkitARM/bin/arm-none-eabi-g++', '--version'], text=True).splitlines()[0],
        'source_tools': source_tools,
        'bannertool': describe(ROOT / 'tools/bin/bannertool'),
        'bannertool_download_records': json.loads((ROOT / 'tools/bin/download-manifest.json').read_text()),
        'artifacts': {name: describe(ROOT / name) for name in (
            'dist/encore-native.elf', 'dist/encore-native.3dsx', 'dist/encore-native.cia',
            'dist/encore-native.smdh', 'build/fixtures/sandbox.encpak')},
        'emulator_verification': 'not run', 'hardware_verification': 'not run',
        'bit_reproducibility': 'not claimed; tools embed build dates/paths and makerom uses random identifiers',
    }
    output = ROOT / 'reports/ctr-toolchain-lock.json'
    output.write_text(json.dumps(result, indent=2) + '\n')
    print(f'Recorded {output.relative_to(ROOT)}; no hardware verification claimed.')


if __name__ == '__main__':
    main()
