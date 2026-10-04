#!/usr/bin/env python3
"""Check genuine console outputs; no upload, execution or hardware claim."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile


def embedded_romfs(blob):
    if len(blob) < 44 or blob[:4] != b'3DSX':
        raise ValueError('Missing 3DSX extended header')
    if struct.unpack_from('<HHII', blob, 4) != (44, 8, 0, 0):
        raise ValueError('Unsupported 3DSX header/version/flags')
    icon, size, romfs = struct.unpack_from('<III', blob, 32)
    if not (44 <= icon < icon + size <= romfs < len(blob)):
        raise ValueError('Invalid 3DSX embedded-resource bounds')
    if blob[icon:icon + 4] != b'SMDH':
        raise ValueError('Missing embedded SMDH')
    return blob[romfs:]


def check(project, output):
    import resource_catalog
    from romfs_layout import checked_inventory
    project, output = Path(project).resolve(), Path(output).resolve()
    output.mkdir(parents=True, exist_ok=True)
    dist, stage = project / 'dist', project / 'build/ctr/native-romfs'
    blob = (dist / 'encore-native.3dsx').read_bytes()
    romfs = embedded_romfs(blob)
    resource_catalog.stage_files(stage)
    expected = checked_inventory(stage)
    ctrtool = shutil.which('ctrtool')
    if not ctrtool:
        raise ValueError('CTRTool unavailable; CIA extraction cannot be checked')
    with tempfile.TemporaryDirectory(prefix='console-check-', dir=output) as temporary:
        work = Path(temporary)

        def run(name, args):
            with (output / (name + '.log')).open('wb') as log:
                subprocess.run([ctrtool, '-p', *args], check=True,
                               stdout=log, stderr=subprocess.STDOUT)

        run('cia-contents', ['--contents=' + str(work / 'contents'),
                             str(dist / 'encore-native.cia')])
        contents = list(work.glob('contents.*'))
        if len(contents) != 1 or not contents[0].is_file():
            raise ValueError('Expected exactly one CIA content')
        extracted = work / 'romfs'
        run('cia-romfs', ['--romfsdir=' + str(extracted), str(contents[0])])
        actual = checked_inventory(extracted)
        if actual != expected:
            raise ValueError('CIA RomFS file inventory differs from checked staging')
        for relative in sorted(expected):
            if (stage / relative).read_bytes() != (extracted / relative).read_bytes():
                raise ValueError('CIA RomFS bytes differ: ' + str(relative))
        resource_catalog.stage_files(extracted)
        catalog = (stage / 'data/native.encresources').read_bytes()
        if catalog not in romfs:
            raise ValueError('3DSX embedded RomFS lacks the checked resource catalog')
        for row in resource_catalog.decode(catalog)['bindings']:
            if (stage / row['path']).read_bytes() not in romfs:
                raise ValueError('3DSX lacks checked catalog resource: ' + row['path'])
    artifacts = {}
    for extension in ('elf', '3dsx', 'cia', 'smdh'):
        path = dist / ('encore-native.' + extension)
        artifacts[path.name] = dict(bytes=path.stat().st_size,
                                   sha256=hashlib.sha256(path.read_bytes()).hexdigest())
    receipt = dict(source_commit=subprocess.check_output(
        ['git', 'rev-parse', 'HEAD'], cwd=project, text=True).strip(),
        staged_files=len(expected), artifacts=artifacts,
        scope='Real ARM build and CIA extraction; emulator and hardware not run')
    (output / 'console-check.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print(json.dumps(receipt, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--project', type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument('--output', type=Path, default=Path('build/console-ci'))
    args = parser.parse_args()
    check(args.project, args.output)
