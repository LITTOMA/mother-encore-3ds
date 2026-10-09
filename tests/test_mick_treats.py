#!/usr/bin/env python3
import copy
import struct
import unittest
from pathlib import Path
import tools.mick_treats as mick

ROOT = Path(__file__).resolve().parents[1]


class MickTreatsTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.ir = mick.load(ROOT)
        cls.blob = mick.encode(cls.ir)

    def test_actor_and_programme(self):
        a = self.ir['actor']
        self.assertEqual(a['source_path'], 'Objects/NPCS/npc21')
        self.assertEqual(a['item'], 'DogTreats')
        self.assertEqual(a['require_flag'], 'got_dog_treats')
        self.assertEqual(a['consume_flag'], 'gave_treats')
        self.assertEqual(a['frame'], 1)
        treats = next(p for p in self.ir['programmes'] if p['flag'] == 'got_dog_treats')
        ops = [c['op'] for c in self.ir['commands'][treats['first']:treats['first'] + treats['count']]]
        self.assertEqual(ops, ['ShowText', 'AwaitText', 'ShowText', 'AwaitText',
                               'SetFlag', 'RemoveKeyItem', 'ShowText', 'AwaitText', 'End'])
        bark = next(p for p in self.ir['programmes'] if p['kind'] == 'Bark')
        bark_ops = [c['op'] for c in self.ir['commands'][bark['first']:bark['first'] + bark['count']]]
        self.assertEqual(bark_ops, ['JumpActor', 'TurnActor', 'ShowText', 'AwaitText',
                                    'ShowText', 'AwaitText', 'MovePlayer', 'Wait', 'End'])
        self.assertEqual(self.ir['actor']['bark_path'], 'Cutscenes/Cutscene Area11')
        self.assertEqual(self.ir['actor']['bark_center'], [-24.0, 64.0])
        self.assertEqual(self.ir['actor']['bark_extents'], [56.0, 8.0])
        self.assertTrue(any(c['op'] == 'Choice' for c in self.ir['commands']))
        self.assertTrue(any(c['op'] == 'SetFlag' and c['a'] == 'mick_scratch' for c in self.ir['commands']))

    def test_pack_round_trip(self):
        rows = mick.parse(self.blob)
        self.assertEqual(len(rows['Actor']) // 200, 1)
        self.assertEqual(len(rows['Texture']) // 12, 1)
        self.assertGreater(len(rows['Programmes']) // 16, 1)
        self.assertGreater(len(rows['Rng']) // 32, 1)

    def test_reject_truncated_and_bad_magic(self):
        with self.assertRaises(Exception):
            mick.parse(self.blob[:-1])
        bad = bytearray(self.blob)
        bad[0:8] = b'ENCMIK00'
        with self.assertRaises(Exception):
            mick.parse(bytes(bad))

    def test_reject_stale_ir(self):
        mutated = copy.deepcopy(self.ir)
        mutated['actor']['frame'] = 0
        path = ROOT / 'content/mick-treats.json'
        original = path.read_bytes()
        try:
            mick.write_json(path, mutated)
            with self.assertRaises(ValueError):
                mick.load(ROOT)
        finally:
            path.write_bytes(original)


if __name__ == '__main__':
    unittest.main()
