import hashlib,json,tempfile,unittest
from pathlib import Path
from tools import run_dungeon_mechanism_timeline_reference as r

ROOT=r.ROOT
REPORT=ROOT/'reports/dungeon-mechanism-timelines/source-integration-1'
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()

class MechanismTimelineReference(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.first=REPORT/'original-5';cls.second=REPORT/'original-6'
  cls.proof=json.loads((cls.second/'receipt.json').read_bytes())
  cls.config=json.loads((cls.second/'input.json').read_bytes())
  cls.observed=json.loads((cls.second/'observations.json').read_bytes())
  cls.rows={v['action']:v for v in cls.observed['rows']}
 def test_pinned_source_producer_and_engine(self):
  p=self.proof;self.assertEqual(p['commit'],'7d9246600fffe518408f5830d4848635019005a3')
  self.assertEqual(sha(Path(r.__file__)),p['producer_sha256'])
  self.assertEqual(p['engine_sha256'],'e748acc92ee5205dd3cc1754d4422870f6a68ae6e00a16ff88bd3438c957adfd')
  for name,digest in p['reference_dependencies'].items():self.assertEqual(sha(ROOT/'upstream/MOTHER-Encore'/name),digest)
  v=self.observed['engine'];self.assertEqual((v['major'],v['minor'],v['patch'],v['hash']),(3,6,2,'3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8'))
 def test_exact_source_scripts_and_explicit_adapters(self):
  for name,digest in self.proof['copied_source_sha256'].items():
   self.assertEqual(sha(ROOT/'upstream/MOTHER-Encore'/name),digest)
   if name.endswith('.gd'):self.assertEqual(sha(self.second/'source'/name),digest)
  for name,digest in self.proof['adapter_sha256'].items():self.assertEqual(sha(self.second/'adapters'/name),digest)
  self.assertEqual((self.second/'adapters/probe.gd').read_text(encoding='utf8'),r.probe())
  self.assertEqual((self.second/'adapters/global.gd').read_text(encoding='utf8'),r.s.GLOBAL)
  self.assertEqual((self.second/'adapters/logged-audio.gd').read_text(encoding='utf8'),r.AUDIO)
  original=(ROOT/'upstream/MOTHER-Encore'/self.config['paths']['wall']).read_text(encoding='utf8')
  self.assertEqual((self.second/'adapters/wall.tscn').read_text(encoding='utf8'),original.replace('NodePath("AudioStreamPlayer:playing")','NodePath("AudioStreamPlayer:requested_playing")'))
 def test_two_runs_have_exact_complete_projection(self):
  for name in ('observations.json','input.json'):self.assertEqual((self.first/name).read_bytes(),(self.second/name).read_bytes())
  self.assertEqual(len(self.observed['rows']),232);self.assertEqual(len(self.rows),232)
  paths=[v['path']for v in self.config['nodes']];self.assertEqual(len(paths),14)
  for row in self.observed['rows']:self.assertEqual([v['path']for v in row['nodes']],paths)
  self.assertEqual(len([n for n in self.rows if n.startswith('opening/')]),145)
 def test_original_independent_camera_and_wall_continuations(self):
  wall=lambda row:next(n for n in row['nodes']if n['path']==self.config['wall'])
  begin=self.rows['main_interact'];self.assertTrue(begin['paused']and begin['cutscene'])
  self.assertEqual((wall(begin)['assigned'],wall(begin)['position']),('On','0.00000000000000000'))
  self.assertEqual(begin['camera_events'],[dict(kind='move',x=-648,y=164,duration=1)])
  self.assertGreater(float(wall(self.rows['focus_complete'])['position']),0)
  pending=self.rows['wall_timer_pending'];self.assertTrue(pending['paused']);self.assertFalse(wall(pending)['playing'])
  timed=self.rows['wall_timer_complete_before_camera_return'];self.assertFalse(timed['paused']or timed['cutscene'])
  self.assertEqual(timed['audio_events'],[dict(kind='resume')]);self.assertEqual(timed['player_events'],['unpause'])
  self.assertEqual(timed['camera_events'],[dict(kind='return',duration=1)])
  self.assertEqual(self.rows['camera_return_complete']['player_events'],['unpause'])
 def test_native_timeline_requests_and_flag_only_reset(self):
  rows=self.observed['rows'];audio=[v for row in rows for v in row['timeline_events']if v['kind']=='audio']
  shakes=[v for row in rows for v in row['camera_events']if v['kind']=='shake']
  self.assertEqual(audio,[dict(kind='audio',playing=True)])
  self.assertEqual(shakes,[dict(kind='shake',arguments=[5,.5,0,0,.02,1,False]),dict(kind='shake',arguments=[5,1,0,0,.02,1,False])])
  repeat=self.rows['one_way_repeat'];self.assertFalse(repeat['camera_events']or repeat['player_events']or repeat['timeline_events'])
  before=self.rows['other_switch_animations/64'];after=self.rows['leave_resets_flag_only']
  lever=lambda row:next(n for n in row['nodes']if n['path']=='Bottom2/switch1')
  self.assertTrue(lever(before)['on']);self.assertFalse(lever(after)['on'])
  self.assertEqual({k:v for k,v in lever(before).items()if k!='on'},{k:v for k,v in lever(after).items()if k!='on'})
 def test_unsafe_existing_output_or_missing_engine_rejected(self):
  with tempfile.TemporaryDirectory(dir=ROOT/'build',prefix='mechanism-timeline-negative-')as directory:
   base=Path(directory);engine=base/'dummy';engine.write_bytes(b'never invoke')
   for out,exe in [(ROOT,engine),(ROOT/'build',engine),(base,engine),(base/'new',base/'missing')]:
    with self.subTest(out=out,engine=exe),self.assertRaises(ValueError):r.run(out,exe)
   self.assertFalse((base/'new').exists())

if __name__=='__main__':unittest.main()
