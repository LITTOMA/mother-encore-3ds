"""Manual-only source negative cases; not registered or executed."""
import copy,unittest
from tools import field_door_npc as s
class SourceCases(unittest.TestCase):
 def test_negative_sources(self):
  good=s.load()
  for mutate in [lambda d:d.update(schema=2),lambda d:d['timer'].update(process_pause=False),lambda d:d['timer'].update(strict_negative=False),lambda d:d['timer'].update(seconds=0),lambda d:d['bindings'][0].update(groups=[['flag']]),lambda d:d['bindings'][0]['audio'].update(bus='invented'),lambda d:d['bindings'][0]['programmes'][0].update(sha256='00'*32),lambda d:d['bindings'][0].update(half=[0,4]),lambda d:d['connections'][0].update(role=7)]:
   d=copy.deepcopy(good);mutate(d)
   with self.assertRaises(ValueError):s.validate(d)
if __name__=='__main__':unittest.main()
