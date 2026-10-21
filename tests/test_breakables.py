import copy,hashlib,json,struct,tempfile,unittest,zlib
from pathlib import Path
from unittest.mock import patch
from tools import catacombs_breakables as b,run_breakable_reference as native
from tests import prepare_breakables as fixture

class BreakableTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.recipe,cls.model,cls.proof=b.source();cls.blob=b.encode(cls.model);cls.files,cls.copied,cls.native_proof=native.source();cls.root=b.ROOT/'build/tests';cls.root.mkdir(parents=True,exist_ok=True)
 def temporary(self):
  t=tempfile.TemporaryDirectory(prefix='breakable-',dir=self.root);self.addCleanup(t.cleanup);return Path(t.name)
 def changed(self,at,value):
  blob=bytearray(self.blob);struct.pack_into('<I',blob,at,value);struct.pack_into('<I',blob,16,zlib.crc32(blob[32:]));return blob
 def test_source_coverage_and_unchanged_nested_format(self):
  parsed=b.parse(self.blob);self.assertEqual((len(parsed['families']),len(parsed['owners']),len(parsed['contents'])),(2,65,16));self.assertEqual(len(self.proof['sources']),50);self.assertEqual(sum(v['coins']for v in self.model['owners']if v['spawner']),147);self.assertEqual(len({v['id']for v in self.model['owners']}),65)
  for family in self.model['families']:self.assertEqual(family['clip_data'][:8],b'ENCPCL01');self.assertEqual(family['grid'][0],8)
  self.assertEqual(self.model['sound']['stable_id'],3176391583)
 def test_all_truncations_corruption_and_trailing(self):
  for n in range(len(self.blob)):
   with self.assertRaises(ValueError):b.parse(self.blob[:n])
  changed=bytearray(self.blob);changed[-1]^=1
  for value in [None,changed,self.blob+b'\0']:
   with self.assertRaises(ValueError):b.parse(value)
 def test_header_directory_versions_and_reserved(self):
  changes=[(at,0xffffffff)for at in [0,8,12,20,24,28,32,52,56,60]]+[(64+16*n+4*field,0xffffffff)for n in range(7)for field in range(4)]
  for at,value in changes:
   with self.subTest(at=at),self.assertRaises(ValueError):b.parse(self.changed(at,value))
 def test_all_record_domains_and_stable_ownership(self):
  policy,family,owner,content,audio,nested,pool=[struct.unpack_from('<I',self.blob,68+16*n)[0]for n in range(7)];changes=[policy+n for n in [4,72,76,80,84,88,92,96,100]]+[family+n for n in range(0,80,4)]+[family+n for n in [80,96,108,112,124,128,132]]+[owner+n for n in range(0,88,4)]+[content,content+20,audio,audio+4,audio+8,audio+12,audio+16,nested,pool]
  for at in changes:
   # UINT32_MAX is the canonical null content reference; it is tested via a
   # non-null out-of-range index instead of incorrectly rejecting null.
   value=0xfffffffe if at==owner+24 else 0xffffffff
   with self.subTest(at=at),self.assertRaises(ValueError):b.parse(self.changed(at,value))
  for at,value in [(owner+88,struct.unpack_from('<I',self.blob,owner)[0]),(family+136,struct.unpack_from('<I',self.blob,family)[0]),(owner+96,struct.unpack_from('<I',self.blob,owner+8)[0])]:
   with self.assertRaises(ValueError):b.parse(self.changed(at,value))
 def test_model_unknown_fields_and_types_reject(self):
  mutations=[lambda m:m.update(extra=1),lambda m:m.update(scene=True),lambda m:m['policy'].update(lift_curve='linear'),lambda m:m['policy'].update(maximum_delay=float('nan')),lambda m:m['owners'][0].update(id=True),lambda m:m['owners'][0].update(coins=True),lambda m:m['owners'][0].update(pause_on_break=1),lambda m:m['owners'][0].update(flag_kind='other'),lambda m:m['families'][0].update(clip_data=b''),lambda m:m['families'][0].update(audio_spatial=1),lambda m:m['families'][0]['audio'][0].update(value=True),lambda m:m['families'][0]['clips'][0].update(extra=1),lambda m:m['families'][0]['clips'][0]['keys'][0].update(value=2),lambda m:m['families'][0]['shapes'][0].update(extents=[-1,2]),lambda m:m['contents'][0].update(path='../escape'),lambda m:m['sound'].update(stable_id=True)]
  for mutate in mutations:
   m=copy.deepcopy(self.model);mutate(m)
   with self.subTest(mutate=mutate),self.assertRaises(ValueError):b.encode(m)
 def test_changed_recipe_pin_source_symbols_and_bindings(self):
  path=self.temporary()/'recipe.json';mutations=[lambda p:p.update(schema=True),lambda p:p.update(commit='0'*40),lambda p:p.update(extra=1),lambda p:p.update(audio_recipe_sha256='0'*64),lambda p:p['sources'].update({p['paths']['script']:'0'*64}),lambda p:p['symbols']['script'].update(_drop='0'*64),lambda p:p['owners'][0].update(id=1),lambda p:p['owners'].reverse(),lambda p:p['families'][0].update(sprite='unmapped'),lambda p:p['families'][0]['clips'][0].update(name='unmapped')]
  for mutate in mutations:
   p=copy.deepcopy(self.recipe);mutate(p);path.write_text(json.dumps(p))
   with self.subTest(mutate=mutate),self.assertRaises((ValueError,KeyError)):b.source(path)
 def test_original_observer_scope_and_input_mapping(self):
  config=json.loads(self.files['input.json']);self.assertEqual(len(config['cases']),6);self.assertEqual(config['cases'][4]['flag'],True);self.assertIn('class_name CoinSpawner',self.files['factory.gd']);self.assertEqual(len(self.copied),8);self.assertIn('No contents AI',self.native_proof['scope']);self.assertNotIn('producer_sha256',self.native_proof);self.assertEqual(self.native_proof['content_producer_sha256'],hashlib.sha256(Path(b.__file__).read_bytes()).hexdigest())
 def test_frozen_original_and_fixture(self):
  review=json.loads(fixture.REVIEW.read_bytes());runs=[b.ROOT/v['path']for v in review['results']];self.assertEqual((runs[0]/'reference.json').read_bytes(),(runs[1]/'reference.json').read_bytes());doc=json.loads((runs[0]/'reference.json').read_bytes());self.assertEqual(len(doc['rows']),282);self.assertEqual(fixture.validate(doc,json.loads(self.files['input.json'])),doc);out=self.temporary()/'fixture';self.assertEqual(fixture.build(out),fixture.build(out));(out/'native.tsv').write_text('changed')
  with self.assertRaises(ValueError):fixture.build(out)
 def test_unknown_native_fields_and_receipts_reject(self):
  review=json.loads(fixture.REVIEW.read_bytes());doc=json.loads((b.ROOT/review['results'][0]['path']/'reference.json').read_bytes());config=json.loads(self.files['input.json']);mutations=[lambda d:d.update(schema=True),lambda d:d.update(extra=1),lambda d:d['rows'].pop(),lambda d:d['rows'][0].update(playing=1),lambda d:d['rows'][0].update(time=0),lambda d:d['rows'][0].update(frame_index=True),lambda d:d['rows'][0].update(audio=[[0,'true']]),lambda d:d['rows'][0]['content'].update(extra=1)]
  for mutate in mutations:
   d=copy.deepcopy(doc);mutate(d)
   with self.subTest(mutate=mutate),self.assertRaises(ValueError):fixture.validate(d,config)
  p=self.temporary()/'review.json'
  for key in ['producer_sha256','native_producer_sha256','fixture_protocol_sha256']:
   d=copy.deepcopy(review);d[key]='0'*64;p.write_text(json.dumps(d))
   with patch.object(fixture,'REVIEW',p),self.assertRaises(ValueError):fixture.build(self.temporary()/'fixture')

if __name__=='__main__':unittest.main()
