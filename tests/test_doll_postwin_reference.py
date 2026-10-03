"""Validate the original-engine oracle inputs and test-only C++ fixture identity."""
import copy,hashlib,json,unittest
from pathlib import Path
from tools import reference_doll_postwin_actions as ref
from tools.doll_dialogue import decode
from tools.doll_postwin import PIN,RECEIPT
from tools.reference_actor_actions import REVIEW

def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()

class DollPostwinReferenceTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.report=ref.ROOT/'reports/doll-postwin-actions-reference-3'
  cls.data=json.loads((cls.report/'reference.json').read_text())
  cls.native=decode(json.loads((ref.ROOT/RECEIPT).read_text()))
 def test_original_sources_method_hashes_and_case_domain(self):
  d=self.data;inventory=json.loads((ref.ROOT/'compatibility/upstream-inventory.json').read_text());review=json.loads(REVIEW.read_text())
  self.assertEqual(d['schema'],1);self.assertEqual(d['commit'],PIN);self.assertEqual(d['sources'],review['sources']);self.assertEqual(d['symbols'],review['function_sha256'])
  self.assertEqual(d['godot']['hash'],'3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8')
  for p,h in {**d['sources'],**d['additional_sources']}.items():
   self.assertEqual(inventory['files'][p]['sha256'],h);self.assertEqual(sha(ref.ROOT/'upstream/MOTHER-Encore'/p),h)
  self.assertEqual([r['definition']for r in d['cases']],ref.cases(self.native['yaml'][0]))
  self.assertEqual(sum(len(r['frames'])for r in d['cases']),2150)
 def test_fixture_and_recorded_pipeline_identity(self):
  receipt=json.loads((self.report/'receipt.json').read_text());path=ref.ROOT/'tests/fixtures/doll_postwin_actions_v0410.hpp'
  self.assertEqual(path.read_text(),ref.fixture(self.data));self.assertEqual(sha(path),receipt['fixture_sha256'])
  self.assertEqual(sha(self.report/'reference.json'),receipt['reference_sha256'])
  self.assertEqual(sha(Path(ref.__file__)),receipt['tool_sha256'])
  self.assertEqual(sha(ref.ROOT/'tools/godot_exporter/doll_actor_actions_probe.gd'),receipt['probe_sha256'])
 def test_fixture_rejects_incomplete_unknown_and_nonfinite_frames(self):
  for mutate in [lambda d:d['cases'][0]['frames'].pop(),lambda d:d['cases'][0]['frames'][0].update(extra=1),lambda d:d['cases'][0]['frames'][0]['position'].__setitem__(0,float('nan'))]:
   data=copy.deepcopy(self.data);mutate(data)
   with self.assertRaises(ValueError):ref.fixture(data)
 def test_extended_cleanup_includes_equal_value_signals(self):
  path=ref.ROOT/'reports/doll-postwin-cleanup-reference-3';d=json.loads((path/'reference.json').read_text());receipt=json.loads((path/'receipt.json').read_text())
  self.assertEqual(sha(path/'reference.json'),receipt['reference_sha256'])
  self.assertEqual(sha(ref.ROOT/'tools/reference_doll_postwin_cleanup.py'),receipt['tool_sha256'])
  self.assertEqual(sha(ref.ROOT/'tools/godot_exporter/doll_postwin_cleanup_probe.gd'),receipt['probe_sha256'])
  completions=[]
  for c in d['cases']:
   self.assertEqual(len(c['frames']),120);self.assertTrue(c['frames'][-1]['replacement_visible']);self.assertFalse(c['frames'][-1]['proxy_exists'])
   completions.append(next(e['frame']for e in c['events']if e['event']=='update_npcs_completed'))
   self.assertEqual(c['frames'][1]['position'],c['definition']['position'])
  self.assertEqual(completions,[15,12,3,12,3])
  for c in [d['cases'][0],d['cases'][1],d['cases'][3]]:
   events=[e for e in c['events']if e['event']=='replacement_frame_changed']
   self.assertGreater(len(events),2);self.assertEqual(events[0]['sprite'],events[1]['sprite'])

if __name__=='__main__':unittest.main()
