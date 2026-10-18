import copy,json,unittest
from tools import run_music_player_reference as r
class MusicPlayerReferenceTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.proof=r.source()[1]
  cls.result=json.loads((r.ROOT/'reports/mayor-office-import/music-player-choice-native.json').read_bytes())
 def test_exact_original_source_and_mapped_scope(self):
  self.assertEqual(self.proof,json.loads((r.ROOT/'compatibility/reviews/music-player-choice-v0410.json').read_bytes()))
  self.assertFalse(self.proof['whole_gameplay_approved'])
 def test_native_choices_and_generated_fixture(self):
  self.assertEqual(r.fixture(self.result,self.proof),(r.ROOT/'tests/fixtures/music-player-choice.tsv').read_text(encoding='utf-8'))
  oldest=self.result['rows'][5]
  self.assertEqual(oldest['after']['attached'],oldest['before']['voices'][0]['id'])
  self.assertEqual(self.result['rows'][6]['after']['runtime'],1.5)
  self.assertEqual(self.result['rows'][6]['half']['voices'][0]['gain'],.625)
  self.assertEqual(self.result['rows'][7]['half']['voices'][0]['gain'],3)
  self.assertTrue(self.result['rows'][10]['after']['voices'][0]['stream'].endswith('Pollyanna.ogg'))
 def test_reject_changed_or_malformed_results(self):
  changes=[lambda p:p.update(schema=True),lambda p:p['rows'].pop(),lambda p:p['parameters'].update(fade_to_seconds=True),lambda p:p['imports'][0].update(loop=1),lambda p:p['imports'][0].update(offset=0),lambda p:p['rows'][0]['after']['voices'][0].update(id=True),lambda p:p['rows'][0]['after']['voices'][0].update(playing=1),lambda p:p['rows'][0]['after']['voices'][0].update(gain=float('nan')),lambda p:p['rows'][0]['after']['voices'][0].update(stream='res://unreviewed'),lambda p:p['rows'][0]['before'].update(registered=['front']),lambda p:p['rows'][0]['after'].update(attached=-1),lambda p:p['rows'][0]['after'].update(runtime=True),lambda p:p['rows'][1]['case']['seed'][0].__setitem__(1,1),lambda p:p['rows'][5]['before']['voices'].pop(),lambda p:p['rows'][0]['after']['voices'].append(copy.deepcopy(p['rows'][0]['after']['voices'][0]))]
  for change in changes:
   value=copy.deepcopy(self.result);change(value)
   with self.assertRaises(ValueError):r.validate_result(value,self.proof)
if __name__=='__main__':unittest.main()
