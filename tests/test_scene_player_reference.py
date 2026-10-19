import copy,json,unittest
from tools import run_scene_player_reference as r
class ScenePlayerReferenceTests(unittest.TestCase):
 def test_review_matches_exact_source_and_parent_transforms(self):
  self.assertEqual(r.source()[-1],json.loads((r.ROOT/'compatibility/reviews/scene-player-v0410.json').read_bytes()))
 def test_native_results_generate_transport_fixture(self):
  doc=json.loads((r.ROOT/'reports/mayor-office-import/scene-player-native.json').read_bytes())
  self.assertEqual(r.fixture(doc),(r.ROOT/'tests/fixtures/scene-player.tsv').read_text(encoding='utf-8'))
  self.assertEqual(doc['rows'][2]['direction'],[0,-1]);self.assertEqual(doc['rows'][0]['local_position'],[136,809]);self.assertEqual(doc['rows'][0]['global_position'],[236,759])
 def test_reject_corrupt_results_and_false_identity(self):
  doc=json.loads((r.ROOT/'reports/mayor-office-import/scene-player-native.json').read_bytes())
  mutations=[lambda p:p.update(schema=True),lambda p:p['rows'].pop(),lambda p:p['rows'][0].update(same_player=False),lambda p:p['rows'][0].update(same_camera=1),lambda p:p['rows'][0].update(old_freed=False),lambda p:p['rows'][0].update(disabled_during=False),lambda p:p['rows'][0]['after'].update(tap=1),lambda p:p['rows'][0]['after'].update(speed=True),lambda p:p['rows'][0]['after'].update(run_sound='unknown'),lambda p:p['rows'][0]['after'].update(velocity=[float('nan'),0]),lambda p:p['rows'][0]['case'].update(scene=True),lambda p:p['rows'][0]['case'].update(direction=[0,True]),lambda p:p['rows'][0].update(left=[[0,True]]),lambda p:p['rows'][0].update(global_position=[136,809]),lambda p:p['rows'][2].update(direction=[0,0]),lambda p:p['rows'][0].update(enabled_after=False)]
  for mutate in mutations:
   candidate=copy.deepcopy(doc);mutate(candidate)
   with self.assertRaises(ValueError):r.validate_result(candidate)
if __name__=='__main__':unittest.main()
