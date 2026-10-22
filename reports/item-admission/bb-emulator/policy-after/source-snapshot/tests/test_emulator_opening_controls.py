import copy,hashlib,json,unittest
from pathlib import Path
from tools import field_menus,run_emulator_semantics as runner
from tools.extract_battle_entry import Extractor

ROOT=Path(__file__).resolve().parents[1]
REPORT=ROOT/'reports/opening-emulator-controls/ordinary0f-1'
NAMES=('opening-reward-handoff','opening-phone-handoff','opening-field-exit')

class OpeningControlContract(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.ir=field_menus.load();cls.run_button=cls.ir['button_names'][4]
  cls.routes={name:json.loads((ROOT/'tests/emulator'/f'{name}.json').read_bytes())for name in NAMES}
 def test_scenarios_validate_and_use_checked_factory_run(self):
  self.assertEqual((ROOT/'romfs'/field_menus.PACK).read_bytes(),field_menus.encode(self.ir))
  for name,route in self.routes.items():
   with self.subTest(name=name):
    runner.validate_scenario(route);step=next(v for v in route['steps']if v['name']=='sister_ram')
    self.assertEqual(step['buttons'],['UP',self.run_button])
 def test_ordinary_runup_precedes_source_enter_contact(self):
  for name,route in self.routes.items():
   with self.subTest(name=name):
    names=[v['name']for v in route['steps']];self.assertEqual(names.count('sister_runup_space'),1)
    self.assertLess(names.index('sister_runup_space'),names.index('sister_ram'))
    step=route['steps'][names.index('sister_runup_space')];self.assertEqual(step['buttons'],['DOWN'])
    self.assertEqual(step['wait'],dict(world_stage=0,paused=0));self.assertIn('player_y',step['hold_until'])
 def test_all_preexisting_outcomes_and_targets_remain_exact(self):
  for name,route in self.routes.items():
   before=json.loads((REPORT/'before'/f'{name}.json').read_bytes());after=copy.deepcopy(route['steps']);after=[v for v in after if v['name']!='sister_runup_space'];ram=next(v for v in after if v['name']=='sister_ram');old=next(v for v in before['steps']if v['name']=='sister_ram');ram['buttons']=old['buttons']
   with self.subTest(name=name):
    if name=='opening-field-exit':
     # Actual cold repetitions retained in item-admission/bb-emulator exposed
     # an unobserved one-frame turn. Admit this bounded normal-input strategy
     # only; the complete original observations and targets below stay exact.
     face=next(v for v in after if v['name']=='doll_face_left')
     self.assertEqual(face['buttons'],['LEFT']);self.assertEqual(face['frames'],1)
     self.assertEqual(face.pop('repeat_until'),face['wait']);self.assertEqual(face.pop('max_attempts'),8)
    self.assertEqual(after,before['steps']);self.assertEqual(route['projection'],before['projection'])
    self.assertEqual(route.get('position_tolerance'),before.get('position_tolerance'))
 def test_original_contact_condition_and_reviewed_identity(self):
  ex=Extractor(ROOT);path='Scripts/Main/Openable Door.gd';source=ex.data(path);text=source.decode()
  self.assertIn('func _on_Area2D_body_entered(body):',text)
  self.assertIn('if blocked and global.get_player().is_running() and global.get_player().get_direction().y == -1 and !_unlocked:',text)
  digest=hashlib.sha256(source).hexdigest()
  for route in self.routes.values():self.assertEqual(route['reference']['sources'][path],digest)

if __name__=='__main__':unittest.main()
