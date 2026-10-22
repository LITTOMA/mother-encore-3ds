import copy,json,struct,tempfile,unittest,zlib
from pathlib import Path
from unittest.mock import patch
from tools import coin_scene as c
from tools import run_coin_scene_reference as native
from tools.coin_scene_protocol import validate
from tests import prepare_coin_scene as fixture

class CoinSceneTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.recipe,cls.model,cls.proof=c.source();cls.blob=c.encode(cls.model);cls.root=c.ROOT/'build/tests';cls.root.mkdir(parents=True,exist_ok=True)
 def temporary(self):
  t=tempfile.TemporaryDirectory(prefix='coin-scene-',dir=self.root);self.addCleanup(t.cleanup);return Path(t.name)
 def changed(self,at,value):
  blob=bytearray(self.blob);struct.pack_into('<I',blob,at,value);struct.pack_into('<I',blob,16,zlib.crc32(blob[32:]));return blob
 def test_actual_complete_unique_bindings_and_offsets(self):
  parsed=c.decode(self.blob);self.assertEqual(len(parsed['bindings']),49);self.assertEqual(parsed['scene'],self.model['scene']);self.assertEqual(len({v['id']for v in self.model['bindings']}),49);self.assertEqual(len({v['owner']for v in self.model['bindings']}),49);self.assertEqual({tuple(v['local'])for v in self.model['bindings']},{(0.,8.)});self.assertEqual({v['reparent']for v in self.model['bindings']},{'Bottom2'});self.assertEqual(self.model['sink'],'Objects');self.assertEqual(self.model['bindings'][0]['position'],[-592.,-264.]);self.assertEqual(c.encode(self.model),self.blob)
 def test_truncation_crc_header_version_capability_and_directory(self):
  for n in range(len(self.blob)):
   with self.assertRaises(ValueError):c.decode(self.blob[:n])
  changes=[0,8,12,20,24,28,32,52,56,60]+[64+16*n+4*field for n in range(3)for field in range(4)]
  for at in changes:
   with self.subTest(at=at),self.assertRaises(ValueError):c.decode(self.changed(at,0xffffffff))
  corrupt=bytearray(self.blob);corrupt[-1]^=1
  for blob in [None,self.blob+b'\0',corrupt]:
   with self.assertRaises(ValueError):c.decode(blob)
 def test_record_domains_tree_order_strings_and_stable_identity(self):
  meta,records,pool=[struct.unpack_from('<I',self.blob,68+16*n)[0]for n in range(3)]
  for at in [meta+4*n for n in range(8)]+[records+4*n for n in range(16)]+[pool]:
   with self.subTest(at=at),self.assertRaises(ValueError):c.decode(self.changed(at,0xffffffff))
  for at,value in [(records+64,struct.unpack_from('<I',self.blob,records)[0]),(records+68,struct.unpack_from('<I',self.blob,records+4)[0]),(records+72,struct.unpack_from('<I',self.blob,records+8)[0]),(records+12,0),(records+40,0)]:
   with self.subTest(at=at),self.assertRaises(ValueError):c.decode(self.changed(at,value))
 def test_model_unknown_transform_fields_and_bindings_fail_closed(self):
  mutations=[lambda m:m.update(extra=1),lambda m:m.update(scene=True),lambda m:m.update(root_position=[1,0]),lambda m:m.update(sink_position=[0,float('nan')]),lambda m:m.update(source='../escape'),lambda m:m.update(bindings=[]),lambda m:m['bindings'][0].update(extra=1),lambda m:m['bindings'][0].update(id=True),lambda m:m['bindings'][0].update(id=1),lambda m:m['bindings'][0].update(order=0),lambda m:m['bindings'][0].update(owner_order=0),lambda m:m['bindings'][0].update(default_count=True),lambda m:m['bindings'][0].update(default_count=0),lambda m:m['bindings'][0].update(position=[float('nan'),0]),lambda m:m['bindings'][0].update(sink='Other'),lambda m:m['bindings'][0].update(parent='Other'),lambda m:m['bindings'].reverse()]
  for mutate in mutations:
   m=copy.deepcopy(self.model);mutate(m)
   with self.subTest(mutate=mutate),self.assertRaises(ValueError):c.encode(m)
 def test_reviewed_source_and_complete_owner_coverage(self):
  path=self.temporary()/'recipe.json';mutations=[lambda p:p.update(schema=True),lambda p:p.update(extra=1),lambda p:p.update(commit='0'*40),lambda p:p.update(scene=True),lambda p:p.update(source_sha256='0'*64),lambda p:p.update(breakable_recipe_sha256='0'*64),lambda p:p.update(coin_recipe_sha256='0'*64),lambda p:p['instances'].pop(),lambda p:p['instances'].reverse(),lambda p:p['instances'][0].update(owner=1),lambda p:p['instances'][0].update(path='Other'),lambda p:p['instances'][0].update(extra=1)]
  for mutate in mutations:
   p=copy.deepcopy(self.recipe);mutate(p);path.write_text(json.dumps(p))
   with self.subTest(mutate=mutate),self.assertRaises(ValueError):c.source(path)
 def test_actual_composition_protocol_and_retained_input_bytes(self):
  files,copied,proof=native.source();config=json.loads(files['input.json']);review=json.loads(fixture.REVIEW.read_bytes());folder=c.ROOT/review['results'][0]['path'];doc=json.loads((folder/'reference.json').read_bytes());self.assertEqual(validate(doc,config),doc);self.assertEqual([len(v['births'])for v in doc['cases']],[147,147,144]);self.assertEqual(sum(len(v['rows'])*len(config['bindings'])for v in doc['cases']),6174);self.assertEqual(proof['copied_sha256'],{k:__import__('hashlib').sha256(v).hexdigest()for k,v in copied.items()});self.assertEqual((folder/'reference.json').read_bytes(),(c.ROOT/review['results'][1]['path']/'reference.json').read_bytes())
 def test_unknown_composition_versions_fields_clocks_and_voices_reject(self):
  files,_,_=native.source();config=json.loads(files['input.json']);review=json.loads(fixture.REVIEW.read_bytes());doc=json.loads((c.ROOT/review['results'][0]['path']/'reference.json').read_bytes())
  def live(d):return next(r for r in d['cases'][0]['rows']if 'live'in r)['live'][0]
  changes=[lambda d:d.update(schema=True),lambda d:d.update(schema=1),lambda d:d.update(extra=1),lambda d:d['godot'].update(major=True),lambda d:d['cases'].pop(),lambda d:d['cases'][0].update(mode='other'),lambda d:d['cases'][0].update(cash=44),lambda d:d['cases'][0].update(cash_opens=True),lambda d:d['cases'][0].update(collect_audio=[[0,'other']]),lambda d:d['cases'][0]['births'].pop(),lambda d:d['cases'][0]['births'][0].update(factory=0),lambda d:d['cases'][0]['births'][0].update(ordinal=True),lambda d:d['cases'][0]['births'][0].update(position=[0,0]),lambda d:d['cases'][0]['births'][0].update(speed='000000000000f87f'),lambda d:d['cases'][0]['rows'].pop(),lambda d:d['cases'][0]['rows'][0].update(physics=True),lambda d:d['cases'][0]['rows'][0].update(born=1),lambda d:d['cases'][0]['rows'][0].update(extra=1),lambda d:d['cases'][0]['rows'][0]['factories'].pop(),lambda d:d['cases'][0]['rows'][0]['factories'][0].update(spawning=1),lambda d:d['cases'][0]['rows'][0]['factories'][0].update(remaining=True),lambda d:d['cases'][0]['rows'][0]['factories'][0].update(parent='Other'),lambda d:d['cases'][0]['rows'][0]['factories'][0].update(timer='000000000000f87f'),lambda d:live(d).update(ordinal=True),lambda d:live(d)['state'].update(extra=1),lambda d:live(d)['state'].update(animation='Other'),lambda d:live(d)['state'].update(time=0x7fc00000),lambda d:live(d).update(drops=[-1]),lambda d:live(d).update(drops=[0,1])]
  for mutate in changes:
   d=copy.deepcopy(doc);mutate(d)
   with self.subTest(mutate=mutate),self.assertRaises(ValueError):validate(d,config)
 def test_source_gated_fixture_and_changed_cache_reject(self):
  out=self.temporary()/'fixture';outputs=fixture.build(out);self.assertEqual(outputs,fixture.build(out));self.assertEqual(outputs['scene.enccf'],self.blob);self.assertEqual(len(outputs),5);(out/'native.tsv').write_text('changed')
  with self.assertRaises(ValueError):fixture.build(out)
  unowned=self.temporary()/'unowned';unowned.mkdir()
  with self.assertRaises(ValueError):fixture.build(unowned)
 def test_frozen_composition_source_producers_and_repeats_reject(self):
  review=json.loads(fixture.REVIEW.read_bytes());path=self.temporary()/'review.json';changes=[lambda r:r.update(schema=True),lambda r:r.update(extra=1),lambda r:r.update(source={}),lambda r:r.update(native_producer_sha256='0'*64),lambda r:r.update(fixture_protocol_sha256='0'*64),lambda r:r.update(observation_protocol_sha256='0'*64),lambda r:r['results'][0].update(sha256='0'*64),lambda r:r['results'][0].update(path='../escaped'),lambda r:r['results'].__setitem__(1,r['results'][0]),lambda r:r['results'].pop()]
  for mutate in changes:
   r=copy.deepcopy(review);mutate(r);path.write_text(json.dumps(r))
   with self.subTest(mutate=mutate),patch.object(fixture,'REVIEW',path),self.assertRaises(ValueError):fixture.build(self.temporary()/'fixture')

if __name__=='__main__':unittest.main()
