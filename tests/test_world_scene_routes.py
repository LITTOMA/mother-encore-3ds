import copy,json,math,struct,tempfile,unittest
from pathlib import Path
from tools import world_scene_routes as w
from tools import podunk_field as f

class Routes(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.recipe=w.load();cls.blob,cls.proof=w.build()
 def test_actual_transformed_routes(self):
  rows=self.proof['routes'];self.assertEqual(len(rows),6)
  # Directly audited Door props: (-88,-73)+(4,1)*(24,-439)-(0,7).
  self.assertEqual((rows[4]['x'],rows[4]['y']),(8,-519))
  # (-16,-504)+(6,1)*(-9.33334,448)-(0,7); tolerance for source float32.
  self.assertAlmostEqual(rows[5]['x'],-72.00004,places=4);self.assertEqual(rows[5]['y'],-63)
  self.assertEqual([r['music_fade']for r in rows[4:]],[0,0])
  self.assertEqual((rows[2]['x'],rows[2]['y']),(8,9));self.assertEqual((rows[3]['x'],rows[3]['y']),(192,1761))
  self.assertTrue(all(r['music_fade']>0 for r in rows[:4]))
  scene,sections=f.unpack_container(self.blob,f.LINK_MAGIC,w.STRIDES,f.LINK_LIMIT,capability=3)
  self.assertEqual(scene,0);self.assertEqual(len(sections[1])//24,3);self.assertEqual(len(sections[2])//84,6)
  self.assertEqual(len(sections[3])//36,2);self.assertEqual(len(sections[4])//4,7)
 def test_legacy_route_semantics_unchanged(self):
  legacy=copy.deepcopy(f.load_recipe());legacy.pop('route_recipe');legacy['routes']=legacy['routes'][:2]
  model=f.Model(legacy)
  self.assertEqual(self.proof['routes'][:2],model.route_rows)
 def test_legacy_schema1_retains_capability2(self):
  recipe=copy.deepcopy(self.recipe);recipe['schema']=1
  for scene in recipe['scenes']:
   if scene['map']:del scene['assets']['steps']
  blob=w.encode(recipe,self.proof['routes']);_,sections=f.unpack_container(blob,f.LINK_MAGIC,w.STRIDES,f.LINK_LIMIT,capability=2)
  _,current=f.unpack_container(self.blob,f.LINK_MAGIC,w.STRIDES,f.LINK_LIMIT,capability=3)
  self.assertEqual(sections[:3],current[:3]);self.assertEqual(sections[4],current[4])
  self.assertTrue(all(struct.unpack_from('<I',sections[3],at+32)[0]==0 for at in range(0,len(sections[3]),36)))
 def test_steps_missing_unknown_and_downgraded(self):
  mutations=[lambda a:a.pop('steps'),lambda a:a.update(steps=True),lambda a:a.update(steps=43),lambda a:a.update(steps=1023),lambda a:a.update(steps=-1),lambda a:a.update(steps=0),lambda a:a.update(steps=2**32)]
  for mutate in mutations:
   recipe=copy.deepcopy(self.recipe);mutate(recipe['scenes'][2]['assets'])
   with self.subTest(recipe=recipe),self.assertRaises(f.FieldError):w.validate(recipe)
 def test_recipe_negative_paths(self):
  bad=[]
  for value in [True,0,1,3]:
   p=copy.deepcopy(self.recipe);p['schema']=value;bad.append(p)
  p=copy.deepcopy(self.recipe);p['extra']=0;bad.append(p)
  p=copy.deepcopy(self.recipe);p['scenes'][2]['room']=32;bad.append(p)
  p=copy.deepcopy(self.recipe);p['scenes'][2]['assets']['npcs']=[322,322];bad.append(p)
  p=copy.deepcopy(self.recipe);p['scenes'][2]['assets']['zones']=0;bad.append(p)
  p=copy.deepcopy(self.recipe);p['scenes'][0]['assets']={};bad.append(p)
  p=copy.deepcopy(self.recipe);p['routes'][2]['to']=99;bad.append(p)
  p=copy.deepcopy(self.recipe);p['routes'][2]['door']='../bad';bad.append(p)
  p=copy.deepcopy(self.recipe);p['routes'][2]=copy.deepcopy(p['routes'][0]);bad.append(p)
  p=copy.deepcopy(self.recipe);p['scenes'][2]['id']=2;bad.append(p)
  for recipe in bad:
   with self.subTest(recipe=recipe),self.assertRaises(f.FieldError):w.validate(recipe)
 def test_source_semantic_rejection(self):
  for mutate in [lambda p:p['routes'][2].update(to=1),lambda p:p['routes'][4].update(to=2),lambda p:p['routes'][4].update(door='Doors/missing'),lambda p:p['door'].update(destination_offset_y=-8)]:
   p=copy.deepcopy(self.recipe);mutate(p)
   with self.assertRaises(f.FieldError):w.rows(p,f.Sources())
 def test_lowered_tuning_rejection(self):
  for key,value in [('dx',.5),('x',math.nan),('in_speed',0),('out_speed',17),('music_fade',-1),('in_kind',3),('flag_value',1),('sound','outside/path')]:
   rows=copy.deepcopy(self.proof['routes']);rows[0][key]=value
   with self.subTest(key=key),self.assertRaises(f.FieldError):w.encode(self.recipe,rows)
  rows=copy.deepcopy(self.proof['routes']);rows[0]['extra']=0
  with self.assertRaises(f.FieldError):w.encode(self.recipe,rows)
 def test_duplicate_json_keys(self):
  with tempfile.TemporaryDirectory()as d:
   path=Path(d)/'recipe.json';path.write_text('{"schema":1,"schema":1}',encoding='utf-8')
   with self.assertRaises(f.FieldError):w.load(path)
if __name__=='__main__':unittest.main()
