#!/usr/bin/env python3
import copy
import json
from pathlib import Path
import sys
import tempfile
import unittest
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from tools.dialogue_choice_assets import RECIPE, PACK, encode, verify_recipe, stage_files


class ChoicesAssets(unittest.TestCase):
    @classmethod
    def setUpClass(cls): cls.recipe = json.loads(RECIPE.read_text())

    def test_source_and_pack_reproduce(self):
        verify_recipe(self.recipe)
        self.assertEqual(PACK.read_bytes(), encode(self.recipe))

    def test_native_layout_and_source_semantics(self):
        r = self.recipe
        self.assertEqual(r['option_rects'], [[0, 0, 64, 12], [76, 0, 64, 12]])
        self.assertEqual(r['grid'], [52, 38, 203, 12])
        self.assertEqual(r['option_min_size'], [60, 10])
        self.assertEqual(r['sounds'], ['cursor1', 'cursor2', 'cursor2'])
        self.assertEqual([x['text'] for x in r['groups'][0]['options']], ['Record', 'Nothing, really'])
        self.assertEqual(r['groups'][0]['cancel_target_pc'], r['groups'][0]['options'][1]['target_pc'])
        self.assertEqual([x['frame'] for x in r['arrow_keys']], [0, 1, 2, 1])

    def test_recipe_drift_rejected(self):
        for field, value in [('columns', 2), ('sounds', ['cursor1', 'cursor2', 'back']),
                             ('arrow_move', .2), ('unexpected', True)]:
            changed = copy.deepcopy(self.recipe); changed[field] = value
            with self.assertRaises(ValueError): verify_recipe(changed)
        changed = copy.deepcopy(self.recipe); changed['groups'][0]['options'][0]['target_pc'] += 1
        with self.assertRaises(ValueError): verify_recipe(changed)

    def test_shared_resources_and_safe_staging(self):
        files = stage_files(ROOT / 'romfs')
        self.assertEqual(set(map(str, files)), {'data/opening.encchoices', 'house-preview/cursor.t3x', 'battle-preview/font.t3x'})
        with tempfile.TemporaryDirectory() as td:
            p = Path(td); (p / 'data').mkdir(); (p / 'data/opening.encchoices').symlink_to(PACK)
            with self.assertRaises(ValueError): stage_files(p)


if __name__ == '__main__': unittest.main()
