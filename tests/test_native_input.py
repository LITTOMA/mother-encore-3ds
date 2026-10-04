import copy
import json
import math
import struct
import tempfile
import unittest
import zlib
from pathlib import Path
from tools import native_input as n


class NativeInputTests(unittest.TestCase):
    def setUp(self):
        self.ir = n.load_ir()

    def test_checked_resource_roundtrip_and_external_tuning(self):
        blob = n.encode(self.ir)
        self.assertEqual(len(blob), 88)
        self.assertEqual(n.OUT.read_bytes(), blob)
        self.assertEqual(n.encode(n.decode(blob)), blob)
        changed = copy.deepcopy(self.ir)
        changed['circle_pad']['activate_radius'] = 30
        self.assertNotEqual(n.encode(changed), blob)
        self.assertEqual(n.decode(n.encode(changed))['circle_pad']['activate_radius'], 30)

    def test_unknown_missing_schema_and_duplicate_fields(self):
        for key in self.ir:
            ir = copy.deepcopy(self.ir)
            del ir[key]
            with self.subTest(missing=key), self.assertRaises(ValueError):
                n.encode(ir)
        for target in (None, 'circle_pad', 'touch', 'visual'):
            ir = copy.deepcopy(self.ir)
            (ir if target is None else ir[target])['unexpected'] = 0
            with self.subTest(target=target), self.assertRaises(ValueError):
                n.encode(ir)
        for version in (0, 2, True, '1'):
            ir = copy.deepcopy(self.ir)
            ir['schema'] = version
            with self.subTest(version=version), self.assertRaises(ValueError):
                n.encode(ir)
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / 'duplicate.json'
            path.write_text('{"schema":1,"schema":1}')
            with self.assertRaisesRegex(ValueError, 'Duplicate'):
                n.load_ir(path)

    def test_unknown_header_and_corruption(self):
        original = n.encode(self.ir)
        for size in range(len(original)):
            with self.subTest(size=size), self.assertRaises(ValueError):
                n.decode(original[:size])
        with self.assertRaises(ValueError):
            n.decode(original + b'\x00')
        for offset in (0, 8, 12, 20, 24, 28, 40):
            blob = bytearray(original)
            blob[offset] ^= 1
            with self.subTest(offset=offset), self.assertRaises(ValueError):
                n.decode(blob)
            if offset != 40:
                struct.pack_into('<I', blob, 16, 0)
                struct.pack_into('<I', blob, 16, zlib.crc32(blob))
                with self.subTest(resealed=offset), self.assertRaises(ValueError):
                    n.decode(blob)

    def test_nonfinite_boolean_and_wrong_type_tuning(self):
        for key in self.ir['circle_pad']:
            for value in (math.nan, math.inf, -math.inf, True, '24', None):
                ir = copy.deepcopy(self.ir)
                ir['circle_pad'][key] = value
                with self.subTest(key=key, value=value), self.assertRaises(ValueError):
                    n.encode(ir)

    def test_invalid_radii_angle_duration_viewport_and_alpha(self):
        mutations = [('circle_pad', 'nominal_radius', 23), ('circle_pad', 'activate_radius', 18),
                     ('circle_pad', 'release_radius', 0), ('circle_pad', 'angular_hysteresis_degrees', 22.5),
                     ('touch', 'activate_radius', 6), ('touch', 'release_radius', 0),
                     ('touch', 'tap_max_travel', 9), ('touch', 'tap_max_seconds', 0),
                     ('touch', 'tap_max_seconds', 3), ('touch', 'width', 400), ('touch', 'height', 180),
                     ('touch', 'width', 320.0), ('visual', 'thumb_radius', 32), ('visual', 'base_radius', 121),
                     ('visual', 'base_rgba', [0, 0, 0, 128]), ('visual', 'thumb_rgba', [1, 2, 3, 0]),
                     ('visual', 'base_rgba', [0, 0, 0]), ('visual', 'base_rgba', [256, 0, 0, 40]),
                     ('visual', 'base_rgba', [True, 0, 0, 40])]
        for group, key, value in mutations:
            ir = copy.deepcopy(self.ir)
            ir[group][key] = value
            with self.subTest(group=group, key=key, value=value), self.assertRaises(ValueError):
                n.encode(ir)

    def test_tuning_precision_collapse_rejected(self):
        ir = copy.deepcopy(self.ir)
        ir['circle_pad']['release_radius'] = 24 - 1e-8
        with self.assertRaisesRegex(ValueError, 'precision'):
            n.encode(ir)

    def test_resealed_invalid_payload_rejected(self):
        for field in range(10):
            blob = bytearray(n.encode(self.ir))
            struct.pack_into('<f', blob, 32 + field*4, math.nan)
            struct.pack_into('<I', blob, 16, 0)
            struct.pack_into('<I', blob, 16, zlib.crc32(blob))
            with self.subTest(field=field), self.assertRaises(ValueError):
                n.decode(blob)

    def test_compile_file_creates_checked_independent_resource(self):
        with tempfile.TemporaryDirectory() as tmp:
            source, output = Path(tmp) / 'input.json', Path(tmp) / 'romfs/input/test.encinput'
            source.write_text(json.dumps(self.ir))
            blob = n.compile_file(source, output)
            self.assertEqual(output.read_bytes(), blob)
            self.assertEqual(n.decode(blob)['visual']['base_rgba'], [170, 192, 202, 45])

    def test_staging_rejects_stale_corrupt_and_escaping_resource(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / 'romfs'
            path = root / 'data/native.encinput'
            path.parent.mkdir(parents=True)
            blob = n.encode(self.ir)
            path.write_bytes(blob)
            self.assertEqual(n.stage_files(root), {Path('data/native.encinput'): blob})
            changed = copy.deepcopy(self.ir)
            changed['circle_pad']['activate_radius'] = 30
            path.write_bytes(n.encode(changed))
            with self.assertRaisesRegex(ValueError, 'stale'):
                n.stage_files(root)
            path.write_bytes(blob[:-1])
            with self.assertRaises(ValueError):
                n.stage_files(root)
            path.unlink()
            outside = Path(tmp) / 'outside.encinput'
            outside.write_bytes(blob)
            path.symlink_to(outside)
            with self.assertRaisesRegex(ValueError, 'escape'):
                n.stage_files(root)


if __name__ == '__main__':
    unittest.main()
