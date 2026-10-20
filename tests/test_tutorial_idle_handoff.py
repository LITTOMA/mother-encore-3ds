"""The unchanged upstream actor/camera clauses yield a scene idle frame."""
import copy,json,tempfile,unittest
from pathlib import Path
from tools import graveyard_tutorial_programme as t

class TutorialIdleHandoff(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.p,cls.review,cls.source=t.read()
 def test_actor_ready_clause_has_source_idle_handoff(self):
  code=self.source.text('Scripts/UI/DialogueBox.gd')
  self.assertIn('yield(new_actor, "actor_ready")',code)
  self.assertIn('yield(get_tree(), "idle_frame")',code)
  prefix=self.p['commands'][:self.p['labels']['0']]
  self.assertEqual([c['kind']for c in prefix],['BeginCutscene']+['BindActor','ActorPersistent']*len(self.p['actors'])+['YieldIdle'])
  self.assertEqual(prefix[-1],dict(kind='YieldIdle',actor=None,phrase=0))
 def test_every_camera_change_yields_before_return_camera_or_text(self):
  commands=self.p['commands'];cameras=[i for i,c in enumerate(commands)if c['kind']=='ChangeCamera']
  self.assertEqual(len(cameras),sum('changecam'in row for row in self.review['documents'][self.p['source']].values()))
  for i in cameras:
   with self.subTest(index=i):self.assertEqual(commands[i+1],dict(kind='YieldIdle',actor=None,phrase=commands[i]['phrase']))
  self.assertEqual(sum(c['kind']=='YieldIdle'for c in commands),1+len(cameras))
 def test_missing_or_misowned_idle_handoff_rejected(self):
  indices=[i for i,c in enumerate(self.p['commands'])if c['kind']=='YieldIdle']
  self.assertTrue(indices)
  with tempfile.TemporaryDirectory(prefix='tutorial-idle-',dir=t.ROOT/'build')as directory:
   file=Path(directory)/'bad.json'
   for index in indices:
    for operation in ('remove','actor','phrase'):
     p=copy.deepcopy(self.p)
     if operation=='remove':p['commands'].pop(index)
     else:p['commands'][index][operation]='ninten'if operation=='actor'else 999
     file.write_text(json.dumps(p),encoding='utf8')
     with self.subTest(index=index,operation=operation),self.assertRaises(ValueError):t.read(file)

if __name__=='__main__':unittest.main()
