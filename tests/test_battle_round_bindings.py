import copy, hashlib, json, sys, tempfile, unittest
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import battle_round_bindings as b
import extract_battle_round
import doll_round
import native_round

class BattleRoundBindings(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.recipe = b.load()

    def test_actual_lowering_retains_all_default_bytes(self):
        extracted = extract_battle_round.build()
        stored = json.loads((ROOT / 'content/native-round.json').read_text(encoding='utf-8'))
        self.assertEqual(extracted, stored)
        self.assertEqual(native_round.encode(native_round.lower(extracted)), (ROOT / 'romfs/data/opening.encround').read_bytes())
        doll = doll_round.build()
        self.assertEqual(doll, json.loads((ROOT / 'content/doll-round.json').read_text(encoding='utf-8')))
        self.assertEqual(native_round.encode(native_round.lower(doll)), (ROOT / 'romfs/data/doll-entry.encround').read_bytes())

    def test_schema_unknown_missing_and_duplicate_fields(self):
        for schema in [None, True, 0, 2, 1.0, '1']:
            value = copy.deepcopy(self.recipe); value['schema'] = schema
            with self.subTest(schema=schema), self.assertRaises(ValueError): b.load(recipe=value)
        for route in [(), ('skills', 0), ('constants', 'basic'), ('menus',), ('boss_shake',)]:
            for missing in (False, True):
                value = copy.deepcopy(self.recipe); target = value
                for key in route: target = target[key]
                if missing: target.pop(next(iter(target)))
                else: target['unknown'] = 1
                with self.subTest(route=route, missing=missing), self.assertRaises(ValueError): b.load(recipe=value)
        with self.assertRaises(ValueError): json.loads('{"schema":1,"schema":1}', object_pairs_hook=b.unique_pairs)

    def test_skill_reference_actor_order_and_identity_fail(self):
        for field, value in [('id', True), ('id', 2), ('name', 'unknown'), ('source', '../attack.yaml'), ('actor', 'all')]:
            recipe = copy.deepcopy(self.recipe); recipe['skills'][0][field] = value
            with self.subTest(field=field), self.assertRaises(ValueError): b.load(recipe=recipe)
        for change in ['duplicate', 'missing', 'constant', 'actor', 'menu']:
            recipe = copy.deepcopy(self.recipe)
            if change == 'duplicate': recipe['skills'][1] = copy.deepcopy(recipe['skills'][0])
            elif change == 'missing': recipe['skills'].pop()
            elif change == 'constant': recipe['constants']['basic']['skill'] = 'float'
            elif change == 'actor': recipe['skills'][1]['actor'] = 'party'
            else: recipe['menus']['guard']['identity'] = 'Run'
            with self.subTest(change=change), self.assertRaises(ValueError): b.load(recipe=recipe)
        recipe = copy.deepcopy(self.recipe)
        recipe['menus']['basic'], recipe['menus']['items'] = recipe['menus']['items'], recipe['menus']['basic']
        with self.assertRaises(ValueError): b.load(recipe=recipe)

    def test_source_hash_path_and_missing_facts_fail(self):
        for change in ['hash', 'path', 'empty', 'bad', 'fact']:
            recipe = copy.deepcopy(self.recipe)
            if change == 'hash': recipe['sources'][next(iter(recipe['sources']))] = '0' * 64
            elif change == 'path': recipe['sources']['../escaped.gd'] = '0' * 64
            elif change == 'empty': recipe['source_facts'] = {}
            elif change == 'bad': recipe['boss_shake']['facts'] = [{}, {}]
            else: recipe['boss_shake']['facts'][0] += ' changed'
            with self.subTest(change=change), self.assertRaises(ValueError): b.load(recipe=recipe)

    def test_shake_bounds_and_unreviewed_tuning_fail(self):
        for value in [True, None, 0, -1, 1.1, float('nan'), float('inf'), .6]:
            recipe = copy.deepcopy(self.recipe); recipe['boss_shake']['value'] = value
            with self.subTest(value=value), self.assertRaises(ValueError): b.load(recipe=recipe)

    def test_semantic_source_changes_fail_even_after_fixture_rehash(self):
        # Private synthetic source copies exercise the semantic gate after the
        # fingerprint gate; the readonly real checkout and pin are not changed.
        changes = [('Scripts/global/globalData.gd', 'SKILL_ATTACK := "attack"', 'SKILL_ATTACK := "float"'),
                   ('Scripts/misc/Shaker.gd', 'weight := 0.5', 'weight := 0.6'),
                   ('Scripts/UI/Battle/BattleSystem.gd', 'globaldata.get_battle_skill(globaldata.SKILL_GUARD)', 'globaldata.get_battle_skill(globaldata.SKILL_ATTACK)')]
        for path, before, after in changes:
            with self.subTest(path=path), tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp); recipe = copy.deepcopy(self.recipe)
                inventory = json.loads((ROOT / 'compatibility/upstream-inventory.json').read_text())
                for source_path in recipe['sources']:
                    data = (ROOT / 'upstream/MOTHER-Encore' / source_path).read_bytes()
                    if source_path == path:
                        self.assertIn(before.encode(), data); data = data.replace(before.encode(), after.encode())
                        sha = hashlib.sha256(data).hexdigest(); recipe['sources'][path] = sha; inventory['files'][path]['sha256'] = sha
                    output = root / 'upstream/MOTHER-Encore' / source_path
                    output.parent.mkdir(parents=True, exist_ok=True); output.write_bytes(data)
                (root / 'compatibility').mkdir(); (root / 'content').mkdir()
                (root / 'compatibility/upstream-inventory.json').write_text(json.dumps(inventory))
                (root / 'content/native-battle.json').write_bytes((ROOT / 'content/native-battle.json').read_bytes())
                with self.assertRaises(ValueError): b.load(root, recipe)

    def test_binary_ir_binding_mismatch_fails(self):
        stored = json.loads((ROOT / 'content/doll-round.json').read_text(encoding='utf-8'))
        for change in ['order', 'basic', 'weight']:
            ir = copy.deepcopy(stored)
            if change == 'order': ir['skills'][0], ir['skills'][1] = ir['skills'][1], ir['skills'][0]
            elif change == 'basic': ir['binding']['basic_skill'] = 99
            else: ir['boss_shakes'][0]['weight'] = .6
            with self.subTest(change=change), self.assertRaises(ValueError): b.check_round(ir, self.recipe)

if __name__ == '__main__': unittest.main()
