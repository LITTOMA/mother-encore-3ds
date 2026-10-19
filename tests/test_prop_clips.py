import copy,json,tempfile,unittest
from pathlib import Path
from unittest.mock import patch
from tools import prop_clips as c
from tools.run_prop_clip_reference import numeric_scene
from tools.godot_text import Document,NodePathValue
ROOT=Path(__file__).resolve().parents[1]

class PropClips(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.policy,cls.ex=c.read();cls.source=c.source();cls.doc=Document(cls.policy['scene'],cls.ex.text(cls.policy['scene']))
 def test_source_clips_reproduce_checked_fixture(self):
  r=self.source;self.assertEqual((r['frames'],r['initial_frame'],r['initial_mask']),(21,12,15))
  self.assertEqual([r['name']for r in r['clips']],['Hidden','Idle','Grow','Break','RESET'])
  self.assertEqual(c.encode(r),(ROOT/'tests/fixtures/prop-clips-v0410.encpcl').read_bytes())
  self.assertEqual([len(r['keys'])for r in r['clips']],[4,4,15,12,4]);self.assertEqual(r['clips'][4]['constant_tracks'],[0,1])
  self.assertNotIn(4,[k['kind']for k in r['clips'][0]['keys']]) # Hidden preserves the source hitbox state.
 def test_unknown_recipe_fields_versions_ids_and_sources_reject(self):
  mutations=[lambda p:p.update(schema=True),lambda p:p.update(schema=2),lambda p:p.update(unknown=True),lambda p:p.update(commit='0'*40),lambda p:p.update(scope=False),lambda p:p['clips'][0].update(id=0),lambda p:p['clips'][0].update(id=True),lambda p:p['clips'][1].update(id=1),lambda p:p['clips'][1].update(name='Hidden'),lambda p:p['tracks'][0].update(kind=True),lambda p:p['tracks'][1].update(kind=1),lambda p:p['sources'].update({p['script']:'0'*64})]
  with tempfile.TemporaryDirectory()as folder:
   path=Path(folder)/'recipe.json'
   for mutate in mutations:
    p=copy.deepcopy(self.policy);mutate(p);path.write_text(json.dumps(p),encoding='utf8')
    with self.subTest(recipe=p),self.assertRaises(ValueError):c.read(path)
   path.write_text('{"schema":1,"schema":1}',encoding='utf8')
   with self.assertRaises(ValueError):c.read(path)
 def reject_document(self,mutation):
  d=copy.deepcopy(self.doc);mutation(d)
  with patch.object(c,'Document',return_value=d),self.assertRaises(ValueError):c.source()
 def test_unknown_properties_tracks_modes_and_timing_reject(self):
  # Animation subresource4 is the actual Break animation, not a manufactured IR.
  mutations=[lambda d:d.sub[4][1].update(loop=True),lambda d:d.sub[4][1].update(length=0),lambda d:d.sub[4][1].update(step=float('nan')),lambda d:d.sub[4][1].update(unknown=0),lambda d:d.sub[4][1].update({'tracks/0/type':'method'}),lambda d:d.sub[4][1].update({'tracks/0/interp':True}),lambda d:d.sub[4][1].update({'tracks/0/path':NodePathValue('Unknown:value')}),lambda d:d.sub[4][1]['tracks/0/keys'].update(update=2),lambda d:d.sub[4][1]['tracks/0/keys'].update(update=0),lambda d:d.sub[4][1]['tracks/0/keys']['values'].__setitem__(0,21),lambda d:d.sub[4][1]['tracks/1/keys']['values'].__setitem__(0,1),lambda d:d.sub[4][1]['tracks/0/keys']['times'].__setitem__(1,0),lambda d:d.sub[4][1]['tracks/0/keys']['times'].__setitem__(1,float('inf')),lambda d:d.sub[4][1].update({'tracks/00/type':'value'})]
  for mutate in mutations:
   with self.subTest(mutation=mutate):self.reject_document(mutate)
 def test_target_role_grid_and_initial_state_reject(self):
  def node(d,name,parent='.'):return next(n for n in d.nodes if n.attrs.get('name')==name and n.attrs.get('parent')==parent)
  for mutate in (lambda d:node(d,'Sprite').props.update(frame=21),lambda d:node(d,'Sprite').props.update(hframes=True),lambda d:node(d,'Sprite').props.update(visible=1),lambda d:node(d,'Hitbox').props.update(collision_mask=1),lambda d:node(d,'interact').props.update(monitoring=True),lambda d:node(d,'AnimationPlayer').props.update(autoplay='Grow')):
   self.reject_document(mutate)
 def test_only_prompt_graphics_are_quarantined_in_numeric_reference(self):
  raw=self.ex.text(self.policy['scene']);adapted,receipt=numeric_scene(raw,self.policy['scene']);d=Document(self.policy['scene'],adapted)
  for id,(kind,properties)in self.doc.sub.items():
   if kind=='Animation':self.assertEqual(d.sub[id],(kind,properties))
  self.assertEqual(receipt['property'],'interact/ButtonPrompt:material')
  self.assertNotIn('Shader',{kind for kind,_ in d.sub.values()})
  for text in (raw.replace('material = SubResource( 10 )','material = null'),raw+'\n[node name="Unexpected" type="Node2D" parent="."]\nmaterial = SubResource( 10 )\n'):
   with self.assertRaises(ValueError):numeric_scene(text,self.policy['scene'])
 def test_nonfinite_real_types_reject(self):
  for value in (True,float('nan'),float('inf'),1000001,'1'):
   with self.assertRaises(ValueError):c.f32(value)

if __name__=='__main__':unittest.main()
