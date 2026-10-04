"""Actual pinned UI bindings and production checked converters; no M0 fixtures."""
from pathlib import Path
import copy,json,sys,tempfile,unittest
from unittest.mock import patch
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import ui_presentation_bindings as binding,continue_assets,save_menu_assets,house_button_prompt_assets

class UiPresentationBindings(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.modules={'continue':continue_assets,'save':save_menu_assets,'prompts':house_button_prompt_assets}
  cls.recipes={name:json.loads((ROOT/'content'/f'{name}-presentation-bindings.json').read_text(encoding='utf8')) for name in cls.modules}
 def checked(self,name,data):
  module=self.modules[name];original=self.recipes[name]
  with tempfile.TemporaryDirectory(dir=ROOT/'build') as temp:
   recipe=Path(temp)/'binding.json';recipe.write_text(json.dumps(data),encoding='utf8')
   return binding.load(f'{name}-presentation-bindings.json',module.BINDING_SCHEMA,ROOT,recipe,expected_checks=len(original['checks']),expected_contracts=len(original['contracts']))
 def rejected(self,name,mutate):
  data=copy.deepcopy(self.recipes[name]);mutate(data)
  with self.assertRaises((ValueError,KeyError,IndexError,TypeError)):self.checked(name,data)
 def test_actual_source_and_default_pack_consumers(self):
  for name,module in self.modules.items():
   with self.subTest(name=name):
    self.checked(name,self.recipes[name])
    path=module.IR if name=='prompts' else module.RECIPE;data=json.loads(path.read_text(encoding='utf8'))
    if name=='prompts':module.verify(data)
    else:module.verify_recipe(data)
    self.assertEqual(module.encode(data),module.PACK.read_bytes())
    files=module.stage_files(ROOT/'romfs');self.assertTrue(files)
 def test_unknown_role_and_missing_role(self):
  for name in self.modules:
   self.rejected(name,lambda d:d['bindings'].update(unknown_role='unknown'))
   self.rejected(name,lambda d:d['bindings'].pop(next(iter(d['bindings']))))
 def test_unknown_schema_and_pin(self):
  self.rejected('continue',lambda d:d.update(schema=2))
  for name in self.modules:self.rejected(name,lambda d:d.update(schema=True))
  self.rejected('save',lambda d:d.update(commit='0'*40))
  self.rejected('prompts',lambda d:d.update(extra=True))
 def test_unknown_source_path_and_alias(self):
  for name in self.modules:
   self.rejected(name,lambda d:d['bindings'].update(source_0='../outside.png'))
   self.rejected(name,lambda d:d['bindings'].update(source_0='Graphics/UI/not-reviewed.png'))
   self.rejected(name,lambda d:d['bindings'].update(source_0=d['bindings']['source_1']))
   self.rejected(name,lambda d:d['bindings'].update(resource_root='../outside/'))
 def test_duplicate_missing_source_checks_and_contracts(self):
  for name in self.modules:
   self.rejected(name,lambda d:d['checks'].pop())
   self.rejected(name,lambda d:d['checks'].__setitem__(1,d['checks'][0]))
   self.rejected(name,lambda d:d['contracts'].pop())
   self.rejected(name,lambda d:d['contracts'].__setitem__(1,d['contracts'][0]))
 def test_source_fingerprint_and_semantics_change(self):
  for name in self.modules:
   self.rejected(name,lambda d:d['sources'].__setitem__(next(iter(d['sources'])),'0'*64))
   self.rejected(name,lambda d:d['contracts'][0].update(expected={'invented_property':1}))
 def test_unknown_source_selector_and_node(self):
  self.rejected('continue',lambda d:d['checks'][0].update(selector=['execute_source']))
  self.rejected('save',lambda d:d['bindings'].update(node_0='Unknown/Node'))
 def test_title_source_sheet_and_option_mapping(self):
  self.rejected('continue',lambda d:d['bindings']['sheets'][0].__setitem__(1,d['bindings']['sheets'][1][1]))
  self.rejected('continue',lambda d:d['bindings']['sheets'][0][2].__setitem__(0,4))
  self.rejected('continue',lambda d:d['bindings']['options'][0].__setitem__(0,d['bindings']['options'][1][0]))
 def test_prompt_source_preview_mapping_masks_and_door_offset(self):
  self.rejected('prompts',lambda d:d['bindings']['preview_rows'][0].__setitem__(1,d['bindings']['preview_rows'][1][1]))
  self.rejected('prompts',lambda d:d['bindings']['preview_rows'][0].__setitem__(2,2))
  self.rejected('prompts',lambda d:d['bindings']['choice_masks'].__setitem__(0,0))
  self.rejected('prompts',lambda d:d['bindings'].update(door_origin=33.))
 def test_save_source_palette_font_and_scroll_tuning(self):
  self.rejected('save',lambda d:d['bindings']['old_palette'].__setitem__(0,'000000'))
  self.rejected('save',lambda d:d['bindings']['font_recipe'].update(char_spacing=0))
  self.rejected('save',lambda d:d['bindings'].update(scroll=.3))
  self.rejected('save',lambda d:d['bindings']['icons'].__setitem__(0,'invented'))
  self.rejected('save',lambda d:d['bindings']['font_recipe'].update(columns=0))
 def test_independent_data_changes_real_continue_pack(self):
  module=continue_assets;recipe=copy.deepcopy(self.recipes['continue']);recipe['bindings']['background_color']=0xff010203
  with tempfile.TemporaryDirectory(dir=ROOT/'build') as temp:
   path=Path(temp)/'binding.json';path.write_text(json.dumps(recipe),encoding='utf8');load=binding.load
   with patch.object(module,'load_ui_bindings',lambda name,spec,project,**kw:load(name,spec,project,path,**kw)):
    source=module.extract()[1];data=json.loads(module.RECIPE.read_text(encoding='utf8'))
    data.update(source);module.verify_recipe(data);changed=module.encode(data)
    self.assertEqual(int.from_bytes(changed[24:28],'little'),0xff010203)
    self.assertNotEqual(changed,module.PACK.read_bytes())
  module.checked_bindings()
 def test_duplicate_json_key(self):
  module=continue_assets
  with tempfile.TemporaryDirectory(dir=ROOT/'build') as temp:
   file=Path(temp)/'bad.json';file.write_text('{"schema":1,"schema":1}',encoding='utf8')
   with self.assertRaisesRegex(ValueError,'Duplicate'):binding.load('ignored',module.BINDING_SCHEMA,ROOT,file)

if __name__=='__main__':unittest.main()
