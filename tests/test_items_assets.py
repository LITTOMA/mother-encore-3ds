import copy
import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch
from PIL import Image
from tools import items_assets as a


class ItemsAssetsTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.ir=json.loads(a.IR.read_text())
        cls.recipe=json.loads(a.RECIPE.read_text())
        cls.reference=json.loads((a.REPORT/'native-layout.json').read_text())
        cls.review=json.loads((a.REPORT/'source-review.json').read_text())

    def fixture(self,root):
        source=root/'art.png';Image.new('RGBA',(8,8),(2,4,6,0)).save(source)
        recipe=dict(schema=1,commit=a.PIN,game_version='fixture',licence_review=a.LICENSE_REVIEW,sources={'art.png':a.sha(source)},resources=[dict(id=1,name='art',source='art.png',size=[8,8],grid=[1,1],output='graphics/ui/items/art.t3x',reuse=False)])
        return recipe,dict(commit=a.PIN,game_version='fixture')

    def test_pristine_sources_resources_and_ir_verify(self):
        a.verify()

    def test_original_inventory_and_resolved_description(self):
        self.assertEqual(self.ir['capacity'],16)
        self.assertEqual(self.ir['initial_inventory'],[dict(id=1,definition=0,equipped=True,doses=1)])
        self.assertEqual(len(self.ir['definitions']),1)
        item=self.ir['definitions'][0]
        self.assertEqual((item['source'],item['name'],item['flags'],item['equipment_slot'],item['can_use']),('BaseballCap','Baseball Cap',1,3,1))
        self.assertEqual(item['description'],'A hat for baseball players.\nOther @ Defense +5.')
        self.assertEqual((item['heal_hp'],item['heal_pp'],item['max_hp_boost'],item['max_pp_boost']),(0,0,0,0))
        self.assertEqual(self.reference['line_widths'],[114,89])
        self.assertEqual(self.reference['engine']['hash'],'3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8')

    def test_source_one_based_disabled_palette_and_cursor_backplate(self):
        disabled=self.ir['parameters']['DisabledColor']
        self.assertEqual([round(v*255)for v in disabled],[122,108,134,255])
        cursor,back=[self.ir['layouts'][self.review['layout_bindings'][k]]for k in ['cursor','cursor_back']]
        self.assertEqual((cursor['flags'],back['flags']),(5,9))
        self.assertEqual(back['parent'],cursor['id']-1)
        self.assertEqual(back['rect'],[-5,-3,10,8])
        self.assertEqual([round(v*255)for v in back['color']],[20,17,23,255])

    def test_native_container_layout_remains_unscaled(self):
        p=self.ir['parameters'];self.assertEqual(p['GridShape'],[2,5,112,16])
        self.assertEqual(p['LabelSize'],[100,12,0,0])
        self.assertEqual(p['SourceViewport'],[320,180,0,0]);self.assertEqual(p['PlatformViewport'],[400,240,0,0])
        labels=[n for n in self.ir['layouts']if n['role']=='ItemLabel']
        self.assertEqual([n['frame']for n in labels],list(range(10)))
        self.assertEqual([n['rect']for n in labels],[[i%2*112,i//2*16,100,12]for i in range(10)])
        info=self.ir['layouts'][self.review['layout_bindings']['info']]
        self.assertEqual((info['rect'],info['anchor']),([32,200,256,48],[.5,1]))
        self.assertEqual(p['InfoMotion'],[.1,68,0,0])
        desc=[n for n in self.ir['layouts']if n['role']=='Description']
        self.assertEqual([n['rect']for n in desc],[[37,12,213,12],[37,25,213,12]])

    def test_native_hint_uses_original_distinct_font(self):
        resource=self.recipe['resources'][8]
        self.assertEqual((resource['source'],resource['glyph']),('Fonts/BottleRocket.ttf','L'))
        self.assertEqual(self.reference['hint_metrics'],dict(advance=7,ascent=11,height=11,size=16))
        hint=self.ir['layouts'][self.review['layout_bindings']['hint']]
        self.assertEqual(hint['rect'],[227,-9,16,18])
        self.assertEqual(self.ir['parameters']['InputBinding'],[6,16777238,512,0])

    def test_original_menu_clips_and_cursor_idle(self):
        opened,closed,idle=self.ir['clips']
        self.assertEqual([opened['duration'],closed['duration'],idle['duration']],[.1,.1,.8])
        self.assertEqual([k['value'][:2]for k in opened['tracks'][0]['keys']],[[32,-100],[32,8]])
        self.assertEqual([k['value'][:2]for k in closed['tracks'][0]['keys']],[[32,8],[32,-100]])
        self.assertEqual([k['ease']for k in opened['tracks'][0]['keys']],[.25,.25])
        self.assertEqual([k['value'][0]for k in closed['tracks'][1]['keys']],[1,0])
        self.assertEqual(len(idle['tracks']),3)
        for track in idle['tracks']:
            self.assertEqual(track['property'],'Frame')
            self.assertEqual([k['time']for k in track['keys']],[0,.2,.4,.6])
            self.assertEqual([k['value'][0]for k in track['keys']],[0,1,2,1])

    def test_existing_font_box_cursor_are_checked_references(self):
        reused=[r for r in self.recipe['resources']if r['reuse']]
        self.assertEqual([r['name']for r in reused],['box','cursor','font'])
        self.assertEqual(self.ir['resources'][9]['path'],'graphics/battle/lamp/font.t3x')
        self.assertFalse((a.OUT/'font.t3x').exists())
        self.assertEqual(set(self.ir['dependencies']),{'content/asset-receipts/graphics/battle/lamp/source.json','content/asset-receipts/graphics/battle/round/source.json'})

    def test_rotation_is_lossless_per_frame_including_transparent_rgb(self):
        image=Image.new('RGBA',(24,8));image.putdata([(i%256,i//256,255-i%256,0 if i%3==0 else 255)for i in range(192)])
        up=a.rotate_frames(image,[3,1],1);down=a.rotate_frames(image,[3,1],3)
        for frame in range(3):
            source=image.crop((frame*8,0,frame*8+8,8))
            self.assertEqual(up.crop((frame*8,0,frame*8+8,8)).tobytes(),source.transpose(Image.Transpose.ROTATE_90).tobytes())
            self.assertEqual(down.crop((frame*8,0,frame*8+8,8)).tobytes(),source.transpose(Image.Transpose.ROTATE_270).tobytes())
        for turns,grid,im in [(2,[3,1],image),(1,[1,3],image),(1,[3,1],Image.new('RGBA',(23,8)))]:
            with self.subTest(turns=turns,grid=grid),self.assertRaises(ValueError):a.rotate_frames(im,grid,turns)

    def test_changed_source_is_rejected(self):
        with tempfile.TemporaryDirectory()as directory:
            root=Path(directory);recipe,lock=self.fixture(root);a.validate_source(root,recipe,lock)
            (root/'art.png').write_bytes(b'changed')
            with self.assertRaisesRegex(ValueError,'Changed Items source'):a.validate_source(root,recipe,lock)

    def test_unknown_version_paths_fields_geometry_and_resource_fail(self):
        with tempfile.TemporaryDirectory()as directory:
            root=Path(directory);recipe,lock=self.fixture(root)
            for key,value in [('id',0),('output','../outside.t3x'),('grid',[0,1]),('size',[7,8]),('reuse',True),('crop',[7,7,2,2]),('glyph','X'),('quarter_turns',2),('unknown',1)]:
                bad=copy.deepcopy(recipe);bad['resources'][0][key]=value
                if key=='size':bad['resources'][0]['grid']=[2,1]
                with self.subTest(key=key),self.assertRaises(ValueError):a.validate_source(root,bad,lock)
            for key,value in [('schema',2),('commit','unreviewed'),('licence_review',''),('game_version','unknown')]:
                bad=copy.deepcopy(recipe);bad[key]=value
                with self.subTest(key=key),self.assertRaises(ValueError):a.validate_source(root,bad,lock)

    def test_unknown_animation_track_rejected(self):
        ex,definitions,raw=a.reviewed_extract();raw['clips'][0]['tracks'][0]['path']='.:unsupported'
        with self.assertRaisesRegex(ValueError,'Unreviewed Items animation property'):
            a.export_ir(ex,definitions,raw,self.reference,self.ir['resources'],self.ir['dependencies'],write=False)

    def test_manually_changed_ir_is_rejected(self):
        with tempfile.TemporaryDirectory()as directory:
            path=Path(directory)/'changed-ir.json';bad=copy.deepcopy(self.ir);bad['definitions'][0]['description']='invented';path.write_text(json.dumps(bad))
            with patch.object(a,'IR',path),self.assertRaisesRegex(ValueError,'IR differs'):a.verify()

    def test_native_reference_mutation_is_rejected(self):
        with tempfile.TemporaryDirectory()as directory:
            root=Path(directory);(root/'source-review.json').write_text(json.dumps(self.review));bad=copy.deepcopy(self.reference);bad['nodes']['InfoBox']['rect'][0]+=1;(root/'native-layout.json').write_text(json.dumps(bad))
            with patch.object(a,'REPORT',root),self.assertRaisesRegex(ValueError,'Changed native Items reference'):a.verify()


if __name__=='__main__':unittest.main()
