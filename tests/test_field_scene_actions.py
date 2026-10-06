"""Manual source/compiler negatives; no auto registration/execution."""
import copy,sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools import field_scene_actions as actions
class FieldSceneActions(unittest.TestCase):
 def test_source(self):
  d=actions.load();self.assertEqual(sum(b['kind']==1 for b in d['bindings']),14);self.assertEqual(sum(b['kind']==2 for b in d['bindings']),4);self.assertEqual(actions.encode(d),actions.PACK.read_bytes());self.assertTrue(any(r['kind']==2 and not r['has_collision_properties']for r in d['references']))
 def test_unreviewed_source(self):
  d=actions.load()
  for mutate in [lambda x:x.update(schema=2),lambda x:x['bindings'][0].update(id=0),lambda x:x['references'][0]['local_position'].__setitem__(0,float('nan')),lambda x:x.update(deferred_method='unknown'),lambda x:x['connections'][0].update(method='unknown'),lambda x:x['engine'][0].update(sha256='0'*64),lambda x:x['bindings'][0].update(parent_id=0)]:
   bad=copy.deepcopy(d);mutate(bad)
   with self.assertRaises(ValueError):actions.validate(bad)
if __name__=='__main__':unittest.main()
