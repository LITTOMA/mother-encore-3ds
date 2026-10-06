#!/usr/bin/env python3
"""Isolated reviewed resource generation with complete, recoverable publication.

Normal check never extracts source or repairs tracked files. Explicit refresh
uses the same make DAG plus source derivation edges. Neither mode approves a
new upstream pin. Failed producers cannot publish a partial resource closure.
"""
from __future__ import annotations
import argparse
from contextlib import contextmanager
import hashlib
import importlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import uuid

ROOT = Path(__file__).resolve().parents[1]
OUTPUT_ROOTS = ('content/', 'reports/', 'romfs/')
INPUT_ROOTS = OUTPUT_ROOTS + ('tools/', 'make/', 'compatibility/', 'platform/ctr/shaders/')
EXTRA_INPUTS = ('tools/content_pipeline.py', 'tools/catalog_projection.py', 'make/refresh-content.mk', 'tools/godot_exporter/introduction_font_metrics.gd')


def require(value, message):
    if not value:
        raise ValueError(message)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest() if path.is_file() else None


def safe_path(name):
    require(type(name) is str and name and '\\' not in name and ':' not in name and
            not Path(name).is_absolute() and
            all(p not in ('', '.', '..') for p in name.split('/')), 'Unsafe generation path')
    return name


def write_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix('.tmp')
    temporary.write_text(json.dumps(value, indent=2, sort_keys=True) + '\n', encoding='utf-8')
    os.replace(temporary, path)


@contextmanager
def generation_lock(root):
    path = root / 'build/content-generation.lock'
    path.parent.mkdir(parents=True, exist_ok=True)
    require(not path.is_symlink(), 'Linked generation lock rejected')
    descriptor = os.open(path, os.O_CREAT | os.O_RDWR, 0o600)
    locked = False
    try:
        if os.name == 'nt':
            import msvcrt
            if not os.fstat(descriptor).st_size:
                os.write(descriptor, b'0')
            os.lseek(descriptor, 0, os.SEEK_SET)
            msvcrt.locking(descriptor, msvcrt.LK_NBLCK, 1)
        else:
            import fcntl
            fcntl.flock(descriptor, fcntl.LOCK_EX | fcntl.LOCK_NB)
        locked = True
        os.lseek(descriptor, 0, os.SEEK_SET)
        os.ftruncate(descriptor, 0)
        os.write(descriptor, (str(os.getpid()) + '\n').encode('ascii'))
        yield
    except BlockingIOError as exc:
        raise ValueError('Another content operation owns ' + str(path)) from exc
    finally:
        if locked and os.name == 'nt':
            os.lseek(descriptor, 0, os.SEEK_SET)
            msvcrt.locking(descriptor, msvcrt.LK_UNLCK, 1)
        os.close(descriptor)


def require_no_pending(root):
    pending = [p for p in (root / 'build/content-generation').glob('*/publication.json')
               if json.loads(p.read_text(encoding='utf-8')).get('state') == 'publishing']
    require(not pending, 'Interrupted publication requires recover --journal: ' + ', '.join(str(p) for p in pending))


def tracked_paths(root):
    raw = subprocess.check_output(['git', 'ls-files', '-z'], cwd=root)
    names = {safe_path(p.decode('utf-8')) for p in raw.split(b'\0') if p}
    # Validate newly implemented infrastructure before its first commit, without
    # accidentally including another contributor's unregistered producers.
    names.update(n for n in EXTRA_INPUTS if (root / n).is_file())
    # Existing restored PCM and derived ignored assets are part of the current
    # filesystem preimage, even though they are deliberately absent from Git.
    ignored = subprocess.check_output(['git', 'ls-files', '--others', '--ignored',
                                      '--exclude-standard', '-z', '--', 'romfs'], cwd=root)
    names.update(safe_path(p.decode('utf-8')) for p in ignored.split(b'\0') if p)
    return sorted(n for n in names if not n.startswith(('upstream/', 'build/', 'dist/')))


