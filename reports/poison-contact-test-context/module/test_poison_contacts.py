import copy,hashlib,json,struct,tempfile,unittest,zlib
from pathlib import Path
from unittest.mock import patch
from tools import poison_contacts as g,run_poison_contact_reference as native
from tests import prepare_poison_contacts as fixture

class PoisonContactTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.recipe,cls.model,cls.proof=g.source();cls.blob=g.encode(cls.model);cls.files,cls.copied,cls.native_proof=native.source();cls.root=g.ROOT/'build/tests';cls.root.mkdir(parents=True,exist_ok=True);cls.first=g.ROOT/'reports/poison-contacts/native1';cls.doc=json.loads((cls.first/'reference.json').read_bytes());cls.receipt=json.loads((cls.first/'receipt.json').read_bytes())
 def temporary(self):
  t=tempfile.TemporaryDirectory(prefix='poison-contacts-',dir=self.root);self.addCleanup(t.cleanup);return Path(t.name)
 def changed(self,at,value):
  b=bytearray(self.blob);struct.pack_into('<I',b,at,value);struct.pack_into('<I',b,16,zlib.crc32(b[32:]));return b
 def test_roundtrip_source_geometry_and_unchanged_nested_format(self):
  self.assertEqual(g.parse(self.blob),self.model);self.assertEqual(g.encode(g.parse(self.blob)),self.blob);self.assertEqual((len(self.model['body']),len(self.model['areas']),sum(len(v['parts'])for v in self.model['areas'])),(8,11,22));self.assertEqual(g.poison.parse(self.model['poison'])['source'],self.model['source']);self.assertEqual(len(self.blob),8088)
 def test_all_truncations_and_checksum(self):
  for n in range(len(self.blob)):
   with self.assertRaises(ValueError):g.parse(self.blob[:n])
  b=bytearray(self.blob);b[-1]^=1
  for bad in[b,self.blob+b'\0']:
   with self.assertRaises(ValueError):g.parse(bad)
 def test_versions_domains_and_directory_ownership(self):
  changes=[(0,0),(8,0),(8,2),(12,0),(20,0),(20,2),(24,0),(28,1),(32,0),(52,7),(56,1),(60,1)]
  for n in range(6):changes.extend((64+16*n+4*field,0xffffffff)for field in range(4))
  for at,value in changes:
   with self.subTest(at=at,value=value),self.assertRaises(ValueError):g.parse(self.changed(at,value))
 def test_policy_string_and_geometry_spans(self):
  pool,policy,nested,areas,parts,points=[struct.unpack_from('<I',self.blob,68+16*n)[0]for n in range(6)]
  changes=[(policy,0xffffffff),(policy+4,0),(policy+8,0),(policy+16,0x100000),(policy+20,0x100000),(policy+24,1),(policy+28,2),(policy+28,65),(areas,0),(areas+4,1),(areas+8,0),(areas+12,1),(parts,1),(parts+4,0),(parts+8,2),(parts+8,65),(parts+12,1),(points,0x7fc00000)]
  for at,value in changes:
   with self.subTest(at=at,value=value),self.assertRaises(ValueError):g.parse(self.changed(at,value))
 def test_model_types_nonconvex_foreign_vertex_and_nested_identity(self):
  changes=[lambda m:m.update(scene=True),lambda m:m.update(layer=True),lambda m:m.update(mask=0x100000),lambda m:m.update(source='../outside'),lambda m:m.update(player='/absolute'),lambda m:m.update(poison=bytearray(m['poison'])),lambda m:m['body'].__setitem__(1,m['body'][0][:]),lambda m:m['body'][0].__setitem__(0,float('nan')),lambda m:m['areas'].pop(),lambda m:m['areas'][0].update(id=True),lambda m:m['areas'][0].update(extra=1),lambda m:m['areas'][0]['parts'][0][0].__setitem__(0,m['areas'][0]['parts'][0][0][0]+1),lambda m:m['areas'][0]['parts'].pop(),lambda m:m.update(extra=1)]
  for change in changes:
   value=copy.deepcopy(self.model);change(value)
   with self.subTest(change=change),self.assertRaises(ValueError):g.encode(value)
  for field,value in[('scene',self.model['scene']+1),('source','fixture/other.tscn')]:
   m=copy.deepcopy(self.model);m[field]=value
   with self.assertRaises(ValueError):g.encode(m)
 def test_changed_source_recipe_and_dependency_reject(self):
  p=self.temporary()/'recipe.json';changes=[lambda v:v.update(schema=True),lambda v:v.update(commit='0'*40),lambda v:v.update(poison_recipe='../outside'),lambda v:v.update(poison_sha256='0'*64),lambda v:v.update(player_sha256='0'*64),lambda v:v.update(player_reader_sha256='0'*64),lambda v:v.update(geometry_producer_sha256='0'*64),lambda v:v.update(extra=1)]
  for change in changes:
   v=copy.deepcopy(self.recipe);change(v);p.write_text(json.dumps(v))
   with self.assertRaises(ValueError):g.source(p)
 def test_actual_two_original_runs_and_generated_observer_identity(self):
  self.assertEqual((self.first/'reference.json').read_bytes(),(g.ROOT/'reports/poison-contacts/native2/reference.json').read_bytes());self.assertEqual(len(self.doc['rows']),105);self.assertEqual(native.validate_result(self.doc,self.receipt),self.doc);self.assertEqual(set(self.copied),{'Audio/Sound effects/Hurt 1.mp3'})
 def test_unknown_observations_and_changed_receipts_reject(self):
  # These cases mutate only the result/receipt. Admit the actual source once,
  # then exercise every payload check against that fixed source observation.
  # The independent positive test still calls the complete uncached validator.
  damage_source=native.damage.damage.source();poison_source=native.poison.source();steps_source=native.steps.source()
  with patch.object(native,'source',return_value=(self.files,self.copied,self.native_proof)),patch.object(native.damage.damage,'source',return_value=damage_source),patch.object(native.poison,'source',return_value=poison_source),patch.object(native.steps,'source',return_value=steps_source):
   changes=[lambda v:v.update(schema=True),lambda v:v['rows'].pop(),lambda v:v['rows'][0].update(extra=1),lambda v:v['rows'][0].update(paused=1),lambda v:v['rows'][0].update(phase='unknown'),lambda v:v['rows'][0].update(continuous=1),lambda v:v['rows'][0].update(layer=0x100000),lambda v:v['rows'][0].update(contacts=[[0,True,True],[0,True,True]]),lambda v:v['rows'][0].update(overlaps=[]),lambda v:v['rows'][0].update(events=[['unknown']])]
   for change in changes:
    v=copy.deepcopy(self.doc);change(v)
    with self.subTest(change=change),self.assertRaises(ValueError):native.validate_result(v,self.receipt)
   for field in['generated_sha256','copied','scope','poison','player']:
    v=copy.deepcopy(self.receipt);v[field]='changed'
    with self.subTest(field=field),self.assertRaises(ValueError):native.validate_result(self.doc,v)
 def test_owned_fixture_repeatability_and_corruption_reject(self):
  out=self.temporary()/'fixture';a=fixture.build(out);self.assertEqual(a,fixture.build(out));self.assertEqual(a['contacts.encpcn'],self.blob);(out/'native.tsv').write_text('changed')
  with self.assertRaises(ValueError):fixture.build(out)
  with self.assertRaises(ValueError):fixture.build(g.ROOT/'build')
 def test_review_and_protocol_changes_fail_closed(self):
  base=json.loads(fixture.REVIEW.read_bytes());p=self.temporary()/'review.json';changes=[lambda v:v.update(schema=True),lambda v:v.update(native_producer_sha256='0'*64),lambda v:v.update(producer_sha256='0'*64),lambda v:v.update(fixture_protocol_sha256='0'*64),lambda v:v['results'][0].update(path='../outside'),lambda v:v['results'][0].update(sha256='0'*64),lambda v:v.update(extra=1)]
  for change in changes:
   v=copy.deepcopy(base);change(v);p.write_text(json.dumps(v))
   with patch.object(fixture,'REVIEW',p),self.assertRaises(ValueError):fixture.build(self.temporary()/'fixture')

if __name__=='__main__':unittest.main()
