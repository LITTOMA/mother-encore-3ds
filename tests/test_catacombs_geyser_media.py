import copy,hashlib,json,struct,tempfile,unittest,zlib
from pathlib import Path
from unittest.mock import patch
from tools import catacombs_geyser_media as media,run_geyser_idle_reference as native
from tests import prepare_geyser_media as fixture

class GeyserMediaTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.temporary=tempfile.TemporaryDirectory(prefix='geyser-media-python-',dir=media.ROOT/'build');cls.root=Path(cls.temporary.name);cls.out=cls.root/'fixture';cls.candidate=fixture.build(cls.out);cls.policy,cls.timeline,cls.capacity,cls.joins,cls.audio,cls.source=media.source();cls.blob=(cls.candidate/cls.policy['output']).read_bytes();cls.model=media.parse(cls.blob);cls.proof=json.loads((cls.candidate/'source.json').read_bytes());cls.native=media.ROOT/'reports/geyser-media/native-1';cls.doc=json.loads((cls.native/'reference.json').read_bytes());cls.receipt=json.loads((cls.native/'receipt.json').read_bytes());cls.native_source=native.source()
 @classmethod
 def tearDownClass(cls):cls.temporary.cleanup()
 def test_original_three_actual_pcm_and_source_identity(self):
  self.assertEqual(self.source['review_sha256'],hashlib.sha256(media.RECIPE.read_bytes()).hexdigest());self.assertEqual(self.capacity,128);self.assertEqual(len(self.blob),2392);bank=media.audio_asset.parse_bank(self.model['bank']);self.assertEqual(len(bank['assets']),3);self.assertEqual(sum(a['pcm_bytes']for a in bank['assets']),1645056)
  for a,s in zip(bank['assets'],self.timeline['sounds']):
   self.assertEqual(a['source_path'],'res://'+s['source']);self.assertEqual(a['sample_rate'],44100);self.assertFalse(a['loop']);self.assertEqual(a['frames'],137088);self.assertEqual(zlib.crc32((self.candidate/a['pcm_path']).read_bytes()),a['pcm_crc32'])
 def test_roundtrip_and_truncations(self):
  self.assertEqual(media.encode(self.model),self.blob)
  for n in range(len(self.blob)):
   with self.assertRaises(ValueError):media.parse(self.blob[:n])
  for b in [self.blob+b'\0',self.blob[:200]+bytes([self.blob[200]^1])+self.blob[201:]]:
   with self.assertRaises(ValueError):media.parse(b)
 def test_versions_directory_reserved_and_source_pin(self):
  for at,v in [(0,0),(8,2),(12,0),(20,2),(24,1),(28,1),(32,0),(52,3),(56,1),(60,1)]+[(64+16*n+k,v)for n in range(4)for k,v in [(0,99),(4,0xffffffff),(8,0xffffffff),(12,0)]]:
   b=bytearray(self.blob);struct.pack_into('<I',b,at,v);struct.pack_into('<I',b,16,zlib.crc32(b[32:]))
   with self.subTest(at=at),self.assertRaises(ValueError):media.parse(b)
  policy=struct.unpack_from('<I',self.blob,68)[0]
  for offset,v in [(0,0),(4,0),(4,129)]+[(n*4,1)for n in range(2,8)]:
   b=bytearray(self.blob);struct.pack_into('<I',b,policy+offset,v);struct.pack_into('<I',b,16,zlib.crc32(b[32:]))
   with self.assertRaises(ValueError):media.parse(b)
 def test_model_join_nested_owner_and_format_negatives(self):
  for change in [lambda m:m.update(rate=True),lambda m:m.update(capacity=True),lambda m:m.update(capacity=129),lambda m:m.update(timeline=b'bad'),lambda m:m.update(bank=b'bad'),lambda m:m['joins'].pop(),lambda m:m['joins'][0].update(id=0),lambda m:m['joins'][0].update(sha256='0'*64),lambda m:m['joins'].reverse(),lambda m:m.update(ignore=True),lambda m:m['joins'][0].update(extra=1)]:
   m=copy.deepcopy(self.model);change(m)
   with self.assertRaises(ValueError):media.encode(m)
 def test_recipe_version_paths_dependencies_and_duplicates(self):
  path=self.root/'recipe.json'
  for key,value in [('schema',True),('schema',2),('kind','unknown'),('commit','0'*40),('voice_capacity',0),('voice_capacity',129),('timeline_recipe','../outside'),('poison_sha256','0'*64),('output','/absolute'),('bank','sound/banks/x.pcm'),('scope',''),('license_review',False),('unknown',0)]:
   p=copy.deepcopy(self.policy);p[key]=value;path.write_text(json.dumps(p))
   with self.subTest(key=key),self.assertRaises(ValueError):media.read(path)
  for text in ['{"schema":1,"schema":1}','{"schema":NaN}']:
   path.write_text(text)
   with self.assertRaises(ValueError):media.read(path)
 def test_two_actual_idle_and_named_tree_runs(self):
  self.assertEqual(self.doc,json.loads((media.ROOT/'reports/geyser-media/native-2/reference.json').read_bytes()));self.assertEqual(len(self.doc['rows']),224);self.assertEqual(len(self.doc['tree_lifecycle']),8)
  with patch.object(native,'source',return_value=self.native_source):native.validate_result(self.doc,self.receipt)
 def test_audited_engine_bytes_and_license_are_required(self):
  original=Path.read_bytes;recipe=media.g.read()[0];engine=media.ROOT/'reports/geyser-timeline/official-engine-source'
  for name in [next(iter(recipe['engine_sources'])),'LICENSE.txt']:
   target=(engine/name).resolve()
   def changed(path,target=target):return b'changed engine input'if path.resolve()==target else original(path)
   with patch.object(Path,'read_bytes',changed),self.subTest(name=name),self.assertRaises(ValueError):media.source()
 def test_native_protocol_negative_states_and_input_order(self):
  for change in [lambda d:d.update(schema=True),lambda d:d['rows'].pop(),lambda d:d['rows'][0].update(delta=0),lambda d:d['rows'][0].update(tick=True),lambda d:d['rows'][24].update(actions=[]),lambda d:d['rows'][0]['before'].update(commands=[['ignored']]),lambda d:d['rows'][0]['after'].update(frame=32),lambda d:d['tree_lifecycle'].pop(),lambda d:d['tree_lifecycle'][0].update(mode=2),lambda d:d['tree_lifecycle'][0].update(paused=1)]:
   d=copy.deepcopy(self.doc);change(d)
   with patch.object(native,'source',return_value=self.native_source),self.assertRaises(ValueError):native.validate_result(d,self.receipt)
 def test_cached_fixture_is_rechecked_without_new_decode(self):
  with patch.object(media,'compile',side_effect=AssertionError('Repeated decode')):self.assertEqual(fixture.build(self.out),self.candidate)
 def test_cached_binding_coverage_pcm_and_tree_corruption(self):
  path=self.candidate/'source.json';original=path.read_bytes()
  for change in [lambda p:p['output_bindings'].pop(),lambda p:p['output_bindings'].append(p['output_bindings'][0]),lambda p:p['output_bindings'][0].update(candidate_path='../escape'),lambda p:p['output_bindings'][0].update(sha256='0'*64)]:
   p=copy.deepcopy(self.proof);change(p);path.write_text(json.dumps(p))
   try:
    with self.assertRaises(ValueError):fixture.build(self.out)
   finally:path.write_bytes(original)
  for target in [self.out/'tree.tsv',self.candidate/self.audio['assets'][0]['pcm_path']]:
   original=target.read_bytes();target.write_bytes(original+b'\0')
   try:
    with self.assertRaises(ValueError):fixture.build(self.out)
   finally:target.write_bytes(original)
 def test_fixture_ownership_and_review_changed(self):
  with self.assertRaises(ValueError):fixture.build(media.ROOT/'build')
  path=self.root/'review.json';p=json.loads(fixture.REVIEW.read_bytes());p['schema']=True;path.write_text(json.dumps(p))
  with patch.object(fixture,'REVIEW',path),self.assertRaises(ValueError):fixture.build(self.out)
if __name__=='__main__':unittest.main()
