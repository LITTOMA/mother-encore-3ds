import hashlib,json,tempfile,unittest
from pathlib import Path
from tools import run_door_camera_reference as r

ROOT=r.ROOT
REPORT=ROOT/'reports/dungeon-wall-timeline/source-implementation-1'
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()

class DoorCameraReference(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.first=REPORT/'door-original-1';cls.second=REPORT/'door-original-2'
  cls.proof=json.loads((cls.second/'receipt.json').read_bytes())
 def test_source_tool_and_engine_identity(self):
  p=self.proof
  self.assertEqual(p['commit'],'7d9246600fffe518408f5830d4848635019005a3')
  self.assertEqual(set(p['sources']),{'Scripts/misc/Shaker.gd','Scripts/Main/Camera2D.gd','Scripts/Main/Openable Door.gd'})
  for name,digest in p['sources'].items():self.assertEqual(sha(ROOT/'upstream/MOTHER-Encore'/name),digest)
  self.assertEqual(sha(Path(r.__file__)),p['producer_sha256'])
  self.assertEqual(sha(ROOT/'content/native-house.json'),p['house_ir_sha256'])
  self.assertEqual(hashlib.sha256(r.PROBE.encode()).hexdigest(),p['shared_probe_sha256'])
  self.assertEqual(p['engine_sha256'],'e748acc92ee5205dd3cc1754d4422870f6a68ae6e00a16ff88bd3438c957adfd')
  self.assertEqual(p['source_call'],dict(magnitude=1.,duration=.2,direction=[2.,0.]))
 def test_unmodified_shaker_control_flow_and_typed_delta(self):
  original=(ROOT/'upstream/MOTHER-Encore/Scripts/misc/Shaker.gd').read_text(encoding='utf8')
  expected=original.replace('class_name Shaker','class_name Shaker\nvar runner').replace('rand_range(-1, 1)','runner.random_range(-1, 1)')
  self.assertEqual((self.second/'adapters/shaker.gd').read_text(encoding='utf8'),expected)
  self.assertEqual((self.second/'adapters/probe.gd').read_text(encoding='utf8'),r.PROBE)
  self.assertIn('shaker._physics_process(real32(test.deltas[tick]))',r.PROBE)
 def test_repeated_exact_original_projection(self):
  for name,digest in self.proof['outputs'].items():
   self.assertEqual((self.first/name).read_bytes(),(self.second/name).read_bytes())
   self.assertEqual(sha(self.second/name),digest)
  self.assertEqual((ROOT/'tests/fixtures/door-camera.tsv').read_bytes(),(self.second/'states.tsv').read_bytes())
  observed=json.loads((self.second/'observations.json').read_bytes())
  self.assertEqual(sum(len(c['rows'])for c in observed['cases']),180)
  self.assertEqual([c['name']for c in observed['cases']],['source_openable_door','nonunit_diagonal_fixture','larger_weighted_fixture'])
  self.assertTrue(all(row['raw_count']==0 and not row['calls']for case in observed['cases']for row in case['rows']))
 def test_invalid_output_and_engine_rejected_before_invocation(self):
  with tempfile.TemporaryDirectory(dir=ROOT/'build',prefix='door-camera-negative-')as directory:
   base=Path(directory);engine=base/'dummy';engine.write_bytes(b'never execute')
   for out,exe in [(ROOT,engine),(ROOT/'build',engine),(base,engine),(base/'new',base/'missing')]:
    with self.subTest(out=out,engine=exe),self.assertRaises(ValueError):r.run(out,exe)
   self.assertFalse((base/'new').exists())

if __name__=='__main__':unittest.main()
