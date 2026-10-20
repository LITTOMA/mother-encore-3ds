import copy,json,tempfile,unittest
from pathlib import Path
from tools import native_content as c,graveyard_tutorial as t
from tests.prepare_key_grant import build

class KeyGrant(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.policy,_=t.read();cls.packet,cls.proof=build();cls.decoded=c.parse_pack(cls.packet)
 def ir(self):
  ir=copy.deepcopy(self.decoded);ir.pop('build_input_sha256');ir['provenance']={'sources':{'tests/fixtures/pre-key-grant-room.encroom':self.proof['base_sha256']}};return ir
 def test_source_grant_and_later_completion_are_separate(self):
  v=self.decoded;s=v['sections'];p=s['Program'][self.proof['programme']];cs=s['Command'][p['first_command']:p['first_command']+p['command_count']]
  self.assertEqual((v['rules'],v['capabilities']),(7,13));self.assertEqual([c.OPCODES[x['opcode']]for x in cs],['BeginCutscene','GrantKeyItem','YieldIdle','SetFlag','CutsceneEnded','DialogueDone'])
  self.assertEqual(v['strings'][cs[1]['target_index']],'BatOld');self.assertEqual(cs[1]['value'],1);self.assertEqual(self.proof['grant']['label'],'22');self.assertEqual(self.proof['grant']['completion_label'],'29');self.assertEqual(v['strings'][s['Flag'][cs[3]['target_index']]['name_string']],'bat')
  c.validate_ir(self.ir());self.assertIn('NOT a playable',self.proof['scope'])
 def test_capability_downgrade_and_unknown_reject(self):
  for cap in (7,8,9,10,11,12,14,True):
   ir=self.ir();ir['capabilities']=cap
   with self.subTest(cap=cap),self.assertRaises(ValueError):c.validate_ir(ir)
  ir=self.ir();p=ir['sections']['Program'][self.proof['programme']];g=ir['sections']['Command'][p['first_command']+1];g.update(opcode=c.OPCODES.index('HideDialogue'),target_index=c.NONE,value=0)
  with self.assertRaises(ValueError):c.validate_ir(ir)
 def test_typed_operand_rejection(self):
  for change in ({'value':0},{'value':2},{'value':.5},{'value':float('nan')},{'actor_index':0},{'target_index':c.NONE},{'target_index':0},{'auxiliary_index':0},{'flags':1},{'vector':[1,0]},{'duration':1},{'opcode':49}):
   ir=self.ir();p=ir['sections']['Program'][self.proof['programme']];ir['sections']['Command'][p['first_command']+1].update(change)
   with self.subTest(change=change),self.assertRaises(ValueError):c.validate_ir(ir)
 def test_unknown_review_and_false_early_flag_reject(self):
  changes=[lambda p:p.update(schema=2),lambda p:p.update(schema=True),lambda p:p.update(unknown=0),lambda p:p.update(commit='0'*40),lambda p:p.update(scene=0),lambda p:p.update(scope=''),lambda p:p.update(npc='Objects/NPCS/missing'),lambda p:p.update(boundary='Cutscenes/Cutscene Area7'),lambda p:p.update(source='../bad'),lambda p:p['item_grants'][0].update(doses=True),lambda p:p['item_grants'][0].update(doses=2),lambda p:p['item_grants'][0].update(item='AsthmaSpray'),lambda p:p['item_grants'][0].update(completion_label='22'),lambda p:p['item_grants'][0].update(completion_flag='invented'),lambda p:p['sources'].update({p['source']:'0'*64})]
  with tempfile.TemporaryDirectory()as folder:
   path=Path(folder)/'review.json'
   for change in changes:
    p=copy.deepcopy(self.policy);change(p);path.write_text(json.dumps(p),encoding='utf8')
    with self.subTest(change=change),self.assertRaises(ValueError):t.read(path)
   path.write_text('{"schema":1,"schema":1}',encoding='utf8')
   with self.assertRaises(ValueError):t.read(path)
if __name__=='__main__':unittest.main()
