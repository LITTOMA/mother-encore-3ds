import copy,json,struct,tempfile,unittest
from pathlib import Path
from tools import mayor_story as m,native_content as nc
class MayorStory(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.policy,cls.ex,cls.rows=m.read();cls.room=nc.parse_pack((m.ROOT/'tests/fixtures/mayor-initial-story.encroom').read_bytes());cls.flags={cls.room['strings'][f['name_string']]:i for i,f in enumerate(cls.room['sections']['Flag'])}
 def test_actual_source_graph_and_candidate(self):
  commands,programmes,texts,groups=m.graph(self.policy,self.flags)
  self.assertEqual(commands,self.room['sections']['Command']);self.assertEqual([g['source_label']for g in groups],['0']);self.assertEqual(len(texts),7)
  blob,receipt=m.build_story(self.room)
  self.assertEqual(blob,(m.ROOT/'tests/fixtures/mayor-initial.encstory').read_bytes());self.assertEqual(receipt['unavailable_interactions'],5)
  self.assertEqual(struct.unpack_from('<I',blob,20)[0],3)
  pc=programmes[1]['first_command'];group=groups[0]
  self.assertEqual([commands[pc+o['target_pc']]['target_index']for o in group['options']],[2002,2003]);self.assertEqual(group['cancel_target_pc'],group['options'][1]['target_pc'])
 def test_pippi_flag_branch_is_after_first_text(self):
  commands,programmes,texts,groups=m.graph(self.policy,self.flags);p=programmes[3];cs=commands[p['first_command']:p['first_command']+p['command_count']]
  self.assertEqual([m.nc.OPCODES[c['opcode']]for c in cs[:5]],['BeginCutscene','ShowDialogue','AwaitDialogue','BranchFlag','Jump']);self.assertEqual(cs[3]['value'],0);self.assertEqual(cs[3]['target_index'],self.flags['courage_badge_received']);self.assertEqual(cs[3]['auxiliary_index'],5)
 def test_recipe_negative(self):
  for mutate in [lambda p:p.update(schema=True),lambda p:p.update(schema=2),lambda p:p.update(extra=1),lambda p:p.update(commit='0'*40),lambda p:p.update(scene_id=True),lambda p:p.update(dialogue_first_id=True),lambda p:p['programmes'].append(p['programmes'][0]),lambda p:p['sources'].pop('Scripts/Main/npc.gd'),lambda p:p['documents'][p['programmes'][0]]['0'].update(unported=True)]:
   p=copy.deepcopy(self.policy);mutate(p)
   with tempfile.TemporaryDirectory()as d:
    file=Path(d)/'candidate.json';file.write_text(json.dumps(p),encoding='utf8')
    with self.assertRaises(ValueError):m.read(file)
 def test_unknown_source_graph_mechanisms_fail(self):
  for mutate in [lambda p:p['documents'][p['programmes'][0]]['0'].update(wait=1),lambda p:p['documents'][p['programmes'][0]]['0'].update(goto='missing'),lambda p:p['documents'][p['programmes'][3]]['0']['if']['flags'].update(other=True),lambda p:p['documents'][p['programmes'][1]]['0']['options'].pop('cancel')]:
   p=copy.deepcopy(self.policy);mutate(p)
   with self.assertRaises(ValueError):m.graph(p,self.flags)
if __name__=='__main__':unittest.main()
