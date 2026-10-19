import copy,json,unittest
from tools import run_music_all_reference as r
class MusicAllReferenceTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.proof=r.source()[1]
  cls.result=json.loads((r.ROOT/'reports/mayor-office-import/music-all-native.json').read_bytes())
 def test_reviewed_method_and_exact_fixture(self):
  self.assertEqual(self.proof,json.loads((r.ROOT/'compatibility/reviews/music-all-v0410.json').read_bytes()))
  self.assertEqual(r.fixture(self.result,self.proof),(r.ROOT/'tests/fixtures/music-all.tsv').read_text(encoding='utf-8'))
  self.assertFalse(self.proof['whole_gameplay_approved'])
  self.assertIn('fadeout_all_music',self.proof['manager_methods'])
 def test_source_completion_waiters_and_stopped_tracks(self):
  rows=self.result['rows'];self.assertEqual(rows[1]['views'][-1][0]['id'],2)
  self.assertFalse(rows[6]['views'][-1][0]['queued'])
  self.assertEqual(rows[7]['views'][-1][0]['id'],1)
  self.assertEqual(rows[2]['views'][-3][1]['gain'],0)
 def test_corrupt_result_rejected(self):
  for mutate in [lambda d:d.update(schema=True),lambda d:d['rows'].pop(),lambda d:d['rows'][0]['views'].pop(),lambda d:d['rows'][0]['views'][0][0].update(id=True),lambda d:d['rows'][0]['views'][0][0].update(paused=1),lambda d:d['rows'][0]['views'][0][0].update(gain=float('nan'))]:
   d=copy.deepcopy(self.result);mutate(d)
   with self.assertRaises(ValueError):r.validate_result(d,self.proof)
if __name__=='__main__':unittest.main()
