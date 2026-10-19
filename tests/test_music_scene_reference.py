import copy,json,unittest
from tools import run_music_scene_reference as r
class MusicSceneReferenceTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.proof=r.source()[1]
  cls.result=json.loads((r.ROOT/'reports/mayor-office-import/music-scene-native.json').read_bytes())
 def test_exact_source_and_bounded_scope(self):
  self.assertEqual(self.proof,json.loads((r.ROOT/'compatibility/reviews/music-scene-graph-v0410.json').read_bytes()))
  self.assertFalse(self.proof['whole_gameplay_approved'])
  self.assertEqual([len(v)for v in self.proof['scenes']],[13,1])
 def test_native_scene_graph_and_fixture(self):
  self.assertEqual(r.fixture(self.result,self.proof),(r.ROOT/'tests/fixtures/music-scenes.tsv').read_text(encoding='utf-8'))
  rows=self.result['rows']
  self.assertEqual(rows[0]['views'][4]['voices'][1]['gain'],-80)
  self.assertEqual(len(rows[1]['views'][6]['voices']),3)
  self.assertEqual(rows[1]['views'][-1]['voices'][0]['id'],2)
  self.assertEqual(rows[2]['views'][-1]['voices'][0]['id'],0)
  self.assertEqual(rows[2]['views'][-1]['voices'][0]['gain'],6)
  self.assertEqual(rows[0]['views'][3]['exits'],list(reversed([r['path']for r in self.proof['scenes'][0]])))
  self.assertEqual(rows[4]['views'][5]['registered'],['Music/MusicArea3'])
 def test_reject_unknown_and_malformed_results(self):
  changes=[lambda d:d.update(schema=True),lambda d:d['rows'].pop(),lambda d:d['rows'][0]['case']['actions'][0].__setitem__(1,True),lambda d:d['rows'][0]['views'].pop(),lambda d:d['rows'][0]['views'][0]['voices'][0].update(id=True),lambda d:d['rows'][0]['views'][0]['voices'][0].update(playing=1),lambda d:d['rows'][0]['views'][0]['voices'][0].update(queued=1),lambda d:d['rows'][0]['views'][0]['voices'][0].update(gain=float('nan')),lambda d:d['rows'][0]['views'][0]['voices'][0].update(stream='res://unreviewed'),lambda d:d['rows'][0]['views'][1]['areas'][0].update(attached=True),lambda d:d['rows'][0]['views'][1]['areas'][0].update(inside=1),lambda d:d['rows'][0]['views'][1]['areas'][0].update(path='Unknown'),lambda d:d['rows'][0]['views'][1]['registered'].append('Unknown'),lambda d:d['rows'][0]['views'][3]['exits'].append('Unknown'),lambda d:d['rows'][0]['views'][0]['voices'].append(copy.deepcopy(d['rows'][0]['views'][0]['voices'][0]))]
  for change in changes:
   value=copy.deepcopy(self.result);change(value)
   with self.assertRaises(ValueError):r.validate_result(value,self.proof)
if __name__=='__main__':unittest.main()
