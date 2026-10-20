import hashlib,json,tempfile,unittest
from pathlib import Path
from tools import run_camera_shaker_reference as r
ROOT=r.ROOT
REPORT=ROOT/'reports/camera-random-shake/source-oracle-1'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
class CameraShakerReference(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.first=REPORT/'original-2';cls.second=REPORT/'original-3'
  cls.proof=json.loads((cls.second/'receipt.json').read_bytes());cls.obs=json.loads((cls.second/'observations.json').read_bytes())
 def test_source_tool_and_engine_scope_remain_exact(self):
  p=self.proof;self.assertEqual(p['commit'],'7d9246600fffe518408f5830d4848635019005a3')
  self.assertEqual(set(p['sources']),{'Scripts/misc/Shaker.gd','Scripts/Main/Camera2D.gd','Scripts/misc/wall door.gd','Nodes/Overworld/switches/Wall Door.tscn'})
  for name,digest in p['sources'].items():self.assertEqual(sha(ROOT/'upstream/MOTHER-Encore'/name),digest)
  self.assertEqual(sha(Path(r.__file__)),p['tool_sha256']);self.assertEqual(p['scope'],r.__doc__)
  self.assertEqual(p['engine_sha256'],'e748acc92ee5205dd3cc1754d4422870f6a68ae6e00a16ff88bd3438c957adfd')
  v=self.obs['engine'];self.assertEqual((v['major'],v['minor'],v['patch'],v['hash']),(3,6,2,'3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8'))
 def test_adapter_preserves_all_original_shaker_control_flow(self):
  original=(self.second/'source-shaker.gd').read_bytes();self.assertEqual(original,(ROOT/'upstream/MOTHER-Encore/Scripts/misc/Shaker.gd').read_bytes())
  expected=original.decode().replace('class_name Shaker','class_name Shaker\nvar runner').replace('rand_range(-1, 1)','runner.random_range(-1, 1)')
  self.assertEqual((self.second/'adapters/shaker.gd').read_text(encoding='utf8'),expected)
  self.assertEqual((self.second/'adapters/probe.gd').read_text(encoding='utf8'),r.PROBE)
  self.assertIn('shaker._physics_process(real32(test.deltas[tick]))',r.PROBE)
  self.assertIn('var value=rand_range(low,high)',r.PROBE);self.assertNotIn('mirror.randf',r.PROBE)
 def test_repeat_observations_and_exact_frozen_projection(self):
  for name in ['observations.json','states.tsv','input.json']:
   self.assertEqual((self.first/name).read_bytes(),(self.second/name).read_bytes());self.assertEqual(sha(self.second/name),self.proof['files'][name])
  self.assertEqual((ROOT/'tests/fixtures/camera-random-shake.tsv').read_bytes(),(self.second/'states.tsv').read_bytes())
  self.assertEqual(json.loads((self.second/'input.json').read_bytes()),r.cases())
  self.assertEqual(len(self.obs['cases']),10);self.assertEqual(sum(len(c['rows'])for c in self.obs['cases']),845)
 def test_retries_and_final_invisible_steps_still_advance_rng(self):
  cases={c['name']:c for c in self.obs['cases']};rows=[v for c in cases.values()for v in c['rows']]
  self.assertEqual(sum(len(v['calls'])>2 for v in rows),45)
  self.assertTrue(all(len(v['calls'])%2==0 for v in rows))
  for v in rows:
   for call in v['calls']:self.assertEqual(len(call['raw']),1 if call['raw'][0]=='0'else 3)
  last=cases['last_two_steps_still_draw']['rows'];self.assertTrue(any(v['calls']and v['after']==[0,0]for v in last));self.assertGreater(last[-1]['raw_count'],0)
  self.assertTrue(all(v['raw_count']==0 for v in cases['diagonal_fixed']['rows']))
 def test_unsafe_or_nonfresh_output_rejected_before_engine_use(self):
  with tempfile.TemporaryDirectory(dir=ROOT/'build',prefix='camera-shaker-negative-')as directory:
   base=Path(directory);engine=base/'dummy';engine.write_bytes(b'never execute')
   for out,exe in [(ROOT,engine),(ROOT/'build',engine),(base,engine),(base/'new',base/'missing')]:
    with self.subTest(out=out,engine=exe),self.assertRaises(ValueError):r.prepare(out,exe)
   self.assertFalse((base/'new').exists())
if __name__=='__main__':unittest.main()
