import copy,json,unittest
from tools import run_footstep_reference as r
class RunReferenceTests(unittest.TestCase):
 def test_exact_source_review(self):
  proof=r.source()[3];self.assertEqual(proof,json.loads((r.ROOT/'compatibility/reviews/run-loop-v0410.json').read_bytes()));self.assertIs(proof['whole_gameplay_approved'],False)
 def test_actual_result_and_negative_domain(self):
  result=json.loads((r.ROOT/'reports/mayor-office-import/run-loop-native.json').read_bytes());r.validate_result(result)
  for change in [lambda x:x.update(schema=True),lambda x:x['rows'].pop(),lambda x:x['rows'][0].update(wanted=3),lambda x:x['rows'][0].update(wanted=True),lambda x:x['rows'][0].update(playing=1),lambda x:x['rows'][0].update(action=[True,1,0,1,0]),lambda x:x['rows'][0].update(events=[[99,0]]),lambda x:x['rows'][0].update(events=[[1,True]])]:
   bad=copy.deepcopy(result);change(bad)
   with self.assertRaises(ValueError):r.validate_result(bad)
if __name__=='__main__':unittest.main()
