import copy,json,tempfile,unittest,subprocess,sys
from pathlib import Path
from unittest.mock import patch
from tools import battle_entry_bindings as b
from tools import native_battle as n
from tools.extract_battle_entry import Extractor

ROOT=n.ROOT
class BattleEntryBindingsTest(unittest.TestCase):
 def test_actual_ordinary_command_line_from_build_directory(self):
  result=subprocess.run([sys.executable,str(ROOT/'tools/native_battle.py'),'verify'],cwd=ROOT/'build',capture_output=True,text=True)
  self.assertEqual(result.returncode,0,result.stdout+result.stderr)
  actual=json.loads((ROOT/'content/native-battle.json').read_text(encoding='utf8'));actual['entry_bindings']['sha256']='0'*64
  with tempfile.TemporaryDirectory(dir=ROOT/'build')as directory:
   path=Path(directory)/'bad.json';path.write_text(json.dumps(actual),encoding='utf8')
   result=subprocess.run([sys.executable,str(ROOT/'tools/native_battle.py'),'verify','--input',str(path)],cwd=directory,capture_output=True,text=True)
   self.assertNotEqual(result.returncode,0);self.assertIn('BATTLE CONTENT ERROR',result.stderr)
 @classmethod
 def setUpClass(cls):cls.recipe=b.read(b.ROOT/b.IR)
 def rejected(self,change):
  r=copy.deepcopy(self.recipe);change(r)
  with self.assertRaises((ValueError,KeyError,TypeError,IndexError)):b.load(Extractor(ROOT),r)
 def test_actual_original_entry_extraction_checked_pack(self):
  from tools import doll_entry_asset,pillow_entry_asset
  for name,extract in [('native-battle',lambda:Extractor(ROOT).build()),('doll-entry',doll_entry_asset.extract),('pillow-entry',pillow_entry_asset.extract)]:
   old=json.loads((ROOT/'content'/str(name+'.json')).read_text(encoding='utf8'));actual=extract();self.assertEqual({k:v for k,v in actual.items()if k not in('entry_bindings','sources')},{k:v for k,v in old.items()if k not in('entry_bindings','sources')})
   n.verify_sources(actual);assets=json.loads((ROOT/actual['presentation']['asset_receipt_path']).read_text(encoding='utf8'));blob=n.encode(n.lower(actual,assets,(ROOT/'romfs/data/opening.encroom').read_bytes()),actual['commit']);n.parse_sections(blob)
   self.assertEqual(blob,(ROOT/'romfs/data'/str('opening.encbattle'if name=='native-battle'else name+'.encbattle')).read_bytes())
 def test_schema_pin_missing_unknown_duplicate(self):
  for value in [None,0,2,True,'1']:self.rejected(lambda r:r.update(schema=value))
  self.rejected(lambda r:r.update(commit='0'*40));self.rejected(lambda r:r.update(unknown=1))
  self.rejected(lambda r:r['templates']['party_transition'].update(unknown='ignored'))
  self.rejected(lambda r:r['sources'].__setitem__(next(iter(r['sources'])),'0'*64))
  with tempfile.TemporaryDirectory(dir=ROOT/'build')as d:
   p=Path(d)/'duplicate.json';p.write_text('{"schema":1,"schema":1}',encoding='utf8')
   with self.assertRaises(ValueError):b.read(p)
 def test_animation_binding_source_resource_and_order(self):
  self.rejected(lambda r:r['animations'].pop());self.rejected(lambda r:r['animations'][1].update(resource_id=73));self.rejected(lambda r:r['animations'][1].update(player_node='AnimAction'))
  self.rejected(lambda r:r['animations'][1].update(alias=r['animations'][0]['alias']))
  self.rejected(lambda r:r['scene_layout'].append(r['scene_layout'][0]));self.rejected(lambda r:r['plate_layout'].append('Unknown'))
 def test_parameter_source_selector_and_coverage(self):
  self.rejected(lambda r:r['bindings'].pop('party_transition/jump_duration'))
  self.rejected(lambda r:r['templates']['party_transition'].update(jump_duration=.7))
  self.rejected(lambda r:r['templates']['party_transition']['arrival_quake_steps'][1].update(to=-8))
  self.rejected(lambda r:r['templates']['menu'].update(cursor_size=[20,10]))
  self.rejected(lambda r:r['templates']['party_transition'].update(y_up_transition='LINEAR'))
  self.rejected(lambda r:r['templates']['menu'].update(move_sound_key='unknown'))
  self.rejected(lambda r:r['templates']['derived_layout'].update(plate_position=[127,132]))
  self.rejected(lambda r:r['bindings'].__setitem__('party_transition/jump_duration',['unknown']))
  self.rejected(lambda r:r['bindings'].__setitem__('party_transition/jump_duration',['get','party_transition/jump_duration']))
  self.rejected(lambda r:r['bindings'].__setitem__('party_transition/jump_duration',['div',1,0]))
  self.rejected(lambda r:r['bindings'].__setitem__('party_transition/jump_duration',['node','../bad','Sprite','frame']))
 def test_menu_action_order_missing_duplicate_node_and_alias(self):
  self.rejected(lambda r:r['menu'].pop());self.rejected(lambda r:r['menu'].reverse());self.rejected(lambda r:r['menu'][1].update(node=r['menu'][0]['node']))
  self.rejected(lambda r:r['menu'][1].update(source_constant='ACTION_RUN'));self.rejected(lambda r:r['menu'][1].update(id='Unknown'));self.rejected(lambda r:r['menu'][1].update(icon_asset='unknown'))
 def test_plate_digit_node_order_namespace(self):
  self.rejected(lambda r:r['plate']['stats'].reverse());self.rejected(lambda r:r['plate']['stats'][0]['digits'].pop());self.rejected(lambda r:r['plate']['stats'][0]['digits'][0].update(node='Name'))
  self.rejected(lambda r:r['plate']['stats'][0]['digits'][0].update(power=1));self.rejected(lambda r:r['compiler'].update(plate_depths=[40]))
 def test_actual_source_semantic_mutation(self):
  for source,method in [('Scripts/UI/Battle/BattleSystem.gd','_jump_player_to_partyinfo'),('Scripts/UI/Battle/PartyInfoPlate.gd','quake'),('Scripts/UI/cursor.gd','set_cursor_from_index')]:
   ex=Extractor(ROOT);original=ex.text;text=original(source);before=self.recipe['source_contracts'][source][method];self.assertIn(before,text)
   with patch.object(ex,'text',lambda p:text.replace(before,before.replace('tween_property','unsupported_tween',1),1)if p==source else original(p)):
    with self.assertRaises(ValueError):b.load(ex,self.recipe)
 def test_checked_compiler_rejects_source_binding_projection_and_alias(self):
  actual=Extractor(ROOT).build();assets=json.loads((ROOT/actual['presentation']['asset_receipt_path']).read_text(encoding='utf8'))
  for mutate in [lambda r:r['presentation']['derived_layout'].update(plate_position=[120,132]),lambda r:r['menu'].update(cursor_repeat_delay_seconds=.1),lambda r:r.update(entry_bindings={'path':b.IR,'sha256':'0'*64})]:
   bad=copy.deepcopy(actual);mutate(bad)
   with self.assertRaises(ValueError):n.lower(bad,assets,(ROOT/'romfs/data/opening.encroom').read_bytes())
  bad=copy.deepcopy(assets);next(r for r in bad['resources']if r['name']==self.recipe['menu'][1]['asset_alias'])['source']='Graphics/UI/Battle/bashIcon.png'
  with self.assertRaises(ValueError):n.lower(actual,bad,(ROOT/'romfs/data/opening.encroom').read_bytes())
 def test_external_compiler_policy_changes_actual_binary_field(self):
  actual=Extractor(ROOT).build();assets=json.loads((ROOT/actual['presentation']['asset_receipt_path']).read_text(encoding='utf8'));room=(ROOT/'romfs/data/opening.encroom').read_bytes();before=n.lower(actual,assets,room)
  changed=copy.deepcopy(self.recipe);changed['compiler']['background_margins'][0]+=1
  with tempfile.TemporaryDirectory(dir=ROOT/'build')as d:
   root=Path(d);p=root/b.IR;p.parent.mkdir();p.write_text(json.dumps(changed),encoding='utf8')
   with patch.object(b,'ROOT',root):
    actual['entry_bindings']['sha256']=b.binding_digest();after=n.lower(actual,assets,room)
  index=before['roles']['plate_bg'];self.assertEqual(before['layouts'][index][11][0]+1,after['layouts'][index][11][0]);self.assertNotEqual(n.encode(before,actual['commit']),n.encode(after,actual['commit']));n.parse_sections(n.encode(after,actual['commit']))

if __name__=='__main__':unittest.main()
