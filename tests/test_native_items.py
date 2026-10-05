import copy
import importlib.util
import json
import struct
import tempfile
import unittest
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('native_items', ROOT / 'tools/native_items.py')
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)


class ItemsBinaryTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.ir = json.loads((ROOT / 'content/native-items.json').read_text())
        cls.tables = m.lower(cls.ir)
        cls.blob = m.encode(cls.tables)

    def corrupt(self, offset, value, fmt='<I'):
        b = bytearray(self.blob)
        struct.pack_into(fmt, b, offset, value)
        struct.pack_into('<I', b, 16, 0)
        struct.pack_into('<I', b, 16, zlib.crc32(b))
        return b

    def section(self, name):
        return struct.unpack_from('<I', self.blob, 64 + 16 * m.SECTION_NAMES.index(name) + 4)[0]

    def test_deterministic_checked_external_resource(self):
        m.verify_sources(self.ir)
        self.assertEqual(self.blob, m.encode(m.lower(self.ir)))
        self.assertEqual(self.blob, (ROOT / 'romfs/data/opening.encitems').read_bytes())
        parsed = m.parse_pack(self.blob)
        self.assertEqual(parsed['Metadata'][0][0], self.ir['capacity'])
        self.assertEqual(parsed['Instances'][0][2:], (1, 1))
        self.assertEqual(len(parsed['Definitions']), 2)
        self.assertEqual(len(parsed['Clips']), 3)

    def test_every_truncation_and_checksum(self):
        for size in range(len(self.blob)):
            with self.assertRaises(ValueError):
                m.parse_pack(self.blob[:size])
        bad = bytearray(self.blob)
        bad[-1] ^= 1
        with self.assertRaises(ValueError):
            m.parse_pack(bad)
        with self.assertRaises(ValueError):
            m.parse_pack(self.blob + b'\0')

    def test_crc_correct_header_directory_and_pin_rejections(self):
        for offset, value in [(0, 0), (8, 3), (12, 0), (20, 12), (24, 3), (28, 2),
                              (32, 0), (52, 1), (64, 0), (68, 0), (72, 0xffffffff),
                              (76, 1), (84, m.HEADER), (88, 2)]:
            with self.subTest(offset=offset, value=value), self.assertRaises(ValueError):
                m.parse_pack(self.corrupt(offset, value))

    def test_crc_correct_record_rejections(self):
        edits = {
            'Metadata': [(0, 0), (0, 65), (4, 0), (8, 1), (12, 1)],
            'Definitions': [(0, 0), (4, 2), (8, 0), (12, 0), (16, 999), (20, 4), (24, 65536), (40, 2), (44, 2)],
            'Instances': [(0, 0), (4, 999), (8, 2), (12, 0)],
            'Resources': [(0, 0), (4, 2), (8, 4), (12, 0), (16, 0), (20, 0), (24, 0)],
            'Layouts': [(0, 0), (4, 99), (8, 0), (12, 99), (16, 999), (20, 999), (24, 16),
                        (28, 0x40000000), (36, 0x7fc00000), (44, 0xbf800000), (52, 0x40000000), (68, 8193)],
            'Parameters': [(0, 99), (4, 0x7fc00000), (4, 0)],
            'Clips': [(0, 0), (4, 99), (8, 999), (12, 0), (16, 0), (20, 2)],
            'Tracks': [(0, 999), (4, 99), (8, 2), (12, 999), (16, 0)],
            'Keys': [(0, 0x7fc00000), (0, 0x3f800000), (4, 0x7fc00000), (8, 0x7fc00000)],
            'Sounds': [(0, 0), (4, 2), (8, 0), (12, 1)],
        }
        for section, changes in edits.items():
            for off, value in changes:
                with self.subTest(section=section, offset=off, value=value), self.assertRaises(ValueError):
                    m.parse_pack(self.corrupt(self.section(section) + off, value))

    def test_utf8_and_string_boundary_rejections(self):
        pool = self.section('Strings')
        for offset, value in [(pool, 1), (pool + 1, 0xff), (pool + 1, 0xc0), (pool + 1, 0xed)]:
            with self.subTest(offset=offset, value=value), self.assertRaises((ValueError, UnicodeError)):
                m.parse_pack(self.corrupt(offset, value, '<B'))
        for name in ['Definitions', 'Resources', 'Sounds']:
            with self.assertRaises(ValueError):
                m.parse_pack(self.corrupt(self.section(name) + 4, 0xffffffff))

    def test_ownership_duplicates_and_parent_cycle(self):
        for name, off in [('Clips', 4), ('Clips', 8), ('Parameters', 0), ('Resources', 0), ('Resources', 4), ('Layouts', 0), ('Sounds', 0)]:
            stride = m.STRIDES[m.SECTION_NAMES.index(name)]
            base = self.section(name)
            value = struct.unpack_from('<I', self.blob, base + off)[0]
            with self.subTest(name=name, off=off), self.assertRaises(ValueError):
                m.parse_pack(self.corrupt(base + stride + off, value))
        with self.assertRaises(ValueError):
            m.parse_pack(self.corrupt(self.section('Layouts') + 84 + 8, 1))

    def test_source_unknown_fields_and_ranges_rejected(self):
        changed = copy.deepcopy(self.ir)
        changed['sources'][next(iter(changed['sources']))] = '0' * 64
        with self.assertRaises(ValueError):
            m.verify_sources(changed)
        for mutation in [lambda ir: ir.update(extra=True),
                         lambda ir: ir['definitions'][0].update(extra=True),
                         lambda ir: ir['definitions'][0].update(flags=2),
                         lambda ir: ir['initial_inventory'][0].update(doses=0),
                         lambda ir: ir['parameters']['GridShape'].__setitem__(0, 0),
                         lambda ir: ir['parameters']['NormalColor'].__setitem__(0, 2),
                         lambda ir: ir['parameters']['InfoMotion'].__setitem__(0, float('nan')),
                         lambda ir: ir['clips'][0]['tracks'][0].update(property='Unknown')]:
            ir = copy.deepcopy(self.ir)
            mutation(ir)
            with self.assertRaises(ValueError):
                m.lower(ir)

    def test_required_bindings_parameter_rules_and_event_coverage(self):
        edits = [(1, 12, 0x3f800000), (3, 12, 0), (5, 12, 0), (6, 4, 0),
                 (6, 8, 0xbf800000), (6, 12, 0), (6, 16, 0x3f800000),
                 (7, 4, 0), (7, 8, 0xbf800000), (7, 12, 0x3f800000),
                 (8, 4, 0x40000000), (11, 12, 0), (11, 12, 0x40400000),
                 (11, 16, 0x3f800000)]
        for parameter, off, value in edits:
            with self.subTest(parameter=parameter, off=off), self.assertRaises(ValueError):
                m.parse_pack(self.corrupt(self.section('Parameters') + (parameter - 1) * 20 + off, value))
        for i, layout in enumerate(self.tables['Layouts']):
            if layout[1] in (3, 8):
                with self.assertRaises(ValueError):
                    m.parse_pack(self.corrupt(self.section('Layouts') + i * 84 + 4, 1))
        for badflag in (4, 16):
            with self.assertRaises(ValueError):
                m.parse_pack(self.corrupt(self.section('Layouts') + 24, badflag))
        ir = copy.deepcopy(self.ir)
        ir['sounds'].pop()
        with self.assertRaises(ValueError):
            m.lower(ir)

    def test_independent_data_edits_and_empty_inventory(self):
        ir = copy.deepcopy(self.ir)
        ir['initial_inventory'] = []
        ir['parameters']['GridShape'][0] = 1
        ir['layouts'][0]['rect'][0] += 1
        b = m.encode(m.lower(ir))
        self.assertNotEqual(b, self.blob)
        parsed = m.parse_pack(b)
        self.assertEqual(parsed['Instances'], [])
        self.assertEqual(parsed['Layouts'][0][9], self.tables['Layouts'][0][9] + 1)

    def test_exact_asset_staging_and_changed_hash(self):
        files = m.stage_files(ROOT / 'romfs')
        self.assertEqual(len(files), len(self.tables['Resources']) + 1)
        self.assertEqual(files[Path('data/opening.encitems')], self.blob)
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            for name, data in files.items():
                (root / name).parent.mkdir(parents=True, exist_ok=True)
                (root / name).write_bytes(data)
            resource = next(name for name in files if name != Path('data/opening.encitems'))
            (root / resource).write_bytes(b'changed')
            with self.assertRaises(ValueError):
                m.stage_files(root)

    def test_explicit_rich_capability_and_legacy_plain_data(self):
        self.assertEqual(struct.unpack_from('<I',self.blob,8)[0],2)
        self.assertEqual(struct.unpack_from('<I',self.blob,24)[0],2)
        self.assertEqual(struct.unpack_from('<I',self.blob,28)[0],1)
        rich=self.tables['Definitions'][1]
        self.assertEqual((rich[10],rich[11]),(2,0))
        self.assertEqual(rich[5],m.NO_INDEX)
        pool=self.tables['Strings'];description=pool[rich[3]:pool.index(0,rich[3])].decode('utf-8')
        self.assertIn('[Ninten]',description);self.assertIn('[Asthma]',description);self.assertIn('%s',description)
        legacy=copy.deepcopy(self.tables);legacy['Definitions']=legacy['Definitions'][:1]
        blob=m.encode(legacy)
        self.assertEqual(struct.unpack_from('<I',blob,8)[0],1)
        self.assertEqual(struct.unpack_from('<I',blob,24)[0],1)
        self.assertEqual(len(m.parse_pack(blob)['Definitions']),1)

    def test_rich_definition_cannot_claim_action_or_old_capability(self):
        definition=self.section('Definitions')+m.STRIDES[m.SECTION_NAMES.index('Definitions')]
        for offset,value in ((definition+40,4),(definition+40,3),(definition+44,1),(definition+20,0)):
            with self.subTest(offset=offset,value=value),self.assertRaises(ValueError):
                m.parse_pack(self.corrupt(offset,value))
        for version,caps in ((1,1),(1,2),(2,1),(2,3),(3,2)):
            bad=bytearray(self.blob);struct.pack_into('<I',bad,8,version);struct.pack_into('<I',bad,24,caps)
            struct.pack_into('<I',bad,16,0);struct.pack_into('<I',bad,16,zlib.crc32(bad))
            with self.subTest(version=version,caps=caps),self.assertRaises(ValueError):m.parse_pack(bad)


if __name__ == '__main__':
    unittest.main()
