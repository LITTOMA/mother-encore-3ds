import copy,hashlib,io,json,tempfile,unittest
from pathlib import Path
from PIL import Image
from tools import field_swing_graphics as g
class SwingGraphics(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.images,cls.proof=g.build();cls.recipe=json.loads(g.RECIPE.read_bytes())
 def test_every_packed_frame_has_exact_original_pixels(self):
  self.assertEqual(self.proof['frame_count'],40)
  for row in self.proof['layers']:
   image=self.images[row['role']];self.assertEqual(image.size,(320,512));self.assertEqual(hashlib.sha256(image.tobytes()).hexdigest(),row['rgba_sha256'])
   with Image.open(g.ROOT/'upstream/MOTHER-Encore'/row['source'])as source:
    source=source.convert('RGBA');self.assertEqual(hashlib.sha256((g.ROOT/'upstream/MOTHER-Encore'/row['source']).read_bytes()).hexdigest(),row['source_sha256'])
    for frame in row['frames']:
     x=frame['source_frame']%10*64;y=frame['source_frame']//10*64;expected=source.crop((x,y,x+64,y+64)).tobytes();packed=image.crop((frame['x'],frame['y'],frame['x']+frame['width'],frame['y']+frame['height'])).tobytes();self.assertEqual(packed,expected);self.assertEqual(hashlib.sha256(expected).hexdigest(),frame['rgba_sha256'])
 def test_source_sprite_origins_and_order_are_data(self):
  base,bat=self.proof['layers'];self.assertEqual((base['role'],bat['role']),(1,2));self.assertEqual(base['position'],[0,-4]);self.assertEqual(bat['position'],[0,0]);self.assertEqual((base['initial_visible'],bat['initial_visible']),(False,True));self.assertTrue(base['centered']and bat['centered']);self.assertEqual([f['source_frame']for f in base['frames']],[f['source_frame']for f in bat['frames']]);self.assertEqual(len({(f['x'],f['y'])for f in base['frames']}),40)
 def test_permission_and_candidate_boundaries(self):
  self.assertIn('candidate',self.proof['scope']);self.assertIn('no rendering',self.proof['scope']);self.assertIn('LICENSE',self.proof['sources']);self.assertIn('game-related port',self.proof['permission']);self.assertEqual(self.proof['dependencies']['content/field-swing.json'],hashlib.sha256((g.ROOT/'content/field-swing.json').read_bytes()).hexdigest())
 def test_unknown_recipe_and_wrong_source_bindings_reject(self):
  changes=[lambda p:p.update(schema=True),lambda p:p.update(schema=2),lambda p:p.update(extra=0),lambda p:p.update(source_recipe='../escape'),lambda p:p.update(atlas_columns=True),lambda p:p.update(atlas_columns=0),lambda p:p.update(atlas_columns=17),lambda p:p.update(scope=''),lambda p:p['sources'].update(LICENSE='0'*64),lambda p:p['layers'].pop(),lambda p:p['layers'][0].update(role=True),lambda p:p['layers'][0].update(role=3),lambda p:p['layers'][1].update(role=1),lambda p:p['layers'][0].update(output='../escape.t3x'),lambda p:p['layers'][0].update(output='INVALID.t3x'),lambda p:p['layers'][1].update(output='ninten.t3x'),lambda p:p['layers'][1].update(node='SpecialAnimations'),lambda p:p['layers'][0].update(node='Missing'),lambda p:p['layers'][0].update(source_key='unknown'),lambda p:p['layers'][0].update(source_key='bat_texture')]
  with tempfile.TemporaryDirectory()as folder:
   path=Path(folder)/'bad.json'
   for edit in changes:
    p=copy.deepcopy(self.recipe);edit(p);path.write_text(json.dumps(p),encoding='utf8')
    with self.subTest(edit=edit),self.assertRaises(ValueError):g.read(path)
   path.write_text('{"schema":1,"schema":1}',encoding='utf8')
   with self.assertRaises(ValueError):g.read(path)
 def test_repeat_conversion_is_byte_identical(self):
  images,proof=g.build();self.assertEqual(proof,self.proof)
  for role,image in images.items():self.assertEqual(image.tobytes(),self.images[role].tobytes())
 def test_actual_texture_conversion_receipt_matches_current_pixels(self):
  folder=g.ROOT/'reports/iggy-tutorial/swing-graphics-atlas-2';proof=json.loads((folder/'source.json').read_bytes());comparison=json.loads((g.ROOT/'reports/iggy-tutorial/swing-graphics-atlas/comparison.json').read_bytes())
  for row,current in zip(proof['layers'],self.proof['layers']):
   self.assertEqual({k:v for k,v in row.items()if k not in('t3x_bytes','t3x_sha256')},current);self.assertEqual(row['t3x_bytes'],1048597)
  self.assertTrue(all('\\'not in name for name in proof['dependencies']));self.assertEqual(proof['dependencies'],self.proof['dependencies']);self.assertEqual(comparison['full_bytes'],8388650);self.assertEqual(comparison['packed_bytes'],2097194);self.assertEqual(comparison['removed_full_baseline_bytes'],comparison['full_bytes']);self.assertIn('not a measured GPU',comparison['scope'])
if __name__=='__main__':unittest.main()
