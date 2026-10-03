import copy,json,sys,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import native_restore as restore

class RestoreCompilerTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.ir=restore.read(restore.IR)
 def test_source_recipe_and_staged_binary(self):
  self.assertEqual(restore.extract(),self.ir)
  self.assertEqual(restore.encode(self.ir),restore.PACK.read_bytes())
  self.assertEqual(restore.stage_files(ROOT/'romfs'),{Path('data/opening.encrestore'):restore.PACK.read_bytes()})
 def test_only_missing_npc_event_data_and_last_match_order(self):
  rows=self.ir['npc_event_positions'];self.assertEqual(len(rows),2)
  self.assertEqual([r['condition']['flag_name']for r in rows],['minnie_leave','doll_defeated'])
  self.assertEqual([r['position']for r in rows],[[32,368],[464,80]])
  self.assertEqual([r['source_path']for r in rows],['Objects/npc3']*2)
  house=restore.read(ROOT/'content/native-house.json');room=restore.read(ROOT/'content/native-opening.json')
  self.assertEqual(house['npcs'][1]['position'],[120,88]);self.assertEqual(house['npcs'][2]['position'],[40,40]);self.assertEqual(room['sections']['ActorInstance'][1]['position'],[496,390])
 def test_existing_flag_mechanisms_are_reused(self):
  room=restore.read(ROOT/'content/native-opening.json');house=restore.read(ROOT/'content/native-house.json')
  f=lambda i:room['strings'][room['sections']['Flag'][i]['name_string']]
  self.assertEqual([(f(c['flag_index']),c['expected_value'])for c in room['sections']['Condition']],[('poltergeist',0),('doll_melody',0)])
  b=[b for b in room['sections']['Binding']if b['kind']==9];self.assertEqual(len(b),1);self.assertEqual(f(b[0]['auxiliary_index']),'talked_to_dad');self.assertEqual(b[0]['target_index'],21)
  self.assertEqual(house['story_conditions'][:2],[dict(kind=1,flag='doll_melody',value=0),dict(kind=2,flag='doll_defeated',value=0)])
  self.assertNotIn('landmarks',self.ir)
 def test_all_source_music_areas_and_explicit_unsupported_area(self):
  rows=self.ir['music_areas'];self.assertEqual([r['source_path']for r in rows],['MusicArea','MusicArea2','MusicArea3','Poltergeist/MusicArea'])
  self.assertEqual([r['supported']for r in rows],[True,True,False,True]);self.assertEqual(rows[1]['volume_db'],-5)
  self.assertEqual(rows[-1]['conditions'][0]['flag_name'],'doll_melody');self.assertFalse(rows[-1]['conditions'][0]['expected_value'])
  self.assertEqual(rows[0]['extents'],[356,456]);self.assertEqual(rows[0]['fadein_seconds'],1);self.assertEqual(rows[0]['fadeout_seconds'],1.5)
 def test_all_registered_characters_and_frozen_inactive_inventory(self):
  rows=self.ir['inventory_load_order'];self.assertEqual([r['character_id']for r in rows],['','','ninten','ana','lloyd','teddy','pippi','canarychick','flyingman','eve'])
  self.assertEqual([r['rebuilds_inventory']for r in rows],[True]*7+[False]*3)
  self.assertEqual([len(r['projected_items'])for r in rows],[1,0,1,0,2,0,0,0,0,0])
  self.assertEqual([r['item_id']for r in rows[4]['projected_items']],['KickMeNote','GlassesCelluloid'])
  self.assertIn('not original missing-character fallback',self.ir['projection_policy'])
 def test_unknown_fields(self):
  for route in [(),('npc_event_positions',0),('npc_event_positions',0,'condition'),('music_areas',0),('music_areas',0,'conditions',0),('inventory_load_order',0),('inventory_load_order',0,'projected_items',0),('fingerprints','room')]:
   ir=copy.deepcopy(self.ir);obj=ir
   for key in route:obj=obj[key]
   obj['unknown']=1
   with self.subTest(route=route),self.assertRaises(ValueError):restore.encode(ir)
 def test_semantic_and_cross_pack_rejection(self):
  mutations=[(('schema',),2),(('uid_policy',),99),(('scene_id',),99),(('npc_event_positions',0,'npc_index'),99),(('npc_event_positions',0,'npc_id'),99),(('npc_event_positions',0,'actor_id'),99),(('npc_event_positions',0,'actor_index'),99),(('npc_event_positions',0,'body_id'),99),(('npc_event_positions',0,'source_path'),'Objects/npc'),(('npc_event_positions',0,'position'),[float('nan'),0]),(('npc_event_positions',0,'condition','flag_id'),999),(('npc_event_positions',0,'condition','flag_name'),'unknown'),(('npc_event_positions',0,'condition','expected_value'),2),(('music_areas',0,'room_resource_index'),999),(('music_areas',0,'room_resource_id'),999),(('music_areas',0,'resource_sha256'),'0'*64),(('music_areas',0,'extents'),[0,1]),(('music_areas',0,'volume_db'),float('inf')),(('music_areas',0,'fadein_seconds'),-1),(('music_areas',2,'supported'),True),(('inventory_load_order',4,'order_id'),2),(('inventory_load_order',4,'character_id'),'ninten'),(('inventory_load_order',4,'rebuilds_inventory'),False),(('inventory_load_order',0,'projected_items',0,'doses'),0),(('fingerprints','room','sha256'),'0'*64),(('fingerprints','house','bytes'),0)]
  for route,value in mutations:
   ir=copy.deepcopy(self.ir);obj=ir
   for key in route[:-1]:obj=obj[key]
   obj[route[-1]]=value
   with self.subTest(route=route),self.assertRaises(ValueError):restore.encode(ir)
 def test_recipe_is_source_checked(self):
  for key in ['sources','dependencies']:
   ir=copy.deepcopy(self.ir);ir[key][next(iter(ir[key]))]='0'*64
   with self.assertRaises(ValueError):restore.verify_recipe(ir)

if __name__=='__main__':unittest.main()
