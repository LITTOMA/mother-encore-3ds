"""Manual-only source/parser cases; not registered or executed automatically."""
import copy,unittest
from tools import field_programme as p
class FieldProgrammeSourceCases(unittest.TestCase):
 def test_real_sources_and_binary(self):
  d=p.load();b=p.encode(d);self.assertEqual(b[:8],b'ENCFPG01');self.assertEqual(d['source_npc']['id'],p.stable(d['source_npc']['node']))
 def test_unknown_op_rejected(self):
  d=copy.deepcopy(p.load());d['programs'][0]['commands'][1]['kind']='UnknownSourceMethod'
  with self.assertRaises(ValueError):p.encode(d)
 def test_branch_target_rejected(self):
  d=copy.deepcopy(p.load());c=next(c for program in d['programs']for c in program['commands']if c['kind']=='Jump');c['target_pc']=999999
  with self.assertRaises(ValueError):p.encode(d)
 def test_unknown_text_control_rejected(self):
  d=copy.deepcopy(p.load());d['texts'][0]['segments']['en'][0]['tokens'][0]['kind']='UnknownSourceControl'
  with self.assertRaises(ValueError):p.encode(d)
 def test_source_pending_branch_never_supported(self):
  d=copy.deepcopy(p.load());row=next(r for r in d['source_npc']['dialogues']if not r['supported']);row['supported']=True
  with self.assertRaises(ValueError):p.encode(d)
if __name__=='__main__':unittest.main()
