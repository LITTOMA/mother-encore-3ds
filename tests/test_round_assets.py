import copy,json,tempfile,unittest
from pathlib import Path
from PIL import Image
from tools import round_assets as a

class RoundAssetsTests(unittest.TestCase):
 def fixture(self,root):
  p=root/'atlas.png';im=Image.new('RGBA',(6,4));im.putdata([(i,255-i,i*3,0 if i%3==0 else 255)for i in range(24)]);im.save(p)
  return dict(schema=1,commit='fixture',game_version='fixture',licence_review='fixture only',sources={'atlas.png':a.sha(p)},resources=[dict(id=1,name='fixture',source='atlas.png',size=[6,4],grid=[3,2],output_grid=[2,3],output='graphics/battle/round/fixture.t3x')])
 def test_repack_preserves_every_linear_frame_and_transparent_rgb(self):
  im=Image.new('RGBA',(6,4));im.putdata([(i,255-i,i*3,0 if i%3==0 else 255)for i in range(24)]);out=a.repack(im,[3,2],[2,3]);self.assertEqual(out.size,(4,6))
  for i in range(6):self.assertEqual(im.crop((i%3*2,i//3*2,i%3*2+2,i//3*2+2)).tobytes(),out.crop((i%2*2,i//2*2,i%2*2+2,i//2*2+2)).tobytes())
 def test_invalid_repack_rejected(self):
  with self.assertRaises(ValueError):a.repack(Image.new('RGBA',(5,4)),[3,2],[2,3])
  with self.assertRaises(ValueError):a.repack(Image.new('RGBA',(6,4)),[3,2],[3,3])
 def test_source_change_rejected(self):
  with tempfile.TemporaryDirectory()as d:
   r=Path(d);j=self.fixture(r);a.validate_source(r,j,dict(commit='fixture',game_version='fixture'));(r/'atlas.png').write_bytes(b'changed')
   with self.assertRaisesRegex(ValueError,'Changed'):a.validate_source(r,j,dict(commit='fixture',game_version='fixture'))
 def test_unknown_fields_paths_ids_and_dimensions_fail(self):
  with tempfile.TemporaryDirectory()as d:
   r=Path(d);j=self.fixture(r);lock=dict(commit='fixture',game_version='fixture')
   for key,value in [('id',0),('output','../outside.t3x'),('grid',[0,2]),('output_grid',[3,3]),('size',[7,4]),('unknown',1)]:
    p=copy.deepcopy(j);p['resources'][0][key]=value
    with self.subTest(key=key),self.assertRaises(ValueError):a.validate_source(r,p,lock)
   for key,value in [('schema',2),('commit','unreviewed'),('licence_review','')]:
    p=copy.deepcopy(j);p[key]=value
    with self.subTest(key=key),self.assertRaises(ValueError):a.validate_source(r,p,lock)
 def test_real_source_receipt_and_display_assumptions(self):
  a.verify(a.ROOT/'upstream/MOTHER-Encore');recipe=json.loads(a.RECIPE.read_text());receipt=json.loads((a.receipt_path(a.OUT,a.ROOT)).read_text())
  party=next(r for r in receipt['resources']if r['name']=='party');self.assertEqual((party['width'],party['height'],party['columns'],party['rows']),(960,768,15,12))
  lamp=Image.open(a.ROOT/'upstream/MOTHER-Encore/Graphics/Battle Sprites/lamp.png').convert('RGBA');self.assertEqual(set(lamp.getchannel('A').getdata()),{0,255})
  self.assertEqual(len(receipt['resources']),len(recipe['resources']))
 def test_bound_methods_and_frames_are_source_backed(self):
  p=json.loads((a.REPORT/'presentation.json').read_text());attack=p['media'][p['skill_media']['attack']['user_media']];events=p['events'][attack['first_event']:attack['first_event']+attack['event_count']]
  self.assertEqual([(e['time'],e['kind'])for e in events],[(.0714286,1),(.142,2)])
  self.assertEqual(attack['duration'],.642857);self.assertNotEqual(p['bindings']['EnemyDefeat'],a.NIL)
  raw=(a.ROOT/'upstream/MOTHER-Encore/Nodes/Ui/Battle/Battle.tscn').read_text();self.assertFalse('/HitEffect.gd"' in raw);self.assertFalse('_on_AnimationPlayer_animation_started' in raw)
  self.assertTrue(all(m['id']==i+1 for i,m in enumerate(p['media'])))
 def test_victory_tracks_remain_source_data(self):
  p=json.loads((a.REPORT/'presentation.json').read_text())
  def media(slot):return p['media'][p['bindings'][slot]]
  def tracks(slot):m=media(slot);return p['tracks'][m['first_track']:m['first_track']+m['track_count']]
  def keys(track):return p['keys'][track['first']:track['first']+track['count']]
  party=media('PartyVictory');self.assertEqual(party['duration'],1.6);self.assertEqual([int(k['value'][0])for k in keys(tracks('PartyVictory')[0])],list(range(81,96)))
  banner=media('VictoryBanner');self.assertEqual((banner['rect'],banner['duration'],banner['flags']),([156.0,26.0,93.0,8.0],.6,3))
  self.assertEqual([int(k['value'][0])for k in keys(tracks('VictoryBanner')[0])],[1,2,3,4,5])
  timeline=media('ReturnTimeline');events=p['events'][timeline['first_event']:timeline['first_event']+timeline['event_count']]
  self.assertEqual(timeline['duration'],2.7);self.assertEqual([(e['time'],e['kind'])for e in events],[(.1,4),(.75,5),(.8,6),(1.65,7),(2.4,8)])
  self.assertEqual(p['parameters']['ReturnPartyFrames'],[183,33,0,0]);self.assertEqual(p['parameters']['ReturnPartyTurn'],[0,-1,.05,0])
  jump=media('PartyJumpToWorld');self.assertEqual((jump['rect'][2:],jump['duration']),([31.0,29.0],.6));self.assertEqual(p['events'][jump['first_event']]['kind'],9)
  self.assertEqual((media('ReturnTop')['anchor'],media('ReturnBottom')['anchor']),([0,0],[0,1]))
 def test_unknown_animation_target_is_rejected(self):
  class FakeExtractor:
   def text(self,path):return '''[gd_scene format=2]
[sub_resource type="Animation" id=1]
tracks/0/type = "value"
tracks/0/path = NodePath("unsupported:state")
tracks/0/interp = 1
tracks/0/enabled = true
tracks/0/imported = false
tracks/0/keys = {"times": [0], "transitions": [1], "update": 1, "values": [1]}
[node name="AnimationPlayer" type="AnimationPlayer" parent="."]
anims/victory = SubResource( 1 )
'''
  with self.assertRaisesRegex(ValueError,'Unreviewed presentation track'):a.Presentation(FakeExtractor(),[]).anim('fixture','victory',1)
if __name__=='__main__':unittest.main()
