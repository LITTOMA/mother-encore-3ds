import copy,json,unittest
from tools import run_music_lifetime_reference as r
class MusicLifetimeReferenceTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.proof=r.source()[1]
  cls.result=json.loads((r.ROOT/'reports/mayor-office-import/music-player-lifetime-native.json').read_bytes())
 def test_unchanged_source_and_checked_scope(self):
  self.assertEqual(self.proof,json.loads((r.ROOT/'compatibility/reviews/music-player-lifetime-v0410.json').read_bytes()))
  self.assertFalse(self.proof['whole_gameplay_approved'])
  self.assertIn('clear_all_music',self.proof['manager_methods'])
 def test_native_lifetimes_and_fixture(self):
  self.assertEqual(r.fixture(self.result,self.proof),(r.ROOT/'tests/fixtures/music-player-lifetime.tsv').read_text(encoding='utf-8'))
  rows=self.result['rows']
  self.assertEqual(rows[0]['views'][-1]['voices'][0]['id'],1)
  self.assertEqual(rows[2]['views'][-1]['voices'],[])
  self.assertEqual(rows[3]['views'][-1]['voices'][0]['id'],1)
  self.assertFalse(rows[5]['views'][-2]['voices'][0]['playing'])
  self.assertTrue(rows[5]['views'][-2]['voices'][0]['queued'])
  self.assertEqual(rows[8]['views'][-1]['voices'][0]['gain'],6)
  early=rows[9]['views'][-3];self.assertEqual(early['waiting'],1)
  self.assertFalse(early['voices'][0]['queued'])
  self.assertEqual(early['voices'][0]['gain'],-80)
  self.assertEqual(rows[10]['views'][-1]['voices'][0]['id'],2)
 def test_bad_results_are_rejected(self):
  changes=[lambda d:d.update(schema=True),lambda d:d['rows'].pop(),lambda d:d['rows'][0]['case']['actions'][0].__setitem__(1,False),lambda d:d['rows'][0]['views'].pop(),lambda d:d['rows'][0]['views'][0].update(waiting=True),lambda d:d['rows'][0]['views'][0].update(elapsed=float('nan')),lambda d:d['rows'][0]['views'][0].update(elapsed=True),lambda d:d['rows'][0]['views'][0]['voices'][0].update(id=True),lambda d:d['rows'][0]['views'][0]['voices'][0].update(queued=1),lambda d:d['rows'][0]['views'][0]['voices'][0].update(playing=1),lambda d:d['rows'][0]['views'][0]['voices'][0].update(stream='res://unreviewed'),lambda d:d['rows'][0]['views'][0]['voices'].append(copy.deepcopy(d['rows'][0]['views'][0]['voices'][0])),lambda d:d['rows'][0]['views'][0]['voices'][0].update(gain=float('inf'))]
  for change in changes:
   bad=copy.deepcopy(self.result);change(bad)
   with self.assertRaises(ValueError):r.validate_result(bad,self.proof)
if __name__=='__main__':unittest.main()
