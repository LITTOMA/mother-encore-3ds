import copy,json,tempfile,unittest
from pathlib import Path
from tools import graveyard_shovel as review,graveyard_shovel_programme as g,graveyard_tutorial_programme as tutorial,native_content as nc
ROOT=Path(__file__).resolve().parents[1]
class ShovelSourceTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.blob,cls.lowered,cls.policy,cls.ex=g.candidate();cls.room=nc.parse_pack(cls.blob)
 def mutant(self,change,compiler=False):
  with tempfile.TemporaryDirectory(prefix='shovel-policy-',dir=ROOT/'build')as folder:
   target=Path(folder).resolve();self.assertTrue(target.is_relative_to((ROOT/'build').resolve()));p=target/'review.json';value=copy.deepcopy(self.policy);change(value);p.write_text(json.dumps(value),encoding='utf8')
   with self.assertRaises(ValueError):(g.candidate if compiler else review.read)(p)
 def test_complete_source_lowering_and_existing_prefix(self):
  p=self.lowered;v=self.room;self.assertEqual((v['rules'],v['capabilities'],len(p['commands']),len(p['dialogues'])),(7,14,68,5))
  self.assertEqual([c['kind']for c in p['commands']].count('YieldIdle'),4)
  self.assertEqual([c['actor']for c in p['commands']if c['kind']=='RestoreActor'],['ninten'])
  self.assertEqual([c['actor']for c in p['commands']if c['kind']=='EraseActor'],['iggy'])
  self.assertFalse(any(c['kind']=='GrantKeyItem'for c in p['commands']))
  self.assertEqual(p['commands'][p['labels']['3']]['kind'],'QueueFreeStoryObject');self.assertEqual(p['commands'][p['labels']['11']]['kind'],'EraseActor')
  for c in p['commands']:
   if c['kind']=='ChangeCamera':self.assertEqual(p['commands'][p['commands'].index(c)+1]['kind'],'YieldIdle')
  wait=[c for c in p['commands']if c['kind']=='MoveActorPath'and c['actor']=='ninten'][0]['path']['entries'][0];self.assertEqual(wait,dict(kind=1,vector=[0,0],duration=.5))
  base=nc.parse_pack(tutorial.build()[0])
  for name,rows in base['sections'].items():self.assertEqual(v['sections'][name][:len(rows)],rows,name)
  self.assertEqual(v['strings'][:len(base['strings'])],base['strings'])
 def test_actual_bilingual_texts_and_voice(self):
  rows=g.texts(self.lowered,self.policy,self.ex);self.assertEqual(len(rows),10)
  self.assertEqual({r['locale']for r in rows},{'en','zh_Hans_CN'});self.assertEqual({r['id']for r in rows},set(range(3200,3205)))
  self.assertTrue(all(r['speaker']and r['body']and r['voice']=='Audio/Sound effects/text/Adult.mp3'for r in rows))
 def test_review_versions_fields_and_scalar_domain_reject(self):
  for change in [lambda p:p.update(schema=2),lambda p:p.update(schema=True),lambda p:p.update(kind='unknown'),lambda p:p.update(commit='0'*40),lambda p:p.update(scene=0),lambda p:p.update(scene=True),lambda p:p.update(scope=''),lambda p:p.update(unknown=0),lambda p:p.update(dialogue_first_id=True),lambda p:p.update(dialogue_first_id=0),lambda p:p.update(dialogue_first_id=999996)]:
   with self.subTest(change=change):self.mutant(change)
 def test_review_source_or_node_binding_reject(self):
  for change in [lambda p:p.update(source='../bad'),lambda p:p.update(npc='Objects/NPCS/missing'),lambda p:p.update(boundary='Cutscenes/Cutscene Area5'),lambda p:p.update(object='Objects/NPCS/npc18'),lambda p:p.update(documents={}),lambda p:p['sources'].update({p['source']:'0'*64}),lambda p:p['documents'][p['source']]['0'].update(unknown_feature=True),lambda p:p['documents'][p['source']]['3']['objectsfunction'].update({p['object']:'open'})]:
   with self.subTest(change=change):self.mutant(change)
 def test_scene_and_dialogue_namespace_collision_reject(self):
  self.mutant(lambda p:p.update(scene=3),True);self.mutant(lambda p:p.update(dialogue_first_id=3113),True)
 def test_duplicate_and_nonfinite_json_reject(self):
  with tempfile.TemporaryDirectory(prefix='shovel-policy-',dir=ROOT/'build')as folder:
   p=Path(folder).resolve()/'review.json';self.assertTrue(p.is_relative_to((ROOT/'build').resolve()))
   for text in ['{"schema":1,"schema":1}','{"schema":NaN}']:
    p.write_text(text,encoding='utf8')
    with self.assertRaises(ValueError):review.read(p)
 def test_unmapped_lowering_mechanisms_reject(self):
  changes=[lambda d:d['2']['actorsmove']['ninten']['movement'][0].update(unknown=1),lambda d:d['3'].update(objectsfunction={self.policy['object']:'queue_free()'}),lambda d:d['11'].update(eraseactors={'ninten':True}),lambda d:d['1'].update(changecam='missing'),lambda d:d['0'].update(givecash=1),lambda d:d['2'].pop('wait')]
  base=nc.parse_pack((ROOT/'romfs/world/podunk/podunk.encroom').read_bytes())
  for change in changes:
   value=copy.deepcopy(self.policy);change(value['documents'][value['source']])
   with self.subTest(change=change),self.assertRaises(ValueError):g.lowered(value,self.ex,base)
if __name__=='__main__':unittest.main()
