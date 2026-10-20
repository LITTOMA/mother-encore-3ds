import copy,hashlib,unittest
from pathlib import Path
from tests.prepare_story_object_delete import build
from tools import native_content as c
class StoryObjectDeleteTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.blob,cls.proof=build();cls.decoded=c.parse_pack(cls.blob)
 def ir(self):
  v=copy.deepcopy(self.decoded);v.pop('build_input_sha256');p=Path(__file__).resolve().parent/'prepare_story_object_delete.py';v['provenance']={'sources':{'tests/prepare_story_object_delete.py':hashlib.sha256(p.read_bytes()).hexdigest()}};return v
 def command(self,v):
  p=v['sections']['Program'][self.proof['programme']];return v['sections']['Command'][p['first_command']+3]
 def test_source_ordering_and_codec(self):
  v=self.decoded;p=v['sections']['Program'][self.proof['programme']];commands=v['sections']['Command'][p['first_command']:p['first_command']+p['command_count']]
  self.assertEqual((v['rules'],v['capabilities']),(7,14));self.assertEqual((self.proof['flag_label'],self.proof['delete_label']),('0','3'))
  self.assertEqual([c.OPCODES[x['opcode']]for x in commands],['BeginCutscene','SetFlag','YieldIdle','QueueFreeStoryObject','YieldIdle','CutsceneEnded','DialogueDone'])
  self.assertEqual(v['strings'][commands[3]['target_index']],'Objects/Items/Item');self.assertEqual(commands[3]['phrase'],3);c.validate_ir(self.ir())
 def test_old_unknown_or_unused_capability_rejected(self):
  for cap in (7,8,9,10,11,12,13,15,True):
   v=self.ir();v['capabilities']=cap
   with self.subTest(cap=cap),self.assertRaises(ValueError):c.validate_ir(v)
  v=self.ir();self.command(v).update(opcode=c.OPCODES.index('HideDialogue'),target_index=c.NONE)
  with self.assertRaises(ValueError):c.validate_ir(v)
 def test_unknown_operation_or_operand_rejected(self):
  for edit in [{'opcode':50},{'actor_index':0},{'target_index':c.NONE},{'target_index':0},{'auxiliary_index':0},{'flags':1},{'vector':[1,0]},{'value':1},{'duration':1}]:
   v=self.ir();self.command(v).update(edit)
   with self.subTest(edit=edit),self.assertRaises(ValueError):c.validate_ir(v)
 def test_object_path_rejected(self):
  for path in ('','/Objects/Items/Item','Objects/Items/Item/','../Item','Objects//Item','Objects/./Item','Objects/a..b','Objects\\Items','Objects:Items','Objects\0Items','x'*513,'Objects/铲子'):
   v=self.ir();v['strings'].append(path);self.command(v)['target_index']=len(v['strings'])-1
   with self.subTest(path=path),self.assertRaises(ValueError):c.validate_ir(v)
if __name__=='__main__':unittest.main()
