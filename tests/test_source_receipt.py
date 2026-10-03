import hashlib
import io
from pathlib import Path
import tarfile
import tempfile
import unittest
from tools.record_upstream import compare_archive

class SourceReceiptTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.archive=Path(self.temp.name)/'source.tar.gz'
        self.files={'file':{'bytes':3,'sha256':hashlib.sha256(b'abc').hexdigest()}}
    def tearDown(self):self.temp.cleanup()
    def archive_members(self,members):
        with tarfile.open(self.archive,'w:gz') as tar:
            for name,data,kind in members:
                entry=tarfile.TarInfo(name);entry.type=kind
                if kind==tarfile.REGTYPE:entry.size=len(data);tar.addfile(entry,io.BytesIO(data))
                else:entry.linkname='file';tar.addfile(entry)
    def test_archive_matches_complete_git_blob_inventory(self):
        self.archive_members([('root/file',b'abc',tarfile.REGTYPE)]);compare_archive(self.archive,self.files)
    def test_archive_mismatched_bytes_or_length_rejected(self):
        for raw in (b'bad',b'longer'):
            self.archive_members([('root/file',raw,tarfile.REGTYPE)])
            with self.assertRaises(ValueError):compare_archive(self.archive,self.files)
    def test_archive_extra_duplicate_or_missing_rejected(self):
        for members in [[],[('root/extra',b'abc',tarfile.REGTYPE)],
                [('root/file',b'abc',tarfile.REGTYPE),('root/file',b'abc',tarfile.REGTYPE)]]:
            self.archive_members(members)
            with self.assertRaises(ValueError):compare_archive(self.archive,self.files)
    def test_archive_symlink_and_unsafe_path_rejected(self):
        for name,kind in [('root/file',tarfile.SYMTYPE),('root/../file',tarfile.REGTYPE),('file',tarfile.REGTYPE)]:
            self.archive_members([(name,b'abc',kind)])
            with self.assertRaises(ValueError):compare_archive(self.archive,self.files)

if __name__=='__main__':unittest.main()
