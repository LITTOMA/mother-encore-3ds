import copy,hashlib,json,struct,tempfile,unittest,zlib
from pathlib import Path
from unittest.mock import patch
from tools import catacombs_poison as poison,run_poison_hazards_reference as native
from tests import prepare_poison_hazards as fixture

class PoisonHazardsTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.recipe,cls.model,cls.source=poison.source();cls.blob=poison.encode(cls.model);cls.root=poison.ROOT/'build/tests';cls.root.mkdir(parents=True,exist_ok=True)
  cls.native_root=poison.ROOT/'reports/poison-hazards/native-1';cls.doc=json.loads((cls.native_root/'reference.json').read_bytes());cls.proof=json.loads((cls.native_root/'receipt.json').read_bytes())
 def temporary(self):
  temporary=tempfile.TemporaryDirectory(prefix='poison-',dir=self.root);root=Path(temporary.name).resolve();self.assertTrue(root.is_relative_to(poison.ROOT/'build'));self.addCleanup(temporary.cleanup);return root
 def changed(self,at,value):
  blob=bytearray(self.blob);struct.pack_into('<I',blob,at,value);struct.pack_into('<I',blob,16,zlib.crc32(blob[32:]));return blob
 def test_original_source_binding(self):
  self.assertEqual((len(self.model['areas']),len(self.model['holes']),len(self.model['shapes']),len(self.model['points'])),(11,37,22,178));self.assertEqual(self.model['damage'],[4,2,5,2]);self.assertEqual(self.model['status'],'poisoned');self.assertEqual(self.model['spawn'],[0.,-8.]);self.assertEqual(self.proof['source'],self.source)
 def test_binary_roundtrip(self):
  self.assertEqual(poison.parse(self.blob),self.model);self.assertEqual(len(self.blob),6360);self.assertEqual(hashlib.sha256(self.blob).hexdigest(),self.source['sha256'])
 def test_binary_version_pin_and_reserved(self):
  for offset,value in[(8,2),(20,2),(24,0),(24,65536),(28,1),(32,1),(52,7),(56,1),(60,1)]:
   with self.subTest(offset=offset,value=value),self.assertRaises(ValueError):poison.parse(self.changed(offset,value))
 def test_binary_directory_and_truncation(self):
  for blob in[self.blob[:n]for n in[0,7,63,159,160,1000,len(self.blob)-1]]+[self.blob+b'\0']:
   with self.assertRaises(ValueError):poison.parse(blob)
  for offset,value in[(64,2),(68,164),(72,65537),(76,2),(84,0),(92,63),(100,0xffffffff),(108,47),(124,41),(140,31),(156,9)]:
   with self.subTest(offset=offset),self.assertRaises(ValueError):poison.parse(self.changed(offset,value))
 def test_binary_typed_policy_and_timer(self):
  policy,holes,areas,shapes,points=[struct.unpack_from('<I',self.blob,64+i*16+4)[0]for i in range(1,6)]
  changes=[(policy,0xffffffff),(policy+24,0xffffffff),(policy+28,1000001),(policy+40,0x7f800000),(policy+48,0x7fc00000),(policy+56,1),(holes,0),(holes+48,struct.unpack_from('<I',self.blob,holes)[0]),(holes+24,0),(holes+24,0x7ff00000),(holes+28,1),(holes+32,0xbff00000),(holes+36,1),(holes+40,0),(holes+44,1),(areas+12,0),(areas+24,4),(areas+28,1),(areas+32,1),(areas+36,1),(shapes,1),(shapes+4,2),(shapes+8,1),(shapes+12,2),(shapes+16,0x7fc00000),(points,0x7f800000)]
  for offset,value in changes:
   with self.subTest(offset=offset,value=value),self.assertRaises(ValueError):poison.parse(self.changed(offset,value))
 def test_model_bad_types_sources_and_capacity(self):
  changes=[lambda m:m.update(scene=True),lambda m:m.update(status='../poisoned'),lambda m:m.update(damage=[4,True,5,2]),lambda m:m.update(hit=[float('nan'),0]),lambda m:m['holes'][0].update(id=True),lambda m:m['holes'][0].update(delay=1e-300),lambda m:m['holes'][0].update(interval=0),lambda m:m['holes'][0].update(process=0),lambda m:m['holes'][0].update(extra=1),lambda m:m['areas'][0].update(count=1),lambda m:m['areas'][0].update(step=0),lambda m:m['shapes'][0].update(owner=1),lambda m:m['shapes'][0].update(first=1),lambda m:m['points'].append([0.,0.]),lambda m:m['holes'].append(copy.deepcopy(m['holes'][0]))]
  for change in changes:
   m=copy.deepcopy(self.model);change(m)
   with self.assertRaises(ValueError):poison.encode(m)
 def test_degenerate_and_crossing_polygons(self):
  m=copy.deepcopy(self.model);m['points'][1]=m['points'][0]
  with self.assertRaises(ValueError):poison.validate(m)
  m=copy.deepcopy(self.model);m['points'][1],m['points'][3]=m['points'][3],m['points'][1]
  with self.assertRaises(ValueError):poison.validate(m)
 def test_source_review_unknown_and_changed(self):
  root=self.temporary();file=root/'review.json';changes=[lambda p:p.update(schema=True),lambda p:p.update(commit='0'*40),lambda p:p.update(extra=1),lambda p:p.update(steps_recipe_sha256='0'*64),lambda p:p['sources'].update({p['paths']['area_script']:'0'*64}),lambda p:p['areas'].pop(),lambda p:p['holes'].reverse(),lambda p:p['symbols']['hole_script'].update(pause='0'*64),lambda p:p['holes'][0].update(path='../outside')]
  for change in changes:
   p=copy.deepcopy(self.recipe);change(p);file.write_text(json.dumps(p))
   with self.assertRaises(ValueError):poison.source(file)
  file.write_text('{"schema":1,"schema":1}')
  with self.assertRaises(ValueError):poison.read(file)
  file.write_text('{"schema":NaN}')
  with self.assertRaises(ValueError):poison.read(file)
 def test_actual_native_repeatability(self):
  other=json.loads((poison.ROOT/'reports/poison-hazards/native-2/reference.json').read_bytes());self.assertEqual(self.doc,other);self.assertIs(native.validate_result(self.doc,self.proof),self.doc);self.assertEqual(len(self.doc['spawned']),102)
 def test_native_schema_types_and_clock(self):
  changes=[lambda d:d.update(schema=True),lambda d:d.update(extra=1),lambda d:d.update(godot={}),lambda d:d['initial'].pop(),lambda d:d['rows'].pop(),lambda d:d['rows'][0].update(delta_bits=0),lambda d:d['rows'][0].update(before_tree_pause='false'),lambda d:d['rows'][0]['clocks'][0].__setitem__(0,False),lambda d:d['rows'][0].update(timeouts=[37]),lambda d:d['rows'][0].update(timeouts=[0,0])]
  for change in changes:
   d=copy.deepcopy(self.doc);change(d)
   with self.assertRaises(ValueError):native.validate_result(d,self.proof)
 def test_native_spawn_geometry_and_callback_rejection(self):
  changes=[lambda d:d['spawned'].pop(),lambda d:d['spawned'][0].__setitem__(0,0),lambda d:d['geometry'][0][0].__setitem__(0,0),lambda d:d['contacts'][0].update(wanted='unsupported'),lambda d:d['contacts'][0]['player'][3].append('unsupported'),lambda d:d['contacts'][0]['party'].__setitem__(0,1),lambda d:d['contacts'][0].update(extra=1)]
  for change in changes:
   d=copy.deepcopy(self.doc);change(d)
   with self.assertRaises(ValueError):native.validate_result(d,self.proof)
 def test_fixture_idempotence_and_stale_bytes(self):
  root=self.temporary()/'fixture';outputs=fixture.build(root);self.assertEqual(fixture.build(root),outputs);self.assertEqual((root/'catacombs.encpoison').read_bytes(),self.blob);(root/'clocks.tsv').write_bytes(b'changed')
  with self.assertRaises(ValueError):fixture.build(root)
  self.assertEqual((root/'clocks.tsv').read_bytes(),b'changed')
 def test_fixture_review_producer_rejection(self):
  root=self.temporary();file=root/'review.json';review=json.loads(fixture.REVIEW.read_bytes());review['producer_sha256']='0'*64;file.write_text(json.dumps(review))
  with patch.object(fixture,'REVIEW',file),self.assertRaises(ValueError):fixture.build(root/'fixture')
  self.assertFalse((root/'fixture').exists())
 def test_output_and_ownership_guards(self):
  with self.assertRaises(ValueError):poison.build(poison.ROOT/'build')
  with self.assertRaises(ValueError):fixture.build(poison.ROOT/'romfs')
  root=self.temporary();(root/'fixture').mkdir()
  with self.assertRaises(ValueError):fixture.build(root/'fixture')

if __name__=='__main__':unittest.main()
