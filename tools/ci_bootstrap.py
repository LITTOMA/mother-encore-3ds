#!/usr/bin/env python3
"""Restore and verify the official upstream pin without rewriting its identity.

A present checkout is only verified, never reset, fetched into or cleaned.
A missing Git submodule is initialized at the superproject's checked gitlink.
Source archives without a superproject restore a separately verified checkout.
This script never changes the lock, inventory, gitlink or upstream source.
"""
from __future__ import annotations
import argparse
from contextlib import contextmanager
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import stat
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from tools.upstream import snapshot, diff_snapshots
OFFICIAL_REPOSITORY = 'https://github.com/motherencore/MOTHER-Encore-Source-Code.git'
MODULE = 'upstream/MOTHER-Encore'


def load_pin(root: Path):
    lock = json.loads((root / 'upstream.lock').read_text(encoding='utf-8'))
    baseline = json.loads((root / 'compatibility/upstream-inventory.json').read_text(encoding='utf-8'))
    if lock.get('schema') != 1 or baseline.get('schema') != 1:
        raise ValueError('Unsupported source lock/inventory schema')
    if lock.get('repository') != OFFICIAL_REPOSITORY:
        raise ValueError('CI restoration accepts only the reviewed official source URL')
    for field in ('commit', 'tree'):
        if not re.fullmatch(r'[0-9a-f]{40}', str(lock.get(field, ''))):
            raise ValueError('Source lock needs a complete '+field+' ID')
        if baseline.get(field) != lock[field]:
            raise ValueError('Source lock and reviewed inventory disagree on '+field)
    if baseline.get('coverage') != 'all_tracked_git_blobs' or not baseline.get('files'):
        raise ValueError('A complete reviewed upstream inventory is required')
    return lock, baseline


def git(checkout: Path, *args: str):
    env = dict(os.environ, GIT_OPTIONAL_LOCKS='0', GIT_TERMINAL_PROMPT='0')
    return subprocess.check_output(['git', '-c', 'core.autocrlf=false', '-c', 'core.hooksPath=' + os.devnull,
                                   '-C', str(checkout), *args],
                                   text=True, stderr=subprocess.PIPE, env=env).strip()


def config_records(root: Path, *args: str):
    """Parse Git's config syntax, retaining duplicates and rejecting includes."""
    try:
        include_flag = '--no-includes' if '--file' in args else '--includes'
        raw = git(root, 'config', '--null', include_flag, *args)
    except subprocess.CalledProcessError as error:
        if error.returncode == 1 and '--get-regexp' in args:
            return []
        raise
    records = []
    for entry in raw.split('\0'):
        if not entry:
            continue
        key, separator, value = entry.partition('\n')
        if not separator:
            raise ValueError('Malformed Git submodule configuration')
        records.append((key, value))
    return records


def submodule_contract(root: Path, lock: dict):
    """Validate declarations before Git is allowed to initialize anything."""
    metadata = root / '.git'
    modules = root / '.gitmodules'
    if metadata.is_symlink() or modules.is_symlink():
        raise ValueError('Git/submodule metadata symlinks are not allowed')
    has_project_git = False
    try:
        has_project_git = Path(git(root, 'rev-parse', '--show-toplevel')).resolve() == root
    except subprocess.CalledProcessError:
        if metadata.exists():
            raise ValueError('Broken superproject Git metadata')
    if metadata.exists() and not has_project_git:
        raise ValueError('Project metadata does not identify this Git root')
    expected = [('submodule.' + MODULE + '.path', MODULE),
                ('submodule.' + MODULE + '.url', OFFICIAL_REPOSITORY)]
    if modules.exists():
        if not modules.is_file() or sorted(config_records(root, '--file', str(modules), '--list')) != sorted(expected):
            raise ValueError('Only the reviewed official submodule path/URL is allowed; no extra settings/modules')
    elif has_project_git:
        raise ValueError('Git checkout is missing its reviewed .gitmodules declaration')
    if not has_project_git:
        # An official source ZIP contains declarations, but has no Git index.
        return None
    entry = git(root, 'ls-files', '--stage', '--', MODULE).splitlines()
    wanted = '160000 ' + lock['commit'] + ' 0\t' + MODULE
    if entry != [wanted]:
        raise ValueError('Superproject gitlink and reviewed source lock disagree')
    # Reject a conflicting local URL or command before submodule update.
    allowed = {'submodule.' + MODULE + '.url': OFFICIAL_REPOSITORY,
               'submodule.' + MODULE + '.active': 'true',
               'submodule.' + MODULE + '.update': 'checkout'}
    seen = set()
    for key, value in config_records(root, '--get-regexp', '^submodule\\.'):
        if key in seen or key not in allowed or allowed[key] != value:
            raise ValueError('Unexpected local submodule settings; refusing initialization')
        seen.add(key)
    return Path(git(root, 'rev-parse', '--path-format=absolute', '--git-path', 'modules/' + MODULE)).resolve()


