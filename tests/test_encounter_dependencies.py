"""Focused source/strict-admission tests; no runtime combat or RNG is executed."""
import copy
import json
from pathlib import Path
import sys
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import encounter_dependencies as e

LAMP = 'Data/Dialogue/Podunk/cutscenes/lamp_attack.yaml#12'
DOLL = 'Data/Dialogue/Podunk/cutscenes/doll_attack.yaml#6'
PILLOW = 'Data/Dialogue/Podunk/cutscenes/pillow_attack.yaml#6'
TRIPLE = 'Data/Dialogue/Merrysville/Cutscenes/DrDistorto3.yaml#battle'
PODUNK = 'Maps/podunk/podunk.tscn'

class EncounterDependenciesTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.compiler = e.Compiler(ROOT)
        cls.doc = cls.compiler.build()
        cls.scenes = {s['key']: s for s in cls.doc['scenes']}
        cls.formations = {f['key']: f for f in cls.doc['formations']}

    def test_real_inventory_coverage(self):
        self.assertEqual(len(self.scenes), 13)
        candidates = [c for s in self.scenes.values() for c in s['candidates']]
        self.assertEqual(len(candidates), 196)
        self.assertEqual(sum(c['source_kind'] == 'spawner' for c in candidates), 192)
        self.assertEqual(sum(c['source_kind'] == 'direct_actor' for c in candidates), 4)
        self.assertEqual(len(self.formations), 20)
        self.assertEqual(len(self.doc['enemy_profiles']), 45)
        expected = sorted(p for p in self.compiler.inventory['files'] if
                          p.startswith('Maps/') and p.endswith('.tscn') or
                          p.startswith('Data/Dialogue/') and p.endswith('.yaml'))
        self.assertEqual(self.doc['scanned_source_files'], expected)

    def test_no_rng_sampling_and_deterministic_bytes(self):
        with patch('random.random', side_effect=AssertionError('RNG sampled')), patch('random.randrange', side_effect=AssertionError('RNG sampled')):
            again = e.Compiler(ROOT).build()
        self.assertEqual(e.canonical(again), e.canonical(self.doc))
        self.assertEqual(self.doc['rng_samples_consumed'], 0)
        self.assertEqual((ROOT / 'content/encounter-dependencies.json').read_bytes(), e.canonical(self.doc))

    def test_actual_podunk_multiple_candidates(self):
        scene = self.scenes[PODUNK]
        self.assertEqual(len(scene['candidates']), 68)
        self.assertEqual(len(scene['source_resource_union']['enemy_ids']), 14)
        self.assertEqual({c['gate']['raw_appearance_rate'] for c in scene['candidates']}, {75, 80, 100})
        eighty = [c for c in scene['candidates'] if c['gate']['raw_appearance_rate'] == 80]
        self.assertEqual(len(eighty), 11)
        self.assertTrue(all(c['gate']['accepted_residue_count'] == 81 for c in eighty))
        self.assertTrue(all(c['gate']['normalized_choice_weight'] is None for c in scene['candidates']))
        self.assertFalse(scene['formation_domain']['spatial_coexistence_proven'])
        self.assertEqual(scene['compiled_dependencies'], [])

    def test_appearance_domain_boundaries(self):
        for rate, count in [(0,0), (1,2), (50,51), (75,76), (80,81), (99,100), (100,100)]:
            self.assertEqual(e.appearance_gate(rate)['accepted_residue_count'], count)
        for rate in [-1,101,True,80.0,'80']:
            with self.subTest(rate=rate), self.assertRaises(e.DependencyError):
                e.appearance_gate(rate)

    def test_actual_ordered_multi_enemy_union(self):
        formation = self.formations[TRIPLE]
        self.assertEqual(formation['ordered_members'], [
            {'enemy':'scrapper','actor':'enemy1'}, {'enemy':'oldrobot','actor':'enemy2'}, {'enemy':'barbot','actor':'enemy3'}])
        resources = set(formation['source_resource_union']['resources'])
        for enemy, sprite in [('scrapper','scrapper'),('oldrobot','old robot'),('barbot','barbot')]:
            self.assertIn('Data/Battlers/' + enemy + '.yaml', resources)
            self.assertIn('Graphics/Battle Sprites/' + sprite + '.png', resources)
        self.assertIn('Graphics/Battle BGS/scrapper_pal.png', resources)
        self.assertIn('Audio/Music/Battle Encounter/Encounter Enemy.mp3', resources)
        self.assertIsNone(formation['compiled'])

    def test_reinforcement_weights_and_provenance(self):
        tables = {t['source']: t for t in self.doc['reinforcement_tables']}
        self.assertEqual(len(tables), 6)
        table = tables['Data/BattleSkills/callHelpDistorto.yaml']
        self.assertEqual(table['entries'], [{'ally':enemy,'weight':1} for enemy in ('oldrobot','scrapper','fireball','bomber','barbot')])
        self.assertFalse(table['weights_normalized'])
        self.assertEqual(table['rng_samples_consumed'], 0)
        starman = self.formations['Data/Dialogue/Podunk/cutscenes/starman_jr_attack.yaml#7']
        self.assertTrue({'starmanjr','rat','hyena','tiger','alligator','gorilla'} <= set(starman['source_resource_union']['enemy_ids']))
        self.assertIsNone(starman['compiled'])

    def test_existing_checked_pairs_adapt(self):
        result = e.adapt_compiled_dependencies(self.doc, [LAMP,DOLL,PILLOW], 42, ROOT)
        self.assertEqual(result['scene_epoch'], 42)
        self.assertEqual([p['battle_pack'] for p in result['encounters']], ['data/opening.encbattle','data/doll-entry.encbattle','data/pillow-entry.encbattle'])
        self.assertTrue(all(set(p) == {'key','battle_pack','round_pack'} for p in result['encounters']))

    def test_source_scene_and_multi_enemy_fail_readiness(self):
        for key in [PODUNK, TRIPLE, 'Data/Dialogue/Podunk/cutscenes/bridge_fight.yaml#2']:
            with self.subTest(key=key), self.assertRaisesRegex(e.DependencyError, 'readiness rejected'):
                e.adapt_compiled_dependencies(self.doc, [key], 1, ROOT)
        with self.assertRaisesRegex(e.DependencyError, 'readiness rejected'):
            e.adapt_compiled_dependencies(self.doc, [LAMP, TRIPLE], 1, ROOT)

    def test_unknown_versions_and_epochs(self):
        for field, value in [('schema',2), ('kind','fake'), ('commit','0'*40), ('rng_samples_consumed',1)]:
            doc = copy.deepcopy(self.doc); doc[field] = value
            with self.subTest(field=field), self.assertRaises(e.DependencyError):
                e.validate_manifest(doc)
        for epoch in [0,-1,True,2**64]:
            with self.subTest(epoch=epoch), self.assertRaises(e.DependencyError):
                e.adapt_compiled_dependencies(self.doc, [LAMP], epoch, ROOT)
        for keys in [[],[LAMP,LAMP],['unknown']]:
            with self.subTest(keys=keys), self.assertRaises(e.DependencyError):
                e.adapt_compiled_dependencies(self.doc, keys, 1, ROOT)

    def test_resource_union_cannot_omit_one_member(self):
        doc = copy.deepcopy(self.doc)
        formation = next(f for f in doc['formations'] if f['key'] == TRIPLE)
        formation['source_resource_union']['resources'].remove('Graphics/Battle Sprites/barbot.png')
        with self.assertRaisesRegex(e.DependencyError, 'Incomplete resource'):
            e.validate_manifest(doc)
        doc = copy.deepcopy(self.doc)
        doc['scenes'][0]['source_resource_union']['enemy_ids'].append('invented')
        with self.assertRaises(e.DependencyError):
            e.validate_manifest(doc)

    def test_unknown_resource_profile_rejected(self):
        compiler = e.Compiler(ROOT)
        original = compiler.text
        def changed(path):
            text = original(path)
            return text.replace('shader="[DEFAULT]"', 'shader="unreviewed.shader"') if path.endswith('lamp.bbg') else text
        with patch.object(compiler, 'text', side_effect=changed), self.assertRaisesRegex(e.DependencyError, 'Unknown background'):
            compiler.background('lamp')
        data = compiler.yaml('Data/Battlers/lamp.yaml'); data['unreviewed_texture_factory'] = 'dynamic'
        with patch.object(compiler, 'yaml', return_value=data), self.assertRaisesRegex(e.DependencyError, 'Unknown enemy resource fields'):
            compiler.enemy('lamp')

    def test_changed_inventory_source_rejected(self):
        compiler = e.Compiler(ROOT)
        compiler.inventory = copy.deepcopy(compiler.inventory)
        compiler.inventory['files']['Data/Battlers/lamp.yaml']['sha256'] = '0'*64
        with self.assertRaisesRegex(e.DependencyError, 'Changed pinned resource'):
            compiler.read('Data/Battlers/lamp.yaml')

    def test_battle_round_mismatch_rejected(self):
        formation = self.formations[LAMP]
        binding = dict(formation['compiled']['source_binding'])
        binding['round_pack'] = 'data/doll-entry.encround'
        with self.assertRaisesRegex(e.DependencyError, 'pair bytes'):
            e.checked_pair(ROOT, binding, formation, e.Compiler(ROOT))

    def test_stale_compiled_hash_rejected(self):
        doc = copy.deepcopy(self.doc)
        pair = next(f for f in doc['formations'] if f['key'] == LAMP)['compiled']
        pair['files'][pair['battle_pack']] = '0'*64
        with self.assertRaisesRegex(e.DependencyError, 'fingerprints/binding are stale'):
            e.adapt_compiled_dependencies(doc, [LAMP], 1, ROOT)

    def test_stored_manifest_admits_every_compiled_pair(self):
        # Test the shipping manifest as well as a freshly constructed document:
        # a layout change must not leave last generation's pack fingerprints.
        stored = json.loads((ROOT / 'content/encounter-dependencies.json').read_bytes())
        keys = [f['key'] for f in stored['formations'] if f['compiled'] is not None]
        self.assertEqual(set(keys), {LAMP, DOLL, PILLOW})
        admitted = e.adapt_compiled_dependencies(stored, keys, 1, ROOT)
        self.assertEqual({row['key'] for row in admitted['encounters']}, set(keys))

    def test_source_member_order_actor_binding_checked(self):
        formation = copy.deepcopy(self.formations[LAMP])
        formation['ordered_members'][0]['actor'] = 'different_actor'
        with self.assertRaisesRegex(e.DependencyError, 'ordering/actor binding'):
            e.checked_pair(ROOT, formation['compiled']['source_binding'], formation, e.Compiler(ROOT))

    def test_direct_actors_not_mislabeled_as_rng(self):
        scene = self.scenes['Maps/podunk/Catacombs.tscn']
        actors = [c for c in scene['candidates'] if c['source_kind'] == 'direct_actor']
        self.assertEqual(len(actors), 4)
        self.assertTrue(all(c['gate']['rng_calls'] == 0 for c in actors))

    def test_unknown_spawner_field_and_factory_rejected(self):
        text = self.compiler.text(PODUNK)
        text = text.replace('enemy = "crow"', 'enemy = "crow"\nunreviewed_rate = 5', 1)
        with self.assertRaisesRegex(e.DependencyError, 'Unknown spawner override'):
            e.parse_spawners(text, PODUNK)
        text = self.compiler.text(PODUNK).replace('Enemies/Enemy Spawner.tscn', 'Enemies/Unknown Factory.tscn')
        with self.assertRaisesRegex(e.DependencyError, 'Unreviewed direct enemy'):
            e.parse_spawners(text, PODUNK)

    def test_yaml_command_duplicates_fail_closed(self):
        with self.assertRaisesRegex(e.DependencyError, 'Duplicate startbattle'):
            list(e.dialogue_battles('"0":\n  startbattle: {}\n  startbattle: {}\n'))
        with self.assertRaisesRegex(e.DependencyError, 'Duplicate YAML key'):
            list(e.dialogue_battles('"0":\n  startbattle:\n    battlers: []\n    battlers: []\n'))

    def test_non_resource_source_duplicate_is_audited(self):
        lynx = self.compiler.yaml('Data/Battlers/lynx.yaml')
        self.assertEqual(lynx['affinity_multipliers'], {'ice':2,'blinding':1.5})
        self.assertIn('Scripts/global/yaml_parser.gd', self.doc['semantic_sources'])
        self.assertTrue(self.doc['source_data_notes'])

    def test_no_fabricated_compiled_random_pair(self):
        doc = copy.deepcopy(self.doc)
        doc['scenes'][0]['compiled_dependencies'] = [{'key':'fake','battle_pack':'fake.encbattle','round_pack':'fake.encround'}]
        with self.assertRaisesRegex(e.DependencyError, 'cannot fabricate'):
            e.validate_manifest(doc)

if __name__ == '__main__':
    unittest.main()
