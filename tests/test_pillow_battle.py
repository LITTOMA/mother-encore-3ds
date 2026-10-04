import copy, json, struct, unittest, zlib
from pathlib import Path
from PIL import Image
from tools import pillow_entry_asset as entry
from tools import native_battle, native_round
from tools.battle_assets import decode_indexed

ROOT = Path(__file__).resolve().parents[1]

class PillowBattleTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.ir = json.loads(entry.IR_PATH.read_text())
        cls.receipt = json.loads(entry.RECEIPT.read_text())
        cls.blob, cls.tables = entry.compile_pack(cls.ir)
        cls.round = json.loads((ROOT / 'content/pillow-round.json').read_text())
        cls.round_blob = (ROOT / 'romfs/data/pillow-entry.encround').read_bytes()

    def test_source_and_deterministic_compilation(self):
        self.assertEqual(self.blob, entry.PACK.read_bytes())
        native_round.verify_sources(self.round)
        self.assertEqual(native_round.encode(native_round.lower(self.round)), self.round_blob)
        self.assertEqual(native_round.parse_pack(self.round_blob).version, 5)
        files = native_battle.stage_files(ROOT / 'romfs', Path('data/pillow-entry.encbattle'))
        self.assertIn(Path('graphics/battle/pillow/pillow-world.t3x'), files)
        files = native_round.stage_files(ROOT / 'romfs', Path('data/pillow-entry.encround'))
        self.assertIn(Path('graphics/battle/pillow/pillow-enemy.t3x'), files)

    def test_exact_enemy_rewards_and_normal_encounter(self):
        enemy = self.ir['enemy']['data']
        self.assertEqual((enemy['hp'], enemy['maxhp'], enemy['exp'], enemy['cash'], enemy['boss']), (28, 28, 5, 5, False))
        self.assertEqual(self.ir['enemy']['sprite_size'], [36, 45])
        self.assertEqual(self.tables['metadata'][0][0:1] + self.tables['metadata'][0][2:3] + self.tables['metadata'][0][5:6], [3, 1001, 6])
        self.assertEqual(self.round['binding']['battle_id'], 3)
        self.assertEqual(self.round['victory']['enemy_body_id'], 12)
        self.assertEqual([x['weight'] for x in self.round['enemy_choices']], [5, 1])
        self.assertEqual((self.round['victory']['initial_exp'], self.round['victory']['reward_exp']), (3, 5))
        self.assertEqual((self.round['victory']['next_level_exp'], self.round['encounter']['following_level_exp']), (9, 27))

    def test_lossless_original_background(self):
        spec = self.ir['presentation']['assets']['background']
        image = Image.open(ROOT / 'upstream/MOTHER-Encore' / spec['source']).convert('RGBA')
        decoded = decode_indexed((ROOT / 'romfs' / spec['output']).read_bytes())
        self.assertEqual([decoded['palette'][n] for n in decoded['pixels']], list(image.getdata()))
        self.assertEqual(self.ir['background']['native_layer_size'], [176, 176])
        layers = self.ir['background']['layers']
        self.assertEqual([p['properties']['move'] for p in layers], [[-.6, .4], [-.4, -.4]])
        self.assertEqual([p['properties']['ping_pong_speed'] for p in layers], [[0, 0], [.7, .7]])
        self.assertEqual([p['properties']['opacity'] for p in layers], [1, .5])
        self.assertTrue(all(r[1] == 2 and r[-4:] == [0xffffffff, 0, 0, 0xffffffff] for r in self.tables['backgrounds']))

    def test_original_postwin_and_absent_boss_effects(self):
        self.assertEqual(self.ir['entry']['win_flag'], '')
        e = self.round['encounter']
        self.assertEqual((e['boss'], e['keep_actor'], e['post_win_script'], e['boss_flash_media'], e['stop_area_music_if_overworld']), (0, 0, 'Podunk/cutscenes/minnie_leave', 0xffffffff, 0))
        self.assertEqual(self.round['boss_shakes'], [])
        self.assertTrue(all(v['kind'] < 10 for v in self.round['presentation']['events']))
        self.assertTrue(all(v['role'] != 12 for v in self.round['presentation']['media']))
        self.assertEqual(len(self.round['growth']), 7)

    def test_live_text_uses_pillow_identity(self):
        binding = self.round['binding']
        indexes = [s['dialog'] for s in self.round['skills']] + [binding[k] for k in ['enemy_name', 'enemy_article', 'enemy_outro', 'mortal_damage', 'no_effect']]
        for index in indexes:
            self.assertNotIn('Lamp', self.round['texts'][index]['text'])
            self.assertNotIn('Doll', self.round['texts'][index]['text'])

    def test_unreviewed_entry_mutations_fail_closed(self):
        for field, value in [('hp', 29), ('boss', True), ('exp', 6)]:
            ir = copy.deepcopy(self.ir); ir['enemy']['data'][field] = value
            with self.subTest(field=field), self.assertRaises(ValueError): entry.validate(ir)
        for field, value in [('move', [0, 0]), ('palette_shifting', True), ('unknown', 1)]:
            ir = copy.deepcopy(self.ir); ir['background']['layers'][0]['properties'][field] = value
            with self.subTest(field=field), self.assertRaises(ValueError): entry.validate(ir)
        ir = copy.deepcopy(self.ir); ir['background']['compatibility_policy'] = {'fixed_palette_row': 0}
        with self.assertRaises(ValueError): entry.validate(ir)
        ir = copy.deepcopy(self.ir); ir['binary_version'] = 1
        with self.assertRaises(ValueError): native_battle.lower(ir, self.receipt)

    def test_new_schema_negative_cases(self):
        for field, value in [('boss', 1), ('keep_actor', 2), ('post_win_script', '../escape'), ('boss_flash_media', 0), ('promoted_level', 1), ('following_level_exp', 8), ('level_text', 0), ('learned_skill', ''), ('stop_area_music_if_overworld', 2)]:
            ir = copy.deepcopy(self.round); ir['encounter'][field] = value
            with self.subTest(field=field), self.assertRaises(ValueError): native_round.parse_pack(native_round.encode(native_round.lower(ir, verify_assets=False)))
        ir = copy.deepcopy(self.round); ir['boss_shakes'] = [dict(time=1, magnitude=2, length=1, interval=.2, weight=.5)]
        with self.assertRaises(ValueError): native_round.parse_pack(native_round.encode(native_round.lower(ir, verify_assets=False)))
        ir = copy.deepcopy(self.round); ir['presentation']['events'][0]['kind'] = 10
        with self.assertRaises(ValueError): native_round.parse_pack(native_round.encode(native_round.lower(ir, verify_assets=False)))
        ir = copy.deepcopy(self.round); ir['presentation']['media'][0]['role'] = 12
        with self.assertRaises(ValueError): native_round.parse_pack(native_round.encode(native_round.lower(ir, verify_assets=False)))
        for version in [2, 3, 4, 6, 99]:
            blob = bytearray(self.round_blob)
            for offset in [8, 24, 28]: struct.pack_into('<I', blob, offset, version)
            struct.pack_into('<I', blob, 16, 0); struct.pack_into('<I', blob, 16, zlib.crc32(blob))
            with self.subTest(version=version), self.assertRaises(ValueError): native_round.parse_pack(blob)

    def test_truncated_resources_rejected(self):
        for blob, parse in [(self.blob, native_battle.parse_sections), (self.round_blob, native_round.parse_pack)]:
            for size in [0, 7, 8, 63, 64, 287, 288, 319, 335, 336, len(blob) - 1]:
                with self.subTest(size=size), self.assertRaises((ValueError, struct.error)): parse(blob[:size])

if __name__ == '__main__': unittest.main()
