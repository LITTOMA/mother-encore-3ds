import copy,json,unittest
from tools import run_music_pause_reference as r
class MusicPauseReferenceTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.proof=r.source()[1]
  cls.result=json.loads((r.ROOT/'reports/mayor-office-import/music-pause-native.json').read_bytes())
 def test_reviewed_original_methods(self):
  self.assertEqual(self.proof,json.loads((r.ROOT/'compatibility/reviews/music-pause-v0410.json').read_bytes()))
  self.assertFalse(self.proof['whole_gameplay_approved'])
  self.assertTrue({'pause_all_music','resume_all_music'}<=self.proof['manager_methods'].keys())
 def test_exact_native_fixture_and_source_edges(self):
  self.assertEqual(r.fixture(self.result,self.proof),(r.ROOT/'tests/fixtures/music-pause.tsv').read_text(encoding='utf-8'))
  rows={row['case']['name']:row['views']for row in self.result['rows']}
  self.assertTrue(rows['playing_pause_new_battle'][-2][0]['paused'])
  self.assertFalse(rows['playing_pause_new_battle'][-2][1]['paused'])
  self.assertFalse(rows['remove_recreates_unpaused'][3][0]['paused'])
  self.assertFalse(rows['pause_finishes_fadeout'][-1][0]['queued'])
  self.assertTrue(rows['paused_fade_replaced_waiter'][-2][0]['queued'])
  self.assertEqual(rows['paused_fadeout_new_player_tween'][-3][0]['gain'],-5)
 def test_corrupt_result_rejects(self):
  changes=[lambda d:d.update(schema=True),lambda d:d['rows'].pop(),lambda d:d['rows'][0]['case']['actions'][0].append(1),lambda d:d['rows'][0]['views'].pop(),lambda d:d['rows'][0]['views'][0][0].update(id=True),lambda d:d['rows'][0]['views'][0][0].update(paused=1),lambda d:d['rows'][0]['views'][0][0].update(queued=0),lambda d:d['rows'][0]['views'][0][0].update(playing=1),lambda d:d['rows'][0]['views'][0][0].update(stream='res://unknown'),lambda d:d['rows'][0]['views'][0][0].update(gain=float('nan')),lambda d:d['rows'][0]['views'][0].append(copy.deepcopy(d['rows'][0]['views'][0][0]))]
  for change in changes:
   value=copy.deepcopy(self.result);change(value)
   with self.assertRaises(ValueError):r.validate_result(value,self.proof)
if __name__=='__main__':unittest.main()
