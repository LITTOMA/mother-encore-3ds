import copy,json,unittest
from pathlib import Path
from tools.world_geometry import geometry,real
ROOT=Path(__file__).resolve().parents[1]
class WorldGeometryTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.house=json.loads((ROOT/'reports/cloud-world/house-exact.json').read_text())
        cls.player=json.loads((ROOT/'reports/cloud-world/player-exact.json').read_text())
    def test_external_geometry_matches_native_export(self):
        actor,polys=geometry(self.house,self.player)
        self.assertEqual(len(actor),8);self.assertEqual(len(polys),105)
        from tools.native_content import compile_ir,parse_pack
        wire=parse_pack(compile_ir(json.loads((ROOT/'content/native-opening.json').read_text()))[0])['sections']
        self.assertEqual([[v['x'],v['y']] for v in wire['Vec2'][:len(actor)]],[list(v) for v in actor])
        for source,row in zip(polys,wire['Polygon']):
            points=wire['Vec2'][row['first_vertex']:row['first_vertex']+row['vertex_count']]
            self.assertEqual([[v['x'],v['y']] for v in points],[list(v) for v in source['vertices']])
            self.assertEqual(bool(row['flags']&1),source['disabled'])
    def test_rounded_value_rejected(self):
        for v in [0.08,1,'1',{'type':'real','value':'nan'}]:
            with self.assertRaises(ValueError):real(v)
    def test_unknown_shape_rejected(self):
        d=copy.deepcopy(self.player)
        for r in d['resources']:
            if r['id']==397:r['class']='CircleShape2D'
        with self.assertRaises(ValueError):geometry(self.house,d)
    def test_unknown_version_rejected(self):
        d=copy.deepcopy(self.house);d['schema']=2
        with self.assertRaises(ValueError):geometry(d,self.player)
    def test_missing_geometry_rejected(self):
        d=copy.deepcopy(self.house);d['nodes']=[n for n in d['nodes']if n['path']!='Collisions']
        with self.assertRaises(ValueError):geometry(d,self.player)
if __name__=='__main__':unittest.main()
