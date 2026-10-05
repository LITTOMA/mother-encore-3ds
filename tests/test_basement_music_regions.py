"""Manual source and malformed-recipe coverage; not run during development."""
import copy,sys,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import basement_music_regions as source
class BasementMusicSource(unittest.TestCase):
    @classmethod
    def setUpClass(cls):cls.ir=source.load(ROOT)
    def test_source(self):self.assertEqual(self.ir,source.build(ROOT))
    def test_staged_pack(self):self.assertEqual(source.stage_files(ROOT/'romfs'),{Path('sound/banks/house.encmusic'):source.encode(self.ir)})
    def reject(self,key,value):
        ir=copy.deepcopy(self.ir);ir[key]=value
        with self.assertRaises((ValueError,KeyError,TypeError)):source.encode(ir)
    def test_version(self):self.reject('schema',2)
    def test_pin(self):self.reject('commit','0'*40)
    def test_unknown_field(self):self.reject('arbitrary_music',True)
    def test_gain_nonfinite(self):
        ir=copy.deepcopy(self.ir);ir['region']['volume_db']=float('nan')
        with self.assertRaises(ValueError):source.encode(ir)
    def test_stop_nonfinite(self):self.reject('default_stop_seconds',float('nan'))
    def test_geometry(self):self.reject('geometry',[0,0,0,1])
    def test_asset_hash(self):
        ir=copy.deepcopy(self.ir);ir['track']['source_sha256']='0'*64
        with self.assertRaises(ValueError):source.encode(ir)
    def test_unknown_shape(self):
        ir=copy.deepcopy(self.ir);ir['region']['shape_paths'].append('Unknown')
        with self.assertRaises(ValueError):source.encode(ir)
if __name__=='__main__':unittest.main()
