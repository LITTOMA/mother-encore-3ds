"""Manual-only source compiler negatives, never registered or auto-run."""
import copy,unittest
from tools import field_dialogue_lifecycle as p
class Cases(unittest.TestCase):
 def test_original_source(self):
  d=p.load();self.assertEqual(p.encode(d)[:8],b'ENCDLIF1');self.assertEqual(len(d['nodes']),12)
 def test_unknown_operation(self):
  d=copy.deepcopy(p.load());d['steps'][0]['kind']='UnknownGodotMethod'
  with self.assertRaises(ValueError):p.encode(d)
 def test_unknown_stage(self):
  d=copy.deepcopy(p.load());d['steps'][0]['stage']='FakeReady'
  with self.assertRaises(ValueError):p.encode(d)
 def test_outside_native_node(self):
  d=copy.deepcopy(p.load());d['steps'][0]['role']=13
  with self.assertRaises(ValueError):p.encode(d)
 def test_unreviewed_close_track(self):
  d=copy.deepcopy(p.load());d['close_animation']['tracks'][0]['type']='method'
  with self.assertRaises(ValueError):p.encode(d)
if __name__=='__main__':unittest.main()
