#!/usr/bin/env python3
"""Manual-only ENCPRS01 admission negative coverage."""
import copy, json, struct, sys, unittest, zlib
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.house_presents import ROOT, PACK, encode, parse, load, program_commands, FIRST_TEXT_ID

class HousePresentsTests(unittest.TestCase):
    def setUp(self):
        self.ir = load(ROOT)
        self.house = json.loads((ROOT / 'content/native-house.json').read_text(encoding='utf-8'))
        self.blob = encode(self.ir, self.house)

    def change(self, blob, offset, fmt, value):
        b = bytearray(blob)
        struct.pack_into(fmt, b, offset, value)
        struct.pack_into('<I', b, 16, 0)
        struct.pack_into('<I', b, 16, zlib.crc32(b[32:]))
        return bytes(b)

    def test_reviewed_pack_matches_encoder(self):
        self.assertEqual((ROOT / 'romfs' / PACK).read_bytes(), self.blob)
        rows = parse(self.blob)
        self.assertEqual(len(rows['Objects']) // 52, 4)
        self.assertEqual(len(rows['Programs']) // 8, 1)
        self.assertEqual([t['id'] for t in self.ir['texts']], list(range(FIRST_TEXT_ID, FIRST_TEXT_ID + 3)))
        present4 = next(o for o in self.ir['objects'] if o['source_path'] == 'Objects/Present4')
        self.assertEqual(present4['item'], 'DogTreats')
        self.assertIsNotNone(present4['program'])
        for path in ('Objects/Present1', 'Objects/Present2', 'Objects/Present3'):
            self.assertIsNone(next(o for o in self.ir['objects'] if o['source_path'] == path)['program'])

    def test_missing_unknown_header_versions_rejected(self):
        for blob in (self.blob[:32], self.blob[:-1], self.change(self.blob, 8, 'I', 2),
                     self.change(self.blob, 20, 'I', 2), self.change(self.blob, 24, 'I', 2)):
            with self.subTest(size=len(blob)), self.assertRaises(ValueError):
                parse(blob)

    def test_crc_and_unknown_opcode_rejected(self):
        bad = bytearray(self.blob)
        bad[-1] ^= 1
        with self.assertRaises(ValueError):
            parse(bytes(bad))
        ir = copy.deepcopy(self.ir)
        ir['programs'][0]['commands'][0]['op'] = 'Invented'
        with self.assertRaises((ValueError, KeyError)):
            encode(ir, self.house)

    def test_text_prefix_and_unsupported_objects(self):
        ir = copy.deepcopy(self.ir)
        ir['texts'][0]['id'] = FIRST_TEXT_ID - 1
        with self.assertRaises(ValueError):
            encode(ir, self.house)
        commands = program_commands(self.ir['programs'][0], self.ir['objects'], FIRST_TEXT_ID + 2)
        self.assertEqual(commands[0]['op'], 'BranchFlag')
        self.assertEqual(commands[0]['a'], 'got_dog_treats')
        self.assertEqual([c['op'] for c in commands[1:6]],
                         ['PlayClip', 'GrantItem', 'SetFlag', 'ShowText', 'AwaitText'])

if __name__ == '__main__':
    unittest.main()
