#!/usr/bin/env python3
"""Source semantic and negative checks for external startup UI bindings."""
import copy
import hashlib
import json
from pathlib import Path
import sys
import unittest
from unittest.mock import patch
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import startup_settings_bindings as bindings
from tools import startup_settings_assets as settings

class BindingsTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):cls.ir=bindings.load_json(bindings.IR)
    def rejected(self,change):
        candidate=copy.deepcopy(self.ir);change(candidate)
        with self.assertRaises((ValueError,TypeError,KeyError)):bindings.validate(candidate)
    def test_default_recipe_and_pack_identity(self):
        bindings.validate(self.ir)
        recipe=json.loads(settings.IR.read_text())
        self.assertEqual(settings.extract(),{k:v for k,v in recipe.items()if k!='outputs'})
        self.assertEqual(settings.encode(recipe),settings.PACK.read_bytes())
    def test_unknown_version_and_fields(self):
        for value in (0,2,True,'1'):self.rejected(lambda r:r.update(schema=value))
        self.rejected(lambda r:r.update(unsupported=1))
        self.rejected(lambda r:r['panels'][0].update(unsupported=1))
    def test_missing_duplicate_and_unknown_rows(self):
        self.rejected(lambda r:r['rows'].pop())
        self.rejected(lambda r:r['rows'].__setitem__(1,copy.deepcopy(r['rows'][0])))
        self.rejected(lambda r:r['rows'][0].update(role='unknown'))
        self.rejected(lambda r:r['rows'][0].update(label='../../unknown'))
        self.rejected(lambda r:r['rows'][0].update(value=r['rows'][1]['value']))
    def test_panel_coverage_and_label_mapping(self):
        self.rejected(lambda r:r['panels'].pop())
        self.rejected(lambda r:r['panels'][0]['labels'].pop())
        self.rejected(lambda r:r['panels'][0]['labels'].reverse())
        self.rejected(lambda r:r['panels'][1].update(inherited_source=r['shader']))
        self.rejected(lambda r:r['panels'][0]['value_keys'].__setitem__(0,'MENU_NOPE'))
        self.rejected(lambda r:r['panels'][0].update(value_constant='FLAVORS'))
    def test_confirmation_order_identity_and_mapping(self):
        self.rejected(lambda r:r['confirmation_fields'].pop())
        self.rejected(lambda r:r['confirmation_fields'].reverse())
        self.rejected(lambda r:r['confirmation_fields'][0].update(icon=r['confirmation_fields'][1]['icon']))
        self.rejected(lambda r:r['confirmation_fields'][0].update(target='unknown'))
        self.rejected(lambda r:r['confirmation_choices'].reverse())
        self.rejected(lambda r:r['confirmation_choices'].pop())
    def test_resource_roles_and_material(self):
        self.rejected(lambda r:r['resources'].pop())
        self.rejected(lambda r:r['resources'][1].update(role='box'))
        self.rejected(lambda r:r['resources'][0].update(node=r['confirmation_fields'][0]['icon']))
    def test_skin_complete_typed_mapping(self):
        self.rejected(lambda r:r['skin_bindings'].pop())
        self.rejected(lambda r:r['skin_bindings'].append(copy.deepcopy(r['skin_bindings'][0])))
        self.rejected(lambda r:r['skin_bindings'][0].update(role='unknown'))
        self.rejected(lambda r:r['skin_bindings'][0].update(path='world-preview/background.t3x'))
        self.rejected(lambda r:r['skin_bindings'][0].update(source='Graphics/Battle Sprites/lamp.png'))
        self.rejected(lambda r:r['skin_bindings'][0].update(manifest='../../source.json'))
        self.rejected(lambda r:r['skin_nodes'][0].update(node='missing'))
        self.rejected(lambda r:r['skin_nodes'].append(copy.deepcopy(r['skin_nodes'][0])))
        self.rejected(lambda r:r['skin_manifests'].pop(next(iter(r['skin_manifests']))))
    def test_source_identity_and_duplicate_json(self):
        self.rejected(lambda r:r.update(commit='0'*40))
        self.rejected(lambda r:r['sources'].__setitem__(r['scene'],'0'*64))
        with self.assertRaises(ValueError):
            with patch.object(Path,'read_text',return_value='{"schema":1,"schema":1}'):
                bindings.load_json('ignored')
    def test_semantic_fact_changes_rejected_independently_of_fingerprint(self):
        actual=bindings.Extractor(ROOT)
        cases=[(self.ir['script'],'_show_setting_panel(_text_speed_menu)','_show_setting_panel(_flavors_menu)'),
               (self.ir['scenario'],'sprite: Ninten','sprite: Ana'),
               (self.ir['scene'],'name="Fast" type="Label" parent="CanvasLayer/TextSpeed/VBoxContainer"','name="Unknown" type="Label" parent="CanvasLayer/TextSpeed/VBoxContainer"'),
               (self.ir['skin_nodes'][1]['scene'],'texture = ExtResource( 1 )','texture = ExtResource( 3 )')]
        for path,before,after in cases:
            text=actual.text(path);self.assertIn(before,text)
            altered=text.replace(before,after,1)
            real_text=actual.text
            def replacement(name):return altered if name==path else real_text(name)
            with patch.object(actual,'text',replacement):
                with self.assertRaises((ValueError,KeyError)):bindings.validate(self.ir,actual)

if __name__=='__main__':unittest.main()
