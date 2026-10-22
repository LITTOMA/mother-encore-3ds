import copy,hashlib,json,tempfile,unittest
from pathlib import Path
from unittest.mock import patch
from tools import run_geyser_contact_reference as native
from tests import prepare_geyser_contacts as fixture

class GeyserContactTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.files,cls.copied,cls.source=native.source();cls.config=json.loads(cls.files['input.json']);cls.first=native.ROOT/'reports/geyser-contacts/native1';cls.doc=json.loads((cls.first/'reference.json').read_bytes());cls.receipt=json.loads((cls.first/'receipt.json').read_bytes());cls.root=native.ROOT/'build/tests';cls.root.mkdir(parents=True,exist_ok=True)
 def temporary(self):
  t=tempfile.TemporaryDirectory(prefix='geyser-contacts-',dir=self.root);self.addCleanup(t.cleanup);return Path(t.name)
 def case(self,name):
  ci=next(i for i,c in enumerate(self.config['cases'])if c['name']==name);return{row['phase']:row for row in self.doc['rows']if row['case']==ci}
 def test_actual_two_native_runs_and_media_source_receipts(self):
  self.assertEqual((self.first/'reference.json').read_bytes(),(native.ROOT/'reports/geyser-contacts/native2/reference.json').read_bytes());self.assertEqual(len(self.doc['rows']),72);self.assertEqual(native.validate_result(self.doc,self.receipt),self.doc);self.assertEqual(len(self.doc['geometry']),4);self.assertEqual(len({tuple(p)for p in self.doc['geometry']}),4);self.assertEqual(len([n for n in self.copied if n.endswith('.mp3')]),4)
 def test_queue_free_and_animation_exit_are_distinct_source_boundaries(self):
  ordinary=self.case('ordinary');self.assertTrue(ordinary['reenter']['continuous']);self.assertTrue(ordinary['before_retire']['continuous']);self.assertEqual(ordinary['before_retire']['contacts'],[]);self.assertTrue(ordinary['before_retire']['geyser']['queued']);self.assertFalse(ordinary['retired']['continuous']);self.assertEqual(ordinary['retired']['contacts'],[[False,True]])
  animation=self.case('animation_retirement');self.assertFalse(animation['before_retire']['continuous']);self.assertEqual(animation['before_retire']['contacts'],[[False,True]]);self.assertEqual(animation['retired']['contacts'],[]);self.assertFalse(animation['retired']['geyser']['alive'])
 def test_pause_entry_consumed_and_exit_unconditional(self):
  paused=self.case('paused_entry');self.assertEqual(paused['enter']['contacts'],[[True,True]]);self.assertFalse(paused['enter']['continuous']);self.assertFalse(paused['stay']['continuous']);self.assertFalse(paused['hit']['continuous']);self.assertTrue(paused['reenter']['continuous']);exiting=self.case('paused_exit');self.assertTrue(exiting['leave']['paused']);self.assertFalse(exiting['leave']['continuous']);self.assertEqual(exiting['leave']['contacts'],[[False,True]])
 def test_monitoring_filters_other_body_and_reactivation(self):
  monitoring=self.case('monitoring_stop');self.assertTrue(monitoring['enter']['continuous']);self.assertEqual(monitoring['stay']['contacts'],[[False,True]]);self.assertFalse(monitoring['stay']['geyser']['monitoring']);self.assertFalse(monitoring['stay']['continuous']);other=self.case('other_body');self.assertEqual(other['enter']['contacts'],[[True,False]]);self.assertTrue(all(not v['continuous']for v in other.values()));filtered=self.case('zero_filter');self.assertEqual(filtered['enter']['contacts'],[]);self.assertTrue(filtered['reenter']['continuous']);disabled=self.case('disabled_shape');self.assertEqual(disabled['enter']['contacts'],[]);self.assertEqual(disabled['stay']['contacts'],[[True,True]])
 def test_declared_base_signals_are_emitted_and_original_handlers_used(self):
  self.assertIn('func observed_pause(value):',self.files['base.gd']);self.assertIn('emit_signal("paused")',self.files['base.gd']);self.assertIn('emit_signal("unpaused")',self.files['base.gd']);path=native.geyser.g.source()[0]['paths']['script'];self.assertIn('body.start_continuous_damage',self.files[path]);self.assertIn('body.stop_continuous_damage()',self.files[path]);self.assertIn('res://geyser_audio.gd',self.files['geyser.tscn'])
 def test_unknown_observation_versions_fields_and_body_phases_reject(self):
  changes=[lambda v:v.update(schema=True),lambda v:v.update(godot=[]),lambda v:v['rows'].pop(),lambda v:v['rows'][0].update(extra=1),lambda v:v['rows'][0].update(phase='unknown'),lambda v:v['rows'][0].update(paused=1),lambda v:v['rows'][0].update(paused=True),lambda v:v['rows'][0].update(layer=0),lambda v:v['rows'][0].update(position=[0,0]),lambda v:v['rows'][0].update(contacts=[[True,True],[True,True]]),lambda v:v['rows'][0].update(contacts=[[True,False]]),lambda v:v['rows'][0].update(geyser={'alive':1}),lambda v:v['rows'][0]['geyser'].update(frame=999),lambda v:v['rows'][0]['geyser'].update(queued=True),lambda v:v['rows'][0]['geyser'].update(extra=1),lambda v:v['rows'][0].update(events=[['unknown']]),lambda v:v['geometry'][0].__setitem__(0,0x7f800000)]
  for change in changes:
   d=copy.deepcopy(self.doc);change(d)
   with self.subTest(change=change),self.assertRaises(ValueError):native.validate_result(d,self.receipt)
 def test_changed_source_dependencies_or_observers_reject(self):
  for field in ['character','geyser','generated_sha256','copied','dependencies','scope']:
   d=copy.deepcopy(self.receipt);d[field]='changed'
   with self.subTest(field=field),self.assertRaises(ValueError):native.validate_result(self.doc,d)
 def test_owned_fixture_and_resource_corruption_preserve_failure(self):
  out=self.temporary()/'fixture';first=fixture.build(out);self.assertEqual(first,fixture.build(out));self.assertEqual(first['body.encpcn'][:8],b'ENCPCN01');self.assertEqual(first['family.encglf'][:8],b'ENCGLF01');(out/'native.tsv').write_text('changed')
  with self.assertRaises(ValueError):fixture.build(out)
  with self.assertRaises(ValueError):fixture.build(native.ROOT/'build')
 def test_reviewed_native_protocol_or_path_change_rejects(self):
  raw=json.loads(fixture.REVIEW.read_bytes());path=self.temporary()/'review.json';changes=[lambda v:v.update(schema=True),lambda v:v.update(native_producer_sha256='0'*64),lambda v:v.update(fixture_protocol_sha256='0'*64),lambda v:v['results'][0].update(path='../outside'),lambda v:v['results'][0].update(sha256='0'*64),lambda v:v.update(extra=1)]
  for change in changes:
   d=copy.deepcopy(raw);change(d);path.write_text(json.dumps(d))
   with patch.object(fixture,'REVIEW',path),self.assertRaises(ValueError):fixture.build(self.temporary()/'fixture')
 def test_unknown_or_missing_actual_engine_receipt_rejects(self):
  original=Path.read_bytes;target=self.first/'receipt.json'
  for missing in [False,True]:
   receipt=copy.deepcopy(self.receipt)
   if missing:receipt.pop('engine_sha256')
   else:receipt['engine_sha256']='0'*64
   def read(path):return json.dumps(receipt).encode()if path==target else original(path)
   with patch.object(Path,'read_bytes',read),self.assertRaises(ValueError):fixture.build(self.temporary()/'fixture')

if __name__=='__main__':unittest.main()
