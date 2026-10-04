import copy
import json
import tempfile
import unittest
from pathlib import Path
from tools import house_assets as h

class HouseAssetsTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.root=h.ROOT/'upstream/MOTHER-Encore'
        cls.recipe=h.read_json(h.RECIPE)
        cls.lock=h.read_json(h.ROOT/'upstream.lock')
    def test_source_and_outputs_verified(self):
        h.validate_source(self.root,self.recipe,self.lock)
        h.verify(self.root)
    def test_source_changes_fail_closed(self):
        recipe=copy.deepcopy(self.recipe)
        path=next(iter(recipe['sources']));recipe['sources'][path]='0'*64
        with self.assertRaisesRegex(ValueError,'Changed house source'):
            h.validate_source(self.root,recipe,self.lock)
    def test_unknown_schema_and_resource_fields_fail(self):
        for mutate in [lambda r:r.update(schema=2),lambda r:r.update(unknown=1),lambda r:r['resources'][0].update(unknown=1),lambda r:r['resources'][0].update(id=0)]:
            recipe=copy.deepcopy(self.recipe);mutate(recipe)
            with self.assertRaises(ValueError):h.validate_source(self.root,recipe,self.lock)
    def test_invalid_geometry_and_duplicate_role_fail(self):
        for mutate in [lambda r:r['resources'][0].update(grid=[4,4]),lambda r:r['resources'][0].update(size=[0,10]),lambda r:r['resources'][0].update(output='../escaped.t3x'),lambda r:r['resources'][0].update(role='shadow')]:
            recipe=copy.deepcopy(self.recipe);mutate(recipe)
            with self.assertRaises(ValueError):h.validate_source(self.root,recipe,self.lock)
    def test_source_tracks_and_font_identity(self):
        resources=h.read_json(h.receipt_path(h.OUT,h.ROOT))['resources']
        with tempfile.TemporaryDirectory() as td:
            dest=Path(td)/'presentation.json';h.export_presentation(self.root,resources,dest)
            data=h.read_json(dest)
        self.assertEqual(data,h.read_json(h.IR))
        self.assertEqual(data['schema'],2)
        self.assertEqual(len([c for c in data['clips']if c['profile']!='minnie']),22)
        by_role={c['role']:c for c in data['clips'] if c['profile'] in (None,'carol')}
        self.assertAlmostEqual(by_role['NpcTalkDown']['keys'][0]['time'],.1)
        self.assertEqual(by_role['NpcTalkDown']['keys'][0]['frame'],4)
        self.assertAlmostEqual(by_role['NpcTalkDown']['duration'],.4)
        self.assertEqual(by_role['DialogueOpen']['keys'][0]['position'],[28,180])
        self.assertEqual(by_role['DialogueOpen']['keys'][-1]['position'],[28,120])
        self.assertEqual(by_role['DialogueOpen']['interpolation'],2)
        profiles={p['role']:p for p in data['profiles']}
        self.assertEqual(profiles['carol']['sprite_offset'],[0,-7])
        self.assertEqual(profiles['mimmie']['sprite_offset'],[0,-3])
        self.assertEqual(profiles['doll']['sprite_offset'],[0,-2])
        self.assertEqual([profiles[n]['directions'] for n in ['carol','mimmie','doll']],[4,4,1])
        self.assertEqual([profiles[n]['flags'] for n in ['carol','mimmie','doll']],[7,7,1])
        self.assertNotIn('NpcSpriteOffset',data['parameters'])
        self.assertNotIn('NpcShadowOffset',data['parameters'])
        doll=[c for c in data['clips'] if c['profile']=='doll']
        self.assertEqual(len(doll),1)
        self.assertEqual((doll[0]['role'],doll[0]['resource'],doll[0]['keys'][0]['frame']),('NpcIdleDown','doll',0))
        self.assertEqual(doll[0]['duration'],.25)
        self.assertTrue(doll[0]['loop'])
        for name in ['carol','mimmie']:
            self.assertEqual(len([c for c in data['clips'] if c['profile']==name]),8)
        self.assertEqual({r['role'] for r in data['resources']},{'carol','mimmie','doll','minnie','shadow','dialogue_box','name_box','cursor','door','door_basement'})
        self.assertEqual(data['parameters']['FontMetrics'],[12,3,11,1])
    def test_missing_or_changed_texture_rejected(self):
        with tempfile.TemporaryDirectory() as td:
            target=Path(td);receipt=h.read_json(h.receipt_path(h.OUT,h.ROOT));h.write_json(target/'source.json',receipt)
            with self.assertRaises(ValueError):h.verify(self.root,target)
            for name in receipt['outputs']:(target/name).write_bytes(b'invalid')
            with self.assertRaisesRegex(ValueError,'Changed house output'):h.verify(self.root,target)

if __name__=='__main__':unittest.main()