def snapshot(root, workspace, names):
    originals = {}
    for name in names:
        source = root / name
        require(source.is_file() and not source.is_symlink() and source.resolve().is_relative_to(root.resolve()), 'Missing/linked generation input: ' + name)
        target = workspace / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, target)
        originals[name] = digest(target)
    upstream = root / 'upstream/MOTHER-Encore'
    require(upstream.is_dir(), 'Missing read-only upstream checkout')
    sys.path.insert(0, str(root))
    from tools.ci_bootstrap import load_pin, verify
    lock, inventory = load_pin(root)
    verify(upstream, lock, inventory)
    target = workspace / 'upstream/MOTHER-Encore'
    target.parent.mkdir(parents=True, exist_ok=True)
    # Source admission deliberately rejects symlink/loose-file checkouts.
    # Clone locally on every platform; --shared reads the existing object DB,
    # never fetches or changes the original pinned checkout.
    subprocess.run(['git', '-c', 'core.autocrlf=false', 'clone', '--quiet', '--shared',
                    '--no-checkout', str(upstream), str(target)], check=True)
    # Retain the already verified official source identity in the local clone;
    # cloning from a local object database itself assigns a local origin URL.
    subprocess.run(['git', '-C', str(target), 'remote', 'set-url', 'origin',
                    lock['repository']], check=True)
    pin = lock['commit']
    subprocess.run(['git', '-c', 'core.autocrlf=false', '-C', str(target),
                    'checkout', '--quiet', '--detach', pin], check=True)
    return originals


def candidate_outputs(workspace):
    result = {}
    for prefix in OUTPUT_ROOTS:
        for path in (workspace / prefix).rglob('*'):
            if path.is_file():
                require(not path.is_symlink(), 'Linked resource output rejected')
                result[safe_path(path.relative_to(workspace).as_posix())] = digest(path)
    return result


def unchanged_inputs(root, originals):
    for name, expected in originals.items():
        if name.startswith(INPUT_ROOTS) or name in ('upstream.lock', '.gitmodules', 'Makefile'):
            require(digest(root / name) == expected, 'Input changed during generation: ' + name)


def atomic_copy(source, target):
    target.parent.mkdir(parents=True, exist_ok=True)
    temporary = target.parent / ('.content-' + uuid.uuid4().hex + '.tmp')
    try:
        shutil.copyfile(source, temporary)
        os.replace(temporary, target)
    finally:
        temporary.unlink(missing_ok=True)


def output_target(root, name):
    target = root / safe_path(name)
    require(name.startswith(OUTPUT_ROOTS) and target.resolve().is_relative_to(root.resolve()),
            'Resource output escapes generation root')
    return target


def rollback(root, journal):
    record = json.loads(journal.read_text(encoding='utf-8'))
    require(record['schema'] == 1 and record['state'] == 'publishing', 'Unknown generation journal')
    for row in record['files']:
        name = safe_path(row['path'])
        require(name.startswith(OUTPUT_ROOTS), 'Journal output outside resource roots')
        require(digest(output_target(root, name)) in (row['before'], row['after']),
                'Concurrent edit blocks generation recovery: ' + name)
    for row in reversed(record['files']):
        target = output_target(root, row['path'])
        if digest(target) == row['before']:
            continue
        if row['before'] is None:
            target.unlink(missing_ok=True)
        else:
            backup = journal.parent / 'backup' / row['path']
            require(digest(backup) == row['before'], 'Generation backup damaged')
            atomic_copy(backup, target)
    record['state'] = 'rolled-back'
    write_json(journal, record)


def publish(root, workspace, originals, outputs, journal, mode, preimages=None, tracked=None):
    require(mode in ('refresh', 'check'), 'Unknown publication mode')
    preimages = originals if preimages is None else preimages
    tracked = set(originals) if tracked is None else tracked
    unchanged_inputs(root, originals)
    changed = []
    for name, after in sorted(outputs.items()):
        before = preimages.get(name)
        require(digest(output_target(root, name)) == before, 'Concurrent/new output collision: ' + name)
        if before != after:
            changed.append(dict(path=name, before=before, after=after))
    removed = [n for n in originals if n.startswith(OUTPUT_ROOTS) and n not in outputs]
    require(not removed, 'Reviewed output removal requires explicit migration: ' + ', '.join(removed))
    if mode == 'check':
        stale = [r['path'] for r in changed if r['path'] in tracked]
        require(not stale, 'Incomplete generated closure; run make regenerate-content and commit all outputs:\n' + '\n'.join(stale))
    for row in changed:
        if row['before'] is not None:
            backup = journal.parent / 'backup' / row['path']
            backup.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(root / row['path'], backup)
    record = dict(schema=1, state='publishing', mode=mode, files=changed)
    write_json(journal, record)
    try:
        for row in changed:
            require(digest(root / row['path']) == row['before'], 'Concurrent output edit: ' + row['path'])
            atomic_copy(workspace / row['path'], root / row['path'])
        # Source edits during backup/publication must not turn an old snapshot
        # into a successful new closure. Generated inputs now have their actual
        # postimages; all other consumed sources retain their original bytes.
        unchanged_inputs(root, dict(originals, **outputs))
        record['state'] = 'complete'
        write_json(journal, record)
    except BaseException:
        rollback(root, journal)
        raise
    return changed


