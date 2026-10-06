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


def embedded_files(blob):
    """Read the 3DSX level-3 tree using libctru's romfs_header/dir/file layout.

    3dsxtool embeds this filesystem directly, without CIA's IVFC wrapper.
    Follow names and table offsets, never search for coincidental payload bytes.
    """
    raw = memoryview(embedded_romfs(blob))
    if len(raw) < 40:
        raise ValueError('Truncated embedded RomFS header')
    header, dh, dhs, dt, dts, fh, fhs, ft, fts, data = struct.unpack_from('<10I', raw)
    if header != 40 or not (40 <= dh <= dh + dhs <= dt <= dt + dts <=
                            fh <= fh + fhs <= ft <= ft + fts <= data <= len(raw)):
        raise ValueError('Invalid embedded RomFS table bounds')

    def records(start, length, directory):
        result = {}
        offset = 0
        size, fmt = (24, '<6I') if directory else (32, '<IIQQII')
        while offset < length:
            if length - offset < size:
                raise ValueError('Truncated embedded RomFS entry')
            fields = struct.unpack_from(fmt, raw, start + offset)
            name_size = fields[-1]
            end = offset + size + name_size
            padded = (end + 3) & ~3
            if name_size % 2 or padded > length:
                raise ValueError('Invalid embedded RomFS name bounds')
            name = bytes(raw[start + offset + size:start + end]).decode('utf-16le')
            if (not name and not (directory and offset == 0)) or any(
                    c in name for c in '/\\:\0') or name in ('.', '..'):
                raise ValueError('Unsafe embedded RomFS name')
            if any(raw[start + end:start + padded]):
                raise ValueError('Invalid embedded RomFS name padding')
            result[offset] = (fields, name)
            offset = padded
        return result

    dirs, files = records(dt, dts, True), records(ft, fts, False)
    if 0 not in dirs or dirs[0][1] or dirs[0][0][0] != 0:
        raise ValueError('Invalid embedded RomFS root')
    sentinel = 0xffffffff
    pending, seen_dirs, seen_files, result = [(0, '', 0)], set(), set(), {}
    while pending:
        index, prefix, parent = pending.pop()
        if index not in dirs or index in seen_dirs:
            raise ValueError('Invalid or cyclic embedded RomFS directory')
        fields, name = dirs[index]
        if fields[0] != parent:
            raise ValueError('Embedded RomFS directory parent differs')
        seen_dirs.add(index)
        path = prefix + name + ('/' if name else '')
        child = fields[2]
        siblings = set()
        while child != sentinel:
            if child not in dirs or child in siblings:
                raise ValueError('Invalid or cyclic embedded RomFS sibling')
            siblings.add(child)
            pending.append((child, path, index))
            child = dirs[child][0][1]
        child = fields[3]
        while child != sentinel:
            if child not in files or child in seen_files:
                raise ValueError('Invalid or cyclic embedded RomFS file')
            record, name = files[child]
            begin, size = data + record[2], record[3]
            relative = path + name
            if record[0] != index or begin > len(raw) or size > len(raw) - begin or relative in result:
                raise ValueError('Invalid embedded RomFS file ownership/bounds')
            seen_files.add(child)
            result[relative] = raw[begin:begin + size]
            child = record[1]
    if len(seen_dirs) != len(dirs) or len(seen_files) != len(files):
        raise ValueError('Unreachable embedded RomFS entries')
    return result


def check(project, output):
    import resource_catalog
    from romfs_layout import checked_inventory
    project, output = Path(project).resolve(), Path(output).resolve()
    output.mkdir(parents=True, exist_ok=True)
    dist, stage = project / 'dist', project / 'build/ctr/native-romfs'
    blob = (dist / 'encore-native.3dsx').read_bytes()
    embedded = embedded_files(blob)
    resource_catalog.stage_files(stage)
    expected = checked_inventory(stage)
    if set(embedded) != {path.as_posix() for path in expected}:
        raise ValueError('3DSX RomFS file paths differ from checked staging')
    for relative in expected:
        if embedded[relative.as_posix()] != (stage / relative).read_bytes():
            raise ValueError('3DSX RomFS file bytes differ: ' + str(relative))
    # Same typed House texture-reference traversal that guards staging. This
    # verifies actual staged paths rather than trusting a producer's file list.
    from native_resource_admission import admit
    admit(stage, global_items=False)
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
    artifacts = {}
    for extension in ('elf', '3dsx', 'cia', 'smdh'):
        path = dist / ('encore-native.' + extension)
        artifacts[path.name] = dict(bytes=path.stat().st_size,
                                   sha256=hashlib.sha256(path.read_bytes()).hexdigest())
    receipt = dict(source_commit=subprocess.check_output(
        ['git', 'rev-parse', 'HEAD'], cwd=project, text=True).strip(),
        staged_files=len(expected), artifacts=artifacts,
        scope='Real ARM build, full 3DSX file-tree/bytes and CIA extraction; emulator and hardware not run')
    (output / 'console-check.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print(json.dumps(receipt, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--project', type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument('--output', type=Path, default=Path('build/console-ci'))
    args = parser.parse_args()
    check(args.project, args.output)
