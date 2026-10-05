"""Manual ENCITD01 source/format cases; never auto-run during development."""
import copy, shutil, struct, sys, tempfile, unittest, zlib
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from tools import item_details as pack


class ItemDetailsFormat(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.ir = pack.load(ROOT)
        # Actual generated receipt/pack is used, never substituted console art.
        cls.receipt = pack.verify_receipt(cls.ir, ROOT)
        cls.tables = pack.lower(cls.ir, cls.receipt)
        cls.blob = pack.encode(cls.tables)

    def blob_change(self, offset, raw):
        b = bytearray(self.blob); b[offset:offset+len(raw)] = raw
        b[16:20] = b'\0'*4; struct.pack_into('<I', b, 16, zlib.crc32(b))
        return b

    def reject_row(self, section, row, column, value):
        t = copy.deepcopy(self.tables); t[section][row][column] = value
        with self.assertRaises((ValueError, TypeError, struct.error, OverflowError)):
            pack.encode(t)

    def test_actual_pack_roundtrip(self):
        self.assertEqual(pack.parse_pack(self.blob), {k:v if k == 'Strings' else [tuple(r) for r in v]
                                                     for k,v in self.tables.items()})
        self.assertEqual((ROOT / pack.PACK).read_bytes(), self.blob)

    def test_source_ir_and_receipt(self):
        self.assertEqual(pack.load(ROOT), pack.build(ROOT))

    def test_stage_actual_resources(self):
        outputs = pack.stage_files(ROOT / 'romfs')
        self.assertEqual(set(outputs), {Path('data/opening.encdetails'),Path(pack.OUTPUT)})
        self.assertEqual(outputs[Path('data/opening.encdetails')], self.blob)

    def staged_copy(self, target):
        for path in ('data/opening.encdetails',pack.OUTPUT):
            dest = target / path; dest.parent.mkdir(parents=True,exist_ok=True)
            shutil.copyfile(ROOT / 'romfs' / path,dest)

    def test_stage_rejects_corrupt_texture(self):
        with tempfile.TemporaryDirectory() as tmp:
            target = Path(tmp); self.staged_copy(target)
            texture = target / pack.OUTPUT; blob = bytearray(texture.read_bytes()); blob[-1] ^= 1
            texture.write_bytes(blob)
            with self.assertRaises(ValueError): pack.stage_files(target)

    def test_stage_rejects_missing_texture(self):
        with tempfile.TemporaryDirectory() as tmp:
            target = Path(tmp); self.staged_copy(target); (target / pack.OUTPUT).unlink()
            with self.assertRaises(OSError): pack.stage_files(target)

    def test_stage_rejects_corrupt_pack(self):
        with tempfile.TemporaryDirectory() as tmp:
            target = Path(tmp); self.staged_copy(target)
            path = target / 'data/opening.encdetails'; blob = bytearray(path.read_bytes()); blob[-1] ^= 1
            path.write_bytes(blob)
            with self.assertRaises(ValueError): pack.stage_files(target)

    def test_current_doses_grammar(self):
        en, zh = self.ir['locales']
        self.assertEqual(en['total'] % 3, '3\u00a0uses.')
        self.assertEqual(en['left_singular'] % 1, '1\u00a0use\u00a0left.')
        self.assertEqual(en['left_plural'] % 0, '0\u00a0uses\u00a0left.')
        self.assertEqual(zh['total'] % 3, '可用3次。')
        self.assertEqual(zh['left_plural'] % 2, '剩余2次。')

    def test_raw_identity_unchanged(self):
        original = pack.read_json(ROOT / 'content/native-items.json')['definitions']
        self.assertEqual([d['raw_description'] for d in self.ir['definitions']],
                         [d['description'] for d in original])

    def test_unknown_ir_field(self):
        ir = copy.deepcopy(self.ir); ir['unknown'] = True
        with self.assertRaises(ValueError): pack.lower(ir, self.receipt)

    def test_unknown_ir_version(self):
        ir = copy.deepcopy(self.ir); ir['schema'] = 2
        with self.assertRaises(ValueError): pack.lower(ir, self.receipt)

    def test_unknown_markup(self):
        for s in ('[Unknown]', '[Ninten8]', '[color=#ffffff]a[/color]', '%d', '{value}'):
            with self.assertRaises(ValueError): pack.tokenize(s)

    def test_raw_token_mismatch(self):
        ir = copy.deepcopy(self.ir); ir['presentations'][0]['tokens'][0]['value'] = 'fake'
        with self.assertRaises(ValueError): pack.lower(ir, self.receipt)

    def test_reject_header_changes(self):
        for offset, value in ((8,2),(20,8),(24,2),(28,2)):
            with self.assertRaises(ValueError): pack.parse_pack(self.blob_change(offset, struct.pack('<I',value)))

    def test_source_pin(self):
        with self.assertRaises(ValueError): pack.parse_pack(self.blob_change(32,b'x'*20))

    def test_reserved_header(self):
        with self.assertRaises(ValueError): pack.parse_pack(self.blob_change(52,b'\1'))

    def test_corruption_and_truncation(self):
        bad = bytearray(self.blob); bad[-1] ^= 1
        for blob in (bad, self.blob[:-1], self.blob+b'\0'):
            with self.assertRaises(ValueError): pack.parse_pack(blob)

    def test_unknown_section_stride_and_overlap(self):
        for offset, raw in ((64,struct.pack('<H',99)),(66,struct.pack('<H',2)),
                            (84,struct.pack('<I',pack.HEADER))):
            with self.assertRaises(ValueError): pack.parse_pack(self.blob_change(offset,raw))

    def test_duplicate_definition(self): self.reject_row('Definitions',1,0,0)
    def test_missing_doses(self): self.reject_row('Definitions',1,3,0)
    def test_wrong_dose_policy(self): self.reject_row('Definitions',1,3,1)
    def test_boolean_metadata(self): self.reject_row('Definitions',1,3,True)
    def test_unknown_locale(self): self.reject_row('Presentations',0,1,2)
    def test_unowned_tokens(self): self.reject_row('Presentations',0,2,1)
    def test_unknown_token(self): self.reject_row('Tokens',0,0,99)
    def test_unknown_color(self): self.reject_row('Tokens',0,2,2)
    def test_reserved_token(self): self.reject_row('Tokens',0,3,1)
    def test_text_color_alpha(self): self.reject_row('Locales',0,6,0)
    def test_invalid_string_offset(self): self.reject_row('Tokens',0,1,1)
    def test_invalid_utf8(self):
        t = copy.deepcopy(self.tables); t['Strings'] = b'\0\xff\0'
        with self.assertRaises(ValueError): pack.encode(t)
    def test_wrong_texture_kind(self): self.reject_row('Resources',0,2,2)
    def test_missing_texture_bytes(self): self.reject_row('Resources',0,8,0)
    def test_unknown_parameter(self): self.reject_row('Parameters',0,0,99)
    def test_nan_parameter(self): self.reject_row('Parameters',0,1,float('nan'))
    def test_fractional_nickname_limit(self): self.reject_row('Parameters',2,1,6.5)


if __name__ == '__main__': unittest.main()
