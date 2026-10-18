import copy,json,unittest
from tools import run_music_finished_reference as r
class MusicFinishedReferenceTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.proof=r.source()[1]
  cls.result=json.loads((r.ROOT/'reports/mayor-office-import/music-finished-native.json').read_bytes())
 def test_unchanged_source_and_bounded_mapping(self):
  self.assertEqual(self.proof,json.loads((r.ROOT/'compatibility/reviews/music-finished-v0410.json').read_bytes()))
  self.assertFalse(self.proof['whole_gameplay_approved'])
  self.assertEqual([v['loop']for v in self.proof['imports']],[False,True,False])
 def test_source_callbacks_and_exact_fixture(self):
  self.assertEqual(r.fixture(self.result,self.proof),(r.ROOT/'tests/fixtures/music-finished.tsv').read_text(encoding='utf-8'))
  rows=self.result['rows']
  self.assertEqual(rows[0]['views'][-1][0]['id'],1)
  self.assertTrue(rows[1]['views'][-1][0]['stream'].endswith('/House.mp3'))
  self.assertFalse(rows[2]['views'][-1][0]['playing'])
  self.assertEqual(rows[3]['views'][-1][1]['id'],2)
  self.assertFalse(rows[3]['views'][-1][0]['playing'])
  self.assertTrue(rows[3]['views'][-1][1]['playing'])
  self.assertTrue(rows[5]['views'][-1][0]['playing'])
  self.assertTrue(rows[6]['views'][-1][0]['stream'].endswith('/House.mp3'))
  self.assertTrue(rows[7]['views'][2][0]['remove'])
  self.assertEqual(rows[7]['views'][-1][0]['stream'],'')
 def test_bad_results_reject(self):
  changes=[lambda d:d.update(schema=True),lambda d:d['rows'].pop(),lambda d:d['rows'][0]['case']['actions'][0].__setitem__(1,False),lambda d:d['rows'][0]['views'].pop(),lambda d:d['imports'][0].update(loop=1),lambda d:d['imports'][1].update(offset=0),lambda d:d['rows'][0]['views'][0][0].update(id=True),lambda d:d['rows'][0]['views'][0][0].update(index=True),lambda d:d['rows'][0]['views'][0][0].update(index=0),lambda d:d['rows'][0]['views'][0][0].update(remove=1),lambda d:d['rows'][0]['views'][0][0].update(queued=1),lambda d:d['rows'][0]['views'][0][0].update(gain=float('nan')),lambda d:d['rows'][0]['views'][0][0].update(stream='res://unreviewed'),lambda d:d['rows'][0]['views'][0][0].update(loop='res://unreviewed'),lambda d:d['rows'][0]['views'][0].append(copy.deepcopy(d['rows'][0]['views'][0][0]))]
  for change in changes:
   value=copy.deepcopy(self.result);change(value)
   with self.assertRaises(ValueError):r.validate_result(value,self.proof)
if __name__=='__main__':unittest.main()
