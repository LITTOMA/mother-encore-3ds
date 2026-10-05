"""Manual-only real source/negative producer coverage; no automatic registration."""
import copy,importlib.util,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('field_psi',ROOT/'tools/field_psi.py');tool=importlib.util.module_from_spec(spec);spec.loader.exec_module(tool)
class FieldPsiSourceTests(unittest.TestCase):
 def test_current_source(self):
  d=tool.load();self.assertEqual(d['commit'],tool.PIN);self.assertEqual(len(d['order']),5);self.assertEqual(sum(s['operation']==1 for s in d['skills']),1);self.assertEqual(sum(s['operation']==2 for s in d['skills']),1);self.assertEqual(tool.build(),d)
 def test_unknown_schema_capability_and_numeric(self):
  d=tool.load()
  for mutate in [lambda x:x.update(schema=99),lambda x:x.update(commit='0'*40),lambda x:x['skills'][0].update(operation=99),lambda x:x['layouts'][0].__setitem__(0,float('nan')),lambda x:x['skills'][0].update(id=0)]:
   bad=copy.deepcopy(d);mutate(bad)
   with self.assertRaises((ValueError,KeyError,TypeError)):tool.validate(bad)
 def test_checked_binary(self):
  d=tool.load();raw=tool.encode(d);self.assertEqual(raw[:8],b'ENCPSI01');self.assertEqual(len(raw),int.from_bytes(raw[12:16],'little'));self.assertNotIn(b'json',raw[:64])
 def test_no_debug_learning_or_keyboard_shortcut(self):
  d=tool.load();self.assertIn('source Ninten level2',d['unsupported'][2]);self.assertIn('Scripts/UI/Pausemenu.gd',d['sources']);self.assertIn('Scripts/Main/party/Player.gd',d['sources']);self.assertEqual(d['telepathy_skill'],'telepathy')
if __name__=='__main__':unittest.main()
