import copy,hashlib,json,struct,tempfile,unittest,zlib
from pathlib import Path
from unittest import mock
from tools import dungeon_key_indicator as d
from tests.prepare_dungeon_key_indicator import build,REFERENCE
ROOT=Path(__file__).resolve().parents[1]

class KeyIndicatorTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.model,cls.proof=d.source_model();cls.blob=d.encode(cls.model)
 def candidate(self,value):
  with tempfile.TemporaryDirectory(prefix='key-hud-review-',dir=ROOT/'build')as folder:
   path=Path(folder)/'review.json';path.write_text(json.dumps(value),encoding='utf8');return d.read(path)
 def test_binary_roundtrip_independent_keys_and_source_parameters(self):
  self.assertEqual(d.parse(self.blob),self.model);self.assertEqual(len(self.blob),749);self.assertEqual(d.keys.parse(self.model['keys'])['region'],'Catacombs');self.assertEqual(self.model['parameters'],[320.,160.,280.,320.,.2,-4.,3.,.1,.2]);self.assertEqual(self.model['initial_text'],'x 4');self.assertEqual((self.model['curve'],self.model['default_curve'],self.model['threshold'],self.model['diagnostics']),(1,0,0,1));self.assertEqual(struct.unpack_from('<d',self.blob,96)[0],.2)
 def test_actual_native_repetitions_and_exact_provenance(self):
  a=json.loads((REFERENCE/'native-14/indicator.json').read_text('utf8'));b=json.loads((REFERENCE/'native-15/indicator.json').read_text('utf8'));self.assertEqual(a,b);self.assertEqual(len(a['rows']),91);self.assertEqual(a['append_attempts'],[dict(case=1,phase=4),dict(case=5,phase=4),dict(case=6,phase=4)])
  self.assertEqual([(r['case'],r['phase'])for r in a['rows']],[(i,p)for i,c in enumerate(json.loads((REFERENCE/'native-14/input.json').read_text('utf8'))['cases'])for p in range(-1,len(c))])
  for number in[14,15]:
   root=REFERENCE/f'native-{number}';proof=json.loads((root/'receipt.json').read_text('utf8'));self.assertEqual((proof['cases'],proof['rows'],proof['expected_negative_exit']),(8,91,17));self.assertEqual(len(proof['expected_original_diagnostics']),6)
   for name in['input','indicator']:self.assertEqual(proof[name+'_sha256'],hashlib.sha256((root/(name+'.json')).read_bytes()).hexdigest())
  linear=next(r for r in a['rows']if r['case']==7 and r['phase']==6);self.assertEqual(linear['y'],159.5);running=next(r for r in a['rows']if r['case']==4 and r['phase']==14);self.assertTrue(running['running'])
 def test_fixture_rebuild_is_identical_and_matches_current_inputs(self):
  with tempfile.TemporaryDirectory(prefix='key-hud-fixture-',dir=ROOT/'build')as folder:
   root=Path(folder);self.assertEqual(build(root),self.blob);first={p.name:p.read_bytes()for p in root.iterdir()if p.is_file()};self.assertEqual(build(root),self.blob);self.assertEqual(first,{p.name:p.read_bytes()for p in root.iterdir()if p.is_file()})
 def test_all_truncations_and_typed_binary_corruption_rejected(self):
  for end in range(len(self.blob)):
   with self.assertRaises(ValueError,msg=str(end)):d.parse(self.blob[:end])
  for at in[8,20,24,28,32,36,40,44,48,52,56,60,136,140,144,148,152,156,160,168,172]:
   b=bytearray(self.blob);struct.pack_into('<I',b,at,0xffffffff);struct.pack_into('<I',b,16,zlib.crc32(b[32:]));
   with self.assertRaises(ValueError,msg=str(at)):d.parse(bytes(b))
  for index in range(9):
   b=bytearray(self.blob);struct.pack_into('<d',b,64+index*8,float('nan'));struct.pack_into('<I',b,16,zlib.crc32(b[32:]));
   with self.assertRaises(ValueError):d.parse(bytes(b))
 def test_model_numeric_text_identity_and_known_signed_threshold(self):
  changed=copy.deepcopy(self.model);changed['threshold']=-1;self.assertEqual(d.parse(d.encode(changed)),changed)
  for key,value in[('scene',5),('keys',b'bad'),('flags',True),('flags',4),('diagnostics',2),('diagnostics',True),('threshold',True),('threshold',65536),('curve',2),('default_curve',True),('default_curve',1),('prefix','中'),('suffix','\0'),('initial_text','a'*257),('parameters',[]),('unknown',1)]:
   changed=copy.deepcopy(self.model);changed[key]=value
   with self.subTest(key=key,value=value),self.assertRaises(ValueError):d.encode(changed)
  for index,value in[(0,True),(4,0),(4,1e-300),(4,-1),(7,61),(8,float('inf'))]:
   changed=copy.deepcopy(self.model);changed['parameters'][index]=value
   with self.subTest(index=index,value=value),self.assertRaises(ValueError):d.encode(changed)
 def test_review_unknown_missing_changed_source_and_engine_identity_rejected(self):
  review,_=d.read()
  for key,value in[('schema',True),('schema',2),('commit','0'*40),('scope',''),('keys_review_sha256','0'*64),('unknown',1),('paths',{})]:
   p=copy.deepcopy(review);p[key]=value
   with self.subTest(key=key),self.assertRaises(ValueError):self.candidate(p)
  for field in review:
   p=copy.deepcopy(review);del p[field]
   with self.subTest(missing=field),self.assertRaises(ValueError):self.candidate(p)
  for field in['commit','native_sha256']:
   p=copy.deepcopy(review);p['engine'][field]='0'*len(p['engine'][field])
   with self.assertRaises(ValueError):self.candidate(p)
  p=copy.deepcopy(review);p['sources'][p['paths']['script']]='0'*64
  with self.assertRaises(ValueError):self.candidate(p)
 def test_duplicate_nonfinite_json_and_unmapped_source_method_rejected(self):
  with tempfile.TemporaryDirectory(prefix='key-hud-json-',dir=ROOT/'build')as folder:
   path=Path(folder)/'bad.json'
   for text in['{"schema":1,"schema":1}','{"schema":NaN}']:
    path.write_text(text,encoding='utf8')
    with self.assertRaises(ValueError):d.read(path)
  review,source=d.read()
  class Changed:
   def __getattr__(self,name):return getattr(source,name)
   def text(self,path):
    raw=source.text(path)
    return raw+'\nfunc unmapped():\n\tpass\n'if path==review['paths']['script']else raw
  with mock.patch.object(d,'read',return_value=(review,Changed())):
   with self.assertRaises(ValueError):d.source_model()
 def test_shared_core_lists_and_platform_independence(self):
  source='runtime/dungeon_key_indicator.cpp';self.assertIn(source,(ROOT/'CMakeLists.txt').read_text('utf8'));self.assertIn(source,(ROOT/'platform/ctr/Makefile').read_text('utf8'));self.assertNotIn('3ds.h',(ROOT/source).read_text('utf8'))
if __name__=='__main__':unittest.main()
