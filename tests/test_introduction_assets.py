#!/usr/bin/env python3
"""Focused checked authoring/staging tests; synthetic bytes never claim GPU builds."""
import copy
import hashlib
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch
import zlib

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from tools import introduction_assets as A


class IntroductionAssets(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.recipe=A.read(A.RECIPE)
        cls.texts=A.validate(cls.recipe)

    def reject_recipe(self,mutate):
        recipe=copy.deepcopy(self.recipe);mutate(recipe)
        with self.assertRaises((ValueError,TypeError,KeyError)):A.validate(recipe)

    def test_original_nodes_fonts_text_and_cloud_pixels(self):
        self.assertEqual(len(self.texts),16)
        self.assertTrue(self.texts['INTRO_CUTSCENE_OLD_01']['zh_Hans_CN'])
        cloud=A.art_records(self.recipe)[-1]
        self.assertEqual((cloud['width'],cloud['height'],cloud['columns'],cloud['rows'],cloud['frame_count']),(720,164,8,4,31))
        self.assertEqual((cloud['source_width'],cloud['source_height'],cloud['trim_x'],cloud['trim_y']),(352,96,131,33))
        now=[f for f in self.recipe['fonts']if f['role']=='now']
        self.assertEqual([f['source']for f in now],['Fonts/EBMain.tres']*2)
        self.assertEqual(now[0]['definition'],now[1]['definition'])

    def test_unknown_version_fields_and_bool_identity(self):
        for mutate in (lambda r:r.update(schema=2),lambda r:r.update(unknown=1),lambda r:r['resources'][0].update(id=True)):
            with self.subTest(mutate=mutate):self.reject_recipe(mutate)

    def test_pin_missing_source_and_scene_binding(self):
        for mutate in (lambda r:r['sources'].update({'LICENSE':'0'*64}),lambda r:r['sources'].pop(r['resources'][0]['source']),lambda r:r['resources'][0].update(node='Images/Intro_1')):
            with self.subTest(mutate=mutate):self.reject_recipe(mutate)

    def test_duplicate_ids_roles_paths_and_traversal(self):
        for key in ('id','role','output'):
            with self.subTest(key=key):self.reject_recipe(lambda r:r['resources'][1].update({key:r['resources'][0][key]}))
        self.reject_recipe(lambda r:r['resources'][0].update(output='graphics/cutscenes/introduction/../../escaped.t3x'))

    def test_cloud_rejects_pixel_loss_grid_and_bool_size(self):
        for mutate in (lambda r:r['resources'][-1].update(trim=[132,33,221,74]),lambda r:r['resources'][-1].update(output_grid=[1,1]),lambda r:r['resources'][0].update(size=[True,112])):
            with self.subTest(mutate=mutate):self.reject_recipe(mutate)

    def test_font_remap_fallback_and_actual_node(self):
        for mutate in (lambda r:r['fonts'][0]['definition']['properties'].update(size='12'),lambda r:r['fonts'][0]['definition']['fallback_chain'][0].update(path='Fonts/EBMain.ttf'),lambda r:r['fonts'][0].update(node='Text/Skip/Label'),lambda r:r.update(font_catalog='../wrong.encfont')):
            with self.subTest(mutate=mutate):self.reject_recipe(mutate)

    def test_duplicate_json_and_unsafe_paths(self):
        with self.assertRaises(ValueError):json.loads('{"schema":1,"schema":2}',object_pairs_hook=A.unique)
        for path in ('../x','/x','a\\b','a//b','C:/x','a/./b'):
            with self.subTest(path=path),self.assertRaises(ValueError):A.safe(ROOT,path)

    def fixture(self,project):
        """Checked stage fixture only; no fake T3X interpreted as actual texture."""
        project=Path(project);root=project/'romfs';root.mkdir()
        recipe_path=project/'recipe.json';A.write_json(recipe_path,self.recipe)
        resources=A.art_records(self.recipe)
        for i,row in enumerate(resources):
            raw=b'stage-only-image-'+bytes([i]);p=A.safe(root,row['path']);p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(raw)
            row.update(bytes=len(raw),sha256=hashlib.sha256(raw).hexdigest(),crc32=zlib.crc32(raw))
        pages=[];faces=[];glyphs=[];native=[]
        for i,spec in enumerate(A.font_specs(self.recipe)):
            cps=sorted({ord(c)for same in self.recipe['fonts']if same['source']==spec['source']for key in same['text_keys']for c in self.texts[key][same['locale']]if ord(c)>=32}|{ord(c)for same in self.recipe['fonts']if same['source']==spec['source']for c in same['extra_characters']})
            raw=bytes([i])*(256*256*2);name='fixture-%d.t3x'%i;p=root/'fonts/introduction'/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(raw)
            pages.append(dict(path=name,width=256,height=256,texture_bytes=len(raw),file_bytes=len(raw),crc32=zlib.crc32(raw),sha256=hashlib.sha256(raw).hexdigest()))
            first=len(glyphs)
            glyphs.extend(dict(codepoint=cp,page=i,u=1,v=1,width=1,height=1,advance=8,offset_x=0,offset_y=0,source_face=spec['definition']['fallback_chain'][0]['path'])for cp in cps)
            faces.append(dict(source=spec['source'],legacy_ascii=False,first_glyph=first,glyph_count=len(cps),first_page=i,page_count=1,ascent=12,descent=4,height=16,definition=spec['definition']))
        for spec,face in zip(A.font_specs(self.recipe),faces):
            lines=sorted({line for same in self.recipe['fonts']if same['source']==spec['source']for key in same['text_keys']for line in self.texts[key][same['locale']].split('\n')})
            spacing=int(spec['definition']['properties'].get('extra_spacing_char','0'));pairs=[];widths=[]
            for line in lines:
                widths.append(sum(8+(spacing if i+1<len(line)and c!=' 'else 0)for i,c in enumerate(line)))
                pairs.extend(dict(codepoint=ord(c),next=ord(line[i+1])if i+1<len(line)else 0,advance=8+(spacing if i+1<len(line)and c!=' 'else 0))for i,c in enumerate(line))
            native.append(dict(source=spec['source'],size=int(spec['definition']['properties'].get('size','16')),ascent=12,descent=4,height=16,advances=[8]*face['glyph_count'],pairs=pairs,texts=lines,widths=widths,char_spacing=spacing,space_spacing=int(spec['definition']['properties'].get('extra_spacing_space','0')),data_settings=spec['definition']['fallback_chain']))
        binary=A.encode_font(faces,pages,glyphs);p=A.safe(root,self.recipe['font_catalog']);p.write_bytes(binary)
        common=dict(schema=1,commit=self.recipe['commit'],recipe_sha256=A.sha(recipe_path),generator_sha256=A.sha(A.__file__),tex3ds_sha256='0'*64,sources=self.recipe['sources'])
        art=dict(common,kind='encore.introduction-asset-receipt',resources=resources,font_catalog=self.recipe['font_catalog'])
        font=dict(common,kind='encore.introduction-font-receipt',metrics_generator_sha256=A.sha(ROOT/'tools/source_fonts.py'),godot_sha256='0'*64,pillow_version='test',freetype_version='test',fonttools_version='test',metrics=dict(freetype_metrics_version='2.12.1',version=dict(major=3,minor=6,patch=2,hash='3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8'),faces=native),faces=faces,pages=pages,glyphs=glyphs,font_embedded_notices={},binary=dict(path=self.recipe['font_catalog'],bytes=len(binary),sha256=hashlib.sha256(binary).hexdigest()),limits='stage fixture only')
        A.write_json(project/A.ART_RECEIPT,art);A.write_json(project/A.FONT_RECEIPT,font)
        return root,recipe_path,art,font

    def test_checked_stage_complete_files_and_no_receipts(self):
        with tempfile.TemporaryDirectory(dir=ROOT/'build',prefix='intro-assets-test-')as d:
            root,recipe,_,_=self.fixture(d)
            with patch.object(A,'validate',return_value=self.texts),patch.object(A,'translations',return_value=self.texts):
                files=A.stage_files(root,Path(d),recipe)
                self.assertEqual(len(files),17)
                self.assertTrue(all(p.suffix in ('.t3x','.encfont')for p in files))

    def test_stage_fingerprint_missing_and_truncated_output(self):
        for damage in ('missing','truncated','changed'):
            with self.subTest(damage=damage),tempfile.TemporaryDirectory(dir=ROOT/'build',prefix='intro-assets-test-')as d:
                root,recipe,art,_=self.fixture(d);p=root/art['resources'][0]['path']
                if damage=='missing':p.unlink()
                elif damage=='truncated':p.write_bytes(p.read_bytes()[:-1])
                else:p.write_bytes(b'X'+p.read_bytes()[1:])
                with patch.object(A,'validate',return_value=self.texts),patch.object(A,'translations',return_value=self.texts),self.assertRaises((ValueError,OSError)):A.stage_files(root,Path(d),recipe)

    def test_stage_receipt_unknown_geometry_and_stale_pin(self):
        for damage in ('version','unknown','geometry','pin','glyph','pairs','width','probe_version'):
            with self.subTest(damage=damage),tempfile.TemporaryDirectory(dir=ROOT/'build',prefix='intro-assets-test-')as d:
                root,recipe,art,font=self.fixture(d)
                if damage=='version':art['schema']=2
                elif damage=='unknown':art['extra']=1
                elif damage=='geometry':art['resources'][-1]['frame_count']=32
                elif damage=='pin':font['recipe_sha256']='0'*64
                elif damage=='glyph':font['glyphs'][0]['advance']=9
                elif damage=='pairs':font['metrics']['faces'][0]['pairs'].pop()
                elif damage=='width':font['metrics']['faces'][0]['widths'][0]+=1
                else:font['metrics']['version']['patch']=3
                A.write_json(Path(d)/A.ART_RECEIPT,art);A.write_json(Path(d)/A.FONT_RECEIPT,font)
                with patch.object(A,'validate',return_value=self.texts),patch.object(A,'translations',return_value=self.texts),self.assertRaises(ValueError):A.stage_files(root,Path(d),recipe)

    def test_font_encoder_rejects_unknown_bounds_scalars_and_unbound_rows(self):
        with tempfile.TemporaryDirectory(dir=ROOT/'build',prefix='intro-assets-test-')as d:
            _,_,_,font=self.fixture(d)
            for damage in ('bool','scalar','page','metric','legacy','count'):
                f=copy.deepcopy(font)
                if damage=='bool':f['glyphs'][0]['codepoint']=True
                elif damage=='scalar':f['glyphs'][0]['codepoint']=0xd800
                elif damage=='page':f['glyphs'][0]['page']=127
                elif damage=='metric':f['faces'][0]['height']=float('nan')
                elif damage=='legacy':f['faces'][0]['legacy_ascii']=True
                else:f['faces'][0]['glyph_count']+=1
                with self.subTest(damage=damage),self.assertRaises(ValueError):A.encode_font(f['faces'],f['pages'],f['glyphs'])


if __name__=='__main__':unittest.main()
