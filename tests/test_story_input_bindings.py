import copy,json,tempfile,unittest
from pathlib import Path
from tools.story_input_bindings import load
ROOT=Path(__file__).resolve().parents[1]
class StoryInputBindings(unittest.TestCase):
 def prepare(self):
  t=tempfile.TemporaryDirectory();self.addCleanup(t.cleanup);root=Path(t.name);(root/'content').mkdir()
  for name in ('story-input-bindings.json','native-field-equipment.json','pillow-input.json'):
   (root/'content'/name).write_bytes((ROOT/'content'/name).read_bytes())
  return root
 def test_current_control_labels(self):
  self.assertEqual(load(self.prepare()),{'ui_select':'START','ui_accept':'A','ui_toggle':'B'})
 def test_crosswired_valid_mask_is_rejected(self):
  root=self.prepare();p=root/'content/story-input-bindings.json';d=json.loads(p.read_text())
  d['bindings'][0]['parameter'],d['bindings'][1]['parameter']=d['bindings'][1]['parameter'],d['bindings'][0]['parameter']
  d['bindings'][0]['mask'],d['bindings'][1]['mask']=d['bindings'][1]['mask'],d['bindings'][0]['mask'];p.write_bytes(json.dumps(d).encode())
  with self.assertRaises(ValueError):load(root)
 def test_unknown_or_missing_action_is_rejected(self):
  for action in ('unknown','ui_accept'):
   with self.subTest(action=action):
    root=self.prepare();p=root/'content/story-input-bindings.json';d=json.loads(p.read_text());d['bindings'][0]['action']=action;p.write_bytes(json.dumps(d).encode())
    with self.assertRaises(ValueError):load(root)
