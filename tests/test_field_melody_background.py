"""Manual-only source negative admission cases; never auto-registered/executed."""
import copy,unittest
from tools import field_melody_background as s
class SourceCases(unittest.TestCase):
 def test_negative_sources(self):
  good=s.load()
  for mutate in [lambda d:d.update(schema=2),lambda d:d['shader_params'].update(barrel=True),lambda d:d['shader_params'].update(compression_amplitude=[0,1]),lambda d:d['vertical'].__setitem__(6,0),lambda d:d['clip']['keys'][1].update(time=0),lambda d:d['clip']['keys'][0].update(color=[float('nan'),0,0,1]),lambda d:d['asset'].update(prepared=[16,239]),lambda d:d['bindings'][0].update(animation_ready=0),lambda d:d.update(rect=[0,0,1,1])]:
   d=copy.deepcopy(good);mutate(d)
   with self.assertRaises(ValueError):s.validate(d)
if __name__=='__main__':unittest.main()
