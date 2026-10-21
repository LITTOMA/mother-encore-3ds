import copy,hashlib,json,struct,sys,tempfile,unittest,zlib
from pathlib import Path
from unittest import mock
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import catacombs_step_audio as audio,audio_asset,run_step_audio_reference as native
from tests import prepare_step_audio as fixture
class CatacombsStepAudioTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.temporary=tempfile.TemporaryDirectory(prefix='step-audio-python-',dir=ROOT/'build');cls.root=Path(cls.temporary.name).resolve();assert cls.root.is_relative_to(ROOT/'build');cls.out=cls.root/'fixture';cls.media=fixture.build(cls.out);cls.policy,cls.metadata,cls.recipe,_,cls.proof=audio.source();cls.receipt=json.loads((cls.media/'source.json').read_bytes());cls.blob=(cls.media/cls.policy['output']).read_bytes();cls.bank=audio_asset.parse_bank(cls.blob)
 @classmethod
 def tearDownClass(cls):cls.temporary.cleanup()
 def test_original_eight_loops_in_independent_bank(self):
  self.assertEqual(len(self.bank['assets']),8);self.assertEqual([s['name']for s in self.metadata['sounds']],['grass','stone','soil','sand','wood','metal','water','cave']);self.assertNotEqual(self.policy['output'],'sound/banks/opening.encaudio')
  for a,s in zip(self.bank['assets'],self.metadata['sounds']):self.assertEqual(a['stable_id'],s['id']);self.assertEqual(a['source_path'],s['source']);self.assertEqual(a['source_sha256'],s['sha256']);self.assertTrue(a['loop']);self.assertEqual(a['loop_start'],0);self.assertEqual(a['gain_db'],self.policy['gain_db'])
 def test_actual_pcm_and_verbatim_converter_receipt(self):
  self.assertEqual(sum(a['pcm_bytes']for a in self.bank['assets']),564480);self.assertEqual(len(self.receipt['output_bindings']),9)
  for row in self.receipt['output_bindings']:
   data=(self.media/row['candidate_path']).read_bytes();self.assertEqual(len(data),row['bytes']);self.assertEqual(hashlib.sha256(data).hexdigest(),row['sha256']);original=next(r for r in self.receipt['conversion']['files']if r['path']==row['conversion_path']);self.assertEqual(row['sha256'],original['sha256'])
  for a in self.bank['assets']:self.assertEqual(zlib.crc32((self.media/a['pcm_path']).read_bytes()),a['pcm_crc32'])
 def test_source_review_and_real_node_defaults(self):
  review=json.loads(fixture.REVIEW.read_bytes());self.assertEqual(review['source'],self.proof);self.assertEqual(review['producer_sha256'],hashlib.sha256(Path(audio.__file__).read_bytes()).hexdigest());self.assertEqual(self.policy['bus'],'SFX');self.assertEqual(self.policy['gain_db'],0);self.assertEqual(self.policy['pitch'],1);self.assertEqual(len(review['results']),2)
 def test_recipe_version_types_and_unknown_fields(self):
  p=self.root/'recipe.json'
  for key,value in[('schema',2),('schema',True),('kind','unknown'),('commit','0'*40),('bus','Music'),('gain_db',-1),('gain_db',False),('pitch',2),('pitch',True),('scope',''),('license_review',''),('steps_recipe_sha256','0'*64),('bus_sha256','0'*64),('manager_sha256','0'*64)]:
   data=copy.deepcopy(self.policy);data[key]=value;p.write_text(json.dumps(data),encoding='utf8')
   with self.subTest(key=key,value=value),self.assertRaises(ValueError):audio.read(p)
  data=copy.deepcopy(self.policy);data['unknown']=0;p.write_text(json.dumps(data),encoding='utf8')
  with self.assertRaises(ValueError):audio.read(p)
 def test_recipe_duplicate_fields_and_nonfinite_numbers(self):
  p=self.root/'recipe.json';p.write_text('{"schema":1,"schema":1}',encoding='utf8')
  with self.assertRaises(ValueError):audio.read(p)
  data=copy.deepcopy(self.policy);data['gain_db']=float('nan');p.write_text(json.dumps(data),encoding='utf8')
  with self.assertRaises(ValueError):audio.read(p)
 def test_recipe_unsafe_paths(self):
  p=self.root/'recipe.json'
  for key,value in[('steps_recipe','../review.json'),('steps_recipe','content/mayor-footsteps.json'),('output','../out.encaudio'),('output','C:/bad.encaudio'),('output','sound/banks/bad.pcm'),('bus_source','../bus.tres'),('manager_source','/absolute.gd')]:
   data=copy.deepcopy(self.policy);data[key]=value;p.write_text(json.dumps(data),encoding='utf8')
   with self.subTest(key=key),self.assertRaises(ValueError):audio.read(p)
 def test_fixture_reuses_validated_media_without_decode(self):
  before={r['candidate_path']:(self.media/r['candidate_path']).read_bytes()for r in self.receipt['output_bindings']}
  with mock.patch.object(audio,'compile',side_effect=AssertionError('Unexpected repeated decode')):self.assertEqual(fixture.build(self.out),self.media)
  self.assertEqual(before,{p:(self.media/p).read_bytes()for p in before})
 def test_fixture_rejects_changed_pcm(self):
  p=self.media/self.bank['assets'][0]['pcm_path'];original=p.read_bytes();bad=bytearray(original);bad[0]^=1;p.write_bytes(bad)
  try:
   with self.assertRaises(ValueError):fixture.build(self.out)
  finally:p.write_bytes(original)
 def test_fixture_review_version_and_duplicate_fields_rejected(self):
  p=self.root/'semantic-review.json';data=json.loads(fixture.REVIEW.read_bytes());data['schema']=True;p.write_text(json.dumps(data),encoding='utf8')
  with mock.patch.object(fixture,'REVIEW',p),self.assertRaises(ValueError):fixture.build(self.out)
  p.write_text('{"schema":1,"schema":1}',encoding='utf8')
  with mock.patch.object(fixture,'REVIEW',p),self.assertRaises(ValueError):fixture.build(self.out)
 def test_cached_receipt_source_and_output_coverage_rejected(self):
  p=self.media/'source.json';original=p.read_bytes()
  try:
   for key,value in[('output_bindings',[]),('native_defaults_unverified',False),('producer_sha256','0'*64)]:
    bad=json.loads(original);bad[key]=value;p.write_text(json.dumps(bad),encoding='utf8')
    with self.subTest(key=key),self.assertRaises(ValueError):fixture.build(self.out)
  finally:p.write_bytes(original)
 def test_compile_rejects_existing_and_external_output(self):
  with self.assertRaises(ValueError):audio.compile(self.media)
  target=ROOT/'docs'/'invalid-step-audio-output';self.assertFalse(target.exists())
  with self.assertRaises(ValueError):audio.compile(target)
  self.assertFalse(target.exists())
 def test_existing_audio_bank_version_and_crc_fail_closed(self):
  for at,value in[(0,0),(8,2),(12,0),(16,0)]:
   bad=bytearray(self.blob);struct.pack_into('<I',bad,at,value)
   with self.subTest(at=at),self.assertRaises(ValueError):audio_asset.parse_bank(bad)
 def test_real_native_pair_is_identical(self):
  docs=[]
  for n in[1,2]:
   p=ROOT/f'reports/catacombs-step-audio/native-{n}';receipt=json.loads((p/'receipt.json').read_bytes());self.assertEqual(receipt['producer_sha256'],hashlib.sha256(Path(native.__file__).read_bytes()).hexdigest());docs.append(native.validate_result(json.loads((p/'reference.json').read_bytes()),receipt))
  self.assertEqual(*docs);self.assertEqual(len(docs[0]['rows']),32)
 def test_native_version_domain_and_real_bus_negative_paths(self):
  p=ROOT/'reports/catacombs-step-audio/native-1';proof=json.loads((p/'receipt.json').read_bytes());doc=json.loads((p/'reference.json').read_bytes())
  for kind in['schema','godot','count','playing','source','gain','pitch','loop','loop_offset_type','length','bus_name','bus_gain','bus_shape']:
   bad=copy.deepcopy(doc)
   if kind=='schema':bad['schema']=True
   elif kind=='godot':bad['godot']=None
   elif kind=='count':bad['rows'].pop()
   elif kind=='playing':bad['rows'][0]['playing']=True
   elif kind=='source':bad['rows'][0]['source']='res://unknown.mp3'
   elif kind=='gain':bad['rows'][0]['gain_db']=-3
   elif kind=='pitch':bad['rows'][0]['pitch']=2
   elif kind=='loop':bad['rows'][0]['loop']=False
   elif kind=='loop_offset_type':bad['rows'][0]['loop_offset']=False
   elif kind=='length':bad['rows'][0]['length']=float('nan')
   elif kind=='bus_name':bad['buses'][0]['name']='Unrecognized'
   elif kind=='bus_gain':bad['buses'][0]['gain_db']+=1
   else:bad['buses'][0]=None
   with self.subTest(kind=kind),self.assertRaises(ValueError):native.validate_result(bad,proof)
if __name__=='__main__':unittest.main()
