"""Manual source Present coverage; not registered or run during development."""
import copy,sys,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import field_present as p
class PresentSource(unittest.TestCase):
 def test_actual_full_scene(self):
  d=p.load();self.assertEqual(len(d['holders']),16);self.assertEqual(len(d['pending_dropped']),3);self.assertTrue(all(h['object_flag'] and h['flag']==d['scene_name']+'/'+h['node'].rsplit('/',1)[-1]for h in d['holders']));self.assertTrue(all(h['collision_layer']==517 for h in d['holders']));self.assertEqual((ROOT/'romfs/data/podunk-presents.encpresent').read_bytes(),p.encode(d))
 def test_negative_identity_geometry_and_source_versions(self):
  for change in [lambda d:d.update(schema=99),lambda d:d.update(commit='0'*40),lambda d:d['holders'][0].update(id=0),lambda d:d['holders'][0].update(collision_layer=1),lambda d:d['holders'][0]['position'].__setitem__(0,float('nan')),lambda d:d['clips'][0]['keys'][0].update(role=99)]:
   d=copy.deepcopy(p.load());change(d)
   with self.assertRaises((ValueError,TypeError,KeyError)):p.validate(d)
 def test_raw_dropped_is_pending(self):
  d=p.load();self.assertTrue(all(h['script']==p.DROPPED_SCRIPT for h in d['pending_dropped']));self.assertEqual({h['overrides']['item']for h in d['pending_dropped']},{'Shovel','EagleFeather','FavFood'})
if __name__=='__main__':unittest.main()
