import copy,hashlib,json,tempfile,unittest
from pathlib import Path
from unittest.mock import patch
from tools import graveyard_tutorial_programme as t,native_content as nc,field_npcs

class TutorialProgramme(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.p,cls.review,cls.ex=t.read();cls.npcs=field_npcs.build();cls.room,cls.story,cls.proof=t.build();cls.decoded=nc.parse_pack(cls.room)
 def test_complete_source_graph_and_old_prefix(self):
  self.assertEqual((self.proof['programme'],self.proof['commands'],self.proof['phrases'],self.proof['dialogues'],self.proof['choice_first']),(6,83,16,14,1))
  old=nc.parse_pack(t.BASE.read_bytes());new=self.decoded
  self.assertEqual((new['rules'],new['capabilities']),(7,13))
  self.assertEqual(new['strings'][:len(old['strings'])],old['strings'])
  for key,rows in old['sections'].items():self.assertEqual(new['sections'][key][:len(rows)],rows,key)
  p=new['sections']['Program'][-1];commands=new['sections']['Command'][p['first_command']:p['first_command']+p['command_count']]
  c=next(x for x in commands if x['opcode']==nc.OPCODES.index('AwaitChoices'));self.assertEqual(c['target_index'],1)
  self.assertIn('Candidate only',self.proof['scope'])
 def test_two_locales_clear_narration_voice_and_keep_current_button_tags(self):
  rows=t.texts(self.p,self.review,self.ex);self.assertEqual(len(rows),28)
  narration=[x for x in rows if x['id']==3111];self.assertEqual(len(narration),2)
  self.assertTrue(all(not x['voice']and not x['speaker']for x in narration))
  self.assertTrue(all('[ui_cancel]'in x['body']and '[if input:gamepad]'in x['body']for x in rows if x['id']==3113))
  self.assertTrue(all(x['voice'].endswith('/Adult.mp3')for x in rows if x['id']!=3111))
 def test_every_branch_and_manual_loop_match_source(self):
  p=self.p;g=p['groups'][0];self.assertEqual((g['options'][0]['target_pc'],g['options'][1]['target_pc'],g['cancel_target_pc']),(p['labels']['18'],p['labels']['17'],p['labels']['17']))
  back=[x for x in p['commands']if x['kind']=='Jump'and x['flags']];self.assertEqual(len(back),1);self.assertEqual(back[0]['target'],p['labels']['16'])
  grant=next(i for i,x in enumerate(p['commands'])if x['kind']=='GrantKeyItem');flag=next(i for i,x in enumerate(p['commands'])if x['kind']=='SetFlag')
  self.assertLess(grant,flag);self.assertEqual((p['commands'][grant]['phrase'],p['commands'][flag]['phrase']),(12,15))
  self.assertEqual(p['commands'][flag+1]['kind'],'AwaitTimer')
 def test_unknown_and_changed_source_lowering_reject(self):
  mutations=[lambda p:p.update(schema=True),lambda p:p.update(schema=2),lambda p:p.update(extra=0),lambda p:p.update(review='../escape'),lambda p:p.update(boundary='Cutscenes/Cutscene Area7'),lambda p:p['actors'].update(iggy=4),lambda p:p['actors'].update(ninten=True),lambda p:p['commands'][0].update(kind='Unsupported'),lambda p:p['commands'][28].update(duration=.5),lambda p:p['commands'][30].update(flags=0),lambda p:p['commands'][51].update(flags=0),lambda p:p['commands'][56].update(item='Invented'),lambda p:p['commands'][75].update(flag='cem_roots_grown'),lambda p:p['commands'][0].update(unknown=0),lambda p:p['commands'].pop(),lambda p:p['groups'][0].update(cancel_target_pc=52),lambda p:p['groups'][0]['options'][1].update(target_pc=52),lambda p:p['labels'].update({'16':46}),lambda p:p['dialogues'][1].update(id=p['dialogues'][0]['id']),lambda p:p['dialogues'].pop(),lambda p:p.update(terminal=76)]
  with tempfile.TemporaryDirectory()as folder,patch.object(t.review,'read',return_value=(self.review,self.ex)),patch.object(field_npcs,'build',return_value=self.npcs):
   file=Path(folder)/'bad.json'
   for mutate in mutations:
    p=copy.deepcopy(self.p);mutate(p);file.write_text(json.dumps(p),encoding='utf8')
    with self.subTest(change=mutate),self.assertRaises(ValueError):t.read(file)
   for text in ('{"schema":1,"schema":1}','{"schema":NaN}'):
    file.write_text(text,encoding='utf8')
    with self.assertRaises(ValueError):t.read(file)
 def test_output_is_deterministic_and_source_identities_are_retained(self):
  room,story,proof=t.build();self.assertEqual((room,story),(self.room,self.story));self.assertEqual(proof,self.proof)
  self.assertIn(self.p['source'],proof['sources']);self.assertIn('Scripts/UI/DialogueBox.gd',proof['sources']);self.assertIn('content/graveyard-swing-programme.json',proof['dependencies'])
 def test_original_direction_observation_provenance(self):
  folder=t.ROOT/'reports/iggy-tutorial/actor-direction-reference-1';proof=json.loads((folder/'source.json').read_bytes());doc=json.loads((folder/'reference.json').read_bytes());cases=json.loads((t.ROOT/'tests/fixtures/actor-direction-cases.json').read_bytes())
  sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
  self.assertEqual(proof['observations'],92);self.assertEqual(proof['source_cases_sha256'],sha(t.ROOT/'tests/fixtures/actor-direction-cases.json'));self.assertEqual([r['definition']for r in doc['cases']],cases['cases'])
  self.assertEqual(doc['godot']['string'],'3.6.2-stable (official)')
  for name,digest in proof['sources'].items():self.assertEqual(digest,sha(t.ROOT/'upstream/MOTHER-Encore'/name))
  for name,digest in proof['reports_sha256'].items():self.assertEqual(digest,sha(folder/name))
  for name,digest in proof['tools_sha256'].items():self.assertEqual(digest,sha(t.ROOT/name))
if __name__=='__main__':unittest.main()
