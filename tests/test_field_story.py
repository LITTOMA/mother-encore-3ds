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
  self.assertEqual(story.build(),(ROOT/'romfs'/story.OUTPUT).read_bytes());self.assertEqual(len(self.room['sections']['Program']),4)
  self.assertEqual([self.room['strings'][p['source_path_string']]for p in self.room['sections']['Program']],['Podunk/cutscenes/bridge_fight','Podunk/cutscenes/pippismom_call','Podunk/cutscenes/pippismom_help','Podunk/pippismom'])
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
  for mutate in (lambda p:p.update(schema=3),lambda p:p.update(schema=True),lambda p:p.update(ignore_unknown=True),lambda p:p['documents'][p['programmes'][0]['source']]['0'].update(wait=.01),lambda p:p['encounters'][0].update(basic_slot=False),lambda p:p['basic_overrides'][0].update(source='Data/BattleSkills/attack.yaml')):
   self.reject_policy(mutate)
 def test_unknown_command_is_not_ignored(self):self.reject_policy(lambda p:p['programmes'][0]['commands'][0].update(kind='Ignore'),True)
 def test_unread_text_and_timer_modes_are_explicit(self):
  s=self.room['sections'];names=native_content.OPCODES
  waits=[c for c in s['Command']if names[c['opcode']]=='AwaitDialogue'];self.assertEqual([c['flags']for c in waits],[0,1,1,1,1]+[0]*6)
  self.assertEqual(sum(names[c['opcode']]=='ShowDialogue'for c in s['Command']),11)
  moves=[c for c in s['Command']if names[c['opcode']]=='MoveCamera'];self.assertEqual([c['duration']for c in moves],[1,1])
 def test_source_npc_selection_and_rich_controls(self):
  self.assertEqual([(r['programme'],r['flag'])for r in self.policy['interactions']],[(2,''),(3,'pippis_mom_help')])
  rows=story.dialogues(*story.read());help_text=next(r['body']for r in rows if r['id']==1011 and r['locale']=='en')
  self.assertEqual(help_text.count('[WAIT@]'),2);self.assertIn('[color][Pippi][/color]',help_text)
  self.assertIn('[DELAY]',next(r['body']for r in rows if r['id']==1012 and r['locale']=='zh_Hans_CN'))
  for mutate in (lambda p:p['interactions'][0].update(actor='dog'),lambda p:p['interactions'][0].update(programme=0),lambda p:p['interactions'][1].update(flag='pippis_mom_call'),lambda p:p['interactions'][0].update(turn_npc=False),lambda p:p['interactions'][0].update(turn_player=1)):
   self.reject_policy(mutate)
if __name__=='__main__':unittest.main()
