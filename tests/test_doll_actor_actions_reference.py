import copy,json,unittest
from tools.reference_doll_actor_actions import fixture,ROOT
class DollActorReferenceTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.reference=json.loads((ROOT/'reports/doll-actor-reference-final/reference.json').read_text())
 def test_regeneration(self):self.assertEqual(fixture(self.reference),(ROOT/'tests/fixtures/doll_actor_actions_v0410.hpp').read_text())
 def reject(self,edit):
  r=copy.deepcopy(self.reference);edit(r)
  with self.assertRaises(ValueError):fixture(r)
 def test_schema(self):self.reject(lambda d:d.update(schema=2))
 def test_source(self):self.reject(lambda d:d['additional_sources'].update({'Data/Animations/4dir.yaml':'0'*64}))
 def test_function(self):self.reject(lambda d:d['symbols'].update(move_queue='0'*64))
 def test_engine(self):self.reject(lambda d:d['godot'].update(string='4.0'))
 def test_domain(self):self.reject(lambda d:d['cases'][0]['definition']['actions'][0].update(speed=33))
 def test_missing_frame(self):self.reject(lambda d:d['cases'][0]['frames'].pop())
 def test_nan(self):self.reject(lambda d:d['cases'][0]['frames'][0].update(position=[float('nan'),0]))
 def test_boolean(self):self.reject(lambda d:d['cases'][0]['frames'][0].update(moving=1))
 def test_bad_frame(self):self.reject(lambda d:d['cases'][0]['frames'][0].update(frame=4))
 def test_unknown(self):self.reject(lambda d:d['cases'][0]['frames'][0].update(unknown=0))
if __name__=='__main__':unittest.main()
