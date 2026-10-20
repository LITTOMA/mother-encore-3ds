import hashlib,json,tempfile,unittest
from pathlib import Path
from tools import run_prop_resume_reference as resume,run_dungeon_wall_timeline_reference as wall
from tools.run_dungeon_switch_clips_reference import PROBE
from tools.run_battle_victory_reference import function
ROOT=resume.ROOT
REPORT=ROOT/'reports/prop-animation-continuation/source-oracle-1'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
class PropContinuationReference(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.r=REPORT/'prop-resume-2';cls.w=REPORT/'wall-timeline-3'
  cls.rp=json.loads((cls.r/'receipt.json').read_bytes());cls.wp=json.loads((cls.w/'receipt.json').read_bytes())
  cls.ro=json.loads((cls.r/'observations.json').read_bytes());cls.wo=json.loads((cls.w/'observations.json').read_bytes())
 def test_exact_upstream_producer_engine_and_shared_probe(self):
  for proof,module in[(self.rp,resume),(self.wp,wall)]:
   self.assertEqual(proof['commit'],'7d9246600fffe518408f5830d4848635019005a3');self.assertFalse(proof['whole_scripts_approved'])
   self.assertEqual(sha(Path(module.__file__)),proof['producer_sha256'])
   self.assertEqual(proof['engine_sha256'],'e748acc92ee5205dd3cc1754d4422870f6a68ae6e00a16ff88bd3438c957adfd')
   for name,digest in proof['sources'].items():self.assertEqual(sha(ROOT/'upstream/MOTHER-Encore'/name),digest)
  self.assertEqual(self.rp['shared_probe_sha256'],hashlib.sha256(PROBE.encode()).hexdigest())
  for observed in(self.ro,self.wo):
   v=observed['engine'];self.assertEqual((v['major'],v['minor'],v['patch'],v['hash']),(3,6,2,'3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8'))
 def test_repeated_resume_observations_and_unchanged_projection(self):
  for name in('observations.json','states.tsv','input.json'):
   self.assertEqual((REPORT/'prop-resume-1'/name).read_bytes(),(self.r/name).read_bytes());self.assertEqual(sha(self.r/name),self.rp['outputs'][name])
  self.assertEqual((ROOT/'tests/fixtures/prop-play-resume.tsv').read_bytes(),(self.r/'states.tsv').read_bytes())
  rows=self.ro['rows'];self.assertEqual(len(rows),231);self.assertEqual(len({row['file']for row in rows}),3)
  self.assertEqual(rows[2]['op'],'play');self.assertGreater(float(rows[2]['position']),0);self.assertEqual(rows[2]['position'],rows[1]['position'])
 def test_wall_repeated_observations_preserve_native_subresources(self):
  for name in('observations.json','input.json'):self.assertEqual((REPORT/'wall-timeline-2'/name).read_bytes(),(self.w/name).read_bytes())
  self.assertEqual(len(self.wo['cases']),6);self.assertEqual(sum(len(c['rows'])for c in self.wo['cases']),309)
  original=(ROOT/'upstream/MOTHER-Encore/Nodes/Overworld/switches/Wall Door.tscn').read_bytes();self.assertEqual((self.w/'source-wall.tscn').read_bytes(),original)
  expected=original.decode().replace('res://Scripts/misc/wall door.gd','res://wall_slice.gd').replace('NodePath("AudioStreamPlayer:playing")','NodePath("AudioStreamPlayer:requested_playing")')
  self.assertEqual((self.w/'adapters/wall.tscn').read_text(encoding='utf8'),expected)
  self.assertEqual((self.w/'adapters/probe.gd').read_text(encoding='utf8'),wall.PROBE)
 def test_wall_original_method_and_logged_audio_forwarding(self):
  original=(ROOT/'upstream/MOTHER-Encore/Scripts/misc/wall door.gd').read_text(encoding='utf8')
  self.assertEqual((self.w/'adapters/wall_slice.gd').read_text(encoding='utf8'),'extends Sprite\nvar global\nvar _move_cam_when_changed=false\n'+function(original,'_shake'))
  self.assertEqual((self.w/'adapters/audio.gd').read_text(encoding='utf8'),wall.AUDIO);self.assertIn('playing=value',wall.AUDIO)
  self.assertEqual(json.loads((self.w/'adapters/audio-policy.json').read_bytes()),{'loop':False,'offset':0})
 def test_zero_delta_audio_and_deferred_methods_are_distinct(self):
  cases={c['name']:c['rows']for c in self.wo['cases']}
  rows=cases['boundaries_and_persistent_audio_request']
  for i in(1,2):self.assertEqual(rows[i]['events'],[{'kind':'audio','playing':True}]);self.assertFalse(rows[i]['body'])
  large=cases['large_step_before_deferred_calls'];self.assertEqual([e['kind']for e in large[1]['events']],['audio','finished']);self.assertEqual([e['duration']for e in large[2]['events']],[.5,1])
  reset=cases['reset_before_pending_methods'];self.assertEqual(reset[3]['events'],[{'kind':'audio','playing':False}]);self.assertEqual([e['duration']for e in reset[4]['events']],[.5,1])
  same=cases['restart_keeps_queued_methods'];self.assertEqual(same[2]['position'],same[1]['position']);self.assertEqual(len(same[-1]['events']),1)
 def test_nonfresh_or_unsafe_reference_outputs_reject(self):
  with tempfile.TemporaryDirectory(dir=ROOT/'build',prefix='prop-reference-negative-')as directory:
   base=Path(directory);engine=base/'dummy';engine.write_bytes(b'never execute')
   for module in(resume,wall):
    for out,exe in[(ROOT,engine),(ROOT/'build',engine),(base,engine),(base/'new',base/'absent')]:
     with self.subTest(module=module.__name__,out=out),self.assertRaises(ValueError):module.run(out,exe)
   self.assertFalse((base/'new').exists())
if __name__=='__main__':unittest.main()
