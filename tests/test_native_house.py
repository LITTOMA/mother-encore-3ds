import copy,json,struct,sys,unittest,zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'tools'))
import native_house as h
import extract_house as x
class HouseCompilerTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.ir=json.loads((ROOT/'content/native-house.json').read_text());cls.pres=json.loads((ROOT/'content/native-house-presentation.json').read_text());cls.blob=(ROOT/'romfs/data/opening.enchouse').read_bytes()
  # Mutations below must start from an accepted current-schema encoding.
  h.parse_pack(h.encode(h.lower(cls.ir,cls.pres),version=cls.ir['schema']))
 def test_deterministic_source_and_binary(self):
  self.assertEqual(x.build(),self.ir);h.verify_sources(self.ir,self.pres);self.assertEqual(h.encode(h.lower(self.ir,self.pres),version=self.ir['schema']),self.blob);h.parse_pack(self.blob)
 def test_authored_transform_and_wait_semantics(self):
  self.assertEqual([d['destination']for d in self.ir['doors']],[[220,385],[430,387],[32,681],[155,401],[128,169],[64,369],[464,169],[176,369]])
  self.assertEqual([d['center']for d in self.ir['doors']],[[416,394],[234,392],[140,416],[32,676],[64,363],[128,188],[176,363],[464,188]])
  self.assertEqual([s['flags']for s in self.ir['segments'][:7]],[3,3,3,5,5,5,5]);self.assertEqual(self.ir['npcs'][0]['body_id'],9)
  self.assertEqual(self.ir['fade_parameters'],{'FadeCuts':[1.,0.,0.,1.],'FadeShader':[1.,1.,0,0]})
 def test_source_openable_policy_and_standalone_dialogue(self):
  self.assertEqual(len(self.ir['openable_doors']),4)
  d=self.ir['openable_doors'][1];self.assertEqual((d['player_body_id'],d['nonplayer_body_id']),(3,4));self.assertEqual(d['policy'],1);self.assertEqual(d['flag'],'mimmie_door_opened')
  self.assertEqual(d['trigger_center'],[64,353.5]);self.assertEqual(d['trigger_extents'],[16,19.5]);self.assertEqual(d['interact_center'],[64,368]);self.assertEqual(d['collision_center'],[64,364])
  self.assertEqual((d['normal_mask'],d['normal_values'],d['action_mask'],d['action_values']),(5,1,7,6));self.assertEqual((d['close_delay'],d['timer_flags'],d['ram_required_y']),(.3,1,-1))
  self.assertEqual(self.ir['openable_doors'][2]['key'],'KeyBasement');self.assertEqual(self.ir['openable_doors'][2]['policy'],0)
  self.assertEqual(self.ir['dialogues'][d['blocked_dialogue']]['first_segment'],4);text=self.ir['segments'][4];self.assertEqual(text['speaker'],'');self.assertEqual(text['voice'],'');self.assertEqual(text['tokens'][0]['text'],"The door won't open!")
 def test_doll_boundary_and_initial_npc_profiles(self):
  n=self.ir['npcs'][:3];self.assertEqual([x['position']for x in n],[[192,704],[120,88],[40,40]])
  self.assertEqual([x['profile']for x in n],['carol','mimmie','doll']);self.assertIsNone(n[2]['shadow_resource']);self.assertEqual([x['segment_count']for x in n],[4,2,0])
  t=self.ir['story_triggers'][0];self.assertEqual(t['center'],[128,176]);self.assertEqual(t['extents'],[16,8]);self.assertEqual(t['disposition'],2);self.assertEqual(t['program_index'],1)
  self.assertEqual(self.ir['story_conditions'][:2],[dict(kind=1,flag='doll_melody',value=0),dict(kind=2,flag='doll_defeated',value=0)])
  self.assertTrue(t['dialogue'].endswith('/doll_attack.yaml'));self.assertEqual([x['directions']for x in self.pres['profiles'][:3]],[4,4,1])
 def test_missing_duplicated_and_misbound_profile_clips(self):
  for kind in ['missing','duplicated','resource']:
   p=copy.deepcopy(self.pres)
   if kind=='missing':p['clips'].pop()
   elif kind=='duplicated':p['clips'][-1]=copy.deepcopy(p['clips'][-2]);p['clips'][-1]['id']=999
   else:p['clips'][0]['resource']='mimmie'
   with self.subTest(kind=kind),self.assertRaises(h.ContentError):h.parse_pack(h.encode(h.lower(self.ir,p),version=self.ir['schema']))
 def test_unknown_fields_rejected(self):
  for route in [(),('doors',0),('npcs',0),('segments',0),('segments',0,'tokens',0),('interaction',),('boundaries',0),('overrides',0),('dialogues',0),('openable_doors',0),('story_triggers',0),('story_conditions',0)]:
   ir=copy.deepcopy(self.ir);part=ir
   for key in route:part=part[key]
   part['unknown']=1
   with self.subTest(route=route),self.assertRaises(h.ContentError):h.lower(ir,self.pres)
  for route in [(),('resources',0),('clips',0),('clips',0,'keys',0),('parameters',),('profiles',0)]:
   p=copy.deepcopy(self.pres);part=p
   for key in route:part=part[key]
   part['unknown']=1
   with self.subTest(route=route),self.assertRaises(h.ContentError):h.lower(self.ir,p)
 def test_fingerprint_mismatch_rejected(self):
  ir=copy.deepcopy(self.ir);ir['sources'][next(iter(ir['sources']))]='0'*64
  with self.assertRaises(h.ContentError):h.verify_sources(ir,self.pres)
  p=copy.deepcopy(self.pres);p['resources'][0]['sha256']='0'*64
  with self.assertRaises(h.ContentError):h.lower(self.ir,p)
 def test_negative_headers(self):
  for size in [0,1,63,h.HEADER-1,len(self.blob)-1]:
   with self.assertRaises(h.ContentError):h.parse_pack(self.blob[:size])
  for off,value in [(8,0),(8,1),(20,0),(24,99),(28,0),(52,1),(68,0),(72,999999)]:
   b=bytearray(self.blob);struct.pack_into('<I',b,off,value);struct.pack_into('<I',b,16,0);struct.pack_into('<I',b,16,zlib.crc32(b))
   with self.subTest(off=off),self.assertRaises(h.ContentError):h.parse_pack(b)
 def test_semantic_rejections(self):
  for sec,index,field,value in [('Doors',0,6,0),('Doors',0,10,3),('Npcs',0,2,0),('Segments',0,5,99),('Tokens',0,0,99),('Interaction',0,2,0),('Boundaries',0,2,9),('Resources',0,3,0),('Clips',0,1,99),('Clips',0,3,999),('Keys',0,0,float('nan')),('Parameters',0,0,99),('Overrides',0,0,99),('Npcs',0,21,99),('Npcs',0,22,2),('Clips',0,8,0xffffffff),('Profiles',0,1,999),('Profiles',0,2,float('nan')),('Profiles',0,6,2),('Profiles',0,7,8),('Dialogues',0,2,999),('Dialogues',0,3,0),('OpenableDoors',0,2,0),('OpenableDoors',0,4,999),('OpenableDoors',0,9,99),('OpenableDoors',0,10,2),('OpenableDoors',0,15,16),('OpenableDoors',0,16,0),('OpenableDoors',0,17,8),('OpenableDoors',0,18,8),('OpenableDoors',0,20,0),('OpenableDoors',0,25,0),('OpenableDoors',0,41,2),('OpenableDoors',0,42,0),('OpenableDoors',0,45,float('nan')),('StoryTriggers',0,7,999),('StoryTriggers',0,8,0),('StoryTriggers',0,9,3),('StoryConditions',0,0,9),('StoryConditions',0,1,0),('StoryConditions',0,2,2)]:
   t=h.lower(self.ir,self.pres);t[sec][index][field]=value
   with self.subTest(sec=sec),self.assertRaises(h.ContentError):h.parse_pack(h.encode(t,version=self.ir['schema']))
 def test_negative_presentation_geometry_and_divisors(self):
  for name,index,value in [('DialogueRect',2,0),('DialogueMargins',0,-1),('NameSizing',1,0),('NameSizing',2,0),('FontMetrics',0,0),('DisplayReference',2,2),('TextTiming',0,0),('TextTagSpeeds',3,1)]:
   p=copy.deepcopy(self.pres);p['parameters'][name][index]=value
   with self.subTest(name=name),self.assertRaises(h.ContentError):h.parse_pack(h.encode(h.lower(self.ir,p),version=self.ir['schema']))
 def test_data_only_and_staging(self):
  ir=copy.deepcopy(self.ir);ir['doors'][0]['destination'][0]+=1;ir['segments'][0]['tokens'][0]['text']='Changed externally, ';ir['npcs'][0]['view_radius']=45
  t=h.parse_pack(h.encode(h.lower(ir,self.pres),version=ir['schema']));self.assertEqual(t['Doors'][0][8],221);self.assertEqual(t['Npcs'][0][18],45)
  files=h.stage_files(ROOT/'romfs');self.assertIn(Path('data/opening.enchouse'),files);self.assertEqual(len(files),1+len(self.pres['resources']))
 def test_revision6_delay_newline_tokens_and_legacy_bounds(self):
  # Keep a bounded historical prefix so token admission is independent of
  # current House7's additive source dialogues and capacity expansion.
  t=self.bounded_legacy_tables(47);self.assertEqual(self.ir['schema'],7)
  self.assertEqual(len(t['Segments']),83);self.assertTrue(any(tok[0]==8 for tok in t['Tokens']));self.assertTrue(any(tok[0]==9 for tok in t['Tokens']))
  delay=next(i for i,tok in enumerate(t['Tokens'])if tok[0]==8)
  newline=next(i for i,tok in enumerate(t['Tokens'])if tok[0]==9)
  def with_delay(text):
   table=copy.deepcopy(t);offset=len(table['Strings']);table['Strings']+=text.encode()+b'\0';table['Tokens'][delay][1]=offset;return table
  for text in ['1','0.5','3600','0.000001']:
   with self.subTest(valid=text):h.parse_pack(h.encode(with_delay(text),version=6))
  for text in ['','0','0.0','-1','+1','.5','5.','1e2','NaN','inf','1_0',' 1','1 ','3601','000000001','١']:
   with self.subTest(invalid=text),self.assertRaises(h.ContentError):h.parse_pack(h.encode(with_delay(text),version=6))
  for version in [4,5]:
   with self.subTest(version=version),self.assertRaises(h.ContentError):h.parse_pack(h.encode(t,version=version))
  changed=copy.deepcopy(t);changed['Tokens'][newline][1]=changed['Tokens'][delay][1]
  with self.assertRaises(h.ContentError):h.parse_pack(h.encode(changed,version=6))
  changed=copy.deepcopy(t);changed['Tokens'][newline][0]=10
  with self.assertRaises(h.ContentError):h.parse_pack(h.encode(changed,version=6))
 def bounded_legacy_tables(self,dialogue_count):
  t=h.lower(self.ir,self.pres);last=t['Dialogues'][dialogue_count-1]
  count=last[2]+last[3];tokens=t['Segments'][count-1][3]+t['Segments'][count-1][4]
  t['Segments']=t['Segments'][:count];t['Tokens']=t['Tokens'][:tokens];t['Dialogues']=t['Dialogues'][:dialogue_count]
  for override in t['Overrides']:
   if override[3]!=0xffffffff and override[3]>=dialogue_count:override[3]=0xffffffff
  return t
 def test_revision7_capacity_and_unknown_revision_fail_closed(self):
  t=h.lower(self.ir,self.pres)
  self.assertEqual(self.ir['schema'],7)
  self.assertEqual((len(t['Segments']),len(t['Dialogues'])),(129,66))
  h.parse_pack(h.encode(t,version=7))
  with self.assertRaises(h.ContentError):h.parse_pack(h.encode(t,version=6))
  for section,limit in [('Segments',256),('Dialogues',128),('Tokens',256)]:
   oversized=copy.deepcopy(t);oversized[section]+=[copy.deepcopy(t[section][0]) for _ in range(limit+1-len(t[section]))]
   with self.subTest(section=section),self.assertRaisesRegex(h.ContentError,'capacity'):h.parse_pack(h.encode(oversized,version=7))
  changed=bytearray(h.encode(t,version=7))
  for offset in (8,24,28):struct.pack_into('<I',changed,offset,8)
  struct.pack_into('<I',changed,16,0);struct.pack_into('<I',changed,16,zlib.crc32(changed))
  with self.assertRaises(h.ContentError):h.parse_pack(changed)
 def test_revision4_and5_remain_accepted_without_new_tokens(self):
  # Strip only the appended Dad dialogues/segments, then retain the old token
  # capabilities independently of the new 67-segment capacity requirement.
  ir=copy.deepcopy(self.ir);ir['dialogues']=ir['dialogues'][:28]
  last=ir['dialogues'][-1];ir['segments']=ir['segments'][:last['first_segment']+last['segment_count']]
  t=h.lower(self.ir,self.pres);count=len(ir['segments']);tokens=t['Segments'][count-1][3]+t['Segments'][count-1][4]
  t['Segments']=t['Segments'][:count];t['Tokens']=t['Tokens'][:tokens];t['Dialogues']=t['Dialogues'][:28]
  # New Minnie open-door override references appended text; the old-format
  # fixture omits that capability together with the removed dialogue records.
  for override in t['Overrides']:
   if override[3]!=0xffffffff and override[3]>=28:override[3]=0xffffffff
  h.parse_pack(h.encode(t,version=5))
  for kind in [8,9]:
   changed=copy.deepcopy(t);changed['Tokens'][0]=[kind,0]
   with self.subTest(version=5,kind=kind),self.assertRaises(h.ContentError):h.parse_pack(h.encode(changed,version=5))
  # Original schema4 allowed just literals and name substitutions.
  for tok in t['Tokens']:
   if tok[0]>2:tok[:]=[2,0]
  h.parse_pack(h.encode(t,version=4))
  t['Tokens'][0]=[3,0]
  with self.assertRaises(h.ContentError):h.parse_pack(h.encode(t,version=4))
if __name__=='__main__':unittest.main()
