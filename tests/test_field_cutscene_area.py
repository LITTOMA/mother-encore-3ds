"""Manual source/producer negatives; not run or auto registered."""
import copy,sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools import field_cutscene_area as area
class CutsceneAreaSource(unittest.TestCase):
 def test_actual_source(self):
  d=area.load();self.assertEqual(len(d['bindings']),15);self.assertEqual(d['close'],[True,False,False,False]);self.assertEqual(d['pause'],[False,True,True]);self.assertEqual(len(area.encode(d)),area.PACK.stat().st_size)
 def test_version_identity_geometry_and_reference_rejection(self):
  d=area.load()
  for mutate in [lambda x:x.update(schema=2),lambda x:x['bindings'][0].update(id=0),lambda x:x['bindings'][0].update(centre=[float('nan'),0]),lambda x:x['bindings'][0].update(half_extents=[-1,1]),lambda x:x['bindings'][0].update(programme_sha256='0'*64),lambda x:x['close'].append(False),lambda x:x['connections'][0].update(method='unknown')]:
   bad=copy.deepcopy(d);mutate(bad)
   with self.assertRaises(ValueError):area.validate(bad)
if __name__=='__main__':unittest.main()