def real_checkout(checkout: Path):
    metadata = checkout / '.git'
    if checkout.is_symlink() or not checkout.is_dir() or metadata.is_symlink():
        raise ValueError('Upstream must be a real Git directory, not a symlink or loose files')
    if metadata.is_file():
        match = re.fullmatch(r'gitdir: ([^\r\n]+)\n?', metadata.read_text(encoding='utf-8'))
        if not match:
            raise ValueError('Malformed upstream Git pointer file')
        pointer = Path(match.group(1))
        target = pointer if pointer.is_absolute() else checkout / pointer
        if any(part.is_symlink() for part in (target, *target.parents)) or not target.is_dir():
            raise ValueError('Broken/symlinked upstream Git pointer')
        if Path(git(checkout, 'rev-parse', '--absolute-git-dir')).resolve() != target.resolve():
            raise ValueError('Upstream Git pointer disagrees with Git')
        # A submodule's Git directory explicitly binds its own work tree.
        worktree = git(checkout, 'config', '--local', '--get', 'core.worktree')
        if (target / worktree).resolve() != checkout.resolve():
            raise ValueError('Git pointer does not bind this upstream work tree')
    elif not metadata.is_dir():
        raise ValueError('Upstream checkout has no valid Git metadata')
    if Path(git(checkout, 'rev-parse', '--show-toplevel')).resolve() != checkout.resolve():
        raise ValueError('Upstream is not its own actual Git root')


@contextmanager
def readonly_inventory_git():
    """The pinned inventory helper inherits read-only Git, without editing it."""
    previous = os.environ.get('GIT_OPTIONAL_LOCKS')
    os.environ['GIT_OPTIONAL_LOCKS'] = '0'
    try:
        yield
    finally:
        if previous is None:
            os.environ.pop('GIT_OPTIONAL_LOCKS', None)
        else:
            os.environ['GIT_OPTIONAL_LOCKS'] = previous


def verify(checkout: Path, lock: dict, baseline: dict):
    real_checkout(checkout)
    if git(checkout, 'remote', 'get-url', 'origin') != OFFICIAL_REPOSITORY:
        raise ValueError('Existing checkout is not the reviewed official source')
    if git(checkout, 'rev-parse', 'HEAD') != lock['commit']:
        raise ValueError('Existing checkout HEAD differs; refusing to overwrite it')
    if git(checkout, 'rev-parse', 'HEAD^{tree}') != lock['tree']:
        raise ValueError('Existing checkout tree differs from the reviewed pin')
    with readonly_inventory_git():
        candidate = snapshot(checkout.resolve())
    changes = diff_snapshots(baseline, candidate)
    if any(changes[field] for field in ('added', 'removed', 'changed')):
        raise ValueError('Upstream bytes differ from reviewed inventory: '+json.dumps(changes))
    if any(candidate['files'][name]['bytes'] != record['bytes']
           for name, record in baseline['files'].items()):
        raise ValueError('Upstream byte sizes differ from reviewed inventory')
    return {'commit': lock['commit'], 'tree': lock['tree'], 'files': len(candidate['files']),
            'bytes': sum(r['bytes'] for r in candidate['files'].values()),
            'inventory_matches': True, 'project_lock_modified': False}


