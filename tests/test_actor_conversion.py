import copy
import json
import struct
import hashlib
from pathlib import Path
import unittest
from tools.upstream import read_json
from tools.reference_movement import extract,fixture as movement_fixture
from tools.reference_animation import fixture as animation_fixture
from tools.character_animation import build

ROOT=Path(__file__).resolve().parents[1]

class MovementReferenceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.review=read_json(ROOT/'compatibility/reviews/movement-v0410.json')
        cls.data=read_json(ROOT/'reports/m3-actor-reference-reviewed/movement.json')

    def test_unchanged_function_body_and_missing_ambiguous_symbols(self):
        raw=b'extends Node\nfunc _move(delta):\n\tposition += delta\n\nfunc other():\n\tpass\n'
        expected=hashlib.sha256(raw).hexdigest()
        block,hashes=extract(raw,expected,['_move'])
        self.assertEqual(block,'func _move(delta):\n\tposition += delta\n')
        self.assertEqual(hashes['_move'],hashlib.sha256(block.encode()).hexdigest())
        with self.assertRaises(ValueError):extract(raw.replace(b'+=',b'-='),expected,['_move'])
        for bad,names in [(raw,['_missing']),(raw+raw,['_move']),(b'func _move():\n',['_move'])]:
            with self.subTest(raw=bad),self.assertRaises(ValueError):
                extract(bad,hashlib.sha256(bad).hexdigest(),names)

    def test_actual_reference_generates_recorded_fixture(self):
        self.assertEqual(movement_fixture(self.data,self.review),(ROOT/'tests/fixtures/movement_v0410.hpp').read_text())

    def test_provenance_version_and_symbol_hashes_rejected(self):
        for key in ['schema','commit','game_version','sources','scope','symbols','reference_overrides','not_implemented_by_this_review']:
            data=copy.deepcopy(self.data);data[key]=None
            with self.subTest(key=key),self.assertRaises(ValueError):movement_fixture(data,self.review)
        for key,value in [('patch',3),('hash','unreviewed'),('build','custom')]:
            data=copy.deepcopy(self.data);data['godot'][key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):movement_fixture(data,self.review)
        data=copy.deepcopy(self.data);data['controls_threshold_type']=3
        with self.assertRaises(ValueError):movement_fixture(data,self.review)
        review=copy.deepcopy(self.review);review['whole_file_approved']=True
        with self.assertRaises(ValueError):movement_fixture(self.data,review)

    def test_missing_reordered_input_and_controls_domain_rejected(self):
        modifications=[lambda d:d['sequences'].pop(),lambda d:d['sequences'][0]['frames'].pop(),
            lambda d:d['sequences'][0]['frames'][0]['input'].update(x=1),
            lambda d:d['controls'].pop(),lambda d:d['controls'][1].__setitem__(2,0),
            lambda d:d['controls'][0].__setitem__(3,2)]
        for modify in modifications:
            data=copy.deepcopy(self.data);modify(data)
            with self.subTest(modify=modify),self.assertRaises(ValueError):movement_fixture(data,self.review)

    def test_unknown_animation_nonfinite_flags_and_state_domain_rejected(self):
        for key,value in [('animation','Teleport'),('x',float('nan')),('running',1),('speed',100),('dx',2),('moved',-1),('new_rule',0)]:
            data=copy.deepcopy(self.data);data['sequences'][0]['frames'][0]['state'][key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):movement_fixture(data,self.review)


class AnimationConversionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.review=read_json(ROOT/'compatibility/reviews/ninten-animation-v0410.json')
        cls.scene=read_json(ROOT/'reports/m3-actor-reference-reviewed/player-data.json')
        cls.playback=read_json(ROOT/'reports/m3-actor-reference-reviewed/animation.json')

    def profile(self,scene=None,review=None,playback=None):
        return build(scene if scene is not None else self.scene,review if review is not None else self.review,
                     playback if playback is not None else self.playback)

    def animation(self,scene,name='Walk Down'):
        player=next(n['properties'] for n in scene['nodes'] if n['path']=='AnimationPlayer')
        return scene['resources'][player['anims/'+name]['id']]['properties']

    def test_actual_tracks_native_precision_and_visibility_are_preserved(self):
        profile=self.profile()
        self.assertEqual(len(profile['clips']),32)
        self.assertEqual(profile['clips'][16]['main_visible'],None)
        self.assertTrue(profile['clips'][0]['main_visible'])
        self.assertEqual(len(profile['clips'][2]['keys']),7)
        self.assertEqual(len(profile['clips'][2]['keys_outside_duration']),6)
        self.assertGreater(profile['clips'][16]['keys'][1][0],0.083333)
        from tools.native_content import compile_ir,parse_pack
        wire=parse_pack(compile_ir(read_json(ROOT/'content/native-opening.json'))[0])['sections']
        for source,row in zip(profile['clips'],wire['Clip'][:32]):
            self.assertEqual(struct.pack('<f',source['length']),struct.pack('<f',row['length']))
            keys=wire['Key'][row['first_key']:row['first_key']+row['key_count']]
            self.assertEqual([(struct.pack('<f',t),f) for t,f in source['keys']],[(struct.pack('<f',k['time']),k['frame']) for k in keys])
            self.assertEqual(bool(row['flags']&1),source['loop'])
        self.assertEqual(animation_fixture(self.playback),(ROOT/'tests/fixtures/animation_v0410.hpp').read_text())

    def test_unknown_version_grid_scope_and_scene_rejected(self):
        for key,value in [('schema',2),('game_version','0.5'),('whole_scene_approved',True),('grid',[5,40]),('texture_size',[31,29]),('states',['Idle']),('licence_review','')]:
            review=copy.deepcopy(self.review);review[key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):self.profile(review=review)
        scene=copy.deepcopy(self.scene);scene['source']='res://Unknown.tscn'
        with self.assertRaises(ValueError):self.profile(scene=scene)

    def test_unknown_method_property_interpolation_import_and_script_rejected(self):
        for key,value in [('tracks/0/type','method'),('tracks/0/enabled',False),('tracks/0/imported',True),
                          ('tracks/0/path',{'type':'NodePath','value':'Position/main:offset'}),
                          ('tracks/0/interp',{'type':'int64','value':'2'}),('new_animation_rule',True)]:
            scene=copy.deepcopy(self.scene);self.animation(scene)[key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):self.profile(scene=scene)

    def test_missing_duplicate_extra_track_and_changed_visibility_rejected(self):
        for operation in ['missing','duplicate','extra','visibility']:
            scene=copy.deepcopy(self.scene);props=self.animation(scene)
            if operation=='missing':
                for key in list(props):
                    if key.startswith('tracks/2/'):del props[key]
            elif operation=='duplicate':props['tracks/1/path']=props['tracks/0/path']
            elif operation=='extra':props['tracks/3/type']='value'
            else:
                keys=dict(props['tracks/1/keys']['pairs']);keys['values']['value']=[False]
            with self.subTest(operation=operation),self.assertRaises(ValueError):self.profile(scene=scene)

    def test_clock_reference_engine_scope_samples_and_precision_rejected(self):
        modifications=[lambda d:d.update(schema=2),lambda d:d['godot'].update(hash='unknown'),
            lambda d:d['samples'].pop(),lambda d:d['samples'][0].update(time=0.5),
            lambda d:d['samples'][0].update(frame=200),lambda d:d['samples'][0].update(main_visible=1),
            lambda d:d['sequences'][0]['frames'].pop(),lambda d:d['timelines'].pop(),
            lambda d:d['timelines'][0].update(length='nan'),
            lambda d:d['timelines'][0]['keys'][0].__setitem__(0,'0.01')]
        for modify in modifications:
            data=copy.deepcopy(self.playback);modify(data)
            with self.subTest(modify=modify),self.assertRaises(ValueError):self.profile(playback=data)

    def test_new_key_modes_frame_range_and_out_of_duration_keys_rejected(self):
        for operation in ['mode','frame','order','outside']:
            scene=copy.deepcopy(self.scene);keys=dict(self.animation(scene)['tracks/0/keys']['pairs'])
            if operation=='mode':keys['update']['value']='0'
            elif operation=='frame':keys['values']['value'][0]['value']='200'
            elif operation=='order':keys['times']['value'][1]=0
            else:keys['times']['value'][-1]=2
            with self.subTest(operation=operation),self.assertRaises(ValueError):self.profile(scene=scene)

if __name__=='__main__':unittest.main()
