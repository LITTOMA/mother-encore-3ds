import copy,json,unittest
from tools import run_npc_return_reference as n

class NpcReturnReference(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.result=json.loads((n.ROOT/'reports/mayor-office-import/npc-return-native.json').read_bytes())
  cls.review=json.loads((n.ROOT/'compatibility/reviews/npc-return-v0410.json').read_bytes())
 def test_actual_native_domain_and_timer_gates(self):
  n.validate_review(self.review,'7d9246600fffe518408f5830d4848635019005a3');n.validate_result(self.result)
  expected=[[60],[140],[60,90],[],[70],[160],[],[]]
  for i,want in enumerate(expected):
   rows=[r for r in self.result['rows']if r[1]==i]
   got=[r[0]for j,r in enumerate(rows)if r[2:4]==[0,1]and(j==0 or rows[j-1][2:4]!=[0,1])]
   self.assertEqual(got,want,n.CASES[i]['name'])
  # Tick zero is the first subtraction: frame60 is the 61st timer tick.
  self.assertEqual(self.result['rows'][59*8][2:4],[1,0]);self.assertEqual(self.result['rows'][60*8][2:4],[0,1])
 def test_review_unknown_version_and_symbols_fail(self):
  for mutate in [lambda r:r.update(schema=True),lambda r:r.update(schema=2),lambda r:r.update(extra=1),lambda r:r.update(whole_file_approved=True),lambda r:r['symbols'].append('interact'),lambda r:r.update(commit='0'*40)]:
   r=copy.deepcopy(self.review);mutate(r)
   with self.assertRaises(ValueError):n.validate_review(r,self.review['commit'])
 def test_result_unknown_version_shape_and_domain_fail(self):
  for mutate in [lambda r:r.update(schema=True),lambda r:r.update(extra=1),lambda r:r['rows'].pop(),lambda r:r['rows'][0].__setitem__(0,1),lambda r:r['rows'][0].__setitem__(4,0),lambda r:r['rows'][0].__setitem__(2,True),lambda r:r['godot'].update(string='other'),lambda r:r['cases'][0].update(wander=True)]:
   r=copy.deepcopy(self.result);mutate(r)
   with self.assertRaises(ValueError):n.validate_result(r)

if __name__=='__main__':unittest.main()
