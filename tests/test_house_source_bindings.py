"""Reviewed House source bindings, strict authoring and ordinary compiler gate."""
import copy,hashlib,json,os,shutil,sys,tempfile,unittest
from pathlib import Path
from unittest.mock import patch
ROOT=Path(os.environ.get('ENCORE_SOURCE_ROOT',Path(__file__).resolve().parents[1])).resolve()
sys.dont_write_bytecode=True;sys.path.insert(0,str(ROOT));sys.path.insert(0,str(ROOT/'tools'))
from tools import house_source_bindings as bindings,house_assets as assets,extract_house,native_house

class HouseSourceBindings(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.recipe=bindings.read(bindings.IR);cls.ir=bindings.read(ROOT/'content/native-house.json');cls.presentation=bindings.read(ROOT/'content/native-house-presentation.json')
 def reject(self,path,value):
  candidate=copy.deepcopy(self.recipe);cursor=candidate
  for key in path[:-1]:cursor=cursor[key]
  cursor[path[-1]]=value
  with self.assertRaises((ValueError,KeyError,TypeError,IndexError)):bindings.load(ROOT,candidate)
 def test_default_source_ir_presentation_and_pack_equivalence(self):
  self.assertEqual(extract_house.build(ROOT),self.ir)
  receipt=bindings.read(ROOT/'romfs/house-preview/source.json')
  self.assertEqual(assets.export_presentation(ROOT/'upstream/MOTHER-Encore',receipt['resources'],write=False),self.presentation)
  native_house.verify_sources(self.ir,self.presentation,ROOT)
  blob=native_house.encode(native_house.lower(self.ir,self.presentation,ROOT),self.ir['commit'],self.ir['schema'])
  self.assertEqual(blob,(ROOT/'romfs/data/opening.enchouse').read_bytes())
  assets.verify(ROOT/'upstream/MOTHER-Encore',ROOT/'romfs/house-preview')
 def test_closed_root_version_pin_unknown_and_duplicate_fields(self):
  for path,value in [(['schema'],2),(['schema'],True),(['commit'],'0'*40),(['kind'],'other'),(['nodes','door_target'],'../Position2D')]:
   with self.subTest(path=path,value=value):self.reject(path,value)
  candidate=copy.deepcopy(self.recipe);candidate['unknown']=0
  with self.assertRaises(ValueError):bindings.load(ROOT,candidate)
  with tempfile.TemporaryDirectory(dir=ROOT/'build',prefix='house-bindings-json-')as name:
   file=Path(name)/'duplicate.json';file.write_text('{"schema":1,"schema":1}',encoding='utf-8')
   with self.assertRaises(ValueError):bindings.read(file)
 def test_missing_source_and_changed_fingerprint(self):
  self.reject(['source_refs','door_script'],'missing.gd')
  self.reject(['sources',self.recipe['source_refs']['door_script']],'0'*64)
 def test_door_fade_and_geometry_selectors(self):
  for path,value in [(['nodes','door_target'],'Absent'),(['door','destination_offset'],[0,8]),(['door','shape_id'],2),(['door','fade_player'],'Absent'),(['door','fade_callback'],'other_callback'),(['door','fade_value_property'],'.:modulate:a'),(['door','fade_callback_node'],'Absent'),(['door','fade_value_track'],99),(['door','fade_callback_track'],True)]:
   with self.subTest(path=path):self.reject(path,value)
  self.reject(['same_scene_doors',1,'id'],self.recipe['same_scene_doors'][0]['id'])
  self.reject(['same_scene_doors',1,'source_path'],self.recipe['same_scene_doors'][0]['source_path'])
 def test_npc_preserved_room_identity_and_animation_profiles(self):
  for path,value in [(['npcs',0,'body_id'],10),(['npcs',0,'actor_stable_id'],999),(['npcs',0,'room_actor'],True),(['npcs',0,'actor_name'],'npc2'),(['npcs',0,'resource'],'unknown'),(['npcs',0,'shadow_resource'],''),(['npcs',1,'profile_id'],1),(['npc','default_direction'],[1,0]),(['npc','primary_segments'],3),(['presentation','profile_order'],[])]:
   with self.subTest(path=path):self.reject(path,value)
 def test_openable_source_masks_shape_roles_and_ram(self):
  for path,value in [(['openable','shapes','nonplayer'],3),(['openable','expected_masks','Normal'],[5,True]),(['openable','rows',0,'player_body_id'],True),(['openable','ram','strength'],True),(['openable','expected_masks','Action'],[7,7]),(['openable','base_y'],31),(['openable','rows',0,'player_body_id'],99),(['openable','rows',0,'resource'],'dialogue_box'),(['openable','ram','direction'],[1,0]),(['openable','ram','strength'],2),(['openable','ram','duration'],.3),(['openable','ram','required_y'],0)]:
   with self.subTest(path=path):self.reject(path,value)
 def test_literal_and_appended_house_mappings(self):
  phone_key=next(iter(self.recipe['phone']['text_ids']));pillow_key=next(iter(self.recipe['pillow']['text_ids']))
  for path,value in [(['text','rows',0,'id'],1),(['text','rows',0,'label'],'999'),(['text','rows',0,'parts'],2),(['text','rows',0,'table'],'menu_text'),(['text','default_parts'],1),(['phone','text_ids',phone_key],999),(['phone','actor'],4),(['phone','triggers',1],self.recipe['phone']['triggers'][0]),(['phone','triggers',0,'program'],'Podunk/absent'),(['pillow','text_ids',pillow_key],999),(['pillow','tutorial'],'Podunk/absent'),(['pillow','opened'],'Podunk/absent'),(['pillow','triggers',0],'Objects/Absent'),(['pillow','triggers',1],self.recipe['pillow']['triggers'][0])]:
   with self.subTest(path=path):self.reject(path,value)
 def test_presentation_adapter_limits_and_source_tween(self):
  for path,value in [(['presentation','name_sizing'],[21,.2,4]),(['presentation','name_sizing'],[20,.2,99]),(['presentation','display_reference'],[400,240,.5,1]),(['presentation','display_reference'],[320,180,1.1,1]),(['presentation','cursor_animation'],'Absent')]:
   with self.subTest(path=path):self.reject(path,value)
 def test_rehashed_source_semantics_still_fail_closed(self):
  with tempfile.TemporaryDirectory(dir=ROOT/'build',prefix='house-source-semantic-')as name:
   root=Path(name)
   for path in ['upstream.lock','compatibility/upstream-inventory.json','content/house-assets.json','content/native-opening.json']:
    target=root/path;target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(ROOT/path,target)
   for path in self.recipe['sources']:
    target=root/'upstream/MOTHER-Encore'/path;target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(ROOT/'upstream/MOTHER-Encore'/path,target)
   path=self.recipe['source_refs']['door_script'];file=root/'upstream/MOTHER-Encore'/path;original=file.read_text(encoding='utf-8');self.assertIn('Vector2(0, 7)',original);file.write_text(original.replace('Vector2(0, 7)','Vector2(0, 8)'),encoding='utf-8')
   candidate=copy.deepcopy(self.recipe);digest=hashlib.sha256(file.read_bytes()).hexdigest();candidate['sources'][path]=digest
   inventory=bindings.read(root/'compatibility/upstream-inventory.json');inventory['files'][path]['sha256']=digest;(root/'compatibility/upstream-inventory.json').write_text(json.dumps(inventory),encoding='utf-8')
   with self.assertRaisesRegex(ValueError,'destination source mismatch'):bindings.load(root,candidate)
 def test_ordinary_compiler_rejects_unbound_content_edits(self):
  candidate=copy.deepcopy(self.ir);candidate['doors'][0]['destination'][0]+=1
  with self.assertRaisesRegex(ValueError,'IR differs'):native_house.verify_sources(candidate,self.presentation,ROOT)
  presentation=copy.deepcopy(self.presentation);presentation['parameters']['DisplayReference'][2]=.75
  with self.assertRaisesRegex(ValueError,'presentation differs'):native_house.verify_sources(self.ir,presentation,ROOT)

if __name__=='__main__':unittest.main()
