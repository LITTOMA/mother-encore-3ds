import copy
import json
import struct
import tempfile
import unittest
import zlib
from pathlib import Path
from tools import native_phone as p


class PhonePackTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.blob=p.compile_pack()
        cls.ir=json.loads((p.ROOT/'content/phone-stage/presentation.json').read_text())

    def test_checked_binary_is_reproducible_and_independent(self):
        self.assertEqual(p.OUT.read_bytes(),self.blob)
        parsed=p.parse_pack(self.blob)
        self.assertEqual(parsed['commit'],p.PIN)
        self.assertEqual(p.encode(parsed['sections']),self.blob)
        self.assertEqual(list(parsed['sections']),p.SECTIONS)
        self.assertEqual(len(parsed['sections']['Objects']),1)
        self.assertEqual(len(parsed['sections']['Clips']),2)
        self.assertEqual(len(parsed['sections']['FrameKeys']),9)
        self.assertEqual(len(parsed['sections']['SoundKeys']),2)
        self.assertEqual(len(parsed['sections']['Dispatch']),2)

    def test_unknown_staging_fields_and_semantics_fail_before_pack(self):
        for mutate in [lambda x:x.update(extra=1),lambda x:x.update(schema=2),
                       lambda x:x['objects'][0]['use'].update(is_payphone=True),
                       lambda x:x['objects'][0]['interaction'].update(creates_actor=True),
                       lambda x:x['objects'][0]['dispatch']['overrides'].reverse(),
                       lambda x:x['objects'][0]['clips'][1]['events'][0].update(frame=999),
                       lambda x:x['sources'].update({'Maps/Testing/phone.gd':'0'*64})]:
            ir=copy.deepcopy(self.ir);mutate(ir)
            with self.assertRaisesRegex(ValueError,'Unreviewed'):
                p.lower(ir)

    def test_header_directory_crc_truncation_rejected(self):
        def rehash(blob):
            blob[16:20]=b'\0'*4
            struct.pack_into('<I',blob,16,zlib.crc32(blob))
            return blob
        for offset in [8,24,28]:
            blob=bytearray(self.blob);struct.pack_into('<I',blob,offset,2)
            with self.assertRaisesRegex(ValueError,'schema'):
                p.parse_pack(rehash(blob))
        for size in [0,191,len(self.blob)-1]:
            with self.assertRaises(ValueError):p.parse_pack(self.blob[:size])
        blob=bytearray(self.blob);blob[-1]^=1
        with self.assertRaisesRegex(ValueError,'CRC'):p.parse_pack(blob)
        blob=bytearray(self.blob);blob[52]=1
        with self.assertRaisesRegex(ValueError,'reserved'):p.parse_pack(rehash(blob))
        blob=bytearray(self.blob);struct.pack_into('<I',blob,64+16+4,192)
        with self.assertRaisesRegex(ValueError,'span'):p.parse_pack(rehash(blob))
        blob=bytearray(self.blob);struct.pack_into('<H',blob,64+16,999)
        with self.assertRaisesRegex(ValueError,'directory'):p.parse_pack(rehash(blob))

    def test_staging_checks_exact_pack_and_referenced_texture(self):
        files=p.stage_files(p.ROOT/'romfs')
        self.assertEqual(set(files),{Path('data/opening.encphone'),Path('graphics/ui/phone/phone.t3x')})
        for target in files:
            with tempfile.TemporaryDirectory()as td:
                root=Path(td)
                for path,data in files.items():
                    (root/path).parent.mkdir(parents=True,exist_ok=True)
                    (root/path).write_bytes(data)
                (root/target).write_bytes(b'changed')
                with self.assertRaises(ValueError):p.stage_files(root)
        with tempfile.TemporaryDirectory()as td:
            root=Path(td)
            (root/'data').mkdir()
            (root/'data/opening.encphone').write_bytes(self.blob)
            (root/'phone-preview').symlink_to(p.ROOT/'romfs/graphics/ui/phone',target_is_directory=True)
            with self.assertRaisesRegex(ValueError,'path escape'):p.stage_files(root)


if __name__=='__main__':unittest.main()
