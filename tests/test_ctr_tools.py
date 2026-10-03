import io
from pathlib import Path
import tarfile
import tempfile
import unittest
from unittest.mock import patch

from tools.build_ctr_tools import COMMIT, extract_source


class SourceArchiveTests(unittest.TestCase):
    def archive(self, names, kind=tarfile.REGTYPE):
        data = io.BytesIO()
        with tarfile.open(fileobj=data, mode='w:gz') as archive:
            for name in names:
                member = tarfile.TarInfo(name)
                member.type = kind
                if kind == tarfile.REGTYPE:
                    member.size = 3
                    archive.addfile(member, io.BytesIO(b'abc'))
                else:
                    member.linkname = '/outside'
                    archive.addfile(member)
        return data.getvalue()

    def test_valid_source(self):
        with tempfile.TemporaryDirectory() as temporary:
            extract_source(self.archive([f'Project_CTR-{COMMIT}/makerom/LICENSE']), temporary)
            self.assertEqual((Path(temporary) / 'makerom/LICENSE').read_bytes(), b'abc')

    def test_invalid_paths_write_nothing(self):
        prefix = f'Project_CTR-{COMMIT}'
        for name in ('/outside', f'{prefix}/../outside', 'other/file',
                     f'{prefix}/dir\\outside', f'{prefix}/C:/outside'):
            with self.subTest(name=name), tempfile.TemporaryDirectory() as temporary:
                with self.assertRaises(ValueError):
                    extract_source(self.archive([f'{prefix}/valid', name]), temporary)
                self.assertEqual(list(Path(temporary).iterdir()), [])

    def test_unsupported_types(self):
        for kind in (tarfile.SYMTYPE, tarfile.LNKTYPE, tarfile.FIFOTYPE, tarfile.CHRTYPE):
            with self.subTest(kind=kind), tempfile.TemporaryDirectory() as temporary:
                with self.assertRaises(ValueError):
                    extract_source(self.archive([f'Project_CTR-{COMMIT}/link'], kind), temporary)
                self.assertEqual(list(Path(temporary).iterdir()), [])

    def test_duplicate_and_empty(self):
        name = f'Project_CTR-{COMMIT}/file'
        for names in ([], [name, name]):
            with self.subTest(names=names), tempfile.TemporaryDirectory() as temporary:
                with self.assertRaises(ValueError):
                    extract_source(self.archive(names), temporary)

    def test_corrupt_archive(self):
        with tempfile.TemporaryDirectory() as temporary:
            with self.assertRaises(tarfile.TarError):
                extract_source(b'not a tar archive', temporary)

    def test_size_limit(self):
        with tempfile.TemporaryDirectory() as temporary:
            with patch('tools.build_ctr_tools.MAX_SOURCE_BYTES', 2):
                with self.assertRaises(ValueError):
                    extract_source(self.archive([f'Project_CTR-{COMMIT}/file']), temporary)
            self.assertEqual(list(Path(temporary).iterdir()), [])


if __name__ == '__main__':
    unittest.main()
