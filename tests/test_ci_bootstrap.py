"""Small local Git witnesses for the pinned, read-only upstream bootstrap."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import stat
import subprocess
import tempfile
import unittest
from unittest.mock import patch

from tools import ci_bootstrap
from tools.upstream import snapshot


class CiBootstrapTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.directory = Path(self.temp.name).resolve()
        self.source = self.directory / 'official-fixture'
        self.root = self.directory / 'native'
        self.source.mkdir()
        self.root.mkdir()
        # Substitute the one allowed repository URL for this test only. This
        # keeps all clone/submodule/verification operations real and offline.
        self.official_url = patch.object(ci_bootstrap, 'OFFICIAL_REPOSITORY', self.source.as_uri())
        self.official_url.start()
        self.addCleanup(self.official_url.stop)
        self.run_git(self.source, 'init', '--quiet')
        self.run_git(self.source, 'config', 'user.name', 'Original fixture')
        self.run_git(self.source, 'config', 'user.email', 'fixture@example.invalid')
        (self.source / 'scene.gd').write_bytes(b'extends Node\n# Original bootstrap fixture, not game content.\n')
        (self.source / '.gitignore').write_bytes(b'ignored-cache\n')
        self.run_git(self.source, 'add', '.')
        self.run_git(self.source, 'commit', '--quiet', '-m', 'Original fixture')
        self.run_git(self.source, 'remote', 'add', 'origin', ci_bootstrap.OFFICIAL_REPOSITORY)
        baseline = snapshot(self.source)
        self.lock = {'schema': 1, 'repository': ci_bootstrap.OFFICIAL_REPOSITORY,
                     'commit': baseline['commit'], 'tree': baseline['tree']}
        (self.root / 'upstream.lock').write_text(json.dumps(self.lock), encoding='utf-8')
        (self.root / 'compatibility').mkdir()
        (self.root / 'compatibility/upstream-inventory.json').write_text(json.dumps(baseline), encoding='utf-8')
        self.baseline = baseline
        self.metadata_before = self.metadata_hashes()
        # Exercise Windows autocrlf=true without changing source bytes.
        self.environment = patch.dict(os.environ, {
            'GIT_CONFIG_COUNT': '2',
            'GIT_CONFIG_KEY_0': 'protocol.file.allow', 'GIT_CONFIG_VALUE_0': 'always',
            'GIT_CONFIG_KEY_1': 'core.autocrlf', 'GIT_CONFIG_VALUE_1': 'true',
        })
        self.environment.start()
        self.addCleanup(self.environment.stop)

    def tearDown(self):
        # Git object files can be read-only on Windows.
        def writable_remove(function, path, unused):
            os.chmod(path, stat.S_IWRITE | stat.S_IREAD)
            function(path)
        shutil.rmtree(self.directory, onerror=writable_remove)
        self.temp.cleanup()

    def run_git(self, root, *arguments):
        return subprocess.check_output(['git', '-c', 'core.autocrlf=false', '-C', str(root),
                                        *arguments], stderr=subprocess.PIPE, text=True).strip()

    def metadata_hashes(self):
        return [hashlib.sha256((self.root / path).read_bytes()).hexdigest()
                for path in ('upstream.lock', 'compatibility/upstream-inventory.json')]

    def declare_module(self):
        self.run_git(self.root, 'init', '--quiet')
        (self.root / '.gitmodules').write_text(
            '[submodule "' + ci_bootstrap.MODULE + '"]\n'
            '\tpath = ' + ci_bootstrap.MODULE + '\n'
            '\turl = ' + ci_bootstrap.OFFICIAL_REPOSITORY + '\n', encoding='utf-8')
        self.run_git(self.root, 'add', '.gitmodules')
        self.run_git(self.root, 'update-index', '--add', '--cacheinfo',
                     '160000,' + self.lock['commit'] + ',' + ci_bootstrap.MODULE)

    @property
    def checkout(self):
        return self.root / ci_bootstrap.MODULE

    def initialize(self):
        self.declare_module()
        result = ci_bootstrap.bootstrap(self.root)
        self.assertEqual(result['files'], len(self.baseline['files']))
        self.assertEqual(self.metadata_hashes(), self.metadata_before)
        return result

    def no_mutation(self):
        return patch.object(ci_bootstrap, 'git', wraps=ci_bootstrap.git)

    def test_modern_module_gitfile_and_verify_only_are_real_read_only(self):
        self.initialize()
        self.assertTrue((self.checkout / '.git').is_file())
        self.assertEqual(self.run_git(self.checkout, 'config', '--local', '--get', 'core.autocrlf'), 'false')
        gitdir = Path(self.run_git(self.checkout, 'rev-parse', '--absolute-git-dir'))
        files_before = {str(path): path.read_bytes() for path in gitdir.rglob('*') if path.is_file()}
        with self.no_mutation() as calls:
            self.assertTrue(ci_bootstrap.bootstrap(self.root, verify_only=True)['inventory_matches'])
            verbs = [call.args[1] for call in calls.call_args_list]
            self.assertNotIn('submodule', verbs)
            self.assertNotIn('fetch', verbs)
            self.assertNotIn('checkout', verbs)
        self.assertEqual(files_before, {str(path): path.read_bytes() for path in gitdir.rglob('*') if path.is_file()})
        self.assertEqual(self.metadata_hashes(), self.metadata_before)

    def test_uninitialized_empty_module_directory_can_initialize(self):
        self.declare_module()
        self.checkout.mkdir(parents=True)
        self.assertTrue(ci_bootstrap.bootstrap(self.root)['inventory_matches'])
        self.assertEqual(self.metadata_hashes(), self.metadata_before)

    def test_normal_clone_module_is_resolved_from_a_different_cwd(self):
        self.declare_module()
        original = Path.cwd()
        try:
            os.chdir(self.source)
            self.assertTrue(ci_bootstrap.bootstrap(self.root)['inventory_matches'])
            self.assertTrue(ci_bootstrap.bootstrap(self.root, verify_only=True)['inventory_matches'])
        finally:
            os.chdir(original)

    def test_verify_only_uninitialized_does_not_create_files(self):
        self.declare_module()
        before = {str(path) for path in self.root.rglob('*')}
        with self.assertRaisesRegex(ValueError, 'verify-only'):
            ci_bootstrap.bootstrap(self.root, verify_only=True)
        self.assertEqual(before, {str(path) for path in self.root.rglob('*')})

    def test_mismatched_gitlink_never_initializes(self):
        self.declare_module()
        self.run_git(self.root, 'update-index', '--cacheinfo',
                     '160000,' + '1' * 40 + ',' + ci_bootstrap.MODULE)
        with self.assertRaisesRegex(ValueError, 'gitlink'):
            ci_bootstrap.bootstrap(self.root)
        self.assertFalse(self.checkout.exists())

    def test_mismatched_lock_and_inventory_reject(self):
        self.declare_module()
        lock = dict(self.lock, commit='1' * 40)
        (self.root / 'upstream.lock').write_text(json.dumps(lock), encoding='utf-8')
        with self.assertRaisesRegex(ValueError, 'disagree'):
            ci_bootstrap.bootstrap(self.root)

    def test_wrong_module_url_path_unknown_module_and_hook_reject(self):
        self.declare_module()
        path = self.root / '.gitmodules'
        good = path.read_text(encoding='utf-8')
        for bad in (good.replace(ci_bootstrap.OFFICIAL_REPOSITORY, 'https://example.invalid/source.git'),
                    good.replace('\tpath = ' + ci_bootstrap.MODULE, '\tpath = ../outside'),
                    good + '[submodule "unknown"]\npath = extra\nurl = https://example.invalid/extra\n',
                    good + '\tupdate = !echo unsafe\n',
                    good + '[include]\npath = hidden-settings\n',
                    good + '\tbranch = main\n',
                    good + '\turl = ' + ci_bootstrap.OFFICIAL_REPOSITORY + '\n'):
            with self.subTest(config=bad):
                path.write_text(bad, encoding='utf-8')
                with self.assertRaises(ValueError):
                    ci_bootstrap.bootstrap(self.root)
                self.assertFalse(self.checkout.exists())

    def test_local_url_command_and_unknown_module_reject(self):
        self.declare_module()
        for key, value in (('submodule.' + ci_bootstrap.MODULE + '.url', 'https://example.invalid/wrong'),
                           ('submodule.' + ci_bootstrap.MODULE + '.update', '!echo unsafe'),
                           ('submodule.unknown.url', ci_bootstrap.OFFICIAL_REPOSITORY)):
            with self.subTest(key=key):
                self.run_git(self.root, 'config', key, value)
                with self.assertRaisesRegex(ValueError, 'local submodule'):
                    ci_bootstrap.bootstrap(self.root)
                self.run_git(self.root, 'config', '--unset', key)
                self.assertFalse(self.checkout.exists())

    def test_missing_gitmodules_or_gitlink_reject(self):
        self.declare_module()
        (self.root / '.gitmodules').unlink()
        with self.assertRaises(ValueError):
            ci_bootstrap.bootstrap(self.root)

    def test_dirty_tracked_source_never_fetches_or_resets(self):
        self.initialize()
        path = self.checkout / 'scene.gd'
        path.write_bytes(b'# Local source modification must survive.\n')
        before = path.read_bytes()
        with self.assertRaises(ValueError), self.no_mutation() as calls:
            ci_bootstrap.bootstrap(self.root)
        self.assertEqual(path.read_bytes(), before)
        self.assertNotIn('submodule', [call.args[1] for call in calls.call_args_list])

    def test_untracked_and_ignored_source_never_cleaned(self):
        self.initialize()
        for name in ('extra-file', 'ignored-cache'):
            with self.subTest(name=name):
                extra = self.checkout / name
                extra.write_bytes(b'Keep local data.\n')
                with self.assertRaises(ValueError):
                    ci_bootstrap.bootstrap(self.root)
                self.assertEqual(extra.read_bytes(), b'Keep local data.\n')
                extra.unlink()

    def test_broken_gitfile_and_parent_repository_impersonation_reject(self):
        self.initialize()
        pointer = self.checkout / '.git'
        # Git for Windows deliberately marks submodule pointer files read-only.
        # Only the disposable test fixture is made writable for corruption.
        os.chmod(pointer, pointer.stat().st_mode | stat.S_IWRITE)
        for text in ('gitdir: missing-directory\n', 'not a Git pointer\n',
                     'gitdir: ' + str(self.root / '.git') + '\n'):
            with self.subTest(pointer=text):
                # Windows also marks .git hidden; truncating via open('w')
                # refuses that attribute, while updating the existing file works.
                with pointer.open('r+b') as stream:
                    stream.write(text.encode('utf-8'))
                    stream.truncate()
                with self.assertRaises((ValueError, subprocess.CalledProcessError)):
                    ci_bootstrap.bootstrap(self.root)

    def test_nonempty_uninitialized_source_is_not_overwritten(self):
        self.declare_module()
        self.checkout.mkdir(parents=True)
        marker = self.checkout / 'local-file'
        marker.write_bytes(b'Do not overwrite me.\n')
        with self.assertRaises((ValueError, subprocess.CalledProcessError)):
            ci_bootstrap.bootstrap(self.root)
        self.assertEqual(marker.read_bytes(), b'Do not overwrite me.\n')

    def test_archive_restores_without_lock_or_inventory_changes(self):
        self.assertTrue(ci_bootstrap.bootstrap(self.root)['inventory_matches'])
        self.assertTrue((self.checkout / '.git').is_dir())
        self.assertEqual(self.metadata_hashes(), self.metadata_before)
        self.assertTrue(ci_bootstrap.bootstrap(self.root, verify_only=True)['inventory_matches'])

    def test_archive_empty_module_directory_restores_without_superproject(self):
        self.checkout.mkdir(parents=True)
        self.assertTrue(ci_bootstrap.bootstrap(self.root)['inventory_matches'])
        self.assertEqual(self.metadata_hashes(), self.metadata_before)

    def test_source_lock_inventory_schema_hash_and_coverage_reject(self):
        lock_path = self.root / 'upstream.lock'
        inventory_path = self.root / 'compatibility/upstream-inventory.json'
        for document, field, value in (('lock', 'schema', 2), ('inventory', 'schema', 2),
                                       ('lock', 'commit', 'main'), ('lock', 'tree', 'partial'),
                                       ('inventory', 'tree', '1' * 40),
                                       ('inventory', 'coverage', 'directory_without_commit'),
                                       ('inventory', 'files', {})):
            with self.subTest(document=document, field=field):
                lock_path.write_text(json.dumps(self.lock), encoding='utf-8')
                inventory_path.write_text(json.dumps(self.baseline), encoding='utf-8')
                original = self.lock if document == 'lock' else self.baseline
                path = lock_path if document == 'lock' else inventory_path
                path.write_text(json.dumps(dict(original, **{field: value})), encoding='utf-8')
                with self.assertRaises(ValueError):
                    ci_bootstrap.bootstrap(self.root)
                self.assertFalse(self.checkout.exists())

    def test_unknown_source_url_in_lock_rejects(self):
        lock = dict(self.lock, repository='https://example.invalid/wrong')
        (self.root / 'upstream.lock').write_text(json.dumps(lock), encoding='utf-8')
        with self.assertRaisesRegex(ValueError, 'official source URL'):
            ci_bootstrap.bootstrap(self.root)

    def test_inventory_git_environment_is_read_only_and_restored(self):
        self.initialize()
        original_snapshot = ci_bootstrap.snapshot
        def checked_snapshot(root):
            self.assertEqual(os.environ.get('GIT_OPTIONAL_LOCKS'), '0')
            return original_snapshot(root)
        for initial in (None, '1'):
            with self.subTest(initial=initial), patch.dict(os.environ):
                if initial is None:
                    os.environ.pop('GIT_OPTIONAL_LOCKS', None)
                else:
                    os.environ['GIT_OPTIONAL_LOCKS'] = initial
                with patch.object(ci_bootstrap, 'snapshot', side_effect=checked_snapshot):
                    ci_bootstrap.bootstrap(self.root, verify_only=True)
                self.assertEqual(os.environ.get('GIT_OPTIONAL_LOCKS'), initial)
                with patch.object(ci_bootstrap, 'snapshot', side_effect=ValueError('Rejected fixture')):
                    with self.assertRaisesRegex(ValueError, 'Rejected fixture'):
                        ci_bootstrap.bootstrap(self.root, verify_only=True)
                self.assertEqual(os.environ.get('GIT_OPTIONAL_LOCKS'), initial)


if __name__ == '__main__':
    unittest.main()
