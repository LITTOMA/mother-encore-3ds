"""Reviewed world namespace selectors, negative semantics and pack payload proof."""
import copy
import json
from pathlib import Path
import tempfile
import subprocess
import sys
import unittest
from unittest.mock import patch
from tools import world_program_bindings as bindings
from tools.extract_native_content import Extractor
from tools.native_content import compile_ir,HEADER_BYTES

ROOT=bindings.ROOT

class WorldProgramBindingsTest(unittest.TestCase):
    def test_actual_command_line_from_build_directory(self):
        result=subprocess.run([sys.executable,str(ROOT/'tools/native_content.py'),'verify'],cwd=ROOT/'build',capture_output=True,text=True)
        self.assertEqual(result.returncode,0,result.stdout+result.stderr)
    @classmethod
    def setUpClass(cls):
        cls.ir=bindings.load_json(ROOT/bindings.IR)
        cls.baseline=json.loads((ROOT/'content/native-opening.json').read_text())
    def rejected(self,change):
        ir=copy.deepcopy(self.ir);change(ir)
        with self.assertRaises((ValueError,KeyError,TypeError)):bindings.validate(ir,Extractor(ROOT))
    def test_actual_extraction_payload_and_namespace_equivalence(self):
        ex=Extractor(ROOT);actual,mapping=ex.run()
        self.assertEqual(actual['strings'],self.baseline['strings'])
        self.assertEqual(actual['sections'],self.baseline['sections'])
        self.assertEqual(mapping,json.loads((ROOT/'content/native-source-map.json').read_text()))
        self.assertEqual(compile_ir(actual)[0][HEADER_BYTES:],(ROOT/'romfs/data/opening.encroom').read_bytes()[HEADER_BYTES:])
        bindings.verify_room(bindings.load(ex),actual,ex)
    def test_schema_pin_missing_duplicate_json(self):
        for value in (None,0,2,True,'1'):self.rejected(lambda r:r.update(schema=value))
        self.rejected(lambda r:r.update(commit='0'*40))
        self.rejected(lambda r:r.update(unknown=1))
        self.rejected(lambda r:r['sources'].__setitem__(r['scene'],'0'*64))
        with tempfile.TemporaryDirectory(dir=ROOT/'build')as temp:
            source=Path(temp)/'bad.json';source.write_text('{"schema":1,"schema":1}')
            with self.assertRaises(ValueError):bindings.load_json(source)
    def test_program_order_selector_coverage_and_continuation(self):
        self.rejected(lambda r:r['programs'].pop())
        self.rejected(lambda r:r['programs'].reverse())
        self.rejected(lambda r:r['programs'][1].update(stage='unknown'))
        self.rejected(lambda r:r['programs'][0].update(path='../unknown'))
        self.rejected(lambda r:r['programs'][0]['selector'].update(kind='unknown'))
        self.rejected(lambda r:r['programs'][0]['selector'].update(node='Objects/npcdoll'))
        self.rejected(lambda r:r['programs'][2]['selector'].update(program_id=1))
        self.rejected(lambda r:r['programs'][4]['labels'].append('2'))
    def test_actor_and_npc_source_mapping(self):
        self.rejected(lambda r:r['actors'].pop())
        self.rejected(lambda r:r['actors'][2].update(alias='Unknown'))
        self.rejected(lambda r:r['actors'][2].update(node=r['actors'][3]['node']))
        self.rejected(lambda r:r['npc_profiles'].pop())
        self.rejected(lambda r:r['npc_profiles'].reverse())
        self.rejected(lambda r:r['actors'][2].update(resource_role='unknown'))
        self.rejected(lambda r:r['actors'][3].update(resource_role=r['actors'][4]['resource_role']))
        self.rejected(lambda r:r['npc_profiles'][0].update(yaml_index=2,animation_index=1))
        self.rejected(lambda r:r['npc_profiles'][1].update(animation_source=r['npc_profiles'][0]['animation_source']))
        self.rejected(lambda r:r['npc_profiles'][0].update(execution_kind=3))
        self.rejected(lambda r:r['npc_profiles'][0].update(phase='postwin'))
    def test_actor_assets_source_output_identity_mapping(self):
        self.rejected(lambda r:r['animation']['assets'].pop())
        self.rejected(lambda r:r['animation']['assets'].reverse())
        self.rejected(lambda r:r['animation']['assets'][0].update(role='unknown'))
        self.rejected(lambda r:r['animation']['assets'][3].update(role='unknown'))
        self.rejected(lambda r:r['animation']['assets'][2].update(role='shadow'))
        self.rejected(lambda r:r['animation']['assets'][0].update(path=r['animation']['assets'][1]['path']))
        self.rejected(lambda r:r['animation']['assets'][0].update(path='graphics/actors/missing.t3x'))
        self.rejected(lambda r:r['animation']['assets'][0].update(path='graphics/ui/house/ninten-main.t3x'))
        self.rejected(lambda r:r['animation']['assets'][0].update(recipe_key='scene'))
        self.rejected(lambda r:r['animation']['assets'][0].update(grid=[1,1]))
        self.rejected(lambda r:r['animation'].update(sprite_manifest='../source.json'))
    def test_animation_direction_clip_and_actor_namespace(self):
        self.rejected(lambda r:r['animation']['directions'].reverse())
        self.rejected(lambda r:r['animation']['directions'].append('Unknown'))
        self.rejected(lambda r:r['animation']['motions'][0].update(animation='Run'))
        self.rejected(lambda r:r['animation']['motions'][1].update(state=5))
        self.rejected(lambda r:r['animation']['base_profiles'].reverse())
        self.rejected(lambda r:r['animation']['base_profiles'][1]['clips'].reverse())
        self.rejected(lambda r:r['animation']['base_profiles'][1]['clips'][0].update(flags=0))
        self.rejected(lambda r:r['npc_profiles'][0]['clips'].reverse())
        self.rejected(lambda r:r['npc_profiles'][1]['clips'].pop())
        self.rejected(lambda r:r['npc_profiles'][1]['clips'][1].update(animation='Idle'))
        self.rejected(lambda r:r['npc_profiles'][1].update(idle_animation='Talk'))
        self.rejected(lambda r:r['animation']['actor_walk'].update(yaml_index=2,animation_index=1))
        self.rejected(lambda r:r['animation']['actor_walk'].update(prefix='Unknown Actor'))
        self.rejected(lambda r:r['animation']['emotes'][0].update(animation='exclamation'))
        self.rejected(lambda r:r['animation']['emotes'][1].update(actor_id=3))
        self.rejected(lambda r:r['animation']['emotes'][2].update(receipt_key='exclamation'))
    def test_animation_source_semantics_change(self):
        a=self.ir['animation'];recipe=json.loads((ROOT/a['sprite_review']).read_text())
        changes=[(recipe['character_sprite_script'],'dirTitle = " Down"','dirTitle = " Left"'),
                 (a['actor_script'],'var _idle_anim := "Idle"','var _idle_anim := "Talk"'),
                 (recipe['lamp_yaml'],'  Open:','  Unknown:'),
                 (self.ir['npc_profiles'][1]['animation_source'],'  Walk:','  Unknown:'),
                 (self.ir['scene'],'sprite = "Npcs/4dir/mimmie"','sprite = "Npcs/4dir/minnie"')]
        for path,before,after in changes:
            ex=Extractor(ROOT);original=ex.text;source=original(path);self.assertIn(before,source)
            with patch.object(ex,'text',lambda p:source.replace(before,after,1)if p==path else original(p)):
                with self.subTest(path=path),self.assertRaises((ValueError,KeyError)):bindings.validate(self.ir,ex)
    def test_audio_effect_source_order_and_targets(self):
        self.rejected(lambda r:r['audio'].pop())
        self.rejected(lambda r:r['audio'].reverse())
        self.rejected(lambda r:r['audio'][0].update(path=r['audio'][1]['path']))
        self.rejected(lambda r:r['audio'][0]['selector'].update(kind='unknown'))
        self.rejected(lambda r:r['audio'][0]['selector'].update(phrase='4'))
        self.rejected(lambda r:r['effect'].update(id=30))
        self.rejected(lambda r:r['effect'].update(path='../bad.encfx'))
        self.rejected(lambda r:r['effect'].update(source_ir=r['presentation']))
    def test_encounter_source_actor_identity_and_catalog_reference(self):
        self.rejected(lambda r:r['encounters'].pop())
        self.rejected(lambda r:r['encounters'].reverse())
        self.rejected(lambda r:r['encounters'][0].update(actor_id=1))
        self.rejected(lambda r:r['encounters'][1].update(catalog_id=3))
        self.rejected(lambda r:r['encounters'][0].update(resource_path='data/unknown.encbattle'))
        self.rejected(lambda r:r['encounters'][1].update(source_ir='content/native-battle.json'))
        self.rejected(lambda r:r['encounters'][1].update(phrase='0'))
    def test_object_methods_order_coverage_and_wrong_script(self):
        self.rejected(lambda r:r['melody_bindings'].pop())
        self.rejected(lambda r:r['melody_bindings'].reverse())
        self.rejected(lambda r:r['initial_bindings'][0].update(method='unknown'))
        self.rejected(lambda r:r['initial_bindings'][0].update(node='MusicArea'))
        self.rejected(lambda r:r['initial_bindings'][0].update(kind=5))
        self.rejected(lambda r:r['melody_bindings'][0].update(target='effect'))
        self.rejected(lambda r:r['stop_binding'].update(target_binding_id=1))
        self.rejected(lambda r:r['melody_bindings'][1].update(method_source=r['melody_bindings'][0]['method_source']))
    def test_source_semantics_change_without_hash_shortcut(self):
        changes=[('scene',self.ir['scene'],'dialog = "Podunk/cutscenes/lamp_attack"','dialog = "Podunk/cutscenes/doll_attack"'),
                 ('actors','Data/Dialogue/'+self.ir['programs'][1]['path']+'.yaml','doll: Objects/npcdoll','doll: Objects/npc2'),
                 ('sprite',self.ir['scene'],'sprite = "Npcs/1dir/doll"','sprite = "Npcs/1dir/pillow"'),
                 ('method','Data/Dialogue/'+self.ir['programs'][3]['path']+'.yaml','MusicArea: stop_music_immediately','MusicArea: play_music')]
        for name,path,before,after in changes:
            ex=Extractor(ROOT);original=ex.text;source=original(path);self.assertIn(before,source)
            def altered(p):return source.replace(before,after,1)if p==path else original(p)
            with patch.object(ex,'text',altered):
                with self.subTest(name=name),self.assertRaises((ValueError,KeyError)):bindings.validate(self.ir,ex)
    def test_checked_compiled_namespace_mismatches(self):
        ex=Extractor(ROOT);bindings.validate(self.ir,ex)
        changes=[lambda r:r['sections']['Program'][0].update(stable_id=2),
                 lambda r:r['sections']['ActorInstance'][2].update(profile_index=3),
                 lambda r:r['sections']['ActorProfile'][2].update(execution_kind=3),
                 lambda r:r['sections']['Resource'][31].update(path_string=r['sections']['Resource'][32]['path_string']),
                 lambda r:r['sections']['Binding'][2].update(target_index=0),
                 lambda r:r['sections']['Binding'][0].update(target_index=33),
                 lambda r:r['sections']['Resource'][0].update(path_string=r['sections']['Resource'][1]['path_string']),
                 lambda r:r['sections']['Clip'][32].update(flags=0),
                 lambda r:r['sections']['Battle'][1].update(actor_instance_index=1),
                 lambda r:r['sections']['AnimationBinding'][40].update(direction=1),
                 lambda r:r['sections']['Key'][r['sections']['Clip'][44]['first_key']].update(frame=9)]
        for change in changes:
            room=copy.deepcopy(self.baseline);change(room)
            with self.assertRaises((ValueError,KeyError)):bindings.verify_room(self.ir,room,ex)

if __name__=='__main__':unittest.main()
