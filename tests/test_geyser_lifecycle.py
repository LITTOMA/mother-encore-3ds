import copy
import hashlib
import json
import struct
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from tools import catacombs_geyser_lifecycle as family
from tools import run_geyser_factory_reference as native
from tools import run_geyser_map_factory_reference as mapped
from tests import prepare_geyser_lifecycle as fixture


class GeyserLifecycleTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.recipe,cls.model,cls.proof=family.source()
  cls.blob=family.encode(cls.model)
  cls.folder=family.ROOT/'reports/geyser-lifecycle'
  cls.docs={name:json.loads((cls.folder/name/'reference.json').read_bytes()) for name in ['native1','map-native1']}
  cls.receipts={name:json.loads((cls.folder/name/'receipt.json').read_bytes()) for name in cls.docs}
  cls.scratch=family.ROOT/'build/tests';cls.scratch.mkdir(parents=True,exist_ok=True)

 def temporary(self):
  owned=tempfile.TemporaryDirectory(prefix='geyser-lifecycle-',dir=self.scratch)
  self.addCleanup(owned.cleanup)
  return Path(owned.name)

 def test_independent_resource_roundtrip_and_nested_identity(self):
  self.assertEqual(family.parse(self.blob),self.model)
  self.assertEqual(len(self.blob),8140)
  self.assertEqual(family.p.parse(self.model['poison'])['source'],self.model['source'])
  self.assertEqual(family.g.parse(self.model['geyser'])['source'],self.model['prefab'])

 def test_every_truncation_rejects(self):
  for size in range(len(self.blob)):
   with self.assertRaises(ValueError):family.parse(self.blob[:size])

 def reject_word(self,at,value):
  bad=bytearray(self.blob);struct.pack_into('<I',bad,at,value)
  if at>=32:struct.pack_into('<I',bad,16,family.p.f.zlib.crc32(bad[32:]))
  with self.assertRaises(ValueError):family.parse(bytes(bad))

 def test_unknown_versions_capabilities_and_directory(self):
  for at,value in [(8,0),(8,2),(20,0),(20,2),(24,0),(24,65536),(28,1),(52,3),(52,5),(56,1),(60,1),(64,2),(68,0),(72,0),(76,2),(80,1),(88,2),(92,31),(104,0),(120,0)]:
   self.reject_word(at,value)

 def test_checksum_pin_padding_and_exact_coverage(self):
  for at in [0,32,len(self.blob)-1]:
   bad=bytearray(self.blob);bad[at]^=1
   if at==32:struct.pack_into('<I',bad,16,family.p.f.zlib.crc32(bad[32:]))
   with self.assertRaises(ValueError):family.parse(bytes(bad))
  with self.assertRaises(ValueError):family.parse(self.blob+b'\0')

 def test_policy_spans_alias_reserved_and_unsafe_text(self):
  policy=struct.unpack_from('<I',self.blob,84)[0]
  pool=struct.unpack_from('<I',self.blob,68)[0]
  for at,value in [(policy,0xffffffff),(policy+4,0),(policy+8,0),(policy+24,1),(policy+28,1)]:self.reject_word(at,value)
  for char in [0,10,ord(':'),ord('\\'),128]:
   bad=bytearray(self.blob);bad[pool]=char;struct.pack_into('<I',bad,16,family.p.f.zlib.crc32(bad[32:]))
   with self.assertRaises(ValueError):family.parse(bytes(bad))

 def test_model_identity_types_and_nested_versions(self):
  edits=[lambda m:m.update(scene=True),lambda m:m.update(scene=0),lambda m:m.update(scene=m['scene']+1),lambda m:m.update(extra=1),lambda m:m.update(source='changed'),lambda m:m.update(prefab='changed'),lambda m:m.update(parent='../Objects'),lambda m:m.update(parent='a//b'),lambda m:m.update(parent=''),lambda m:m.update(poison=bytearray(m['poison']))]
  for edit in edits:
   bad=copy.deepcopy(self.model);edit(bad)
   with self.assertRaises(ValueError):family.encode(bad)
  for key in ['poison','geyser']:
   for at in [8,20]:
    bad=copy.deepcopy(self.model);raw=bytearray(bad[key]);struct.pack_into('<I',raw,at,2);bad[key]=bytes(raw)
    with self.assertRaises(ValueError):family.encode(bad)

 def test_reviewed_source_binding_rejects_changes(self):
  path=self.temporary()/'recipe.json'
  for edit in [lambda r:r.update(schema=True),lambda r:r.update(commit='0'*40),lambda r:r.update(extra=1),lambda r:r.update(poison_recipe='../outside'),lambda r:r.update(poison_recipe_sha256='0'*64),lambda r:r.update(geyser_recipe_sha256='0'*64),lambda r:r.update(hole_sha256='0'*64),lambda r:r.update(timeout_sha256='0'*64),lambda r:r.update(hole_source='Scripts/unknown.gd')]:
   bad=copy.deepcopy(self.recipe);edit(bad);path.write_text(json.dumps(bad))
   with self.assertRaises((ValueError,FileNotFoundError)):family.source(path)

 def test_two_actual_runs_for_each_tree_order(self):
  for name,other,observer in [('native1','native2',native),('map-native1','map-native2',mapped)]:
   self.assertEqual((self.folder/name/'reference.json').read_bytes(),(self.folder/other/'reference.json').read_bytes())
   self.assertEqual(observer.validate_result(self.docs[name],self.receipts[name]),self.docs[name])
   self.assertEqual(len(self.docs[name]['rows']),720)
   self.assertEqual(self.docs[name]['spawn_count'],97)
  self.assertNotEqual(self.docs['native1'],self.docs['map-native1'])

 def test_unknown_native_fields_operations_and_boolean_aliases(self):
  edits=[lambda d:d.update(schema=True),lambda d:d.update(extra=1),lambda d:d.update(spawn_count=True),lambda d:d['rows'].pop(),lambda d:d['rows'][0].update(extra=1),lambda d:d['rows'][0].update(tick=False),lambda d:d['rows'][0].update(delta=0),lambda d:d['rows'][0].update(actions=[['unknown',True]]),lambda d:d['rows'][0]['before'].update(ui=[0,False,False]),lambda d:d['rows'][0]['before'].update(events=[['unknown']]),lambda d:d['rows'][0]['before']['clocks'][0].__setitem__(2,0),lambda d:d['rows'][0]['before'].update(global_calls=[['unknown']])]
  for edit in edits:
   bad=copy.deepcopy(self.docs['native1']);edit(bad)
   with self.assertRaises(ValueError):native.validate_result(bad,self.receipts['native1'])

 def test_changed_generated_input_or_source_receipt_rejects(self):
  for name,observer in [('native1',native),('map-native1',mapped)]:
   for key in ['generated_sha256','copied_source','scope']:
    bad=copy.deepcopy(self.receipts[name]);bad[key]='changed'
    with self.assertRaises(ValueError):observer.validate_result(self.docs[name],bad)
  with self.assertRaises(ValueError):native.validate_result(self.docs['map-native1'],self.receipts['map-native1'])

 def test_native_actor_lifetime_and_commands_fail_closed(self):
  first=next(i for i,r in enumerate(self.docs['native1']['rows']) if r['after']['actors'])
  edits=[lambda a:a.update(ticket=0),lambda a:a.update(ready=1),lambda a:a.update(position=[0,0]),lambda a:a.update(frame=999),lambda a:a.update(time=0xffffffff),lambda a:a.update(commands=[['unknown']]),lambda a:a.update(commands=[['stream','res://unknown']]),lambda a:a.update(commands=[['play',True]]),lambda a:a.update(queued=True)]
  for edit in edits:
   bad=copy.deepcopy(self.docs['native1']);edit(bad['rows'][first]['after']['actors'][0])
   with self.assertRaises(ValueError):native.validate_result(bad,self.receipts['native1'])

 def test_frozen_fixture_is_repeatable_and_owned(self):
  out=self.temporary()/'fixture';first=fixture.build(out)
  self.assertEqual(first,fixture.build(out))
  self.assertTrue(first['native.tsv'].startswith(b'2\n720 97 '))
  (out/'native.tsv').write_bytes(b'changed')
  with self.assertRaises(ValueError):fixture.build(out)
  with self.assertRaises(ValueError):fixture.build(family.ROOT/'build')
  unowned=self.temporary()
  with self.assertRaises(ValueError):fixture.build(unowned)

 def test_unknown_review_profiles_and_replaced_producers_reject(self):
  original=json.loads(fixture.REVIEW.read_bytes());path=self.temporary()/'review.json'
  for edit in [lambda r:r.update(schema=True),lambda r:r.update(extra=1),lambda r:r.update(producer_sha256='0'*64),lambda r:r.update(native_producer_sha256='0'*64),lambda r:r.update(map_native_producer_sha256='0'*64),lambda r:r['results'].pop(),lambda r:r['results'][0].update(profile='unknown'),lambda r:r['results'][0].update(sha256='0'*64),lambda r:r['results'][0].update(path='../outside')]:
   bad=copy.deepcopy(original);edit(bad);path.write_text(json.dumps(bad))
   with patch.object(fixture,'REVIEW',path),self.assertRaises(ValueError):fixture.build(self.temporary()/'fixture')


if __name__=='__main__':unittest.main()