def font_step(refresh):
    sys.path.insert(0, str(ROOT))
    from tools.source_fonts import stage_files
    try:
        stage_files(ROOT / 'romfs')
        print('Source font closure is current')
        return
    except (ValueError, OSError, KeyError) as exc:
        require(refresh, str(exc))
        print('Regenerating genuine source fonts: ' + str(exc), flush=True)
    godot, tex3ds = os.environ.get('ENCORE_GENERATION_GODOT'), os.environ.get('ENCORE_GENERATION_TEX3DS')
    require(godot and tex3ds, 'GODOT3 and TEX3DS required for changed font inputs')
    subprocess.run([sys.executable, 'tools/source_fonts.py', '--source', str(ROOT / 'upstream/MOTHER-Encore'),
                    '--catalog', str(ROOT / 'content/native-localization.json'),
                    '--godot', godot, '--tex3ds', tex3ds], cwd=ROOT, check=True)
    stage_files(ROOT / 'romfs')


# Explicit audited producer registrations, never commands taken from a receipt.
# The normal DAG remains verify/compile only. A refresh can reconstruct admitted
# artwork with its real toolchain after an IR or producer byte change.
ASSET_PRODUCERS = {
    'canvas': ('field_canvas_art', 'assets', 'canvas'),
    'doll': ('doll_entry_asset', 'assets', 'entry'),
    'pillow': ('pillow_entry_asset', 'assets', 'entry'),
    'item-details': ('item_details', 'assets', 'receipt-ir'),
    'field-equipment': ('field_equipment', 'assets', 'receipt-ir'),
    'field-item-details': ('field_item_details', 'assets', 'receipt-ir'),
    'sparkles': ('present_sparkles', 'assets', 'receipt'),
    'basement-actors': ('basement_actor_assets', 'compile', 'receipt'),
    'storage': ('storage_assets', 'compile', 'receipt'),
    'drawer-item': ('drawer_item', 'compile', 'receipt'),
    'shop': ('field_shop', 'compile', 'pack'),
    'cash-box': ('field_cash_box', 'compile', 'pack'),
    'payphone': ('field_payphone', 'compile', 'pack'),
    'vending': ('field_vending_machine', 'compile', 'pack'),
    'introduction': ('introduction_assets', 'compile', 'stage'),
}


def asset_step(name):
    require(os.environ.get('ENCORE_GENERATION_REFRESH') == '1',
            'Asset generation is explicit refresh only')
    require(name in ASSET_PRODUCERS, 'Unknown resource producer')
    module_name, action, check_kind = ASSET_PRODUCERS[name]
    sys.path.insert(0, str(ROOT))
    module = importlib.import_module('tools.' + module_name)
    def check():
        if check_kind == 'canvas':
            module.checked_assets(module.load(), ROOT / 'romfs')
        elif check_kind == 'entry':
            module.compile_pack(module.read_json(module.IR_PATH))
        elif check_kind == 'receipt-ir':
            module.verify_receipt(module.load())
        elif check_kind == 'receipt':
            module.verify_receipt()
        elif check_kind == 'pack':
            module.compile_pack()
        else:
            module.stage_files(ROOT / 'romfs')
    try:
        check()
        print('Genuine asset closure is current: ' + name)
        return
    except (ValueError, OSError, KeyError) as exc:
        print('Regenerating admitted ' + name + ' assets: ' + str(exc), flush=True)
    tex3ds = os.environ.get('ENCORE_GENERATION_TEX3DS')
    require(tex3ds, 'TEX3DS required for changed asset inputs')
    command = [sys.executable, 'tools/' + module_name + '.py', action, '--tex3ds', tex3ds]
    if check_kind == 'canvas':
        picasso = os.environ.get('ENCORE_GENERATION_PICASSO')
        require(picasso, 'PICASSO required for changed Canvas inputs')
        command += ['--picasso', picasso]
    if name == 'introduction':
        godot = os.environ.get('ENCORE_GENERATION_GODOT')
        require(godot, 'GODOT3 required for changed Introduction inputs')
        command += ['--godot', godot]
    subprocess.run(command, cwd=ROOT, check=True)
    check()


