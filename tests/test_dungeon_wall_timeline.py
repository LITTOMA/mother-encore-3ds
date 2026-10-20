import copy,json,struct,tempfile,unittest,zlib
from pathlib import Path
from unittest.mock import patch
from tools import dungeon_wall_timeline as d
from tools.godot_text import NodePathValue
class DungeonWallTimeline(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.recipe=json.loads(d.RECIPE.read_bytes());cls.data,cls.blob,cls.proof=d.extract()
 def reject_review(self,edit):
  p=copy.deepcopy(self.recipe);edit(p)
  with tempfile.TemporaryDirectory(dir=d.ROOT/'build',prefix='wall-review-negative-')as directory:
   file=Path(directory)/'review.json';file.write_text(json.dumps(p),encoding='utf8')
   with self.assertRaises(ValueError):d.extract(file)
 def changed_source(self,edit):
  original=d.Document
  def changed(path,text):
   doc=original(path,text)
   if path==self.recipe['paths']['scene']:edit(doc)
   return doc
  with patch.object(d,'Document',side_effect=changed),self.assertRaises(ValueError):d.extract()
 def test_exact_source_four_roles_and_typed_cues(self):
  self.assertEqual(d.parse(self.blob),self.data);self.assertEqual(len(self.blob),948);self.assertFalse(self.proof['whole_scripts_approved'])
  self.assertEqual(self.data['roles'],[4,7,5,6]);self.assertEqual([len(c['cues'])for c in self.data['cue_clips']],[3,1,0,0])
  self.assertEqual(self.data['properties']['frames'],15);self.assertEqual(self.data['audio']['id'],90211837);self.assertFalse(self.data['audio']['loop'])
  self.assertEqual(self.data['camera'],dict(magnitude=5.,direction=[0.,0.],interval=.02,weight=1.,diminish=False))
  self.assertEqual([c['kind']for c in self.data['cue_clips'][0]['cues']],[1,2,2]);self.assertEqual([c['value']for c in self.data['cue_clips'][0]['cues']],[1,.5,1])
 def test_review_schema_pin_fields_and_dependency_rejected(self):
  for edit in(lambda p:p.update(schema=True),lambda p:p.update(schema=2),lambda p:p.update(kind='unknown'),lambda p:p.update(commit='0'*40),lambda p:p.update(unknown=1),lambda p:p.update(graph_recipe='../escape'),lambda p:p.update(graph_review_sha256='0'*64),lambda p:p.update(scope=False),lambda p:p.update(license_review='')):
   with self.subTest(edit=edit):self.reject_review(edit)
 def test_review_paths_sources_and_duplicate_json_rejected(self):
  for edit in(lambda p:p['paths'].update(scene=[]),lambda p:p['paths'].update(scene='../escape'),lambda p:p['paths'].update(audio_import='bad'),lambda p:p['sources'].pop(next(iter(p['sources']))),lambda p:p['sources'].update({next(iter(p['sources'])):'0'*64})):
   with self.subTest(edit=edit):self.reject_review(edit)
  with tempfile.TemporaryDirectory(dir=d.ROOT/'build',prefix='wall-json-negative-')as directory:
   file=Path(directory)/'review.json';file.write_text('{"schema":1,"schema":1}',encoding='utf8')
   with self.assertRaises(ValueError):d.read(file)
 def test_review_owner_role_and_audio_identity_rejected(self):
  for edit in(lambda p:p['owner'].update(scene_id=True),lambda p:p['owner'].update(id=0),lambda p:p['owner'].update(id=1),lambda p:p['owner'].update(path='Bottom2/unknown'),lambda p:p['roles'].pop(),lambda p:p['roles'][0].update(id=True),lambda p:p['roles'][1].update(id=4),lambda p:p['roles'][0].update(name='Unknown'),lambda p:p['audio_identity'].update(value=True),lambda p:p['audio_identity'].update(value=21),lambda p:p['audio_identity'].update(kind='dynamic')):
   with self.subTest(edit=edit):self.reject_review(edit)
 def test_source_animation_coverage_and_unrecognized_track_rejected(self):
  def on(doc):return doc.sub[3][1]
  self.changed_source(lambda doc:next(n for n in doc.nodes if n.attrs['name']=='AnimationPlayer').props.update({'autoplay':'On'}))
  for edit in(lambda doc:on(doc).update({'loop':True}),lambda doc:on(doc).update({'tracks/2/type':'audio'}),lambda doc:on(doc).update({'tracks/2/path':NodePathValue('unknown:playing')}),lambda doc:on(doc).update({'tracks/2/enabled':False}),lambda doc:on(doc).update({'tracks/3/extra':1})):
   with self.subTest(edit=edit):self.changed_source(edit)
 def test_source_unknown_method_arity_and_curve_rejected(self):
  def on(doc):return doc.sub[3][1]
  for edit in(lambda doc:on(doc)['tracks/3/keys']['values'][0].update(method='unknown'),lambda doc:on(doc)['tracks/3/keys']['values'][0].update(args=[]),lambda doc:on(doc)['tracks/3/keys']['values'][0].update(args=[True]),lambda doc:on(doc)['tracks/0/keys'].update(update=0),lambda doc:on(doc)['tracks/0/keys']['transitions'].__setitem__(0,2),lambda doc:on(doc)['tracks/2/keys']['values'].__setitem__(0,1)):
   with self.subTest(edit=edit):self.changed_source(edit)
 def test_source_grid_audio_and_body_owner_rejected(self):
  def node(doc,name):return next(n for n in doc.nodes if n.attrs['name']==name)
  for edit in(lambda doc:node(doc,'main').props.update(hframes=True),lambda doc:node(doc,'main').props.update(hframes=14),lambda doc:node(doc,'collision').attrs.update(type='StaticBody2D'),lambda doc:node(doc,'CollisionShape2D').props.update(disabled=0),lambda doc:node(doc,'AudioStreamPlayer').props.update(bus='Master'),lambda doc:node(doc,'AudioStreamPlayer').props.update(pitch_scale=2)):
   with self.subTest(edit=edit):self.changed_source(edit)
 def test_encoder_rejects_schema_role_and_nested_scope(self):
  changes=[lambda p:p.update(extra=1),lambda p:p.update(scene=True),lambda p:p.update(owner=0),lambda p:p.update(path='../escape'),lambda p:p['roles'].__setitem__(0,[]),lambda p:p['roles'].__setitem__(0,True),lambda p:p['roles'].__setitem__(1,p['roles'][0]),lambda p:p['properties'].update(initial_mask=4),lambda p:p['properties']['clips'].pop(),lambda p:p['properties']['clips'][0]['keys'][0].update(kind=2),lambda p:p['properties']['clips'][0]['keys'][0].update(value=999),lambda p:p['cue_clips'][0].update(id=123)]
  for edit in changes:
   p=copy.deepcopy(self.data);edit(p)
   with self.subTest(edit=edit),self.assertRaises(ValueError):d.encode(p)
 def test_encoder_audio_camera_and_cue_domains(self):
  changes=[lambda p:p['audio'].update(id=True),lambda p:p['audio'].update(path='other://bad'),lambda p:p['audio'].update(sha='0'*64),lambda p:p['audio'].update(loop=1),lambda p:p['audio'].update(bus=True),lambda p:p['audio'].update(pitch=2),lambda p:p['audio'].update(gain=float('nan')),lambda p:p['camera'].update(interval=0),lambda p:p['camera'].update(weight=2),lambda p:p['camera'].update(direction=[float('inf'),0]),lambda p:p['camera'].update(diminish=1),lambda p:p['cue_clips'][0]['cues'][0].update(kind=3),lambda p:p['cue_clips'][0]['cues'][0].update(value=.5),lambda p:p['cue_clips'][0]['cues'][1].update(value=.001),lambda p:p['cue_clips'][0]['cues'][2].update(time=0)]
  for edit in changes:
   p=copy.deepcopy(self.data);edit(p)
   with self.subTest(edit=edit),self.assertRaises(ValueError):d.encode(p)
 def test_parser_all_truncations_wrong_version_capability_and_spans(self):
  for size in range(len(self.blob)):
   with self.assertRaises(ValueError):d.parse(self.blob[:size])
  def change(offset,value,sign=True):
   b=bytearray(self.blob);struct.pack_into('<I',b,offset,value)
   if sign:struct.pack_into('<I',b,16,zlib.crc32(b[32:]))
   return bytes(b)
  for at,value in[(8,0),(8,2),(12,len(self.blob)+1),(20,0),(20,2),(24,0),(24,65536),(28,1),(32,0),(52,4),(56,1),(60,1),(64,2),(68,0),(72,2**32-1),(76,2),(84,0),(88,2),(92,64),(112,0)]:
   with self.subTest(offset=at,value=value),self.assertRaises(ValueError):d.parse(change(at,value))
  for b in[self.blob+b'x',bytes([self.blob[0]^1])+self.blob[1:],self.blob[:180]+bytes([self.blob[180]^1])+self.blob[181:]]:
   with self.assertRaises(ValueError):d.parse(b)
 def test_parser_policy_identity_float_and_reserved_fields(self):
  policy=struct.unpack_from('<I',self.blob,84)[0]
  def changed(offset,fmt,value):
   b=bytearray(self.blob);struct.pack_into(fmt,b,policy+offset,value);struct.pack_into('<I',b,16,zlib.crc32(b[32:]));return bytes(b)
  for at,fmt,value in[(0,'<I',0),(4,'<I',0),(8,'<I',0),(12,'<I',4),(24,'<I',1),(28,'<I',0),(32,'<I',0),(36,'<I',0),(72,'<f',float('nan')),(76,'<I',2),(80,'<d',float('inf')),(88,'<f',1000001),(96,'<d',0),(104,'<d',2),(112,'<I',2),(116,'<I',0),(120,'<f',2),(124,'<I',1)]:
   with self.subTest(offset=at,value=value),self.assertRaises(ValueError):d.parse(changed(at,fmt,value))
 def test_parser_nested_properties_and_cue_coverage(self):
  prop=struct.unpack_from('<I',self.blob,100)[0];rows=struct.unpack_from('<I',self.blob,116)[0];cues=struct.unpack_from('<I',self.blob,132)[0]
  policy=prop+struct.unpack_from('<I',self.blob,prop+68)[0];keys=prop+struct.unpack_from('<I',self.blob,prop+100)[0]
  for at,fmt,value,nested in[(prop+8,'<I',2,False),(prop+20,'<I',2,False),(policy+8,'<I',4,True),(keys+4,'<I',2,True),(rows,'<I',999,False),(rows+4,'<I',1,False),(rows+8,'<I',100,False),(rows+12,'<I',1,False),(cues+4,'<I',3,False),(cues+8,'<d',.5,False),(cues+16,'<f',60,False),(cues+24,'<d',.001,False)]:
   b=bytearray(self.blob);struct.pack_into(fmt,b,at,value)
   if nested:
    length=struct.unpack_from('<I',self.blob,104)[0];struct.pack_into('<I',b,prop+16,zlib.crc32(b[prop+32:prop+length]))
   struct.pack_into('<I',b,16,zlib.crc32(b[32:]))
   with self.subTest(offset=at,value=value),self.assertRaises(ValueError):d.parse(bytes(b))
if __name__=='__main__':unittest.main()
