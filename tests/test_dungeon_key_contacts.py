import copy,hashlib,json,sys,tempfile,unittest
from pathlib import Path
from unittest import mock
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import run_dungeon_key_contact_reference as r
from tests.prepare_dungeon_key_contacts import build,SOURCE

class KeyContactTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.observed=json.loads((SOURCE/'contacts.json').read_text('utf8'));cls.config=json.loads((SOURCE/'input.json').read_text('utf8'))
 def test_independent_native_runs_and_hashes(self):
  self.assertEqual(self.observed,json.loads((ROOT/'reports/dungeon-key-contacts/native-4/contacts.json').read_text('utf8')));r.validate_observed(self.observed,self.config)
  for n in[3,4]:
   p=ROOT/f'reports/dungeon-key-contacts/native-{n}';receipt=json.loads((p/'receipt.json').read_text('utf8'));self.assertEqual(receipt['cases'],14);self.assertEqual(receipt['rows'],27)
   for field,name in[('contacts_sha256','contacts.json'),('input_sha256','input.json')]:self.assertEqual(receipt[field],hashlib.sha256((p/name).read_bytes()).hexdigest())
 def test_source_paused_other_disabled_and_saved_outcomes(self):
  rows={(self.config['cases'][row['case']]['name'],row['phase']):row['count']for row in self.observed['rows']}
  self.assertEqual([rows['paused_entry_consumed',i]for i in range(5)],[0,0,0,1,1]);self.assertEqual([rows['other_body_ignored',i]for i in range(2)],[0,1]);self.assertEqual([rows['disabled_body',i]for i in range(3)],[0,1,1]);self.assertEqual(rows['saved_initial_overlap',0],0)
  self.assertEqual(rows['filter_0_0',0],0);self.assertEqual(rows['filter_2_0',0],0);self.assertEqual(rows['filter_0_4353',0],1)
 def test_bad_initial_coverage_and_unknown_observations_rejected(self):
  for mutate in[lambda v:v.pop('initials'),lambda v:v['initials'][0].update(count=1),lambda v:v['rows'].pop(),lambda v:v['rows'][0].update(unknown=True),lambda v:v['rows'][0]['states'][0].update(audio=1),lambda v:v['rows'][0]['flags'].__setitem__(0,1)]:
   value=copy.deepcopy(self.observed);mutate(value)
   with self.assertRaises(ValueError):r.validate_observed(value,self.config)
 def test_player_exact_source_polygon_and_unknown_geometry_rejected(self):
  player,proof=r.player_source();self.assertEqual(len(player['points']),8);self.assertEqual(player['center'],[-.5,6]);self.assertEqual(player['mask'],4353);self.assertEqual(proof['sha256'],hashlib.sha256((ROOT/'upstream/MOTHER-Encore'/proof['path']).read_bytes()).hexdigest());review,ex=r.field_swing.read()
  class Changed:
   def __getattr__(self,name):return getattr(ex,name)
   def text(self,name):return ex.text(name).replace('[node name="CollisionShape2D" type="CollisionPolygon2D" parent="."]','[node name="CollisionShape2D" type="CollisionPolygon2D" parent="."]\nrotation = 0.2',1)if name==review['scene']else ex.text(name)
  with mock.patch.object(r.field_swing,'read',return_value=(review,Changed())):
   with self.assertRaises(ValueError):r.player_source()
 def test_fixture_retains_all_native_rows(self):
  with tempfile.TemporaryDirectory(dir=ROOT/'build')as folder:
   build(Path(folder));self.assertEqual(len((Path(folder)/'reference.tsv').read_text().splitlines()),27)
 def test_shared_core_build_lists(self):
  source='runtime/dungeon_key_contacts.cpp';self.assertIn(source,(ROOT/'CMakeLists.txt').read_text());self.assertIn(source,(ROOT/'platform/ctr/Makefile').read_text());self.assertNotIn('3ds.h',(ROOT/source).read_text())
if __name__=='__main__':unittest.main()