def generate(args):
    require(args.jobs >= 4, 'Resource generation requires at least four workers')
    directory = ROOT / 'build/content-generation' / uuid.uuid4().hex
    workspace = directory / 'workspace'
    workspace.mkdir(parents=True)
    with generation_lock(ROOT):
        require_no_pending(ROOT)
        originals = snapshot(ROOT, workspace, tracked_paths(ROOT))
        # Protect pre-existing ignored producer reports without copying private
        # diagnostics or unregistered work into the isolated source snapshot.
        preimages = candidate_outputs(ROOT)
        tracked = {p.decode('utf-8') for p in subprocess.check_output(
            ['git', 'ls-files', '-z'], cwd=ROOT).split(b'\0') if p}
        write_json(directory / 'inputs.json', originals)
        environment = dict(os.environ, ENCORE_CONTENT_LOG_DIR=str(directory / 'jobs'),
                           ENCORE_GENERATION_GODOT=str(Path(args.godot).resolve()) if args.godot else '',
                           ENCORE_GENERATION_PICASSO=str(Path(args.picasso).resolve()) if args.picasso else '',
                           ENCORE_GENERATION_TEX3DS=str(Path(args.tex3ds).resolve()) if args.tex3ds else '')
        command = [args.make, '--no-print-directory', '-f', 'make/native-content.mk',
                   '-j' + str(args.jobs), 'PYTHON=' + sys.executable,
                   'REFRESH_SOURCES=' + str(int(args.mode == 'refresh')), 'native-content']
        print('CONTENT isolated ' + args.mode + ' workers=' + str(args.jobs) + ' logs=' + str(directory), flush=True)
        with (directory / 'generation.log').open('wb') as log:
            process = subprocess.Popen(command, cwd=workspace, env=environment,
                                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            for line in process.stdout:
                log.write(line)
                log.flush()
                print(line.decode('utf-8', errors='replace'), end='', flush=True)
            code = process.wait()
        require(code == 0, 'Isolated generation failed; no outputs published')
        changed = publish(ROOT, workspace, originals, candidate_outputs(workspace), directory / 'publication.json', args.mode, preimages, tracked)
        write_json(directory / 'result.json', dict(schema=1, mode=args.mode, workers=args.jobs,
                   published=[r['path'] for r in changed], tests_executed=False))
        print('CONTENT complete ' + args.mode + ': ' + str(len(changed)) + ' outputs published; no tests executed')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('mode', choices=('refresh', 'check', 'fonts', 'asset', 'recover'))
    parser.add_argument('--jobs', type=int, default=4)
    parser.add_argument('--make', default='make')
    parser.add_argument('--godot')
    parser.add_argument('--tex3ds')
    parser.add_argument('--picasso')
    parser.add_argument('--producer')
    parser.add_argument('--journal', type=Path)
    args = parser.parse_args()
    if args.mode == 'fonts':
        font_step(os.environ.get('ENCORE_GENERATION_REFRESH') == '1')
    elif args.mode == 'asset':
        asset_step(args.producer)
    elif args.mode == 'recover':
        require(args.journal is not None and args.journal.resolve().is_relative_to(
            (ROOT / 'build/content-generation').resolve()) and args.journal.name == 'publication.json',
            'Recovery requires an actual journal inside build/content-generation')
        with generation_lock(ROOT):
            rollback(ROOT, args.journal)
    else:
        generate(args)
    return 0


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except (ValueError, OSError, KeyError, subprocess.CalledProcessError) as error:
        print('CONTENT PIPELINE ERROR: ' + str(error), file=sys.stderr)
        raise SystemExit(1)
