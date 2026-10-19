import copy,json,tempfile,unittest
from pathlib import Path
from tools import emulator_walk_route as w
ROOT=Path(__file__).resolve().parents[1]
class EmulatorWalkRoute(unittest.TestCase):
 @classmethod
 def setUpClass(c):
  from unittest.mock import patch
  from tools import podunk_field as f,field_npcs as n
  c.current=(ROOT/'romfs/world/podunk/podunk.encmap').read_bytes();c.plan=json.loads((ROOT/'tests/fixtures/mayor-walk-route.json').read_bytes())
  recipe=f.load_recipe();previous=json.loads((ROOT/'tests/fixtures/pre-cemetery-npcs.json').read_bytes())
  # The original route remains frozen against its actual three-NPC map.
  # Reconstruct that map from the same source using only its frozen owner list.
  with patch.object(n,'load_policy',return_value=previous):c.previous_model=f.Model(recipe)
  c.current_model=f.Model(recipe)
  def encode(model):
   atlases=f.Atlases(model);paths=[recipe['atlas_directory']+'/atlas-%02d.t3x'%i for i in range(len(atlases.pages))]
   return f.encode_map(model,atlases,paths)
  c.map=encode(c.previous_model);c.regenerated=encode(c.current_model)
 def test_cemetery_owners_preserve_existing_route_geometry(self):
  import hashlib
  a,b=self.previous_model,self.current_model
  self.assertEqual(self.current,self.regenerated)
  for name in ['tile_rows','shape_rows','points','layers','chunks','cells','sprites','openables','doors','cameras','bodies']:
   self.assertEqual(getattr(a,name),getattr(b,name),name)
  def text(model,span):return bytes(model.strings[span[0]:span[0]+span[1]]).decode()
  def conditions(model):return [(text(model,ref),expected)for ref,expected in model.conditions]
  self.assertEqual(conditions(a),conditions(b))
  def boundary(model,row):
   result=dict(row)
   for key in ['path','label']:result[key]=text(model,row[key])
   first,count=result.pop('cond');result['conditions']=conditions(model)[first:first+count]
   return result
  self.assertEqual([boundary(a,r)for r in a.boundaries],[boundary(b,r)for r in b.boundaries])
  added=[r for r in b.items if r['kind']==6 and r['first']in(4,5)]
  self.assertEqual([r['first']for r in added],[4,5]);self.assertTrue(all(r['sort_y']==4536 for r in added))
  self.assertLess(max(p[1]for p in self.plan['waypoints']),4500)
  self.assertEqual([r for r in b.items if not(r['kind']==6 and r['first']in(4,5))],a.items)
  # Current callers still require the current exact map SHA; no fallback/ignore.
  current=dict(self.plan,map_sha256=hashlib.sha256(self.current).hexdigest())
  self.assertEqual(w.validate(current,self.current),current)
  with self.assertRaises(ValueError):w.validate(self.plan,self.current)

 def test_source_and_all_route_points_match_checked_map(self):self.assertEqual(w.load(ROOT/'tests/fixtures/mayor-walk-route.json',self.map),self.plan)
 def test_unknown_and_duplicate_envelopes_reject(self):
  for edit in [lambda p:p.update(schema=True),lambda p:p.update(schema=2),lambda p:p.update(extra=0),lambda p:p.pop('grid'),lambda p:p.update(route=True),lambda p:p.update(source_commit='0'*40),lambda p:p.update(map_sha256='0'*64),lambda p:p.update(expanded=250001)]:
   p=copy.deepcopy(self.plan);edit(p)
   with self.assertRaises(ValueError):w.validate(p,self.map)
  with tempfile.TemporaryDirectory()as tmp:
   p=Path(tmp)/'plan';p.write_text('{"schema":1,"schema":1}')
   with self.assertRaisesRegex(ValueError,'Duplicate'):w.load(p,self.map)
 def test_bad_route_geometry_rejects(self):
  for points in [[],[[0,0]],[[0,0],[0,0]],[[0,0],[8,8]],[[0,0],[3,0]],[[0,0],[float('inf'),0]],[[0,0],[True,0]],[[0,0],[100001,0]],[[0],[0,8]]]:
   p=copy.deepcopy(self.plan);p['waypoints']=points
   with self.assertRaises(ValueError):w.validate(p,self.map)
 def test_wrong_map_and_unbounded_path_reject(self):
  for blob in [b'',self.map[:-1],b'x'+self.map[1:]]:
   with self.assertRaises(ValueError):w.validate(self.plan,blob)
  for path in ['romfs/world/../escape.encmap','romfs/world//x.encmap','/absolute','romfs\\world\\x.encmap']:
   p=copy.deepcopy(self.plan);p['map_path']=path
   with self.assertRaises(ValueError):w.validate(p,self.map)
  with tempfile.TemporaryDirectory()as tmp:
   p=Path(tmp)/'plan'
   with self.assertRaises(ValueError):w.load(p,self.map)
   p.write_bytes(b' '*65537)
   with self.assertRaises(ValueError):w.load(p,self.map)
if __name__=='__main__':unittest.main()
