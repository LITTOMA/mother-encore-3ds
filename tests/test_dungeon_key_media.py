import copy,hashlib,json,struct,sys,tempfile,unittest,zlib
from pathlib import Path
from unittest import mock
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import dungeon_key_media as d
from tests.prepare_dungeon_key_media import build,REFERENCE

class KeyMediaTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.temp=tempfile.TemporaryDirectory(prefix='key-media-python-',dir=ROOT/'build');cls.addClassCleanup(cls.temp.cleanup);cls.blob=build(Path(cls.temp.name));cls.data=d.parse(cls.blob);cls.native=json.loads((REFERENCE/'native-6/geometry.json').read_text('utf8'))
 def test_codec_and_exact_nested_clocks(self):
  self.assertEqual(d.encode(self.data,self.data['bank']),self.blob);self.assertEqual(len(self.data['owners']),6);self.assertEqual(len(self.data['profiles']),3);self.assertEqual(len(self.data['shapes']),9);self.assertEqual(len(self.data['sounds']),6);self.assertEqual(len(d.audio.parse_bank(self.data['bank'])['assets']),2);self.assertEqual(self.data['clocks'],d.clocks.extract()[1])
 def test_native_geometry_repetitions_and_provenance(self):
  self.assertEqual(self.native,json.loads((REFERENCE/'native-7/geometry.json').read_text('utf8')));d.validate_native(self.native)
  for n in [6,7]:
   p=REFERENCE/f'native-{n}';r=json.loads((p/'receipt.json').read_text('utf8'));self.assertEqual(r['geometry_sha256'],hashlib.sha256((p/'geometry.json').read_bytes()).hexdigest());self.assertEqual(r['sprites'],69);self.assertEqual(r['shapes'],9);self.assertEqual(r['sounds'],6)
 def test_native_mismatch_fails_closed(self):
  for section,field,value in [('sprites','frame',99),('sprites','flip_h',True),('sprites','tree_visible',False),('shapes','mask',999),('shapes','center',['0','0']),('sounds','pitch','2')]:
   a=copy.deepcopy(self.native);a[section][0][field]=value
   with self.assertRaises(ValueError):d.validate_native(a)
 def test_all_truncations_rejected(self):
  for n in range(len(self.blob)):
   with self.assertRaises(ValueError,msg=str(n)):d.parse(self.blob[:n])
 def test_corrupt_headers_directory_typed_records(self):
  offsets=[struct.unpack_from('<I',self.blob,68+i*16)[0]for i in range(7)];p,o,s,a=(offsets[i]for i in[2,3,4,5]);positions=[8,20,24,28,32,52,56,60]+[64+i*16+j*4 for i in range(7)for j in range(4)]+[p+v for v in[0,4,8,12,16,20,60,64,68,72,76,80,84,88,92,96,104,108]]+[o+v for v in[0,4,8,12,16,20,24,28,32,36,40,44,52,56,60]]+[s+v for v in range(0,64,4)]+[a+v for v in range(0,48,4)]
  for at in positions:
   b=bytearray(self.blob);struct.pack_into('<I',b,at,0xffffffff);struct.pack_into('<I',b,16,zlib.crc32(b[32:]))
   with self.assertRaises(ValueError,msg=str(at)):d.parse(b)
  for at in[p+100,o+48]:
   b=bytearray(self.blob);struct.pack_into('<I',b,at,4097);struct.pack_into('<I',b,16,zlib.crc32(b[32:]))
   with self.assertRaises(ValueError):d.parse(b)
 def test_binding_semantics_are_data_unknowns_rejected(self):
  known=copy.deepcopy(self.data);known['owners'][3]['bits']&=~2;known['sounds'][3]['finish']=0;known['shapes'][0]['mask']=1;known['profiles'][0]['z_index']=1;decoded=d.parse(d.encode(known,known['bank']));self.assertEqual(decoded,known)
  for mutate in [lambda v:v['owners'][1].update(order=v['owners'][0]['order']),lambda v:v['owners'][1].update(owner=v['owners'][0]['owner']),lambda v:v['owners'][0].update(order=65536),lambda v:v['shapes'][0].update(role=4),lambda v:v['shapes'][1].update(role=2),lambda v:v['sounds'][0].update(finish=1),lambda v:v['sounds'][0].update(profile=1),lambda v:v['sounds'][0].update(asset=1),lambda v:v['profiles'][0].update(bits=32)]:
   value=copy.deepcopy(self.data);mutate(value)
   with self.assertRaises(ValueError):d.encode(value,value['bank'])
 def test_review_and_unreviewed_source_rejected(self):
  p,ex=d.read()
  with tempfile.TemporaryDirectory(dir=ROOT/'build')as folder:
   name=Path(folder)/'review.json'
   for mutate in [lambda v:v.update(schema=2),lambda v:v.update(unknown=True),lambda v:v.update(clock_review_sha256='0'*64),lambda v:v['sources'].update({'LICENSE':'0'*64}),lambda v:v['graphics'][0].update(atlas_columns=0),lambda v:v['shapes'][0].update(role='unknown')]:
    value=copy.deepcopy(p);mutate(value);name.write_text(json.dumps(value),encoding='utf8')
    with self.assertRaises(ValueError):d.read(name)
  class Changed:
   def __getattr__(self,name):return getattr(ex,name)
   def doc(self,name):
    raw=ex.text(name)
    if name=='Nodes/Overworld/Objects/Small Key.tscn':raw=raw.replace('[node name="Sprite" type="Sprite" parent="."]','[node name="Sprite" type="Sprite" parent="."]\nrotation = 0.2',1)
    return d.clocks.Document(name,raw)
  with mock.patch.object(d,'read',return_value=(p,Changed())):
   with self.assertRaises(ValueError):d.source_model()
 def test_shared_core_build_lists(self):
  source='runtime/dungeon_key_media.cpp'
  self.assertIn(source,(ROOT/'CMakeLists.txt').read_text('utf8'));self.assertIn(source,(ROOT/'platform/ctr/Makefile').read_text('utf8'));self.assertNotIn('3ds.h',(ROOT/source).read_text('utf8'))
if __name__=='__main__':unittest.main()
