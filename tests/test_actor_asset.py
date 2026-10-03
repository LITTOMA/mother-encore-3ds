import copy
import tempfile
from pathlib import Path
import unittest
from unittest.mock import patch
from tools.actor_asset import validate_source, sha, verify
from tools.upstream import write_json

class ActorAssetTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.root=Path(self.temp.name)
        (self.root/'scene.tscn').write_bytes(b'fixture scene')
        (self.root/'texture.png').write_bytes(b'fixture image fingerprint')
        self.recipe={'schema':1,'game_version':'0.4.1.0','commit':'a'*40,'size':[310,580],
            'grid':[10,20],'sprite_offset':[0,-4],'centered':True,
            'scene':'scene.tscn','scene_sha256':sha(self.root/'scene.tscn'),
            'texture':'texture.png','texture_sha256':sha(self.root/'texture.png'),'licence_review':'fixture'}
        for key in ('lamp_texture','lamp_yaml','npc_script','character_sprite_script','emote_texture','emote_scene','shadow_texture','actor_scene'):
            self.recipe[key]='scene.tscn';self.recipe[key+'_sha256']=sha(self.root/'scene.tscn')
        self.recipe['lamp_layout']={'size':[68,22],'grid':[4,1],'idle_frame':0,'root':[496,390],'child_offset':[0,9],'auto_offset':[0,-11],'shadow':False}
        self.lock={'commit':'a'*40,'game_version':'0.4.1.0'}
    def tearDown(self):self.temp.cleanup()
    def test_missing_bundle_rejected(self):
        with self.assertRaisesRegex(ValueError,'Missing required'):verify(self.root,self.root/'absent')
    def test_reviewed_source(self):validate_source(self.root,self.recipe,self.lock)
    def test_unknown_schema_version_layout_rejected(self):
        for key,value in [('schema',2),('game_version','0.4.2.0'),('size',[320,580]),
                          ('grid',[10,10]),('sprite_offset',[0,0]),('centered',False)]:
            recipe=copy.deepcopy(self.recipe);recipe[key]=value
            with self.assertRaises(ValueError):validate_source(self.root,recipe,self.lock)
    def test_changed_source_or_pin_rejected(self):
        with self.assertRaises(ValueError):validate_source(self.root,self.recipe,dict(self.lock,commit='b'*40))
        (self.root/'texture.png').write_bytes(b'changed')
        with self.assertRaises(ValueError):validate_source(self.root,self.recipe,self.lock)
    def test_permission_and_path_guard(self):
        for recipe in [dict(self.recipe,licence_review=''),dict(self.recipe,texture='../outside')]:
            with self.assertRaises(ValueError):validate_source(self.root,recipe,self.lock)
    def test_receipt_and_output_integrity(self):
        out=self.root/'output';out.mkdir();target=out/'ninten-main.t3x';target.write_bytes(b'fixture compiled image')
        lamp=out/'lamp.t3x';lamp.write_bytes(b'fixture lamp')
        extra=[]
        for name in ('emotes.t3x','shadow.t3x'):
            p=out/name;p.write_bytes(b'fixture sprite');extra.append(p)
        receipt={'schema':1,'recipe':self.recipe,'outputs':{p.name:{'sha256':sha(p),'bytes':p.stat().st_size} for p in (target,lamp,*extra)}}
        write_json(out/'source.json',receipt)
        with patch('tools.actor_asset.read_json',side_effect=[self.recipe,self.lock,receipt]):verify(self.root,out)
        target.write_bytes(b'tampered')
        with patch('tools.actor_asset.read_json',side_effect=[self.recipe,self.lock,receipt]):
            with self.assertRaisesRegex(ValueError,'differs'):verify(self.root,out)
        for broken in [dict(receipt,schema=2),dict(receipt,outputs={})]:
            with patch('tools.actor_asset.read_json',side_effect=[self.recipe,self.lock,broken]):
                with self.assertRaises(ValueError):verify(self.root,out)

if __name__=='__main__':unittest.main()
