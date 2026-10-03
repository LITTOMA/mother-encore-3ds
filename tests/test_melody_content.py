"""Source-backed first melody graph, external bindings and fail-closed schemas."""
import copy, hashlib, json, struct, sys, tempfile, unittest, zlib
from pathlib import Path
from unittest.mock import patch
from tools import melody_dialogue as melody, native_content as room, native_house as house
from tools.doll_dialogue import decode
sys.path.insert(0,str(room.ROOT/'tools'))

def digest(value):return hashlib.sha256(json.dumps(value,sort_keys=True,separators=(',',':')).encode()).hexdigest()

class MelodyContentTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.ir=json.loads((room.ROOT/'content/native-opening.json').read_text());cls.s=cls.ir['sections']
  cls.h=json.loads((room.ROOT/'content/native-house.json').read_text());cls.pres=json.loads((room.ROOT/'content/native-house-presentation.json').read_text())
  cls.native=decode(json.loads((room.ROOT/melody.RECEIPT).read_text()));cls.actions=melody.compile_melody(cls.native['yaml'][0],.5);cls.guard=melody.compile_guard(cls.native['yaml'][1],.5)
 def reject_room(self,section,index,field,value):
  ir=copy.deepcopy(self.ir);ir['sections'][section][index][field]=value
  with self.assertRaises(room.ContentError):room.compile_ir(ir)
 def test_original_native_parser_and_receipt(self):
  from tools.extract_native_content import Extractor
  self.assertEqual(melody.receipt(Extractor()),self.native)
  self.assertEqual([list(doc)for doc in self.native['yaml']],[['0','3','4'],['0','1'],['0'],['0'],['0']])
  self.assertEqual(melody.return_duration((room.ROOT/'upstream/MOTHER-Encore/Scripts/UI/DialogueBox.gd').read_text()),.5)
 def test_source_order_inherited_original_talker_and_synchronous_music(self):
  phases=[[a for a in self.actions if a['phrase']==i]for i in range(3)]
  self.assertEqual([a['kind']for a in phases[0]],['BeginCutscene','ShowDialogue','BindActor','ActorPersistent','YieldIdle','AwaitDialogue'])
  self.assertEqual(phases[0][1],dict(kind='ShowDialogue',phrase=0,actor='None',dialogue_id=6,flags=1))
  self.assertEqual([a['actor']for a in self.actions if a['kind']in('BindActor','RestoreActor')],['Ninten','Ninten'])
  self.assertEqual([a['kind']for a in phases[1]],['StartWait','CallObjectDeferred','CallObjectDeferred','PlayMusicImmediate','AwaitTimer'])
  self.assertEqual([a.get('binding')for a in phases[1][1:3]],['stop_house_music','effect_appear'])
  self.assertEqual(phases[1][0]['duration'],4)
  self.assertEqual([a['kind']for a in phases[2]][:7],['ShowDialogue','StartWait','CallObjectDeferred','CallObjectDeferred','PlaySound','SetFlag','AwaitDialogue'])
  self.assertEqual((phases[2][1]['duration'],phases[2][6]['flags']),(.6,1))
  self.assertEqual(phases[2][-5]['kind'],'StopInteraction');self.assertEqual(phases[2][-5]['flags'],1)
  self.assertEqual(self.actions[-1],dict(kind='DialogueDone',phrase=2,actor='None',duration=.5))
  self.assertEqual([(a['flag'],a['value'])for a in self.actions if a['kind']=='SetFlag'],[('doll_melody',1)])
 def test_guard_preserves_camera_null_and_concurrent_source_movement(self):
  first=[a for a in self.guard if a['phrase']==0]
  self.assertEqual([a['kind']for a in first],['BeginCutscene','ShowDialogue','BindActor','ActorPersistent','BindActor','ActorPersistent','YieldIdle','SetTalker','ChangeCamera','YieldIdle','AwaitDialogue'])
  self.assertEqual(first[8],dict(kind='ChangeCamera',phrase=0,actor='None',flags=1))
  self.assertEqual([a['actor']for a in self.guard if a['kind']=='RestoreActor'],['Ninten','Mimmie'])
  move=next(a for a in self.guard if a['kind']=='MoveActorPath')
  self.assertEqual(move['path'],dict(movement=[dict(x=0,y=-12)],speed=64,animation='Walk',type='step'))
  self.assertFalse(any(a['kind']in('StartWait','AwaitTimer','SetFlag')for a in self.guard))
 def test_program_paths_source_labels_and_binary_roundtrip(self):
  self.assertEqual((self.ir['rules'],self.ir['capabilities'],self.ir['adapter_revision']),(7,7,8))
  for index,path,actions,phrases in [(3,'Podunk/dollmelody',self.actions,3),(4,'Podunk/cutscenes/mimmie_ignore',self.guard,2)]:
   p=self.s['Program'][index];self.assertEqual(self.ir['strings'][p['source_path_string']],path)
   self.assertEqual((p['stable_id'],p['command_count'],p['phrase_count']),(index+1,len(actions),phrases))
  blob,_=room.compile_ir(self.ir);self.assertEqual(room.parse_pack(blob)['sections']['Command'],self.s['Command'])
  house.verify_sources(self.h,self.pres);table=house.parse_pack(house.encode(house.lower(self.h,self.pres),version=self.h['schema']))
  self.assertEqual((house.STRIDES[2],house.STRIDES[11]),(104,20));self.assertEqual(table['Parameters'][-1],(19,1,0,0,0))
 def test_resource_binding_and_audio_identity(self):
  self.assertEqual([(r['stable_id'],r['kind'],self.ir['strings'][r['path_string']])for r in self.s['Resource'][30:34]],[(31,4,'world-effect/melody.encfx'),(32,2,'res://'+melody.AUDIO[0]),(33,2,'res://'+melody.AUDIO[1]),(34,2,'res://'+melody.AUDIO[2])])
  self.assertEqual([(b['stable_id'],b['kind'],b['target_index'])for b in self.s['Binding'][3:7]],[(4,5,33),(5,6,30),(6,1,33),(7,7,30)])
  self.assertEqual(self.s['Resource'][30]['sha256'],hashlib.sha256((room.ROOT/'romfs/world-effect/melody.encfx').read_bytes()).hexdigest())
  from tools.audio_asset import parse_bank
  assets=parse_bank((room.ROOT/'romfs/data/opening.encaudio').read_bytes())['assets']
  new=[a for a in assets if a['stable_id']in(32,33,34)]
  self.assertEqual([a['source_path']for a in new],['res://'+p for p in melody.AUDIO])
  self.assertEqual([(a['loop'],a['loop_start'])for a in new],[(False,0),(False,0),(True,int(2.035*44100))])
 def test_text_voice_wait_tokens_and_override_seen_keys(self):
  rows={d['id']:self.h['segments'][d['first_segment']:d['first_segment']+d['segment_count']]for d in self.h['dialogues']}
  self.assertEqual([s['flags']for s in rows[6]],[3,3,5]);self.assertEqual(rows[6][1]['tokens'][0],dict(kind=2,text=''))
  self.assertEqual(rows[7][0]['tokens'],[dict(kind=2,text=''),dict(kind=1,text=' remembered the tune.')])
  self.assertTrue(all(not s['voice']and not s['speaker']for identity in(6,7,11)for s in rows[identity]))
  self.assertTrue(all(s['voice']=='Audio/Sound effects/text/Kid.mp3'and s['speaker']=='Mimmie'for identity in(8,9,10)for s in rows[identity]))
  self.assertEqual(rows[11][0]['tokens'][0]['text'],"It's not moving anymore.")
  n=self.h['npcs'][1];before=self.h['segments'][n['first_segment']:n['first_segment']+n['segment_count']]
  self.assertEqual([s['flags']for s in before],[3,5]);self.assertEqual(before[0]['tokens'][0]['text'],'Hurry up!')
  supported=[o for o in self.h['overrides']if o['dialogue_index']!=room.NONE and o['flag']=='doll_melody']
  self.assertEqual([(o['npc'],o['flag'],self.h['dialogues'][o['dialogue_index']]['id'])for o in supported],[(1,'doll_melody',10),(2,'doll_melody',11)])
  for o in self.h['overrides']:
   self.assertTrue(o['seen_key'].endswith('/'+self.h['npcs'][o['npc']]['source_path']+':'+o['flag']+':1:'+o['dialogue']))
 def test_exit_guard_and_phone_boundaries_preserve_source_geometry(self):
  g=self.h['story_triggers'][1];self.assertEqual((g['source_path'],g['center'],g['extents'],g['disposition'],g['program_index']),('Cutscene Area5',[128,185],[16,8],2,4))
  cond=self.h['story_conditions'][g['first_condition']:g['first_condition']+g['condition_count']]
  self.assertEqual(cond,[dict(kind=1,flag='doll_defeated',value=1),dict(kind=2,flag='doll_melody',value=0)])
  self.assertEqual([(t['center'],t['disposition'],t['program_index'])for t in self.h['story_triggers'][2:4]], [([136,816],2,6),([136,824],2,7)])
  self.assertEqual(self.h['doors'][5]['center'],[128,188]);self.assertEqual(self.h['npcs'][3]['position'],[472,88])
 def test_preserved_prefixes_and_only_documented_append_metadata(self):
  receipt=json.loads((room.ROOT/'reports/doll-melody/preserved-prefixes.json').read_text())
  strings=receipt['string_prefix'];self.assertEqual(digest(self.ir['strings'][:strings['count']]),strings['sha256'])
  for source,tables in receipt['prefixes'].items():
   d=json.loads((room.ROOT/source).read_text())
   if source=='content/native-house.json':
    # The phone adapter adds Carol's source actor mapping; legacy data
    # fingerprints still cover every earlier field after reversing it.
    self.assertEqual(d['npcs'][0]['room_actor_index'],5)
    d['npcs'][0]['room_actor_index']=room.NONE
    # Reconstruct the previous six-door view from unchanged upstream geometry.
    # Only the two now-supported Mom-room boundaries and Minnie's newly linked
    # program/seen key are projected back; the reviewed historical hashes stay
    # unchanged and every other prefix field is still checked below.
    from tools import extract_house,link_pillow_content
    legacy_doors=['Ninten_upstairs','Upstairs_Ninten','Upstairs_Living','Living_Upstairs','Upstair_Sister','Sister_Upstair']
    with patch.object(extract_house,'SUPPORTED',legacy_doors),patch.object(link_pillow_content,'append_house',lambda ex,ir,room:ir):
     legacy=extract_house.build(room.ROOT)
    current_boundaries=[dict(row,id=i+1)for i,row in enumerate(row for row in legacy['boundaries']if row['source_path']not in ('Doors/Upstair_Mom','Doors/Mom_Upstair'))]
    self.assertEqual(d['boundaries'],current_boundaries)
    d['boundaries']=legacy['boundaries']
    minnie=d['npcs'][3];self.assertEqual(minnie['source_path'],'Objects/npc3')
    self.assertEqual(self.ir['strings'][self.s['Program'][minnie['program_index']]['source_path_string']],'Podunk/minnie_run_tutorial')
    self.assertEqual(minnie['seen_key'],"/root/Ninten's House/Objects/npc3::1:Podunk/minnie_run_tutorial")
    minnie['program_index']=room.NONE;minnie['seen_key']=''
   for section,fields in receipt.get('added_fields',{}).get(source,{}).items():
    for row in d[section]:
     for field in fields:row.pop(field)
   for c in receipt['reviewed_corrections']:
    if c['source']==source:
     self.assertEqual(d.get('sections',d)[c['section']][c['index']][c['field']],c['after']);d.get('sections',d)[c['section']][c['index']][c['field']]=c['before']
   for key,record in tables.items():self.assertEqual(digest(d.get('sections',d)[key][:record['count']]),record['sha256'],(source,key))
 def test_unknown_source_fields_graph_voice_and_objects_rejected(self):
  for label,key,value in [('0','sound','Kid'),('0','actors',{'leader':'leader','doll':'Objects/npcdoll'}),('0','goto','4'),('3','wait',3),('3','music','House.mp3'),('3','objectsfunction',{'MusicArea':'play_music'}),('4','autoadvance',True),('4','wait',0),('4','setflags','minnie_leave')]:
   doc=copy.deepcopy(self.native['yaml'][0]);doc[label][key]=value
   with self.subTest(label=label,key=key),self.assertRaises(ValueError):melody.compile_melody(doc,.5)
  for label,key,value in [('0','changecam','mimmie'),('0','unknown',True),('1','wait',1),('1','setflags','minnie_leave')]:
   doc=copy.deepcopy(self.native['yaml'][1]);doc[label][key]=value
   with self.assertRaises(ValueError):melody.compile_guard(doc,.5)
  for path,doc in zip(melody.YAMLS[2:],self.native['yaml'][2:]):
   d=copy.deepcopy(doc);d['0']['unknown']=True
   with self.assertRaises(ValueError):melody.literal_phrase(path,d)
 def test_new_runtime_schema_modes_reject_unknown_or_misbound_data(self):
  for sec,index,field,value in [('Resource',30,'kind',5),('Resource',30,'width',1),('Resource',31,'kind',4),('Binding',3,'target_index',30),('Binding',3,'duration',1),('Binding',4,'target_index',31),('Binding',6,'kind',10),('Battle',1,'flags',14),('Battle',1,'flags',16)]:
   self.reject_room(sec,index,field,value)
  for op in(16,19,32,33):
   i=next(i for i,c in enumerate(self.s['Command'])if c['opcode']==op and c['flags']==1)
   self.reject_room('Command',i,'flags',8);self.reject_room('Command',i,'actor_index',0)
  i=next(i for i,c in enumerate(self.s['Command'])if c['opcode']==34)
  for field,value in [('flags',1),('target_index',30),('actor_index',0),('duration',.5)]:self.reject_room('Command',i,field,value)
 def test_house_crosspack_links_seen_keys_and_delay_rejected(self):
  for collection,index,key,value in [('npcs',2,'program_index',1024),('npcs',2,'program_index',0),('overrides',7,'dialogue_index',999),('overrides',7,'dialogue_index',0),('overrides',7,'seen_key','')]:
   d=copy.deepcopy(self.h);d[collection][index][key]=value
   with self.subTest(collection=collection,key=key),self.assertRaises(house.ContentError):house.parse_pack(house.encode(house.lower(d,self.pres),version=d['schema']))
  for value in (0,-1,121):
   d=copy.deepcopy(self.h);d['npc_parameters']['NpcInteractionReturn'][0]=value
   with self.assertRaises(house.ContentError):house.parse_pack(house.encode(house.lower(d,self.pres),version=d['schema']))
  blob=bytearray(house.encode(house.lower(self.h,self.pres),version=self.h['schema']));struct.pack_into('<I',blob,8,3);struct.pack_into('<I',blob,16,0);struct.pack_into('<I',blob,16,zlib.crc32(blob))
  with self.assertRaises(house.ContentError):house.parse_pack(blob)
 def test_forged_native_parser_receipt_rejected(self):
  with tempfile.TemporaryDirectory()as td:
   class Inputs:
    root=Path(td)
    def document(self,path):return json.loads((self.root/path).read_text())
   ex=Inputs();p=ex.root/melody.RECEIPT;p.parent.mkdir(parents=True)
   d=json.loads((room.ROOT/melody.RECEIPT).read_text());d['yaml'][0]['3']['music']='House.mp3';p.write_text(json.dumps(d))
   (ex.root/melody.REVIEW).write_bytes((room.ROOT/melody.REVIEW).read_bytes())
   with self.assertRaisesRegex(ValueError,'Changed unreviewed melody native receipt'):melody.receipt(ex)

if __name__=='__main__':unittest.main()
