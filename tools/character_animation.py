#!/usr/bin/env python3
"""Compile scoped original Sprite.frame tracks; reject other animation semantics."""
from __future__ import annotations
import argparse
import hashlib
import math
import struct
from pathlib import Path
import sys

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from tools.scene_data import validate
from tools.upstream import read_json,write_json,safe_path
REVIEW=ROOT/'compatibility/reviews/ninten-animation-v0410.json'


def integer(value):
    if not isinstance(value,dict) or value.get('type')!='int64':raise ValueError('Expected lossless integer')
    return int(value['value'])


def dictionary(value):
    if not isinstance(value,dict) or value.get('type')!='Dictionary':raise ValueError('Expected typed dictionary')
    result=dict(value['pairs'])
    if len(result)!=len(value['pairs']):raise ValueError('Duplicate animation dictionary field')
    return result


def build(document: dict,review: dict,playback: dict) -> dict:
    from tools.reference_animation import fixture
    fixture(playback) # Engine identity and both measured domains must be valid.
    validate(document)
    if review.get('schema')!=1 or review.get('game_version')!='0.4.1.0' or review.get('whole_scene_approved') is not False:
        raise ValueError('Unreviewed animation schema/version/scope')
    if review.get('grid')!=[10,20] or review.get('texture_size')!=[310,580] or not review.get('licence_review'):
        raise ValueError('Unreviewed sprite layout/permission')
    if review.get('states')!=['Idle','Walk','Run','Crouch'] or review.get('directions')!=['Down','Left','Right','Up','DownLeft','DownRight','UpLeft','UpRight']:
        raise ValueError('Unknown animation profile scope')
    if document.get('source')!='res://'+review['scene']:raise ValueError('Wrong character scene')
    nodes={node['path']:node for node in document['nodes']};resources=document['resources']
    sprite=nodes['Position/main']['properties'];player=nodes['AnimationPlayer']['properties']
    if integer(sprite['hframes'])!=10 or integer(sprite['vframes'])!=20 or sprite['centered'] is not True:
        raise ValueError('Sprite defaults differ from review')
    texture=resources[sprite['texture']['id']]
    if texture['path']!='res://'+review['texture'] or texture['size']!={'type':'Vector2','x':310,'y':580}:
        raise ValueError('Unexpected sprite texture')
    clips=[]
    for state in review['states']:
        for direction in review['directions']:
            name=state+' '+direction;resource=resources[player['anims/'+name]['id']]
            if resource['class']!='Animation':raise ValueError('Expected Animation resource')
            props=resource['properties']
            tracks={int(key.split('/')[1]) for key in props if key.startswith('tracks/')}
            # Run intentionally animates only the frame; it retains visibility
            # from the preceding state. Other normal clips set both nodes.
            expected_tracks={0} if state=='Run' else {0,1,2}
            if tracks!=expected_tracks:raise ValueError('Unknown animation tracks: '+name)
            allowed={'resource_local_to_scene','resource_name','length','loop','step','script'}
            allowed|={f'tracks/{i}/{field}' for i in tracks for field in ['type','path','interp','loop_wrap','imported','enabled','keys']}
            if set(props)!=allowed or props['script'] is not None or props['resource_local_to_scene'] is not False:
                raise ValueError('Unknown animation resource property/script')
            paths={props[f'tracks/{track}/path']['value'] for track in tracks}
            expected_paths={'Position/main:frame'} if state=='Run' else {'Position/main:frame','Position/main:visible','SpecialAnimations:visible'}
            if paths!=expected_paths:
                raise ValueError('Missing/duplicate/unknown animated property: '+name)
            for track in sorted(tracks):
                path=props[f'tracks/{track}/path']['value']
                prefix=f'tracks/{track}/'
                if props[prefix+'type']!='value' or props[prefix+'path']!={'type':'NodePath','value':path} or props[prefix+'enabled'] is not True:
                    raise ValueError('Unknown property/method track: '+name)
                if integer(props[prefix+'interp'])!=1 or props[prefix+'loop_wrap'] is not True:
                    raise ValueError('Unknown interpolation/wrap semantics')
                if props[prefix+'imported'] is not False:raise ValueError('Unreviewed imported track')
                keys=dictionary(props[prefix+'keys'])
                if set(keys)!={'times','transitions','update','values'} or integer(keys['update'])!=1:
                    raise ValueError('Unknown discrete animation key semantics')
                times=keys['times']['value'];transitions=keys['transitions']['value'];values=keys['values']['value']
                if keys['times']['type']!='PoolRealArray' or keys['transitions']['type']!='PoolRealArray' or keys['values']['type']!='Array':
                    raise ValueError('Wrong animation arrays')
                if not times or len(times)!=len(values) or len(times)!=len(transitions) or any(n!=1 for n in transitions):
                    raise ValueError('Invalid animation transition keys')
                if path=='Position/main:frame':
                    frames=[integer(value) for value in values]
                    if len(times)>8 or times[0]!=0 or any(not math.isfinite(t) or t<0 for t in times) or any(b<=a for a,b in zip(times,times[1:])):
                        raise ValueError('Invalid frame timeline')
                    if any(not 0<=frame<200 for frame in frames):raise ValueError('Frame outside source sheet')
                    timeline=list(zip(times,frames))
                elif times!=[0] or values!=[path=='Position/main:visible']:
                    raise ValueError('Unimplemented visibility animation')
            if type(props['loop'])!=bool or not math.isfinite(props['length']) or props['length']<=0:
                raise ValueError('Invalid animation length/loop')
            # Idle Right contains historical keys at 8..12.9s despite a 0.1s
            # loop. Preserve every key, and flag them for the reference check.
            outside=[pair for pair in timeline if pair[0]>=props['length']]
            if outside and name!='Idle Right':raise ValueError('New out-of-duration keys require review')
            clips.append({'name':name,'length':props['length'],'loop':props['loop'],'keys':timeline,
                          'keys_outside_duration':outside,'source_resource':resource['path'],
                          'main_visible':None if state=='Run' else True,
                          'special_visible':None if state=='Run' else False})
    precise=playback.get('timelines')
    if not isinstance(precise,list) or len(precise)!=32:raise ValueError('Missing native animation timelines')
    f32=lambda n:struct.unpack('<f',struct.pack('<f',n))[0]
    for clip,native in zip(clips,precise):
        if set(native)!={'name','length','loop','keys'} or native['name']!=clip['name'] or native['loop'] is not clip['loop']:
            raise ValueError('Native timeline scope differs from scene export')
        if not isinstance(native['length'],str) or not math.isfinite(float(native['length'])) or round(float(native['length']),6)!=clip['length']:
            raise ValueError('Native clip length differs')
        if len(native['keys'])!=len(clip['keys']):raise ValueError('Native timeline lost keys')
        for raw,key in zip(clip['keys'],native['keys']):
            if not isinstance(key,list) or len(key)!=2 or not isinstance(key[0],str) or type(key[1])!=int or key[1]!=raw[1] or not math.isfinite(float(key[0])) or round(float(key[0]),6)!=raw[0]:
                raise ValueError('Native key precision/frame differs')
        # Godot JSON.print rounds these PoolRealArray values to six decimal
        # places (e.g. source 0.0833333 -> 0.083333). Compile the separately
        # measured key times, never the shortened scene-export JSON numbers.
        clip['length']=f32(float(native['length']))
        clip['keys']=[(f32(float(time)),frame) for time,frame in native['keys']]
        clip['keys_outside_duration']=[pair for pair in clip['keys'] if pair[0]>=clip['length']]
        clip['native_length_decimal']=native['length'];clip['native_keys_decimal']=native['keys']
    return {'schema':1,'commit':review['commit'],'game_version':review['game_version'],'scope':review['scope'],
            'frame_size':[31,29],'frame_count':200,'clips':clips,'native_compatible':False}


