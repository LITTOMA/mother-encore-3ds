import copy,hashlib,json,tempfile,unittest
from pathlib import Path
from unittest.mock import patch
from tools import world_damage as d,run_poison_damage_reference as native
from tests import prepare_poison_damage as fixture,prepare_world_damage as original
class PoisonDamageTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.files,cls.copied,cls.proof=native.source();cls.root=d.ROOT/'build/tests';cls.root.mkdir(parents=True,exist_ok=True);cls.first=d.ROOT/'reports/poison-damage/native1';cls.doc=json.loads((cls.first/'reference.json').read_bytes());cls.receipt=json.loads((cls.first/'receipt.json').read_bytes())
 def temporary(self):t=tempfile.TemporaryDirectory(prefix='poison-damage-',dir=self.root);self.addCleanup(t.cleanup);return Path(t.name)
 def test_actual_combined_original_repeatability(self):
  other=(d.ROOT/'reports/poison-damage/native2/reference.json').read_bytes();self.assertEqual((self.first/'reference.json').read_bytes(),other);self.assertEqual(len(self.doc['rows']),284);self.assertEqual(native.validate_result(self.doc,self.receipt),self.doc);self.assertEqual(set(self.copied),{'Audio/Sound effects/Hurt 1.mp3'})
 def test_both_damage_sources_and_paused_overlap_outcomes(self):
  rows=self.doc['rows'];self.assertTrue(any(v['action'][0]=='geyser'and v['continuous']and v['damage']==5 for v in rows));self.assertTrue(any(v['action'][0]=='area'and v['continuous']and v['damage']==4 for v in rows));self.assertTrue(any(v['paused']and not v['continuous']for v in rows));self.assertLess(rows[-1]['hp'],rows[0]['hp']);self.assertTrue(all(v['hp']>=1 for v in rows));self.assertEqual(rows[-1]['rng_next'],826935087)
 def test_binary_delta_descriptor_not_display_decimal(self):
  for row in self.doc['rows']:
   if row['action'][0]=='tick':self.assertIsInstance(row['action'][1],int);self.assertEqual(row['action'][1],1015580809)
 def test_unknown_and_missing_observed_paths_reject(self):
  edits=[lambda v:v.update(schema=True),lambda v:v['rows'].pop(),lambda v:v['rows'][0].update(extra=1),lambda v:v['rows'][0].update(continuous=1),lambda v:v['rows'][0].update(damage=-1),lambda v:v['rows'][0].update(run_sound='unknown'),lambda v:v['rows'][0].update(shadow='unknown'),lambda v:v['rows'][1].update(action=['tick',0]),lambda v:v['rows'][0].update(properties=[]),lambda v:v['rows'][0].update(events=[['unsupported']]),lambda v:v['rows'][0].update(label='duplicate'),lambda v:v['rows'][-1].pop('rng_next')]
  for edit in edits:
   value=copy.deepcopy(self.doc);edit(value)
   with self.assertRaises(ValueError):native.validate_result(value,self.receipt)
 def test_changed_dependency_or_observer_rejects(self):
  for key in['damage','poison','steps','generated_sha256','copied','scope']:
   value=copy.deepcopy(self.receipt);value[key]='changed'
   with self.assertRaises(ValueError):native.validate_result(self.doc,value)
 def test_fresh_owned_fixture_and_corruption(self):
  out=self.temporary()/'fixture';a=fixture.build(out);self.assertEqual(a,fixture.build(out));(out/'native.tsv').write_text('changed')
  with self.assertRaises(ValueError):fixture.build(out)
  with self.assertRaises(ValueError):fixture.build(d.ROOT/'build')
 def test_bad_review_and_protocol_dependency_fail_closed(self):
  base=json.loads(fixture.REVIEW.read_bytes());p=self.temporary()/'review.json'
  for edit in[lambda v:v.update(schema=True),lambda v:v.update(native_producer_sha256='0'*64),lambda v:v.update(fixture_protocol_sha256='0'*64),lambda v:v['results'][0].update(path='../outside'),lambda v:v['results'][0].update(sha256='0'*64),lambda v:v.update(extra=1)]:
   v=copy.deepcopy(base);edit(v);p.write_text(json.dumps(v))
   with patch.object(fixture,'REVIEW',p),self.assertRaises(ValueError):fixture.build(self.temporary()/'fixture')
 def test_old_world_fixture_bytes_unchanged(self):
  out=self.temporary()/'original';built=original.build(out);cached=d.ROOT/'build/full-port/world-damage-fixture-2'
  # Original receipts expose no machine-local requirement in normal CI.
  if cached.is_dir():
   for name,raw in built.items():self.assertEqual(raw,(cached/name).read_bytes())
  self.assertEqual(hashlib.sha256(built['native.tsv']).hexdigest(),'ef2c3be0050ff236b812bd3d3758f1b7ef6ced45f214497c7be524646efa488c')
if __name__=='__main__':unittest.main()
