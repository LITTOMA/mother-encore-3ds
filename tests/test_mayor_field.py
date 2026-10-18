import copy,json,struct,tempfile,unittest
from pathlib import Path
from tools import podunk_field as f
from tools import field_scene_candidate as candidate

class MayorField(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.recipe=f.load_recipe('content/mayor-field.json');cls.model=f.Model(cls.recipe)
 def test_original_map_structure(self):
  m=self.model;self.assertEqual(len(m.layers),2);self.assertEqual(len(m.bodies),2);self.assertEqual(len(m.doors),3);self.assertEqual(len(m.cameras),2);self.assertEqual(len(m.sprites),4);self.assertEqual(len(m.npc_bindings),5)
  self.assertEqual([d['route']for d in m.doors],[4,5,6]);self.assertEqual(set(m.npc_bindings.values()),{1,2,3,4,5})
  atlas=f.Atlases(m);self.assertEqual(len(atlas.pages),1)
  blob=f.encode_map(m,atlas,['graphics/world/mayor/atlas-00.t3x']);scene,_=f.unpack_container(blob,f.MAP_MAGIC,f.MAP_STRIDES,f.MAP_LIMIT);self.assertEqual(scene,3)
  self.assertEqual(struct.unpack_from('<I',blob,20)[0],4)
  self.assertEqual(blob,(f.ROOT/'tests/fixtures/mayor-static-actor.encmap').read_bytes())
  for capability in (1,2,3,5):
   bad=bytearray(blob);struct.pack_into('<I',bad,20,capability)
   with self.assertRaises(f.FieldError):f.unpack_container(bad,f.MAP_MAGIC,f.MAP_STRIDES,f.MAP_LIMIT)
  bad=bytearray((f.ROOT/'romfs/world/podunk/podunk.encmap').read_bytes());struct.pack_into('<I',bad,20,4)
  with self.assertRaises(f.FieldError):f.unpack_container(bad,f.MAP_MAGIC,f.MAP_STRIDES,f.MAP_LIMIT)
 def test_original_podunk_outputs_unchanged(self):
  recipe=f.load_recipe();_,_,_,outputs=f.build_outputs(recipe)
  for name,data in outputs.items():self.assertEqual(data,(f.ROOT/'romfs'/name).read_bytes(),name)
 def test_recipe_versions_and_identities(self):
  for mutate in [lambda p:p.update(schema=True),lambda p:p.update(extra=1),lambda p:p['actor_bindings'][0].update(id=99),lambda p:p['actor_bindings'][0].update(extra=1),lambda p:p['static_sprites'].append(p['static_sprites'][0]),lambda p:p['scene'].update(id=2),lambda p:p['routes'][2].update(to=1)]:
   recipe=copy.deepcopy(self.recipe);mutate(recipe)
   with tempfile.TemporaryDirectory()as d:
    path=Path(d)/'field.json';path.write_text(json.dumps(recipe),encoding='utf-8')
    with self.assertRaises(f.FieldError):f.load_recipe(path)
 def test_missing_sprite_actor_and_source_type_fail(self):
  for mutate in [lambda p:p['static_sprites'].append('missing'),lambda p:p['actor_bindings'][0].update(path='missing'),lambda p:p['actor_bindings'][0].update(scene='Nodes/Overworld/Door.tscn'),lambda p:p['static_sprites'].append('Doors')]:
   recipe=copy.deepcopy(self.recipe);mutate(recipe)
   with self.assertRaises(f.FieldError):f.Model(recipe)
 def test_effective_visible_collision_source_order(self):
  source=f.Sources().text('Scripts/Main/npc.gd')
  ready=source.split('func _ready():',1)[1].split('func _physics_process',1)[0]
  self.assertLess(ready.index('$CollisionShape2D.disabled = true'),ready.rfind('update_visibility_changed()'))
  self.assertTrue(ready.strip().endswith('update_visibility_changed()'))
  visibility=source.split('func update_visibility_changed():',1)[1].split('func ',1)[0]
  self.assertIn('collisions.disabled = !is_visible_in_tree()',visibility)
  self.assertEqual(sum(bool(self.model.scene.by_path[a['path']].props.get('no_collision',False))for a in self.recipe['actor_bindings']),3)
 def test_candidate_paths_and_output_owner(self):
  root=f.ROOT/'build'/'candidate-path-test'
  for path in ['', '../escape', '/absolute', 'C:'+chr(92)+'outside', 'x'+chr(92)+'outside']:
   with self.subTest(path=path),self.assertRaises(f.FieldError):candidate.target(root,path)
  with self.assertRaises(f.FieldError):candidate.compile_map('content/mayor-field.json',f.ROOT/'romfs','missing')
 def test_incomplete_room_cannot_enter_release_builder(self):
  with self.assertRaises(f.FieldError):f.build_outputs(self.recipe)
if __name__=='__main__':unittest.main()
