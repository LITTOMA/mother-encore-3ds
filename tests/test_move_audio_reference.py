import copy,json,unittest
from tools import run_move_audio_reference as r
class MoveAudioReferenceTests(unittest.TestCase):
 def test_exact_source_review(self):
  self.assertEqual(r.source()[2],json.loads((r.ROOT/'compatibility/reviews/move-audio-v0410.json').read_bytes()))
 def test_actual_native_result_and_fixture(self):
  result=json.loads((r.ROOT/'reports/mayor-office-import/move-audio-native.json').read_bytes());r.validate_result(result);self.assertEqual(r.fixture(result),(r.ROOT/'tests/fixtures/move-audio.tsv').read_text(encoding='utf-8'))
 def test_reject_unknown_truncated_or_changed_reference(self):
  result=json.loads((r.ROOT/'reports/mayor-office-import/move-audio-native.json').read_bytes())
  mutations=[lambda p:p.update(schema=True),lambda p:p['sequences'].pop(),lambda p:p['sequences'][0]['frames'].pop(),lambda p:p['sequences'][0]['frames'][0].update(calls=[True]),lambda p:p['sequences'][0]['frames'][0].update(calls=[4]),lambda p:p['sequences'][0]['frames'][0].update(calls=[1,2,3,1]),lambda p:p['sequences'][0]['frames'][0]['input'].update(toggle=0),lambda p:p['sequences'][0]['frames'][0]['state'].update(x=float('nan')),lambda p:p['sequences'][0]['frames'][0]['state'].update(running=1)]
  for mutate in mutations:
   p=copy.deepcopy(result);mutate(p)
   with self.assertRaises(ValueError):r.validate_result(p)
if __name__=='__main__':unittest.main()
