import copy,hashlib,json,struct,tempfile,unittest,zlib
from pathlib import Path
from unittest.mock import patch
from tools import catacombs_geyser_graphics as graphics,run_geyser_graphics_reference as native
from tests import prepare_geyser_graphics as fixture

class GeyserGraphicsTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.temporary=tempfile.TemporaryDirectory(prefix='geyser-graphics-python-',dir=graphics.ROOT/'build');cls.root=Path(cls.temporary.name);cls.out=cls.root/'fixture';cls.outputs=fixture.build(cls.out);cls.policy,cls.model,cls.image,cls.source=graphics.source();cls.blob=cls.outputs[cls.policy['output']];cls.model=graphics.parse(cls.blob);folder=graphics.ROOT/'reports/geyser-graphics/native-1';cls.doc=json.loads((folder/'reference.json').read_bytes());cls.receipt=json.loads((folder/'receipt.json').read_bytes());cls.native_source=native.source()
 @classmethod
 def tearDownClass(cls):cls.image.close();cls.temporary.cleanup()
 def changed(self,at,value):
  b=bytearray(self.blob);struct.pack_into('<I',b,at,value);struct.pack_into('<I',b,16,zlib.crc32(b[32:]));return b
 def test_original_untrimmed_frame_pixels_and_independent_timeline(self):
  self.assertEqual(self.image.size,(640,50));self.assertEqual(len(self.model['frames']),32);self.assertEqual(self.source['recipe_sha256'],hashlib.sha256(graphics.RECIPE.read_bytes()).hexdigest());self.assertEqual(graphics.encode(self.model),self.blob)
  for row,digest in zip(self.model['frames'],self.source['source_frame_rgba_sha256']):
   self.assertEqual((row['x'],row['y'],row['width'],row['height']),(row['index']*20,0,20,50));self.assertEqual(hashlib.sha256(self.image.crop((row['x'],0,row['x']+20,50)).tobytes()).hexdigest(),digest)
 def test_every_truncation_checksum_and_trailing_data_rejects(self):
  for n in range(len(self.blob)):
   with self.assertRaises(ValueError):graphics.parse(self.blob[:n])
  for b in [self.blob+b'\0',self.blob[:200]+bytes([self.blob[200]^1])+self.blob[201:]]:
   with self.assertRaises(ValueError):graphics.parse(b)
 def test_versions_capabilities_source_pin_and_directory_reject(self):
  changes=[(0,0),(8,2),(12,0),(20,2),(24,1),(28,1),(32,0),(52,3),(56,1),(60,1)]+[(64+16*n+k,value)for n in range(4)for k,value in [(0,99),(4,0xffffffff),(8,0xffffffff),(12,0)]]
  for at,value in changes:
   with self.subTest(at=at),self.assertRaises(ValueError):graphics.parse(self.changed(at,value))
 def test_policy_nested_version_and_frame_operands(self):
  policy,source,frames=[struct.unpack_from('<I',self.blob,at)[0]for at in [84,100,116]]
  changes=[(policy,1),(policy+4,0),(policy+8,0),(policy+12,1025),(policy+16,0),(policy+24,8),(policy+28,1),(source+8,2)]+[(frames+at,value)for at,value in [(0,1),(4,640),(8,50),(12,0),(16,0),(20,1),(24,1),(28,1)]]
  for at,value in changes:
   with self.subTest(at=at),self.assertRaises(ValueError):graphics.parse(self.changed(at,value))
 def test_model_unknown_fields_paths_types_and_grid(self):
  for change in [lambda m:m.update(width=True),lambda m:m.update(flags=True),lambda m:m.update(flags=8),lambda m:m.update(texture='../escape.t3x'),lambda m:m.update(texture='graphics/../escape.t3x'),lambda m:m.update(texture_bytes=0),lambda m:m.update(timeline=b'unknown'),lambda m:m.update(frames=[]),lambda m:m['frames'][0].update(index=True),lambda m:m['frames'][0].update(width=21),lambda m:m['frames'][0].update(x=640),lambda m:m['frames'][0].update(extra=0),lambda m:m.update(ignore_unknown=True)]:
   m=copy.deepcopy(self.model);change(m)
   with self.assertRaises(ValueError):graphics.encode(m)
 def test_recipe_identity_duplicate_fields_and_dependency_change(self):
  path=self.root/'recipe.json'
  for key,value in [('schema',True),('schema',2),('kind','unknown'),('commit','0'*40),('timeline_recipe','../outside'),('timeline_sha256','0'*64),('output','/absolute.encgeysergfx'),('texture','graphics/x.pcm'),('scope',''),('license_review',False),('ignore',0)]:
   p=copy.deepcopy(self.policy);p[key]=value;path.write_text(json.dumps(p))
   with self.subTest(key=key),self.assertRaises(ValueError):graphics.read(path)
  path.write_text('{"schema":1,"schema":1}')
  with self.assertRaises(ValueError):graphics.read(path)
 def test_audited_sprite_source_and_license_are_required(self):
  original=Path.read_bytes;engine=graphics.ROOT/'reports/geyser-timeline/official-engine-source'
  for name in ['scene/2d/sprite.cpp','LICENSE.txt']:
   target=(engine/name).resolve()
   def changed(path,target=target):return b'changed engine input'if path.resolve()==target else original(path)
   with patch.object(Path,'read_bytes',changed),self.assertRaises(ValueError):graphics.source()
 def test_two_actual_original_geometry_runs_and_signed_zero(self):
  self.assertEqual(self.doc,json.loads((graphics.ROOT/'reports/geyser-graphics/native-2/reference.json').read_bytes()));self.assertEqual(len(self.doc['frames']),32);self.assertEqual(self.doc['positions'][0]['basis'][2],0x80000000)
  d=copy.deepcopy(self.doc);d['positions'][0]['basis'][2]=0
  with patch.object(native,'source',return_value=self.native_source):native.validate_result(d,self.receipt)
 def test_native_protocol_unknown_input_and_nonzero_basis_reject(self):
  changes=[lambda d:d.update(schema=True),lambda d:d['frames'].pop(),lambda d:d['frames'][0].update(frame=True),lambda d:d['frames'][0]['rect'].__setitem__(0,False),lambda d:d['defaults'].update(z=False),lambda d:d['defaults'].update(pixel_snap=False),lambda d:d['positions'].pop(),lambda d:d['positions'][0]['basis'].__setitem__(2,1),lambda d:d['positions'][0]['basis'].__setitem__(2,0x7fc00000),lambda d:d['positions'][0]['top_left'].__setitem__(0,0)]
  for change in changes:
   d=copy.deepcopy(self.doc);change(d)
   with patch.object(native,'source',return_value=self.native_source),self.assertRaises(ValueError):native.validate_result(d,self.receipt)
 def test_fixture_reproduction_ownership_and_corruption(self):
  self.assertEqual(fixture.build(self.out),self.outputs)
  with self.assertRaises(ValueError):fixture.build(graphics.ROOT/'build')
  for name in ['native.tsv',self.policy['texture']]:
   target=self.out/name;original=target.read_bytes();target.write_bytes(original+b'\0')
   try:
    with self.assertRaises(ValueError):fixture.build(self.out)
   finally:target.write_bytes(original)
 def test_review_version_producer_and_duplicates_reject(self):
  path=self.root/'review.json';review=json.loads(fixture.REVIEW.read_bytes())
  for key,value in [('schema',True),('producer_sha256','0'*64),('native_producer_sha256','0'*64)]:
   p=copy.deepcopy(review);p[key]=value;path.write_text(json.dumps(p))
   with patch.object(fixture,'REVIEW',path),self.assertRaises(ValueError):fixture.build(self.out)
  path.write_text('{"schema":1,"schema":1}')
  with patch.object(fixture,'REVIEW',path),self.assertRaises(ValueError):fixture.build(self.out)
if __name__=='__main__':unittest.main()
