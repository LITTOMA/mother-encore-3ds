import copy,hashlib,json,struct,tempfile,unittest
from pathlib import Path
from unittest.mock import patch
from tools import field_player_audio as a,field_swing
from tools.godot_text import Document

class PlayerAudio(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.recipe=a.read();cls.model,cls.proof=a.source();base,ex=field_swing.read();cls.doc=Document(base['scene'],ex.text(base['scene']))
 def test_source_sound_node_and_deterministic_binary(self):
  blob=a.encode(self.model);self.assertEqual(len(blob),456);self.assertEqual(a.source(),(self.model,self.proof));self.assertEqual(struct.unpack_from('<I',blob,20)[0],2)
  self.assertEqual(self.model['audio']['bus'],1);self.assertEqual(self.model['audio']['pause_mode'],2)
  sound=field_swing.ROOT/'upstream/MOTHER-Encore'/self.proof['audio_source'];self.assertEqual(hashlib.sha256(sound.read_bytes()).hexdigest(),self.model['audio']['source_sha256'])
  self.assertEqual(self.proof['engine_defaults']['volume_db'],0);self.assertEqual(self.proof['engine_defaults']['pitch_scale'],1);self.assertFalse(self.proof['engine_defaults']['autoplay'])
  for p,sha in self.proof['dependencies'].items():self.assertNotIn('\\',p);self.assertEqual(hashlib.sha256((a.ROOT/p).read_bytes()).hexdigest(),sha)
 def test_recipe_unknown_schema_types_binding_and_paths_reject(self):
  edits=[lambda p:p.update(schema=True),lambda p:p.update(schema=2),lambda p:p.update(extra=0),lambda p:p.update(action_recipe='unknown'),lambda p:p.update(source_node='Unknown'),lambda p:p.update(bus='Music'),lambda p:p.update(bus_kind=True),lambda p:p.update(bus_kind=2),lambda p:p.update(pause_mode=True),lambda p:p.update(pause_mode=1),lambda p:p.update(gain_db=True),lambda p:p.update(gain_db=float('nan')),lambda p:p.update(gain_db=25),lambda p:p.update(pitch=True),lambda p:p.update(pitch=0),lambda p:p.update(pitch=5),lambda p:p.update(autoplay=True),lambda p:p.update(output='../bad.encaction'),lambda p:p.update(output='world//bad.encaction')]
  with tempfile.TemporaryDirectory()as folder:
   path=Path(folder)/'bad.json'
   for edit in edits:
    p=copy.deepcopy(self.recipe);edit(p);path.write_text(json.dumps(p),encoding='utf8')
    with self.subTest(edit=edit),self.assertRaises((ValueError,KeyError)):a.read(path)
   path.write_text('{"schema":1,"schema":1}',encoding='utf8')
   with self.assertRaises(ValueError):a.read(path)
 def test_unknown_or_changed_source_node_rejects(self):
  edits=[lambda n:n.attrs.update(type='AudioStreamPlayer2D'),lambda n:n.attrs.update(parent='Sprite'),lambda n:n.props.update(unknown=1),lambda n:n.props.update(pause_mode=1),lambda n:n.props.update(bus='Master'),lambda n:n.props.update(stream=None),lambda n:n.props.update(volume_db=-3),lambda n:n.props.update(volume_db=True),lambda n:n.props.update(pitch_scale=2),lambda n:n.props.update(autoplay=True)]
  for edit in edits:
   doc=copy.deepcopy(self.doc);edit(next(n for n in doc.nodes if n.attrs['name']=='AudioStreamPlayer'))
   with self.subTest(edit=edit),patch.object(a,'Document',return_value=doc),self.assertRaises(ValueError):a.source()
 def test_model_unknown_values_and_nonfinite_policy_reject(self):
  edits=[lambda m:m.update(extra=0),lambda m:m['audio'].update(extra=0),lambda m:m['audio'].update(source_sha256='0'*64),lambda m:m['audio'].update(source_sha256='g'*64),lambda m:m['audio'].update(gain_db=True),lambda m:m['audio'].update(gain_db=float('inf')),lambda m:m['audio'].update(gain_db=-121),lambda m:m['audio'].update(pitch=float('nan')),lambda m:m['audio'].update(pitch=0),lambda m:m['audio'].update(bus=True),lambda m:m['audio'].update(bus=2),lambda m:m['audio'].update(pause_mode=0)]
  for edit in edits:
   m=copy.deepcopy(self.model);edit(m)
   with self.subTest(edit=edit),self.assertRaises(ValueError):a.encode(m)
 def test_nonzero_pitch_must_remain_nonzero_in_the_binary_field(self):
  m=copy.deepcopy(self.model);m['audio']['pitch']=1e-300
  with self.assertRaises(ValueError):a.encode(m)

if __name__=='__main__':unittest.main()
