"""Manual source/negative policy tests. No automatic CI/test registration."""
import copy,unittest
from tools import field_inventory as inventory
class SourceInventory(unittest.TestCase):
 def test_exact_stage(self):
  data=inventory.load();files=inventory.stage_files(inventory.ROOT/'romfs');self.assertEqual(len(files),2);self.assertEqual(len(data['policies']),19)
 def test_unknown_metadata_and_action(self):
  data=inventory.load();bad=copy.deepcopy(data);bad['approval_callback']=True
  with self.assertRaises(ValueError):inventory.validate(bad)
  bad=copy.deepcopy(data);bad['policies'][0]['actions']=[dict(function='run_gdscript',name='x',fail='x')]
  with self.assertRaises(ValueError):inventory.validate(bad)
 def test_unknown_owner_status_and_versions(self):
  data=inventory.load()
  for mutate in (lambda d:d.update(schema=99),lambda d:d['owners'][0].update(role=3),lambda d:d['statuses'][0]['receive_blocks'].append(dict(type='invoke_any',message='x')),lambda d:d['policies'][0].update(boost_order=[0]*7)):
   bad=copy.deepcopy(data);mutate(bad)
   with self.assertRaises(ValueError):inventory.validate(bad)
if __name__=='__main__':unittest.main()
