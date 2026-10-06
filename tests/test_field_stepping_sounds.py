"""Manual-only source admission negatives; not registered or executed."""
import copy,unittest
from tools import field_stepping_sounds as s
class SourceCases(unittest.TestCase):
 def test_negative_sources(self):
  good=s.load()
  for mutate in [lambda d:d.update(schema=2),lambda d:d['bindings'][0].update(entering_sound='invented'),lambda d:d['bindings'][0]['shapes'][0]['parts'].append([[0,0],[1,0],[0,0]]),lambda d:d['bindings'][0]['shapes'][0].update(order=99),lambda d:d['connections'][0].update(role=7)]:
   d=copy.deepcopy(good);mutate(d)
   with self.assertRaises(ValueError):s.validate(d)
if __name__=='__main__':unittest.main()
