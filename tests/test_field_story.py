import copy,json,struct,tempfile,unittest
from pathlib import Path
from unittest.mock import patch
from tools import field_story as story,bridge_entry,podunk_field,native_content
ROOT=Path(__file__).resolve().parents[1]
class FieldStoryTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.policy,_=story.read();recipe=podunk_field.load_recipe();model=podunk_field.Model(recipe);cls.room=native_content.parse_pack(podunk_field.build_room(recipe,model,recipe['house']['room_ir']))
 def test_source_programmes_and_metadata_reproduce(self):
  self.assertEqual(story.build(),(ROOT/'romfs'/story.OUTPUT).read_bytes());self.assertEqual(len(self.room['sections']['Program']),6)
  self.assertEqual([self.room['strings'][p['source_path_string']]for p in self.room['sections']['Program']],['Podunk/cutscenes/bridge_fight','Podunk/cutscenes/pippismom_call','Podunk/cutscenes/pippismom_help','Podunk/pippismom','Podunk/iggy_forget','Podunk/cutscenes/graveyard_zombie_roots'])
  self.assertEqual(self.room['sections']['Battle'][0]['stable_id'],4)
  self.assertEqual(self.room['sections']['Battle'][0]['actor_instance_index'],2)
 def test_checked_source_dialogues(self):
  policy,ex=story.read();texts=story.dialogues(policy,ex);self.assertEqual(len(texts),22);self.assertEqual({r['locale']for r in texts},{'en','zh_Hans_CN'})
  self.assertTrue(all(r['speaker']and r['body']and r['voice'].endswith('/Female.mp3')for r in texts))
  self.assertTrue(any('[Pippi]'in r['body']for r in texts));self.assertTrue(all('[@]'in r['body']for r in texts))
 def reject_policy(self,mutation,lower=False):
  candidate=copy.deepcopy(self.policy);mutation(candidate)
  folder=ROOT/'build/podunk-routes';folder.mkdir(parents=True,exist_ok=True)
  with tempfile.TemporaryDirectory(dir=folder)as temporary:
   target=Path(temporary)/'candidate.json';bridge_entry.write(target,candidate)
   with patch.object(story,'POLICY',target),self.assertRaises((ValueError,KeyError,StopIteration)):
    if lower:
     recipe=podunk_field.load_recipe();podunk_field.build_room(recipe,podunk_field.Model(recipe),recipe['house']['room_ir'])
    else:story.read()
 def test_unknown_versions_fields_and_source_changes_fail_closed(self):
  for mutate in (lambda p:p.update(schema=5),lambda p:p.update(schema=True),lambda p:p.update(ignore_unknown=True),lambda p:p.update(supplement='../outside.json'),lambda p:p.update(supplement='content/../outside.json'),lambda p:p.update(supplement=True),lambda p:p['documents'][p['programmes'][0]['source']]['0'].update(wait=.01),lambda p:p['encounters'][0].update(basic_slot=False),lambda p:p['basic_overrides'][0].update(source='Data/BattleSkills/attack.yaml')):
   self.reject_policy(mutate)
 def test_unknown_command_is_not_ignored(self):self.reject_policy(lambda p:p['programmes'][0]['commands'][0].update(kind='Ignore'),True)
 def test_unread_text_and_timer_modes_are_explicit(self):
  s=self.room['sections'];names=native_content.OPCODES
  base_end=s['Program'][4]['first_command'];waits=[c for c in s['Command'][:base_end]if names[c['opcode']]=='AwaitDialogue'];self.assertEqual([c['flags']for c in waits],[0,1,1,1,1]+[0]*6)
  self.assertEqual(sum(names[c['opcode']]=='ShowDialogue'for c in s['Command'][:base_end]),11)
  moves=[c for c in s['Command']if names[c['opcode']]=='MoveCamera'];self.assertEqual([c['duration']for c in moves],[1,1,2])
 def test_source_npc_selection_and_rich_controls(self):
  self.assertEqual([(r['programme'],r['flag'])for r in self.policy['interactions']],[(2,''),(3,'pippis_mom_help')])
  rows=story.dialogues(*story.read());help_text=next(r['body']for r in rows if r['id']==1011 and r['locale']=='en')
  self.assertEqual(help_text.count('[WAIT@]'),2);self.assertIn('[color][Pippi][/color]',help_text)
  self.assertIn('[DELAY]',next(r['body']for r in rows if r['id']==1012 and r['locale']=='zh_Hans_CN'))
  for mutate in (lambda p:p['interactions'][0].update(actor='dog'),lambda p:p['interactions'][0].update(programme=0),lambda p:p['interactions'][1].update(flag='pippis_mom_call'),lambda p:p['interactions'][0].update(turn_npc=False),lambda p:p['interactions'][0].update(turn_player=1)):
   self.reject_policy(mutate)
 def test_previous_schema_two_remains_byte_identical(self):
  with patch.object(story,'POLICY',ROOT/'tests/fixtures/pre-graveyard-story.json'):
   self.assertEqual(story.build(),(ROOT/'tests/fixtures/field_story_v2.encstory').read_bytes())
 def test_cemetery_source_mapping_and_runtime_version_negatives(self):
  from tools.native_content import validate_tables,OPCODES
  s=self.room['sections'];p=s['Program'][5];commands=s['Command'][p['first_command']:p['first_command']+p['command_count']]
  self.assertEqual(self.room['capabilities'],12)
  self.assertEqual([c['duration']for c in commands if c['opcode']==3],[.5,2.5,1,2.2,1,1.2,1.8,.8,1,1,1.6])
  calls=[s['Binding'][c['target_index']]['target_index']for c in commands if c['opcode']==6]
  self.assertEqual(calls,[3,4,6,7,5]);self.assertEqual([(c['actor_index'],c['phrase'])for c in commands if c['opcode']==OPCODES.index('EraseActor')],[(4,6),(5,7)])
  bad=[]
  for cap in (11,):bad.append((cap,copy.deepcopy(s)))
  for field,value in [('kind',11),('target_index',0),('target_index',65536),('auxiliary_index',0),('value',1),('duration',1)]:
   b=copy.deepcopy(s);b['Binding'][-1][field]=value;bad.append((12,b))
  for field,value in [('actor_index',0),('actor_index',999),('flags',1),('target_index',0),('value',1)]:
   b=copy.deepcopy(s);c=next(c for c in b['Command']if c['opcode']==47);c[field]=value;bad.append((12,b))
  b=copy.deepcopy(s)
  for c in b['Command']:
   if c['opcode']==6:c.update(opcode=25,target_index=0xffffffff)
  bad.append((12,b))
  blob=bytearray((ROOT/'romfs/world/podunk/podunk.encroom').read_bytes());struct.pack_into('<I',blob,36,13);struct.pack_into('<I',blob,52,0)
  import zlib
  struct.pack_into('<I',blob,52,zlib.crc32(blob))
  with self.assertRaises(ValueError):native_content.parse_pack(bytes(blob))
  for cap,b in bad:
   with self.subTest(cap=cap),self.assertRaises(ValueError):validate_tables(self.room['strings'],b,self.room['scene_id'],7,cap)
 def test_cemetery_binding_order_and_unknown_calls_reject(self):
  for change in (lambda p:p.update(continuations=[]),lambda p:p.update(deferred_props=[]),lambda p:p['deferred_props'][0].update(id=999),lambda p:p['deferred_props'][0].update(method='ignore'),lambda p:p['deferred_props'][0].update(id=True),lambda p:p['deferred_props'].reverse(),lambda p:p['continuations'][0].update(source='missing.yaml'),lambda p:p['continuations'][0]['commands'].append(dict(kind='CallObjectDeferred',actor=None,phrase=10,prop='missing'))):self.reject_policy(change)

if __name__=='__main__':unittest.main()
