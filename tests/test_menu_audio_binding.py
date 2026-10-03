import json,sys,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.menu_audio_binding import source_sound
from tools import dialogue_choice_assets,save_menu_assets
class MenuAudioBindingTests(unittest.TestCase):
 def test_compiled_resources_match_actual_bank_sources(self):
  bank=json.loads((ROOT/'content/native-audio.json').read_text())
  sources={a['source_path'].removeprefix('res://')for a in bank['assets']}
  for module,path in [(dialogue_choice_assets,'content/native-dialogue-choices.json'),(save_menu_assets,'content/native-save-menu.json')]:
   recipe=json.loads((ROOT/path).read_text());blob=module.encode(recipe)
   for name in recipe['sounds']:
    path=source_sound(name);self.assertIn(path,sources);self.assertIn(path.encode(),blob)
 def test_unknown_and_unsafe_names_rejected(self):
  for name in ['not_a_real_sound','../cursor1','res://cursor1','']:
   with self.subTest(name=name),self.assertRaises(ValueError):source_sound(name)
if __name__=='__main__':unittest.main()
