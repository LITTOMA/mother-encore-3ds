"""Manual source/schema checks; not part of automatic builds."""
import copy,unittest,tempfile
from pathlib import Path
from tools import present_sparkles as p
class PresentSparklesTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.ir=p.load()
 def test_actual_source_pack(self):
  self.assertEqual((p.ROOT/p.PACK).read_bytes(),p.encode(self.ir));self.assertEqual(len(self.ir['animation']['frames']),40);self.assertEqual(self.ir['animation']['random_range'],[0,47]);self.assertTrue(self.ir['binding']['ready_before_parent']);self.assertEqual(self.ir['binding']['parent'],'Objects/Present1')
 def test_stages_the_actual_texture_dependency(self):
  files=p.stage_files(p.ROOT/'romfs');texture=Path(self.ir['resource']['output'])
  self.assertEqual(set(files),{Path('data/house.encsparkles'),texture})
  self.assertEqual(files[texture],(p.ROOT/'romfs'/texture).read_bytes())
 def test_missing_and_changed_staged_texture_rejected(self):
  with tempfile.TemporaryDirectory() as temporary:
   root=Path(temporary);(root/'data').mkdir();(root/'data/house.encsparkles').write_bytes(p.encode(self.ir))
   with self.assertRaises(OSError):p.stage_files(root)
   texture=root/self.ir['resource']['output'];texture.parent.mkdir(parents=True);texture.write_bytes(b'changed')
   with self.assertRaises(ValueError):p.stage_files(root)
 def test_reject_unreviewed_schema(self):
  for key,value in [('commit','0'*40),('schema',2),('engine_reference',{})]:
   x=copy.deepcopy(self.ir);x[key]=value
   with self.assertRaises(ValueError):p.validate(x)
 def test_frame_and_clock_negatives(self):
  for key,value in [('frames',[[0,0,100000,7]]),('random_range',[-1,47]),('speed',float('nan')),('loop',False),('serialized_frame',999),('pixel_snap',False)]:
   x=copy.deepcopy(self.ir);x['animation'][key]=value
   with self.assertRaises(ValueError):p.validate(x)
 def test_unknown_binding_or_resource(self):
  x=copy.deepcopy(self.ir);x['binding']['ready_before_parent']=False
  with self.assertRaises(ValueError):p.validate(x)
  x=copy.deepcopy(self.ir);x['resource']['output']='../other.t3x'
  with self.assertRaises(ValueError):p.validate(x)
if __name__=='__main__':unittest.main()
