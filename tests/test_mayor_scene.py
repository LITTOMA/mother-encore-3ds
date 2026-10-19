import copy,json,tempfile,unittest
from pathlib import Path
from unittest.mock import patch
from tools import mayor_scene as m,podunk_field as f,resource_catalog as catalog,world_scene_routes as routes

class MayorScenePublication(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.files,cls.proof=m.payloads()
 def test_checked_release_outputs_and_source_fixtures(self):
  self.assertEqual(m.stage_files(),self.files)
  for name,fixture in [('world/mayor/mayor.encmap','mayor-area.encmap'),('world/mayor/mayor.encstory','mayor-initial.encstory')]:self.assertEqual(self.files[Path(name)],(m.ROOT/'tests/fixtures'/fixture).read_bytes())
  self.assertEqual(len([p for p in self.files if p.suffix=='.encmick']),5)
  # The source scope now also includes the independently appended Iggy owner;
  # Mayor's own five NPC packets and frozen map/story fixtures stay exact.
  c=catalog.decode((m.ROOT/'romfs/system/native.encresources').read_bytes());self.assertEqual(len(c['bindings']),63)
  self.assertEqual(sorted(r['id']for r in c['bindings']if r['role']=='FieldNpc'),list(range(320,330)))
  graph=routes.load();self.assertEqual(len(graph['scenes']),3);self.assertEqual(len(graph['routes']),6)
  self.assertEqual(next(s for s in graph['scenes']if s['id']==3)['assets']['steps'],1030)
 def test_texture_pcm_and_room_corruption_rejected(self):
  with tempfile.TemporaryDirectory(dir=m.ROOT/'build')as tmp:
   root=Path(tmp)
   for name,data in self.files.items():p=root/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(data)
   for name in ['graphics/world/mayor/atlas-00.t3x','graphics/world/mayor/pippi.t3x',next(str(p)for p in self.files if p.suffix=='.pcm'),'world/mayor/mayor.encroom']:
    with self.subTest(name=name):
     p=root/name;before=p.read_bytes();p.write_bytes(before[:-1]+bytes([before[-1]^1]))
     with self.assertRaises((ValueError,f.FieldError)):m.stage_files(root)
     p.write_bytes(before)
 def test_missing_and_changed_receipts_reject(self):
  receipt=json.loads(m.RECEIPT.read_bytes())
  with tempfile.TemporaryDirectory()as tmp:
   p=Path(tmp)/'source.json'
   for edit in [lambda r:r.update(schema=True),lambda r:r.update(schema=2),lambda r:r.update(commit='0'*40),lambda r:r.update(extra=0),lambda r:r.update(map_sources={}),lambda r:r.update(npc_sources={}),lambda r:r['textures'].clear()]:
    data=copy.deepcopy(receipt);edit(data);p.write_text(json.dumps(data))
    with patch.object(m,'RECEIPT',p),self.assertRaises((ValueError,f.FieldError)):m.payloads()
   with patch.object(m,'BUILD_RECEIPT',Path(tmp)/'absent'),self.assertRaises(OSError):m.stage_files()
 def test_route_recipe_path_and_source_graph_rejected(self):
  recipe=f.load_recipe()
  with tempfile.TemporaryDirectory()as tmp:
   p=Path(tmp)/'recipe.json'
   for value in [None,'../escape','content/../escape.json','content\\routes.json','content//routes.json']:
    bad=copy.deepcopy(recipe);bad['route_recipe']=value;p.write_text(json.dumps(bad))
    with self.assertRaises(f.FieldError):f.load_recipe(p)
   bad=copy.deepcopy(recipe);bad['routes'][2]['to']=2;p.write_text(json.dumps(bad))
   with self.assertRaises(f.FieldError):f.load_recipe(p)
 def test_checked_paths_cannot_escape_or_accept_changed_bytes(self):
  with tempfile.TemporaryDirectory()as tmp:
   root=Path(tmp);(root/'valid').write_bytes(b'abc')
   for name in ['', '../escape','/absolute','x/../escape','C:/escape','x\\escape']:
    with self.assertRaises(f.FieldError):m.checked(root,name,dict(bytes=3,sha256=m.sha(b'abc')))
   with self.assertRaises(f.FieldError):m.checked(root,'valid',dict(bytes=3,sha256='0'*64))
if __name__=='__main__':unittest.main()
