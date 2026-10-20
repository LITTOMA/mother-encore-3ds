import copy,hashlib,json,tempfile,unittest,zlib
from pathlib import Path
from tools import field_swing_presentation as p,field_swing_graphics as g

class SwingPresentation(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.proof=json.loads((p.ROOT/'reports/iggy-tutorial/swing-graphics-metadata-2/source.json').read_bytes());cls.model=cls.proof['model'];cls.blob=(p.ROOT/'tests/fixtures/field-swing-graphics-v0410.encgraphics').read_bytes();cls.recipe=json.loads(p.RECIPE.read_bytes())
 def test_source_model_and_actual_conversion_receipt(self):
  _,source=g.build();atlas=json.loads((p.ROOT/'reports/iggy-tutorial/swing-graphics-atlas-2/source.json').read_bytes());m=self.model
  self.assertEqual(m['meta'],dict(initial_frame=4,frame_width=64,frame_height=64,source_columns=10,source_rows=10,atlas_columns=5))
  self.assertEqual(self.proof['sources'],source['sources']);self.assertEqual(self.proof['atlas_receipt_sha256'],hashlib.sha256((p.ROOT/'reports/iggy-tutorial/swing-graphics-atlas-2/source.json').read_bytes()).hexdigest())
  for row,original,converted in zip(m['layers'],source['layers'],atlas['layers']):
   self.assertEqual(row['role'],original['role']);self.assertEqual(row['source'],original['source']);self.assertEqual(row['source_sha256'],original['source_sha256']);self.assertEqual(row['position'],original['position']);self.assertEqual(row['offset'],original['offset']);self.assertEqual(row['flags'],int(original['centered'])|int(original['flip_h'])<<1|int(original['flip_v'])<<2|int(original['initial_visible'])<<3)
   self.assertEqual(row['path'],self.recipe['texture_directory']+'/'+original['output']);self.assertEqual(row['sha256'],converted['t3x_sha256']);self.assertEqual(row['bytes'],converted['t3x_bytes']);self.assertEqual(row['parent'],row['role']-1)
   self.assertEqual(m['frames'],[{k:v for k,v in f.items()if k!='rgba_sha256'}for f in original['frames']])
  self.assertEqual(p.encode(m),self.blob);self.assertEqual(len(self.blob),self.proof['bytes']);self.assertEqual(hashlib.sha256(self.blob).hexdigest(),self.proof['sha256']);self.assertEqual(p.encode(copy.deepcopy(m)),self.blob)
  for name,sha in self.proof['dependencies'].items():
   self.assertNotIn('\\',name);self.assertEqual(hashlib.sha256((p.ROOT/name).read_bytes()).hexdigest(),sha)
  self.assertIn('No release staging',self.proof['scope'])
 def test_recipe_schema_versions_paths_and_duplicate_fields_reject(self):
  changes=[lambda r:r.update(schema=True),lambda r:r.update(schema=2),lambda r:r.update(kind='unknown'),lambda r:r.update(commit='0'*40),lambda r:r.update(extra=0),lambda r:r.update(atlas_recipe='unknown'),lambda r:r.update(texture_directory='../graphics'),lambda r:r.update(texture_directory='graphics//bad'),lambda r:r.update(texture_directory='graphics/../bad'),lambda r:r.update(texture_directory='graphics\\bad'),lambda r:r.update(output='world/bad.bin'),lambda r:r.update(output='world/../bad.encgraphics'),lambda r:r.update(output='/world/a.encgraphics')]
  with tempfile.TemporaryDirectory()as folder:
   path=Path(folder)/'bad.json'
   for change in changes:
    recipe=copy.deepcopy(self.recipe);change(recipe);path.write_text(json.dumps(recipe),encoding='utf8')
    with self.subTest(change=change),self.assertRaises(ValueError):p.read(path)
   path.write_text('{"schema":1,"schema":1}',encoding='utf8')
   with self.assertRaises(ValueError):p.read(path)
 def test_binary_model_rejects_unknown_and_malformed_content(self):
  edits=[lambda r:r.update(extra=0),lambda r:r['meta'].update(extra=0),lambda r:r['meta'].update(initial_frame=True),lambda r:r['meta'].update(initial_frame=99),lambda r:r['meta'].update(frame_width=0),lambda r:r['meta'].update(frame_height=1025),lambda r:r['meta'].update(source_columns=257),lambda r:r['meta'].update(source_columns=256,source_rows=256),lambda r:r['meta'].update(atlas_columns=0),lambda r:r['meta'].update(atlas_columns=17),lambda r:r.update(layers=[]),lambda r:r.update(frames=[]),lambda r:r['frames'][0].update(extra=0),lambda r:r['frames'][0].update(source_frame=True),lambda r:r['frames'][1].update(source_frame=0),lambda r:r['frames'][0].update(x=1),lambda r:r['frames'][0].update(width=1),lambda r:r['frames'][0].update(y=-1),lambda r:r['layers'][0].update(extra=0),lambda r:r['layers'][0].update(role=True),lambda r:r['layers'][1].update(role=1),lambda r:r['layers'][1].update(parent=0),lambda r:r['layers'][0].update(flags=16),lambda r:r['layers'][0].update(width=1025),lambda r:r['layers'][0].update(height=1),lambda r:r['layers'][0].update(bytes=0),lambda r:r['layers'][0].update(bytes=8*1024*1024+1),lambda r:r['layers'][0].update(crc32=-1),lambda r:r['layers'][0].update(crc32=True),lambda r:r['layers'][0].update(sha256='0'*64),lambda r:r['layers'][0].update(source_sha256='X'*64),lambda r:r['layers'][0].update(position=[float('nan'),0]),lambda r:r['layers'][0].update(offset=[0,4097]),lambda r:r['layers'][0].update(position=[True,0]),lambda r:r['layers'][0].update(offset=[0]),lambda r:r['layers'][0].update(path='graphics/../a.t3x'),lambda r:r['layers'][0].update(path='graphics/a\\b.t3x'),lambda r:r['layers'][0].update(source='Graphics//a.png'),lambda r:r['layers'][1].update(path=r['layers'][0]['path']),lambda r:r['layers'][1].update(source=r['layers'][0]['source'])]
  for change in edits:
   model=copy.deepcopy(self.model);change(model)
   with self.subTest(change=change),self.assertRaises(ValueError):p.encode(model)
 def test_assets_require_owned_outputs_and_matching_conversion_receipt(self):
  with tempfile.TemporaryDirectory()as folder:
   with self.assertRaises(ValueError):p.build(Path(folder))
  with tempfile.TemporaryDirectory(dir=p.ROOT/'build')as folder:
   root=Path(folder);receipt=json.loads((p.ROOT/'reports/iggy-tutorial/swing-graphics-atlas-2/source.json').read_bytes());receipt['layers'][0]['source_sha256']='0'*64;(root/'source.json').write_text(json.dumps(receipt),encoding='utf8')
   with self.assertRaisesRegex(ValueError,'receipt differs'):p.build(root)

if __name__=='__main__':unittest.main()
