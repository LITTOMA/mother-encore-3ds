"""Source and rejection tests for the external seven-phrase Doll program.

The earlier Lamp command expectation is preserved. v3 adds two movement tables,
post-battle metadata, House dialogue spans and explicit cross-pack references;
these new expectations describe the reviewed source expansion, not new goldens
chosen to hide a runtime trace mismatch.
"""
import copy,json,struct,tempfile,unittest
from pathlib import Path
from tools.native_content import ROOT,compile_ir,parse_pack,ContentError,NONE,OPCODES
from tools.doll_dialogue import decode,compile_dialogue,receipt,RECEIPT
from tools import native_house as house

class DollContentTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.ir=json.loads((ROOT/'content/native-opening.json').read_text());cls.h=json.loads((ROOT/'content/native-house.json').read_text());cls.pres=json.loads((ROOT/'content/native-house-presentation.json').read_text())
  cls.native=decode(json.loads((ROOT/'reports/doll-sequence/native-parser-animation.json').read_text()));cls.actions=compile_dialogue(cls.native['yaml'][0]);cls.s=cls.ir['sections']
 def reject(self,section,index,field,value):
  ir=copy.deepcopy(self.ir);ir['sections'][section][index][field]=value
  with self.assertRaises(ContentError):compile_ir(ir)
 def test_program_graph_and_original_handler_order(self):
  p=self.s['Program'][1];self.assertEqual(p['phrase_count'],7);self.assertEqual(p['command_count'],len(self.actions))
  a=[a['kind']for a in self.actions if a['phrase']==0]
  self.assertLess(a.index('TeleportActor'),a.index('SetActorDirection'));self.assertLess(a.index('SetActorDirection'),a.index('MoveActorPath'));self.assertLess(a.index('MoveActorPath'),a.index('AnimateActor'));self.assertLess(a.index('ChangeCamera'),a.index('ReturnCamera'))
  for phrase in (2,4):
   a=[a['kind']for a in self.actions if a['phrase']==phrase];self.assertEqual(a[0],'ShowDialogue');self.assertEqual(a[-1],'AwaitDialogue');self.assertNotIn('AwaitTimer',a)
  a=[a['kind']for a in self.actions if a['phrase']==6];self.assertLess(a.index('CallObjectDeferred'),a.index('SetTalker'));self.assertLess(a.index('SetFlag'),a.index('QueueBattle'));self.assertLess(a.index('QueueBattle'),a.index('AwaitTimer'));self.assertLess(a.index('AwaitTimer'),a.index('RequestBattle'))
  self.assertEqual([x['actor']for x in self.actions if x['kind']=='RestoreActor'],['Ninten','Mimmie'])
 def test_source_paths_preserve_waits_modes_and_moonwalk(self):
  self.assertEqual(len(self.s['MovementPath'][:6]),6);self.assertEqual(len(self.s['MovementEntry'][:14]),14)
  self.assertEqual([(p['entry_count'],p['flags'],p['animation_motion'],p['speed'])for p in self.s['MovementPath'][:6]],[(1,0,4,64),(4,1,65535,32),(4,3,4,32),(3,0,4,128),(1,1,65535,64),(1,1,65535,200)])
  p=self.s['MovementPath'][1];entries=self.s['MovementEntry'][p['first_entry']:p['first_entry']+p['entry_count']]
  self.assertEqual(entries,[dict(kind=0,vector=[0,16],duration=0),dict(kind=1,vector=[0,0],duration=1),dict(kind=0,vector=[0,16],duration=0),dict(kind=1,vector=[0,0],duration=1)])
 def test_native_animation_floats_and_original_sheet_offsets(self):
  emote=self.s['Clip'][self.s['ActorProfile'][3]['emote_clip']];keys=self.s['Key'][emote['first_key']:emote['first_key']+emote['key_count']]
  self.assertEqual(emote['flags'],24);self.assertEqual(emote['length'],self.native['exclamation']['length']);self.assertEqual([[k['time'],k['frame']]for k in keys],self.native['exclamation']['keys'])
  self.assertNotEqual(keys[0]['time'],.0666667)
  self.assertEqual([p['sprite_position']for p in self.s['ActorProfile'][2:4]],[[0,9],[0,9]])
  self.assertEqual([p['sprite_offset']for p in self.s['ActorProfile'][2:4]],[[0,-11],[0,-12]])
  self.assertEqual([p['flags']for p in self.s['ActorProfile'][2:4]],[0,1])
  self.assertEqual(self.s['ActorProfile'][3]['direction_count'],4)
  self.assertEqual({b['motion_state']for b in self.s['AnimationBinding']if b['actor_profile_index']==3},{0,4,5})
 def test_battle_and_house_links_keep_original_followup_without_winflag(self):
  b=self.s['Battle'][1];self.assertEqual(b['win_flag_index'],NONE);self.assertEqual(b['flags'],12)
  self.assertEqual(self.ir['strings'][b['win_cutscene_string']],'Podunk/cutscenes/doll_defeated')
  resource=self.s['Resource'][b['battle_resource_index']];self.assertEqual(resource['kind'],3);self.assertEqual(self.ir['strings'][resource['path_string']],'data/doll-entry.encbattle')
  self.assertEqual([n['room_actor_index']for n in self.h['npcs'][:3]],[5,3,2]);self.assertEqual(self.h['story_triggers'][0]['program_index'],1)
  text=[self.h['segments'][d['first_segment']]for d in self.h['dialogues'][1:3]]
  self.assertEqual([s['speaker']for s in text],['Mimmie','Mimmie']);self.assertEqual([s['tokens'][0]['text']for s in text],['Ahhhh!','Big brother! Help!']);self.assertTrue(all(s['voice']=='Audio/Sound effects/text/Kid.mp3'for s in text))
  ids={d['id']for d in self.h['dialogues']};self.assertTrue(all(c['target_index']in ids for c in self.s['Command']if c['opcode']==32))
 def test_new_schema_paths_reject_invalid_payload_and_ownership(self):
  for sec,i,k,v in [('MovementPath',0,'first_entry',NONE),('MovementPath',0,'entry_count',0),('MovementPath',0,'entry_count',17),('MovementPath',0,'flags',16),('MovementPath',0,'animation_motion',1),('MovementPath',0,'speed',0),('MovementPath',1,'first_entry',0),('MovementEntry',0,'kind',2),('MovementEntry',0,'duration',1),('MovementEntry',2,'duration',0),('MovementEntry',2,'vector',[1,0]),('Battle',1,'win_flag_index',9999),('Battle',1,'battle_resource_index',0),('Battle',1,'win_cutscene_string',NONE),('Binding',2,'target_index',2),('ActorProfile',3,'direction_count',8)]:
   with self.subTest(section=sec,field=k,value=v):self.reject(sec,i,k,v)
  ir=copy.deepcopy(self.ir);ir['sections']['MovementEntry'].append(dict(kind=0,vector=[0,0],duration=0))
  with self.assertRaises(ContentError):compile_ir(ir)
 def test_new_opcodes_reject_wrong_targets_actors_and_extras(self):
  for name,field,value in [('SetActorDirection','vector',[0,0]),('TeleportActor','actor_index',65535),('MoveActorPath','target_index',NONE),('MoveActorPath','actor_index',2),('ReturnCamera','duration',-1),('SetFlag','value',2),('SetFlag','target_index',NONE),('ShowDialogue','target_index',0),('ShowDialogue','actor_index',65535),('AwaitDialogue','value',1)]:
   index=next(i for i,c in enumerate(self.s['Command'])if c['opcode']==OPCODES.index(name))
   with self.subTest(command=name,field=field):self.reject('Command',index,field,value)
 def test_delayed_keys_and_motion_contract_fail_closed(self):
  c=self.s['ActorProfile'][3]['emote_clip'];self.reject('Clip',c,'flags',8);self.reject('Clip',0,'flags',16);self.reject('Clip',c,'flags',32)
  index=next(i for i,b in enumerate(self.s['AnimationBinding'])if b['actor_profile_index']==3);self.reject('AnimationBinding',index,'motion_state',1);self.reject('AnimationBinding',0,'motion_state',5)
 def test_house_cross_pack_and_duplicate_mappings_reject(self):
  for collection,index,key,value in [('npcs',1,'room_actor_index',50),('npcs',2,'room_actor_index',3),('story_triggers',0,'program_index',50),('dialogues',1,'id',99)]:
   ir=copy.deepcopy(self.h);ir[collection][index][key]=value
   with self.subTest(collection=collection,key=key),self.assertRaises(house.ContentError):house.parse_pack(house.encode(house.lower(ir,self.pres),version=ir['schema']))
 def test_unknown_source_handlers_do_not_silently_drop(self):
  for phrase,key,value in [('0','actorsvisible',{'doll':False}),('0','showbox',True),('0','goto','2'),('6','setflags','doll_defeated')]:
   doc=copy.deepcopy(self.native['yaml'][0]);doc[phrase][key]=value
   with self.subTest(phrase=phrase,key=key),self.assertRaises(ValueError):compile_dialogue(doc)
  doc=copy.deepcopy(self.native['yaml'][0]);doc['1']['actorsmove']['mimmie']['movement'].append({'shake':{'x':1}})
  with self.assertRaises(ValueError):compile_dialogue(doc)
 def test_native_receipt_fingerprint_rejects_forged_animation(self):
  with tempfile.TemporaryDirectory()as td:
   class Inputs:
    root=Path(td)
    def document(self,name):return json.loads((self.root/name).read_text())
   ex=Inputs();path=ex.root/RECEIPT;path.parent.mkdir(parents=True)
   data=json.loads((ROOT/RECEIPT).read_text());data['animations'][2]['Float']['keys'][0][1]=99
   path.write_text(json.dumps(data));review=ex.root/'reports/doll-sequence/source-review.json';review.write_bytes((ROOT/'reports/doll-sequence/source-review.json').read_bytes())
   with self.assertRaisesRegex(ValueError,'Changed unreviewed native Doll receipt'):receipt(ex)

if __name__=='__main__':unittest.main()
