"""Original eight-phrase post-win content, preserved identities and rejection paths."""
import copy, hashlib, json, tempfile, unittest
from pathlib import Path
from tools import doll_postwin as post, native_content as room, native_house as house
from tools.doll_dialogue import decode

def digest(value):return hashlib.sha256(json.dumps(value,sort_keys=True,separators=(',',':')).encode()).hexdigest()

class DollPostwinTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.ir=json.loads((room.ROOT/'content/native-opening.json').read_text());cls.s=cls.ir['sections'];cls.h=json.loads((room.ROOT/'content/native-house.json').read_text());cls.pres=json.loads((room.ROOT/'content/native-house-presentation.json').read_text())
  cls.native=decode(json.loads((room.ROOT/post.RECEIPT).read_text()));cls.doc=cls.native['yaml'][0]
  cls.actions=post.compile_dialogue(cls.doc,.5)
 def reject(self,mutate):
  ir=copy.deepcopy(self.ir);mutate(ir)
  with self.assertRaises(room.ContentError):room.compile_ir(ir)
 def test_native_parser_source_receipt(self):
  from tools.extract_native_content import Extractor
  self.assertEqual(post.receipt(Extractor()),self.native)
  self.assertEqual(len(self.native['dot']['keys']),10)
  self.assertNotEqual(self.native['dot']['keys'][1][0],.05)
 def test_original_handler_order_and_flag_notifications(self):
  a=[x['kind']for x in self.actions if x['phrase']==0]
  self.assertEqual(a,['BeginCutscene','BindActor','ActorPersistent','BindActor','ActorPersistent','BindActor','ActorPersistent','BindActor','ActorPersistent','YieldIdle','StartWait','OverworldBattleMusic','PlaySound','TeleportActor','SetActorDirection','SetActorDirection','MoveActorPath','TurnActor','JumpActor','AnimateActor','ChangeCamera','YieldIdle','ReturnCamera','SetFlag','SetFlag','AwaitTimer'])
  self.assertEqual([(x['phrase'],x['flag'],x['value'])for x in self.actions if x['kind']=='SetFlag'],[(0,'doll_defeated',1),(0,'poltergeist',0),(1,'pillow_attack',1)])
  source=(room.ROOT/'upstream/MOTHER-Encore/Scripts/UI/DialogueBox.gd').read_text()
  fields=['ovbattlemusic','soundeffect','talker','teleportactors','actorsdir','actorsmove','actorsturn','actorsshake','actorsjump','actorsanim','actorsemote','changecam','returncam','setflags','unsetflags']
  positions=[source.index('if _curr_phrase.has("'+field+'")')for field in fields]
  self.assertEqual(positions,sorted(positions))
  self.assertEqual(post.return_duration(source),.5)
 def test_manual_text_and_speed_alias(self):
  self.assertEqual([(x['actor'],x['value'],x['duration'])for x in self.actions if x['kind']=='JumpActor'],[('Doll',48,.42),('Doll',10,.3),('Mimmie',10,.2),('Mimmie',10,.2)])
  for phrase,identity in [(2,4),(7,5)]:
   a=[x for x in self.actions if x['phrase']==phrase]
   self.assertEqual((a[0]['kind'],a[0]['dialogue_id']),('ShowDialogue',identity))
   self.assertIn('AwaitDialogue',[x['kind']for x in a]);self.assertNotIn('StartWait',[x['kind']for x in a])
  self.assertEqual([x['actor']for x in self.actions if x['kind']=='RestoreActor'],['Ninten','Doll','Mimmie','Minnie'])
  self.assertEqual(self.actions[-1],dict(kind='DialogueDone',phrase=7,actor='None',duration=.5))
  self.assertFalse(any(x['kind']in('QueueBattle','RequestBattle','ReleaseBattleActor')for x in self.actions))
 def test_program_path_and_terminal_bindings(self):
  self.assertEqual([self.ir['strings'][p['source_path_string']]for p in self.s['Program'][:3]],['Podunk/cutscenes/lamp_attack','Podunk/cutscenes/doll_attack','Podunk/cutscenes/doll_defeated'])
  p=self.s['Program'][2];self.assertEqual((p['stable_id'],p['first_command'],p['command_count'],p['phrase_count']),(3,134,len(self.actions),8))
  end=self.s['Command'][p['first_command']+p['command_count']-1];self.assertEqual((end['opcode'],end['duration']),(23,.5))
  for p in self.s['Program'][:2]:self.assertEqual(self.s['Command'][p['first_command']+p['command_count']-1]['opcode'],24)
  blob,_=room.compile_ir(self.ir);self.assertEqual(room.parse_pack(blob)['sections']['Program'],self.s['Program'])
 def test_minnie_texture_profile_body_and_original_initial_position(self):
  self.assertEqual((self.s['Resource'][28]['stable_id'],self.ir['strings'][self.s['Resource'][28]['path_string']]),(29,'graphics/ui/house/minnie.t3x'))
  self.assertEqual((self.s['ActorInstance'][4]['stable_id'],self.s['ActorInstance'][4]['position']),(5,[472,88]))
  p=self.s['ActorProfile'][4];self.assertEqual((p['primary_resource'],p['execution_kind'],p['sprite_position'],p['sprite_offset'],p['flags']),(28,3,[0,9],[0,-12],1))
  n=self.h['npcs'][3];self.assertEqual((n['id'],n['room_actor_index'],n['source_path'],n['body_id'],n['position']),(4,4,'Objects/npc3',11,[472,88]))
  self.assertEqual([(n['profile'],n['body_id'])for n in self.h['npcs']],[('carol',9),('mimmie',10),('doll',15),('minnie',11)])
  self.assertEqual(self.pres['profiles'][3]['sprite_offset'],[0,-3])
 def test_wait_segments_and_dynamic_name(self):
  a,b=self.h['dialogues'][3:5];self.assertEqual((a['id'],a['first_segment'],a['segment_count']),(4,7,1));self.assertEqual((b['id'],b['first_segment'],b['segment_count']),(5,8,3))
  segments=self.h['segments'][7:11];self.assertEqual([s['flags']for s in segments],[5,3,3,5]);self.assertTrue(all(s['speaker']=='Mimmie'and s['voice']=='Audio/Sound effects/text/Kid.mp3'for s in segments))
  self.assertEqual(segments[0]['tokens'],[dict(kind=1,text='Is it over?')]);self.assertEqual(segments[-1]['tokens'][0],dict(kind=2,text=''))
  self.assertEqual(segments[-1]['tokens'][1]['text'],", can you check it to make sure it's dead?")
  house.verify_sources(self.h,self.pres);house.parse_pack(house.encode(house.lower(self.h,self.pres),version=self.h['schema']))
 def test_dot_exact_native_keys_and_sound_stable_id(self):
  command=next(c for c in self.s['Command'][134:]if c['opcode']==14);clip=self.s['Clip'][command['target_index']]
  keys=self.s['Key'][clip['first_key']:clip['first_key']+clip['key_count']]
  self.assertEqual((clip['key_count'],clip['flags'],clip['channel']),(10,8,1));self.assertEqual([[k['time'],k['frame']]for k in keys],self.native['dot']['keys'])
  self.assertEqual((self.s['Resource'][29]['stable_id'],self.ir['strings'][self.s['Resource'][29]['path_string']]),(30,'res://'+post.SOUND))
  from tools.audio_asset import parse_bank
  assets=parse_bank((room.ROOT/'romfs/sound/banks/opening.encaudio').read_bytes())['assets'];sound=next(x for x in assets if x['stable_id']==30)
  self.assertEqual(sound['source_path'],'res://'+post.SOUND);self.assertFalse(sound['loop'])
 def test_prior_content_prefixes_and_explicit_idle_semantic_correction(self):
  receipt=json.loads((room.ROOT/'reports/doll-postwin/preserved-prefixes.json').read_text())
  docs={'content/native-opening.json':self.ir,'content/native-house.json':self.h,'content/native-house-presentation.json':self.pres}
  for source,entries in receipt['prefixes'].items():
   doc=copy.deepcopy(docs[source])
   if source=='content/native-house.json':
    # Carol now participates in the source-backed phone cutscene. Undo only
    # that reviewed actor binding before checking the older frozen prefix.
    self.assertEqual(doc['npcs'][0]['room_actor_index'],5)
    doc['npcs'][0]['room_actor_index']=room.NONE
   melody=json.loads((room.ROOT/'reports/doll-melody/preserved-prefixes.json').read_text())
   for section,fields in melody.get('added_fields',{}).get(source,{}).items():
    for row in doc[section]:
     for field in fields:row.pop(field)
   for change in melody['reviewed_corrections']:
    if change['source']==source:doc.get('sections',doc)[change['section']][change['index']][change['field']]=change['before']
   for change in receipt.get('reviewed_corrections',[]):
    if change['source']==source:
     row=doc['sections'][change['section']][change['index']]
     self.assertEqual(row[change['field']],change['after'])
     row[change['field']]=change['before']
   for key,row in entries.items():
    data=doc['sections'][key]if'sections'in doc else doc[key]
    if key=='Program':data=[{k:v for k,v in p.items()if k!='source_path_string'}for p in data]
    self.assertEqual(digest(data[:row['count']]),row['sha256'],(source,key))
 def test_unknown_postwin_source_paths_fail_closed(self):
  for phrase,key,value in [('0','unknown',True),('0','actorsvisible',{'doll':False}),('0','goto','7'),('0','ovbattlemusic',True),('0','soundeffect','other.wav'),('0','setflags','doll_melody'),('0','unsetflags','doll_attack'),('1','setflags','minnie_leave'),('2','actorsemote',{'mimmie':'exclamation'}),('7','startbattle',{})]:
   d=copy.deepcopy(self.doc);d[phrase][key]=value
   with self.subTest(phrase=phrase,key=key),self.assertRaises(ValueError):post.compile_dialogue(d,.5)
  for value in [{'height':48,'speed':.42,'length':.2},{'height':48,'queue':True},{'height':48}]:
   d=copy.deepcopy(self.doc);d['0']['actorsjump']['doll']=value
   with self.assertRaises(ValueError):post.compile_dialogue(d,.5)
 def test_program_path_terminal_duration_and_key_capacity_rejection(self):
  for value in ['', '../escape','/absolute','a//b','a:b','a\\b']:
   self.reject(lambda d,value=value:d['strings'].__setitem__(d['sections']['Program'][2]['source_path_string'],value))
  self.reject(lambda d:d['sections']['Program'][2].update(source_path_string=d['sections']['Program'][1]['source_path_string']))
  self.reject(lambda d:d['sections']['Program'][2].update(source_path_string=room.NONE))
  for duration in (-1,0,3601):self.reject(lambda d,duration=duration:d['sections']['Command'][-1].update(duration=duration))
  first_done=next(i for i,c in enumerate(self.s['Command'])if c['opcode']==23)
  self.reject(lambda d:d['sections']['Command'][first_done].update(duration=.5))
  self.reject(lambda d:d['sections']['Command'][-1].update(opcode=22,duration=0))
  self.reject(lambda d:d['sections']['Clip'][-1].update(key_count=17))
 def test_forged_native_receipt_is_rejected(self):
  with tempfile.TemporaryDirectory()as td:
   class Inputs:
    root=Path(td)
    def document(self,p):return json.loads((self.root/p).read_text())
   ex=Inputs();p=ex.root/post.RECEIPT;p.parent.mkdir(parents=True);d=json.loads((room.ROOT/post.RECEIPT).read_text());d['dot']['keys'][0][1]=99;p.write_text(json.dumps(d));(ex.root/post.REVIEW).write_bytes((room.ROOT/post.REVIEW).read_bytes())
   with self.assertRaisesRegex(ValueError,'Changed unreviewed post-win native receipt'):post.receipt(ex)

if __name__=='__main__':unittest.main()
