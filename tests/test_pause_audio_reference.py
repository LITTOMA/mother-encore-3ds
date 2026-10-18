import copy,json,unittest
from tools import run_pause_audio_reference as r
class PauseAudioReferenceTests(unittest.TestCase):
 def test_exact_source_review(self):self.assertEqual(r.source()[2],json.loads((r.ROOT/'compatibility/reviews/pause-audio-v0410.json').read_bytes()))
 def test_actual_result_and_generated_fixture(self):
  doc=json.loads((r.ROOT/'reports/mayor-office-import/pause-audio-native.json').read_bytes());self.assertEqual(r.fixture(doc),(r.ROOT/'tests/fixtures/pause-audio.tsv').read_text(encoding='utf-8'))
 def test_reject_invalid_result(self):
  doc=json.loads((r.ROOT/'reports/mayor-office-import/pause-audio-native.json').read_bytes())
  for mutate in [lambda p:p.update(schema=True),lambda p:p['rows'].pop(),lambda p:p['rows'][0].update(events=[[True,0]]),lambda p:p['rows'][0].update(events=[[1,True]]),lambda p:p['rows'][0].update(events=[[4,0]]),lambda p:p['rows'][0].update(events=[[1,0],[1,0]]),lambda p:p['rows'][0].update(running=1),lambda p:p['rows'][0].update(action=['run',1])]:
   p=copy.deepcopy(doc);mutate(p)
   with self.assertRaises(ValueError):r.validate_result(p)
if __name__=='__main__':unittest.main()
