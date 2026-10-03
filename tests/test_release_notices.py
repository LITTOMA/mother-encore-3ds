import hashlib
import json
from pathlib import Path
import tempfile
import unittest

from tools.release_notices import OFFICIAL_FILES, PROJECT_FILES, stage_files


class ReleaseNoticeTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.notices = self.root / 'docs/licenses'
        self.notices.mkdir(parents=True)
        for name in OFFICIAL_FILES | PROJECT_FILES:
            (self.notices / name).write_bytes(b'fixture notice\n')
        for name in ('LICENSE', 'THIRD_PARTY_NOTICES.md'):
            (self.root / name).write_bytes(b'fixture project notice\n')
        self.manifest = {'schema': 1, 'checked_on': '2026-10-03', 'files': [
            {'file': name, 'sha256': hashlib.sha256(b'fixture notice\n').hexdigest(),
             'source_url': 'https://example.invalid/fixture/' + name,
             'source_revision': None, 'scope': 'Original test fixture',
             'verification': 'Test fixture only'}
            for name in sorted(OFFICIAL_FILES)]}
        self.write_manifest()

    def write_manifest(self):
        (self.notices / 'license-sources.json').write_text(json.dumps(self.manifest))

    def test_all_notices_are_staged_with_exact_bytes(self):
        files = stage_files(self.root)
        self.assertEqual(len(files), len(OFFICIAL_FILES | PROJECT_FILES) + 2)
        for name in OFFICIAL_FILES:
            self.assertEqual(files[Path('licenses') / name], b'fixture notice\n')

    def test_unknown_schema(self):
        self.manifest['schema'] = 2
        self.write_manifest()
        with self.assertRaises(ValueError):stage_files(self.root)

    def test_missing_or_unreviewed_files(self):
        for name in ('extra.txt', 'local-font.ttf'):
            with self.subTest(name=name):
                extra = self.notices / name
                extra.write_bytes(b'extra')
                with self.assertRaises(ValueError):stage_files(self.root)
                extra.unlink()
        (self.notices / 'COPYING.NEWLIB').unlink()
        with self.assertRaises(ValueError):stage_files(self.root)

    def test_changed_official_bytes(self):
        (self.notices / 'Fusion-Pixel-LICENSE-OFL.txt').write_bytes(b'changed notice')
        with self.assertRaises(ValueError):stage_files(self.root)

    def test_invalid_source_entries(self):
        original = self.manifest['files']
        cases = [original[:-1], original + [original[0]],
                 [dict(original[0], file='../LICENSE')] + original[1:],
                 [dict(original[0], source_url='file:///local')] + original[1:]]
        for entries in cases:
            with self.subTest(entries=len(entries)):
                self.manifest['files'] = entries
                self.write_manifest()
                with self.assertRaises(ValueError):stage_files(self.root)

    def test_linked_notice_is_rejected(self):
        file = self.notices / 'COPYING.NEWLIB'
        file.unlink()
        try:file.symlink_to(self.root / 'LICENSE')
        except (OSError, NotImplementedError):self.skipTest('symlink privilege unavailable')
        with self.assertRaises(ValueError):stage_files(self.root)

    def test_empty_project_notice(self):
        (self.root / 'LICENSE').write_bytes(b'')
        with self.assertRaises(ValueError):stage_files(self.root)

    def test_malformed_manifest(self):
        for data in (b'{', b'[]', b'{"schema":true,"files":[]}',
                     b'{"schema":1,"files":[null]}'):
            with self.subTest(data=data):
                (self.notices / 'license-sources.json').write_bytes(data)
                with self.assertRaises(ValueError):stage_files(self.root)

    def test_duplicate_and_unknown_fields_are_rejected(self):
        self.manifest['extra'] = 'unreviewed'
        self.write_manifest()
        with self.assertRaises(ValueError):stage_files(self.root)
        del self.manifest['extra']
        self.manifest['files'][0]['extra'] = 'unreviewed'
        self.write_manifest()
        with self.assertRaises(ValueError):stage_files(self.root)
        del self.manifest['files'][0]['extra']
        data = json.dumps(self.manifest).replace('"schema": 1', '"schema": 1, "schema": 1')
        (self.notices / 'license-sources.json').write_text(data)
        with self.assertRaises(ValueError):stage_files(self.root)

    def test_missing_or_invalid_provenance_is_rejected(self):
        for field in ('source_revision', 'scope', 'verification'):
            original = self.manifest['files'][0][field]
            self.manifest['files'][0][field] = []
            self.write_manifest()
            with self.assertRaises(ValueError):stage_files(self.root)
            self.manifest['files'][0][field] = original
