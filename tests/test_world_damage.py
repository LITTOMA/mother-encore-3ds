import copy,hashlib,json,struct,tempfile,unittest,zlib
from pathlib import Path
from unittest.mock import patch
from tools import world_damage as g
from tests import prepare_world_damage as fixture
class WorldDamageTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.recipe,cls.model,cls.proof=g.source();cls.blob=g.encode(cls.model);cls.root=g.ROOT/'build/tests';cls.root.mkdir(parents=True,exist_ok=True)
 def temporary(self):t=tempfile.TemporaryDirectory(prefix='world-damage-',dir=self.root);self.addCleanup(t.cleanup);return Path(t.name)
 def changed(self,at,value):
  b=bytearray(self.blob);struct.pack_into('<I',b,at,value);struct.pack_into('<I',b,16,zlib.crc32(b[32:]));return b
 def test_roundtrip(self):self.assertEqual(g.parse(self.blob),self.model);self.assertEqual(g.encode(g.parse(self.blob)),self.blob)
 def test_original_source_scope_and_end_keys(self):
  m=self.model;self.assertEqual(len(m['keys']),37);flash=next(c for c in m['clips']if c['name']=='Flash');self.assertTrue(any(k['time']>flash['duration']for k in m['keys'][flash['first']:flash['first']+flash['count']]));self.assertEqual(m['strings'][1:3],['Audio/Sound effects/Hurt 1.mp3','damage']);self.assertEqual(m['integers'][2:], [1,0,2,1,13]);self.assertEqual(m['numbers'][9:11],[32.,64.]);self.assertEqual(m['numbers'][16:],[50.,200.]);self.assertEqual({s['id']for s in m['statuses']},{'poisoned','unconscious'});self.assertEqual(next(s for s in m['statuses']if s['id']=='poisoned')['scroll'],1.5)
 def test_all_truncations_checksum_trailing(self):
  for n in range(len(self.blob)):
   with self.assertRaises(ValueError):g.parse(self.blob[:n])
  for b in[self.blob+b'\0',self.blob[:170]+bytes([self.blob[170]^1])+self.blob[171:]]:
   with self.assertRaises(ValueError):g.parse(b)
 def test_versions_directories_and_reserved(self):
  changes=[(0,0),(8,2),(12,0),(20,2),(24,1),(28,1),(32,0),(52,7),(56,1),(60,1)]
  for n in range(5):changes +=[(64+16*n,99),(68+16*n,0xffffffff),(72+16*n,0xffffffff),(76+16*n,0)]
  for at,value in changes:
   with self.subTest(at=at),self.assertRaises(ValueError):g.parse(self.changed(at,value))
 def test_negative_fields_types_domains(self):
  off=[struct.unpack_from('<I',self.blob,68+16*n)[0]for n in range(5)];policy,clips,keys,status=off[1:];changes=[(policy,0xffffffff),(policy+4,0),(policy+8,0),(policy+32,0),(policy+36,self.model['integers'][0]),(policy+40,0xffffffff),(policy+44,16),(policy+48,1),(policy+52,256),(policy+56,16),(policy+64+4,0x7ff80000),(policy+208+4,0x80000000),(clips,0),(clips+16,0x7fc00000),(clips+20,0),(clips+24,1),(keys,10),(keys+4,2),(keys+8,0x7fc00000),(keys+12,4),(keys+16,0x7fc00000),(keys+20,1),(status+8,16),(status+16,0),(status+24,101),(status+28,1)]
  for at,value in changes:
   with self.subTest(at=at,value=value),self.assertRaises(ValueError):g.parse(self.changed(at,value))
 def test_bad_models(self):
  changes=[lambda m:m['strings'].__setitem__(0,'../outside'),lambda m:m['integers'].__setitem__(0,True),lambda m:m['numbers'].__setitem__(0,float('nan')),lambda m:m['numbers'].__setitem__(1,0.),lambda m:m['clips'][0].update(first=1),lambda m:m['clips'][0].update(duration=0.),lambda m:m['clips'][1].update(name=m['clips'][0]['name']),lambda m:m['keys'][0].update(target=10),lambda m:m['keys'][0].update(type=2),lambda m:m['keys'][0].update(mode=2),lambda m:m['keys'][0].update(time=60.),lambda m:m['keys'][0].update(value=[1.,1.,0.,0.]),lambda m:m['statuses'][0].update(id=m['statuses'][1]['id']),lambda m:m['statuses'][0].update(flags=16),lambda m:m['statuses'][0].update(prob=101),lambda m:m['statuses'][0].update(scroll=float('inf')),lambda m:m['statuses'].append(copy.deepcopy(m['statuses'][0])),lambda m:m.update(extra=1)]
  for change in changes:
   m=copy.deepcopy(self.model);change(m)
   with self.assertRaises(ValueError):g.encode(m)
 def test_source_review_is_pinned(self):
  p=self.temporary()/'recipe.json';changes=[lambda v:v.update(schema=True),lambda v:v.update(commit='0'*40),lambda v:v.update(extra=1),lambda v:v['sources'].update({v['paths']['player']:'0'*64}),lambda v:v.update(engine_commit='0'*40),lambda v:v['engine_sources'].update({'core/math/math_funcs.h':'0'*64}),lambda v:v['clips'][0].update(id=v['clips'][1]['id']),lambda v:v['targets'][0].update(type=8),lambda v:v['statuses'][0].update(path='../outside')]
  for change in changes:
   v=copy.deepcopy(self.recipe);change(v);p.write_text(json.dumps(v))
   with self.assertRaises(ValueError):g.source(p)
  for raw in['{"schema":1,"schema":1}','{"schema":NaN}']:
   p.write_text(raw)
   with self.assertRaises(ValueError):g.read(p)
 def test_two_real_native_results(self):
  a=g.ROOT/'reports/world-damage/native1';b=g.ROOT/'reports/world-damage/native2';self.assertEqual((a/'reference.json').read_bytes(),(b/'reference.json').read_bytes());doc=json.loads((a/'reference.json').read_bytes());self.assertEqual(len(doc['rows']),361);self.assertEqual(doc['rows'][-1]['rng_next'],2150260653);self.assertEqual(doc['rows'][0]['events'][-1][0],'number');self.assertIsInstance(doc['rows'][0]['events'][-1][-1],list)
 def test_fixture_owned_repeatable(self):
  out=self.temporary()/'fixture';a=fixture.build(out);self.assertEqual(a,fixture.build(out));self.assertEqual(g.parse(a['world-damage.encwdm']),self.model);(out/'native.tsv').write_text('wrong')
  with self.assertRaises(ValueError):fixture.build(out)
  with self.assertRaises(ValueError):fixture.build(g.ROOT/'build')
 def test_review_and_result_receipt_fail_closed(self):
  review=json.loads(fixture.REVIEW.read_bytes());p=self.temporary()/'review.json'
  for change in[lambda v:v.update(schema=True),lambda v:v.update(producer_sha256='0'*64),lambda v:v.update(native_producer_sha256='0'*64),lambda v:v['results'][0].update(path='../outside'),lambda v:v['results'][0].update(sha256='0'*64),lambda v:v['source'].update(commit='0'*40),lambda v:v.update(extra=1)]:
   v=copy.deepcopy(review);change(v);p.write_text(json.dumps(v))
   with patch.object(fixture,'REVIEW',p),self.assertRaises(ValueError):fixture.build(self.temporary()/'fixture')
if __name__=='__main__':unittest.main()
