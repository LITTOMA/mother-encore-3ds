import copy,hashlib,json,struct,tempfile,unittest,zlib
from pathlib import Path
from tools import catacombs_geyser as g,run_geyser_reference as native
from tests import prepare_geyser_timeline as fixture
class GeyserTimelineTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.recipe,cls.model,cls.proof=g.source();cls.blob=g.encode(cls.model);cls.root=g.ROOT/'build/tests';cls.root.mkdir(parents=True,exist_ok=True);cls.native=g.ROOT/'reports/geyser-timeline/native-1';cls.doc=json.loads((cls.native/'reference.json').read_bytes());cls.receipt=json.loads((cls.native/'receipt.json').read_bytes())
 def temporary(self):
  temporary=tempfile.TemporaryDirectory(prefix='geyser-',dir=self.root);self.addCleanup(temporary.cleanup);return Path(temporary.name)
 def changed(self,at,value):
  b=bytearray(self.blob);struct.pack_into('<I',b,at,value);struct.pack_into('<I',b,16,zlib.crc32(b[32:]));return b
 def test_original_source_keys_and_media(self):
  m=self.model;self.assertEqual((m['frames'],len(m['keys']),len(m['cues'])),(32,48,3));self.assertEqual(m['root'],[0.,-8.]);self.assertEqual(m['offset'],[-1.,-31.]);self.assertEqual(m['size'],[20.,50.]);self.assertEqual(m['slot'],'geyser');self.assertEqual(m['bus'],'SFX');self.assertEqual(m['notify'],[-10.,-10.,20.,20.]);self.assertEqual(m['normal_gain'],0);self.assertEqual(m['muted_gain'],-80)
 def test_roundtrip(self):self.assertEqual(g.parse(self.blob),self.model);self.assertEqual(g.encode(g.parse(self.blob)),self.blob)
 def test_truncations_checksum_trailing(self):
  for n in range(len(self.blob)):
   with self.assertRaises(ValueError):g.parse(self.blob[:n])
  for blob in [self.blob+b'\0',self.blob[:170]+bytes([self.blob[170]^1])+self.blob[171:]]:
   with self.assertRaises(ValueError):g.parse(blob)
 def test_versions_directory_and_reserved(self):
  changes=[(0,0),(8,2),(12,0),(20,2),(24,1),(28,1),(32,0),(52,7),(56,1),(60,1)]
  for n in range(6):changes += [(64+16*n,99),(68+16*n,0xffffffff),(72+16*n,0xffffffff),(76+16*n,0)]
  for at,value in changes:
   with self.subTest(at=at,value=value),self.assertRaises(ValueError):g.parse(self.changed(at,value))
 def test_parser_operands_domains(self):
  policy,clips,keys,cues,sounds=[struct.unpack_from('<I',self.blob,at)[0]for at in[84,100,116,132,148]];changes=[(policy,0xffffffff),(policy+4,0),(policy+32,0),(policy+36,self.model['spewing']),(policy+40,0),(policy+44,256),(policy+48,2),(policy+60,0x100000),(policy+68,0),(policy+76,2),(policy+80,0x7fc00000),(policy+104,0),(policy+132,0),(policy+172,1),(keys,0x7fc00000),(keys+4,4),(keys+8,999),(keys+12,2),(cues,0x7f800000),(cues+4,3),(cues+8,0),(cues+12,0x7fc00000),(cues+16,0xbf800000),(cues+20,32),(cues+24,1),(sounds,0),(sounds+12,0x7f800000),(sounds+16,4),(sounds+20,1)]
  changes += [(clips+at,0xffffffff)for at in range(0,32,4)]
  for at,value in changes:
   with self.subTest(at=at,value=value),self.assertRaises(ValueError):g.parse(self.changed(at,value))
 def test_bad_model(self):
  changes=[lambda m:m.update(frames=True),lambda m:m.update(spewing=m['reset']),lambda m:m.update(monitorable=1),lambda m:m.update(centered=1),lambda m:m.update(root=[float('nan'),0]),lambda m:m.update(size=[0.,50.]),lambda m:m.update(max_distance=0.),lambda m:m.update(pitch=0.),lambda m:m.update(panning=5.),lambda m:m.update(source='../outside'),lambda m:m['keys'][0].update(kind=4),lambda m:m['cues'][0].update(kind=3),lambda m:m['cues'][0].update(sound=0),lambda m:m['sounds'][0].update(length=0.),lambda m:m['sounds'][1].update(id=m['sounds'][0]['id']),lambda m:m['clips'][0].update(first=1),lambda m:m['clips'][1].update(cue_count=99)]
  for change in changes:
   m=copy.deepcopy(self.model);change(m)
   with self.assertRaises(ValueError):g.encode(m)
 def test_multiple_or_mixed_target_tracks_rejected(self):
  for index,track in [(1,0),(2,3)]:
   m=copy.deepcopy(self.model);m['cues'][index]['track']=track
   with self.assertRaises(ValueError):g.encode(m)
 def test_changed_source_review(self):
  file=self.temporary()/'review.json';changes=[lambda p:p.update(schema=True),lambda p:p.update(commit='0'*40),lambda p:p.update(extra=1),lambda p:p.update(poison_recipe_sha256='0'*64),lambda p:p['sources'].update({p['paths']['script']:'0'*64}),lambda p:p['symbols']['script'].update(play_rumble='0'*64),lambda p:p['sounds'][0].update(source='../outside'),lambda p:p['native_lengths'].clear(),lambda p:p.update(engine_commit='0'*40)]
  for change in changes:
   p=copy.deepcopy(self.recipe);change(p);file.write_text(json.dumps(p))
   with self.assertRaises(ValueError):g.source(file)
  for raw in ['{"schema":1,"schema":1}','{"schema":NaN}']:
   file.write_text(raw)
   with self.assertRaises(ValueError):g.read(file)
 def test_native_repeatability(self):
  second=json.loads((g.ROOT/'reports/geyser-timeline/native-2/reference.json').read_bytes());self.assertEqual(self.doc,second);self.assertEqual(len(self.doc['rows']),451);native.validate_result(self.doc,self.receipt)
 def test_native_negative(self):
  changes=[lambda d:d.update(schema=True),lambda d:d['rows'].pop(),lambda d:d['rows'][0].update(phase='bad'),lambda d:d['rows'][0].update(frame=32),lambda d:d['rows'][0].update(on_screen=False),lambda d:d['rows'][1].update(delta=0),lambda d:d['geometry'].update(centered=False),lambda d:d['meta'].clear(),lambda d:d['rows'][0].update(commands=[['unknown']]),lambda d:d['rows'][0].update(global_calls=[['bad','bad']])]
  for change in changes:
   d=copy.deepcopy(self.doc);change(d)
   with self.assertRaises(ValueError):native.validate_result(d,self.receipt)
 def test_fixture_reproducible_and_owned(self):
  out=self.temporary()/'fixture';a=fixture.build(out);self.assertEqual(a,fixture.build(out));self.assertEqual(g.parse(a['geyser.encgeyser']),self.model);(out/'timeline.tsv').write_text('wrong')
  with self.assertRaises(ValueError):fixture.build(out)
  with self.assertRaises(ValueError):fixture.build(g.ROOT/'build')
if __name__=='__main__':unittest.main()