def bootstrap(root: Path, verify_only: bool = False):
    root = root.resolve()
    lock_path = root / 'upstream.lock'
    lock_before = hashlib.sha256(lock_path.read_bytes()).hexdigest()
    inventory_path = root / 'compatibility/upstream-inventory.json'
    inventory_before = hashlib.sha256(inventory_path.read_bytes()).hexdigest()
    lock, baseline = load_pin(root)
    module_gitdir = submodule_contract(root, lock)
    parent = root / 'upstream'
    checkout = parent / 'MOTHER-Encore'
    if parent.is_symlink() or not parent.resolve().is_relative_to(root):
        raise ValueError('Upstream restoration directory must stay inside the project')
    if checkout.is_symlink():
        raise ValueError('Upstream checkout symlinks are not allowed')
    initialized = checkout.exists() and (not checkout.is_dir() or any(checkout.iterdir()))
    if initialized:
        if module_gitdir is not None and Path(git(checkout, 'rev-parse', '--absolute-git-dir')).resolve() != module_gitdir:
            raise ValueError('Upstream is not the declared superproject submodule')
        result = verify(checkout, lock, baseline)
    elif verify_only:
        raise ValueError('Upstream is absent; --verify-only never downloads or creates a checkout')
    else:
        if module_gitdir is not None:
            # An old module Git directory may hide dirty work from another tree.
            # Only a genuinely absent module is initialized automatically.
            if module_gitdir.exists():
                raise ValueError('Uninitialized checkout has existing module metadata; recover explicitly without overwriting work')
            git(root, 'submodule', 'update', '--init', '--checkout', '--', MODULE)
            if Path(git(checkout, 'rev-parse', '--absolute-git-dir')).resolve() != module_gitdir:
                raise ValueError('Restored module Git identity is unexpected')
            git(checkout, 'config', 'core.autocrlf', 'false')
            result = verify(checkout, lock, baseline)
        else:
            result = restore_archive(parent, checkout, lock, baseline)
    if (hashlib.sha256(lock_path.read_bytes()).hexdigest() != lock_before or
            hashlib.sha256(inventory_path.read_bytes()).hexdigest() != inventory_before):
        raise ValueError('Source identity changed during verification')
    return result


def restore_archive(parent: Path, checkout: Path, lock: dict, baseline: dict):
    """Restore a source ZIP without altering a superproject's module identity."""
    if checkout.exists() and (not checkout.is_dir() or any(checkout.iterdir())):
        raise ValueError('Archive checkout destination exists; refusing to overwrite it')
    parent.mkdir(exist_ok=True)
    temporary = Path(tempfile.mkdtemp(prefix='.ci-restore-', dir=parent))
    try:
        staged = temporary / 'checkout'
        env = dict(os.environ, GIT_OPTIONAL_LOCKS='0', GIT_TERMINAL_PROMPT='0')
        subprocess.run(['git', '-c', 'core.autocrlf=false', '-c', 'core.hooksPath=' + os.devnull,
                        'clone', '--no-checkout', '--filter=blob:none', '--no-tags',
                        OFFICIAL_REPOSITORY, str(staged)], env=env, check=True)
        git(staged, 'config', 'core.autocrlf', 'false')
        git(staged, 'fetch', '--depth=1', 'origin', lock['commit'])
        git(staged, 'checkout', '--detach', lock['commit'])
        result = verify(staged, lock, baseline)
        if checkout.is_symlink() or (checkout.exists() and
                (not checkout.is_dir() or any(checkout.iterdir()))):
            raise ValueError('Destination appeared during restoration; refusing to overwrite it')
        if checkout.exists():
            checkout.rmdir()
        staged.rename(checkout)
        return result
    finally:
        # Git stores read-only object files on Windows. Cleanup is confined to
        # the temporary directory created above, never an existing checkout.
        def remove_readonly(function, path, unused):
            os.chmod(path, os.stat(path).st_mode | stat.S_IWRITE)
            function(path)
        shutil.rmtree(temporary, onerror=remove_readonly)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--verify-only', action='store_true', help='Verify an existing checkout without downloading')
    args = parser.parse_args()
    try:
        print(json.dumps(bootstrap(ROOT, args.verify_only), indent=2))
    except (OSError, ValueError, KeyError, subprocess.CalledProcessError) as error:
        print('CI upstream bootstrap blocked: '+str(error), file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