def main() -> int:
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--reference',type=Path,required=True)
    parser.add_argument('--playback',type=Path,required=True)
    parser.add_argument('--out',type=Path,default=ROOT/'build/native/animation-profile.json')
    args=parser.parse_args()
    try:
        review=read_json(REVIEW);lock=read_json(ROOT/'upstream.lock');root=ROOT/'upstream/MOTHER-Encore'
        if lock['commit']!=review['commit'] or lock['game_version']!=review['game_version']:raise ValueError('Animation differs from source pin')
        for key in ('scene','texture'):
            if hashlib.sha256(safe_path(root,review[key]).read_bytes()).hexdigest()!=review[key+'_sha256']:
                raise ValueError('Character source changed; review required')
        profile=build(read_json(args.reference),review,read_json(args.playback))
        args.out.parent.mkdir(parents=True,exist_ok=True)
        if args.out.suffix != '.json':raise ValueError('Animation output must be external JSON; C++ content generation is disabled')
        profile['reference_sha256']=hashlib.sha256(args.reference.read_bytes()).hexdigest()
        profile['playback_sha256']=hashlib.sha256(args.playback.read_bytes()).hexdigest()
        write_json(args.out,profile)
        print('Compiled 32 original discrete frame tracks; AnimationTree/gameplay remain unapproved.')
        return 0
    except (OSError,ValueError,KeyError) as error:
        print('CHARACTER ANIMATION ERROR: '+str(error),file=sys.stderr);return 1


if __name__=='__main__':raise SystemExit(main())
