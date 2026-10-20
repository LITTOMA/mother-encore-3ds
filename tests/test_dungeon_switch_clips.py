import copy,json,tempfile,unittest
from pathlib import Path
from unittest.mock import patch
from tools import dungeon_switch_clips as d
from tools.godot_text import NodePathValue
ROOT=Path(__file__).resolve().parents[1]
class DungeonSwitchClipsTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.review=json.loads(d.RECIPE.read_bytes());cls.data,cls.proof=d.extract()
 def review_reject(self,edit):
  value=copy.deepcopy(self.review);edit(value)
  with tempfile.TemporaryDirectory(dir=ROOT/'build',prefix='switch-clip-negative-')as directory:
   file=Path(directory)/'review.json';file.write_text(json.dumps(value),encoding='utf8')
   with self.assertRaises(ValueError):d.extract(file)
 def source_reject(self,edit):
  original=d.Document
  def changed(path,text):
   doc=original(path,text)
   if path.endswith('/Gate.tscn'):edit(doc)
   return doc
  with patch.object(d,'Document',side_effect=changed),self.assertRaises(ValueError):d.extract()
 def test_actual_three_profiles_eleven_existing_format_clips(self):
  self.assertEqual(len(self.data),3);self.assertEqual([len(r['model']['clips'])for r in self.data],[3,3,5]);self.assertEqual(sum(len(r['bytes'])for r in self.data),948);self.assertFalse(self.proof['whole_scripts_approved'])
  self.assertEqual([r['model']['frames']for r in self.data],[2,2,6]);self.assertEqual([r['model']['initial_mask']for r in self.data],[3,1,3])
  for row in self.data:self.assertEqual(row['bytes'],d.prop_clips.encode(row['model']))
  gate=self.data[2]['model'];on=next(c for c in gate['clips']if c['name']=='On');off=next(c for c in gate['clips']if c['name']=='Off')
  self.assertEqual([k['value']for k in on['keys']if k['kind']==1],list(range(6)));self.assertEqual([k['value']for k in off['keys']if k['kind']==1],list(reversed(range(6))))
  self.assertEqual([(k['time'],k['value'])for k in on['keys']if k['kind']==3],[(d.prop_clips.f32(.333333),1)])
  self.assertEqual([(k['time'],k['value'])for k in off['keys']if k['kind']==3],[(d.prop_clips.f32(.166667),0)])
 def test_review_versions_pin_fields_and_sources(self):
  for edit in(lambda p:p.update(schema=2),lambda p:p.update(schema=True),lambda p:p.update(commit='0'*40),lambda p:p.update(unknown=1),lambda p:p['sources'].pop(next(iter(p['sources']))),lambda p:p['sources'].update({next(iter(p['sources'])):'0'*64})):
   with self.subTest(edit=edit):self.review_reject(edit)
 def test_profile_id_name_path_and_selector_types(self):
  for edit in(lambda p:p['profiles'][0].update(id=True),lambda p:p['profiles'][1].update(id=1),lambda p:p['profiles'][0].update(name='Gate'),lambda p:p['profiles'][1].update(name='lever'),lambda p:p['profiles'][0].update(scene='../escape'),lambda p:p['profiles'][0].update(body_owner=False),lambda p:p['profiles'][0].update(frame_owner=''),lambda p:p['profiles'][0].update(unknown=1)):
   with self.subTest(edit=edit):self.review_reject(edit)
 def test_clip_identity_and_source_animation_coverage(self):
  for edit in(lambda p:p['profiles'][0]['clips'][0].update(id=True),lambda p:p['profiles'][0]['clips'][0].update(id=0),lambda p:p['profiles'][0]['clips'][0].update(name='unknown'),lambda p:p['profiles'][0]['clips'][1].update(id=7),lambda p:p['profiles'][0]['clips'].pop(),lambda p:p['profiles'][0]['clips'].reverse()):
   with self.subTest(edit=edit):self.review_reject(edit)
 def test_duplicate_json_rejected(self):
  with tempfile.TemporaryDirectory(dir=ROOT/'build',prefix='switch-clip-negative-')as directory:
   file=Path(directory)/'review.json';file.write_text('{"schema":1,"schema":1}',encoding='utf8')
   with self.assertRaises(ValueError):d.read(file)
 def test_sprite_geometry_grid_and_unknown_properties(self):
  def node(doc,name):return next(n for n in doc.nodes if n.attrs['name']==name)
  for edit in(lambda doc:node(doc,'main').props.update(hframes=True),lambda doc:node(doc,'main').props.update(hframes=7),lambda doc:node(doc,'main').props.update(frame=6),lambda doc:node(doc,'main').props.update(visible=1),lambda doc:node(doc,'main').props.update(material=None)):
   with self.subTest(edit=edit):self.source_reject(edit)
 def test_collision_owner_and_disabled_type(self):
  for edit in(lambda doc:next(n for n in doc.nodes if n.attrs['name']=='collision').attrs.update(type='Area2D'),lambda doc:next(n for n in doc.nodes if n.attrs['name']=='CollisionShape2D').props.update(disabled=0)):
   with self.subTest(edit=edit):self.source_reject(edit)
 def test_unknown_audio_method_and_transform_track_rejected(self):
  for value in('audio','method','bezier','animation'):
   with self.subTest(value=value):self.source_reject(lambda doc:next(v for k,v in doc.sub.values()if k=='Animation').update({'tracks/0/type':value}))
  self.source_reject(lambda doc:next(v for k,v in doc.sub.values()if k=='Animation').update({'tracks/0/path':NodePathValue('main:position')}))
 def test_continuous_curve_transition_and_key_mode_rejected(self):
  def first(doc):return next(v for k,v in doc.sub.values()if k=='Animation')
  for edit in(lambda doc:first(doc)['tracks/0/keys'].update(update=0),lambda doc:first(doc)['tracks/0/keys']['transitions'].__setitem__(0,.5),lambda doc:first(doc)['tracks/0/keys'].update(update=True),lambda doc:first(doc).update({'tracks/0/enabled':False})):
   with self.subTest(edit=edit):self.source_reject(edit)
 def test_invalid_times_frame_values_and_loop(self):
  def first(doc):return next(v for k,v in doc.sub.values()if k=='Animation')
  for edit in(lambda doc:first(doc).update(loop=True),lambda doc:first(doc).update(length=float('nan')),lambda doc:first(doc)['tracks/0/keys']['times'].__setitem__(0,-1),lambda doc:first(doc)['tracks/0/keys']['values'].__setitem__(0,True),lambda doc:first(doc)['tracks/0/keys']['values'].__setitem__(0,6)):
   with self.subTest(edit=edit):self.source_reject(edit)
 def test_candidate_outputs_only_and_repeated_bytes(self):
  with self.assertRaisesRegex(ValueError,'under build'):d.build(ROOT/'romfs/graphics/world/catacombs')
  with tempfile.TemporaryDirectory(dir=ROOT/'build',prefix='switch-clips-positive-')as directory:
   d.build(directory)
   for row in self.data:self.assertEqual((Path(directory)/(row['profile']['name']+'.encpcl')).read_bytes(),row['bytes'])
if __name__=='__main__':unittest.main()
