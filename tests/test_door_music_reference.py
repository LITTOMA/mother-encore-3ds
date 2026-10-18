import copy,json,unittest
from tools import run_door_music_reference as n
class DoorMusicReference(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.result=json.loads((n.ROOT/'reports/mayor-office-import/door-music-stop-native.json').read_bytes());cls.proof=n.source()[2];cls.review=json.loads((n.ROOT/'compatibility/reviews/door-music-stop-v0410.json').read_bytes())
 def test_native_live_array_mutation_and_guards(self):
  n.validate_review(self.review,self.proof);n.validate_result(self.result)
  for row in self.result['results']:
   expected=list(range(1,row['count'],2))if row['mode']==0 else list(range(row['count']))
   self.assertEqual(row['remaining'],expected)
  projection=''.join(str(x['count'])+'\t'+str(x['mode'])+'\t'+str(sum(1<<i for i in x['remaining']))+'\n'for x in self.result['results'])
  self.assertEqual(projection,(n.ROOT/'tests/fixtures/door-music-stop.tsv').read_text())
 def test_review_scope_or_source_changes_reject(self):
  for mutate in [lambda x:x.update(whole_handler_approved=True),lambda x:x.update(whole_handler_approved=0),lambda x:x.update(extra=1),lambda x:x.update(commit='0'*40),lambda x:x.update(prefix_sha256='0'*64)]:
   r=copy.deepcopy(self.review);mutate(r)
   with self.assertRaises(ValueError):n.validate_review(r,self.proof)
 def test_native_output_domain_negative(self):
  for mutate in [lambda x:x.update(schema=True),lambda x:x.update(schema=2),lambda x:x.update(extra=1),lambda x:x.update(godot=None),lambda x:x['godot'].update(string='other'),lambda x:x['results'].pop(),lambda x:x['results'][0].update(count=True),lambda x:x['results'][0].update(mode=3),lambda x:x['results'][0].update(remaining=[0]),lambda x:x['results'][9].update(remaining=[True]),lambda x:x['results'][18].update(remaining=[3,1])]:
   r=copy.deepcopy(self.result);mutate(r)
   with self.assertRaises(ValueError):n.validate_result(r)
if __name__=='__main__':unittest.main()
