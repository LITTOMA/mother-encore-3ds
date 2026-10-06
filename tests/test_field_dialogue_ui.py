"""Manual-only source codec cases. No automated registration or execution."""
import copy,unittest
from tools import field_dialogue_ui as ui
class DialogueUiSourceCases(unittest.TestCase):
 def test_source_native_closure(self):
  data=ui.load();self.assertEqual(sum(n['kind']!=0 for n in data['nodes']),20)
  self.assertEqual(len(data['nodes']),47);self.assertEqual(len(data['controls']),17)
  self.assertEqual(ui.encode(data)[:8],b'ENCDUI01')
 def test_unknown_source_kind(self):
  data=copy.deepcopy(ui.load());data['nodes'][0]['kind']=2
  with self.assertRaises(ValueError):ui.validate(data)
 def test_unknown_animation_track(self):
  data=copy.deepcopy(ui.load());data['animations'][0]['clip']['tracks'][0]['type']='method'
  with self.assertRaises(ValueError):ui.encode(data)
 def test_nonfinite_tuning(self):
  data=copy.deepcopy(ui.load());data['display'][0]=float('nan')
  with self.assertRaises(ValueError):ui.encode(data)
if __name__=='__main__':unittest.main()
