import copy,hashlib,json,struct,sys,tempfile,unittest,zlib
from pathlib import Path
from unittest import mock
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import dungeon_key_clocks as d

class KeyClockTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.data,cls.blob,cls.proof=d.extract()
 def test_source_and_binary_roundtrip(self):
  self.assertEqual(d.parse(self.blob),self.data);self.assertEqual(len(self.data['profiles']),3);self.assertEqual(len(self.data['clips']),7);self.assertEqual(len(self.data['audios']),2);self.assertEqual(self.proof['resource_sha256'],hashlib.sha256(self.blob).hexdigest());self.assertEqual(len(self.data['metadata']['nodes']),6)
  position=[t for c in self.data['clips']for t in c['tracks']if t['kind']==3 and len(t['keys'])>1];self.assertEqual(len(position),1);self.assertAlmostEqual(position[0]['keys'][0]['ease'],.366021,places=6)
  edge=copy.deepcopy(self.data);edge['audios'][0]['pitch']=d.real(.01);self.assertEqual(d.parse(d.encode(edge)),edge)
  variant=copy.deepcopy(self.data);variant['profiles'][0]['mask']|=2;variant['profiles'][1]['mask']&=~4;variant['profiles'][1]['diagnostics']=0;variant['profiles'][2]['diagnostics']=0;self.assertEqual(d.parse(d.encode(variant)),variant)
 def test_frozen_native_runs_and_diagnostics(self):
  base=ROOT/'reports/dungeon-key-clocks';a=json.loads((base/'native-6/observations.json').read_text('utf8'));b=json.loads((base/'native-7/observations.json').read_text('utf8'));self.assertEqual(a,b);self.assertGreater(len(a['rows']),450)
  for name in ['native-6','native-7']:
   p=base/name;r=json.loads((p/'receipt.json').read_text('utf8'));self.assertEqual(r['resource_sha256'],hashlib.sha256(self.blob).hexdigest());self.assertEqual(r['process_exit'],17);self.assertEqual(len(r['diagnostics']),13);self.assertEqual(r['rows'],len(a['rows']));self.assertEqual(r['observations_sha256'],hashlib.sha256((p/'observations.json').read_bytes()).hexdigest());self.assertEqual(r['input_sha256'],hashlib.sha256((p/'input.json').read_bytes()).hexdigest())
  self.assertEqual(a['automatic_scheduler'],dict(stopped_before=True,stopped_after=True,finishing_writes=[True,False],six_idle_writes=[]))
  curve=[float(r['pose']['position'][1])for r in a['rows']if r['case']==1 and r['op']=='advance'];self.assertTrue(any(-11<v< -3 for v in curve))
 def test_all_truncations_rejected(self):
  for n in range(len(self.blob)):
   with self.assertRaises(ValueError,msg=str(n)):d.parse(self.blob[:n])
 def test_headers_directories_and_typed_records_rejected(self):
  b=self.blob;offsets=[struct.unpack_from('<I',b,64+i*16+4)[0]for i in range(7)];p,c,t,k,a=(offsets[i]for i in[2,3,4,5,6])
  positions=[8,20,24,28,32,52,56,60]+[64+i*16+j*4 for i in range(7)for j in range(4)]+[p+v for v in[0,4,8,12,16,20,28,44,48,56,60]]+[c+v for v in[0,4,8,12,16,20,24]]+[t+v for v in[0,4,8,12,16,20]]+[k+v for v in[0,4,8,12,20]]+[a+v for v in[0,4,44,48,52,56,60,64]]
  for at in positions:
   bad=bytearray(b);struct.pack_into('<I',bad,at,0xffffffff);struct.pack_into('<I',bad,16,zlib.crc32(bad[32:]));
   with self.assertRaises(ValueError,msg=str(at)):d.parse(bad)
 def test_unknown_semantic_values_rejected(self):
  for field,value in [('kind',4),('diagnostics',8),('diagnostics',5),('child',999),('mask',8)]:
   data=copy.deepcopy(self.data);data['profiles'][0][field]=value
   with self.assertRaises(ValueError):d.encode(data)
  for field,value in [('kind',9),('mode',3),('index',99)]:
   data=copy.deepcopy(self.data);data['clips'][0]['tracks'][0][field]=value
   with self.assertRaises(ValueError):d.encode(data)
 def test_review_fingerprint_and_fields_rejected(self):
  p,_=d.read()
  with tempfile.TemporaryDirectory(dir=ROOT/'build')as folder:
   path=Path(folder)/'review.json'
   for mutate in [lambda v:v.update(schema=2),lambda v:v.update(unknown=True),lambda v:v['sources'].update({'project.godot':'0'*64}),lambda v:v['warning_profile'].update(unused_signal=True),lambda v:v['warning_profile'].update(unused_signal=0)]:
    value=copy.deepcopy(p);mutate(value);path.write_text(json.dumps(value),encoding='utf8')
    with self.assertRaises(ValueError):d.read(path)
   value=copy.deepcopy(p);value['profiles'][0]['diagnostics']=0;path.write_text(json.dumps(value),encoding='utf8')
   with self.assertRaises(ValueError):d.extract(path)
 def test_unknown_source_track_method_root_and_node_rejected(self):
  p,ex=d.read();scene=next(x['scene']for x in p['profiles']if x['kind']=='door')
  class Source:
   def __init__(self,old,new):self.old=old;self.new=new
   def __getattr__(self,name):return getattr(ex,name)
   def text(self,name):return ex.text(name).replace(self.old,self.new,1)if name==scene else ex.text(name)
  for old,new in [('tracks/0/type = "value"','tracks/0/type = "bezier"'),('"method": "open"','"method": "unreviewed"'),('NodePath("..")','NodePath("Sprite")'),('[node name="Sprite" type="Sprite" parent="."]','[node name="Unknown" type="Sprite" parent="."]')]:
   self.assertIn(old,ex.text(scene))
   with mock.patch.object(d,'read',return_value=(p,Source(old,new))):
    with self.assertRaises((ValueError,KeyError)):d.extract()
 def test_build_lists_share_core_and_keep_3ds_out(self):
  source='runtime/dungeon_key_clocks.cpp'
  self.assertIn(source,(ROOT/'CMakeLists.txt').read_text('utf8'));self.assertIn(source,(ROOT/'platform/ctr/Makefile').read_text('utf8'));self.assertNotIn('3ds.h',(ROOT/source).read_text('utf8'))
if __name__=='__main__':unittest.main()
