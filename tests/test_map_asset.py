import copy
import tempfile
from pathlib import Path
import unittest
from unittest.mock import patch
from tools.map_asset import validate_source, layout, sha, verify
from tools.upstream import write_json

class MapAssetTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.root=Path(self.temp.name)
        (self.root/'scene.tscn').write_bytes(b'fixture scene')
        (self.root/'texture.png').write_bytes(b'fixture image fingerprint, not decoded')
        self.recipe={'schema':1,'game_version':'0.4.1.0','commit':'a'*40,'tile_size':256,'size':[624,1104],
            'scene':'scene.tscn','scene_sha256':sha(self.root/'scene.tscn'),
            'texture':'texture.png','texture_sha256':sha(self.root/'texture.png'),'licence_review':'test fixture'}
        self.lock={'commit':'a'*40,'game_version':'0.4.1.0'}
    def tearDown(self):self.temp.cleanup()
    def test_layout_preserves_entire_background(self):
        tiles=layout(self.recipe);self.assertEqual(len(tiles),15)
        self.assertEqual(sum(t['width']*t['height'] for t in tiles),624*1104)
        self.assertEqual(tiles[-1],{'index':14,'x':512,'y':1024,'width':112,'height':80})
        validate_source(self.root,self.recipe,self.lock)
    def test_unknown_schema_version_layout_rejected(self):
        for key,value in [('schema',2),('game_version','0.4.2.0'),('tile_size',128),('size',[640,1104])]:
            recipe=copy.deepcopy(self.recipe);recipe[key]=value
            with self.assertRaises(ValueError):validate_source(self.root,recipe,self.lock)
    def test_changed_source_or_pin_rejected(self):
        (self.root/'texture.png').write_bytes(b'changed')
        with self.assertRaises(ValueError):validate_source(self.root,self.recipe,self.lock)
        with self.assertRaises(ValueError):validate_source(self.root,self.recipe,dict(self.lock,commit='b'*40))
    def test_unreviewed_asset_rights_rejected(self):
        recipe=dict(self.recipe,licence_review='')
        with self.assertRaises(ValueError):validate_source(self.root,recipe,self.lock)
    def test_external_source_path_rejected(self):
        recipe=dict(self.recipe,texture='../outside.png')
        with self.assertRaises(ValueError):validate_source(self.root,recipe,self.lock)
    def test_stale_or_partial_compiled_receipt_rejected(self):
        out=self.root/'output';out.mkdir()
        write_json(out/'source.json',{'schema':2})
        with patch('tools.map_asset.read_json',side_effect=[{'schema':2},self.recipe,self.lock]):
            with self.assertRaises(ValueError):verify(self.root,out)
        with patch('tools.map_asset.read_json',side_effect=[{'schema':1,'recipe':self.recipe,'outputs':{}},self.recipe,self.lock]):
            with self.assertRaisesRegex(ValueError,'Incomplete'):verify(self.root,out)

if __name__=='__main__':unittest.main()
