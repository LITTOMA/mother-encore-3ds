import copy,json,tempfile,unittest
from pathlib import Path
from tools import emulator_walk_route as w
ROOT=Path(__file__).resolve().parents[1]
class EmulatorWalkRoute(unittest.TestCase):
 @classmethod
 def setUpClass(c):c.map=(ROOT/'romfs/world/podunk/podunk.encmap').read_bytes();c.plan=json.loads((ROOT/'tests/fixtures/mayor-walk-route.json').read_bytes())
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
