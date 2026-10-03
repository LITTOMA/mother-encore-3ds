#!/usr/bin/env python3
"""Exact-source walking kernel reference; explicit excluded services and fixtures."""
from __future__ import annotations
import argparse
import hashlib
import json
import math
from pathlib import Path
import re
import sys

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from tools.upstream import git, read_json, safe_path, write_json
REVIEW=ROOT/'compatibility/reviews/movement-v0410.json'


def extract(raw: bytes, expected: str, names: list[str]) -> tuple[str, dict]:
    if hashlib.sha256(raw).hexdigest()!=expected:
        raise ValueError('Movement source changed; semantic review required')
    lines=raw.decode('utf-8').splitlines(keepends=True)
    blocks=[];hashes={}
    for name in names:
        starts=[i for i,line in enumerate(lines) if re.match(r'^func '+re.escape(name)+r'\(',line)]
        if len(starts)!=1:raise ValueError('Missing/ambiguous movement symbol: '+name)
        start=starts[0];end=start+1
        while end<len(lines) and (lines[end].startswith('\t') or not lines[end].strip()):end+=1
        block=''.join(lines[start:end]).rstrip('\r\n')+'\n'
        if end==start+1:raise ValueError('Missing movement function body')
        blocks.append(block);hashes[name]=hashlib.sha256(block.encode()).hexdigest()
    return '\n'.join(blocks),hashes


def cases() -> list[dict]:
    def step(x=0,y=0,toggle=False,paused=False,entering_door=False):
        return {'x':x,'y':y,'toggle':toggle,'paused':paused,'entering_door':entering_door}
    result=[]
    for x,y in [(0,1),(-1,0),(1,0),(0,-1),(-1,1),(1,1),(-1,-1),(1,-1)]:
        result.append({'name':f'walk_{x}_{y}','position':[520,404], 'steps':[step(x,y)]*12+[step()]*3})
    result.append({'name':'run_transitions','position':[-100,-100],
                   'steps':[step(1,0)]*4+[step(1,0,True)]*12+[step(1,0)]*4+[step()]*3})
    result.append({'name':'tap_run_crouch','position':[520,404],
                   'steps':[step(toggle=True)]*2+[step()]*9+[step(toggle=True)]+[step()]*2})
    result.append({'name':'tap_cancel_on_direction','position':[520,404],
                   'steps':[step(toggle=True)]+[step()]*4+[step(1,-1,True)]+[step(1,-1)]*6+[step()]*2})
    result.append({'name':'crouch_second_press','position':[520,404],
                   'steps':[step(toggle=True),step(paused=True),step(toggle=True)]+[step()]*4})
    result.append({'name':'paused_and_door_input_edges','position':[520,404],
                   'steps':[step(1,0)]*3+[step(1,0,True,paused=True)]*3+[step(1,0,True)]*3+
                   [step(-1,1,False,entering_door=True)]*3+[step(-1,1)]*3+[step()]*2})
    result.append({'name':'alternate_directions','position':[-12,13],
                   'steps':[step(x,y,toggle) for x,y,toggle in [(1,0,False),(0,1,False),(-1,0,True),(0,-1,True),
                                                             (1,1,False),(0,0,False),(-1,-1,True),(0,0,False)]]*4})
    return result


def prepare(source: Path,out: Path,review: dict) -> None:
    if review.get('schema')!=1 or review.get('game_version')!='0.4.1.0' or review.get('whole_file_approved') is not False:
        raise ValueError('Unreviewed movement scope/version')
    expected={'Scripts/Main/party/Player.gd':['_move','_movement'], 'Scripts/global/controlsManager.gd':['_get_vector_sign']}
    if review.get('symbols')!=expected:raise ValueError('Unknown movement symbol scope')
    if git(source,'rev-parse','HEAD')!=review['commit'] or git(source,'status','--porcelain'):
        raise ValueError('Movement reference requires pristine pinned upstream')
    if not out.resolve().is_relative_to((ROOT/'build').resolve()) or out.exists():
        raise ValueError('Use fresh movement reference directory in build/')
    extracted={};hashes={}
    for path,names in expected.items():
        extracted[path],hashes[path]=extract(safe_path(source,path).read_bytes(),review['sources'][path],names)
    player_source=safe_path(source,'Scripts/Main/party/Player.gd').read_text(encoding='utf-8')
    for name,value in [('SPEED_WALKING',64),('SPEED_RUNNING',96)]:
        if len(re.findall(r'^const '+name+r' := '+str(value)+r'\s*$',player_source,re.M))!=1:
            raise ValueError('Audited speed constant changed')
    out.mkdir(parents=True)
    (out/'project.godot').write_text('''config_version=4
[application]
config/name="Encore isolated movement reference"
[logging]
file_logging/enable_logging=false
[autoload]
global="*res://global.gd"
audioManager="*res://audio.gd"
[input]
ui_toggle={"deadzone":0.5,"events":[]}
ui_accept={"deadzone":0.5,"events":[]}
[physics]
common/physics_fps=60
''',encoding='utf-8')
    (out/'global.gd').write_text('extends Node\nvar partyObjects = []\nvar party = [null]\nvar entering_door = false\n',encoding='utf-8')
    (out/'audio.gd').write_text('extends Node\nfunc get_sfx(_name): return null\n',encoding='utf-8')
    (out/'controls.gd').write_text('extends Reference\n'+extracted['Scripts/global/controlsManager.gd'],encoding='utf-8')
    wrapper=(ROOT/'tools/godot_exporter/movement_wrapper.gd').read_text(encoding='utf-8')
    (out/'movement.gd').write_text(wrapper+'\n'+extracted['Scripts/Main/party/Player.gd'],encoding='utf-8')
    probe=(ROOT/'tools/godot_exporter/movement_probe.gd').read_text(encoding='utf-8')
    (out/'probe.gd').write_text(probe,encoding='utf-8')
    write_json(out/'cases.json',cases())
    write_json(out/'metadata.json',{'schema':1,'commit':review['commit'],'game_version':review['game_version'],
        'sources':review['sources'],'symbols':hashes,'scope':review['scope'],'reference_overrides':review['reference_overrides'],
        'not_implemented_by_this_review':review['not_implemented_by_this_review']})


def fixture(document: dict, review: dict) -> str:
    if review.get('schema')!=1 or review.get('game_version')!='0.4.1.0' or review.get('whole_file_approved') is not False:
        raise ValueError('Unreviewed movement scope/version')
    for key in ('schema','commit','game_version','sources','scope','reference_overrides','not_implemented_by_this_review'):
        if document.get(key)!=review.get(key):raise ValueError('Movement provenance mismatch: '+key)
    if document.get('symbols')!=review.get('function_sha256') or not review.get('function_sha256'):
        raise ValueError('Movement function hash mismatch')
    engine=document.get('godot',{})
    if [engine.get(k) for k in ('major','minor','patch','status','build','hash')]!=[3,6,2,'stable','official','3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8']:
        raise ValueError('Movement requires Godot 3.6.2 stable reference')
    if document.get('controls_threshold_type')!=2:
        raise ValueError('Godot inferred controls threshold type changed; review required')
    sequences=document.get('sequences',[])
    if len(sequences)!=len(cases()) or document.get('controls') is None:raise ValueError('Incomplete movement domain')
    code='// GENERATED from measured Godot 3.6.2 unchanged movement functions. Do not edit.\n'
    code+='// Scoped reference excludes collisions and non-motion services; see movement-v0410.json.\n'
    code+='#pragma once\n#include "encore/movement.hpp"\nnamespace movement_reference {\nusing namespace encore::upstream;\n'
    code+='struct Frame {WalkInput input; WalkState expected;};\n'
    for i,(sequence,case) in enumerate(zip(sequences,cases())):
        if sequence.get('name')!=case['name'] or sequence.get('initial')!=case['position'] or len(sequence.get('frames',[]))!=len(case['steps']):
            raise ValueError('Movement sequence missing/reordered')
        code+=f'constexpr Frame sequence_{i}[] = {{\n'
        for frame,inp in zip(sequence['frames'],case['steps']):
            if frame.get('input')!=inp:raise ValueError('Reference input differs from audited domain')
            state=frame['state'];required={'x','y','dx','dy','vx','vy','speed','crouch','tap_run','running','substantial','walking','toggle','animation','moved'}
            if set(state)!=required or state['animation'] not in ('Idle','Walk','Run','Crouch'):raise ValueError('Unknown movement result/state')
            nums=[state[k] for k in ('x','y','dx','dy','vx','vy','speed')]
            if any(type(n) not in (int,float) or not math.isfinite(n) for n in nums):raise ValueError('Invalid movement numeric result')
            if state['speed'] not in (64,96) or state['dx'] not in (-1,0,1) or state['dy'] not in (-1,0,1) or (state['dx']==0 and state['dy']==0):
                raise ValueError('Movement result outside audited state domain')
            boolean=lambda key: 'true' if state[key] else 'false'
            for key in ('crouch','tap_run','running','substantial','walking','toggle'):
                if type(state[key])!=bool:raise ValueError('Invalid movement flag')
            if type(state['moved'])!=int or not 0<=state['moved']<2**32:raise ValueError('Invalid signal count')
            floating=lambda n: format(float(n),'.9g')+('f' if '.' in format(float(n),'.9g') or 'e' in format(float(n),'.9g') else '.0f')
            fields=['{'+floating(state['x'])+','+floating(state['y'])+'}',
                    '{'+floating(state['dx'])+','+floating(state['dy'])+'}',
                    '{'+floating(state['vx'])+','+floating(state['vy'])+'}',floating(state['speed'])]
            fields += [boolean(k) for k in ('crouch','tap_run','running','substantial','walking','toggle')]
            fields += ['MotionAnimation::'+state['animation'],str(state['moved'])]
            inputs=[str(inp['x']),str(inp['y'])]+['true' if inp[k] else 'false' for k in ('toggle','paused','entering_door')]
            code+='    {{'+','.join(inputs)+'},{'+','.join(fields)+'}},\n'
        code+='};\n'
    code+='struct Sequence {float x,y;const Frame* frames;unsigned count;};\nconstexpr Sequence sequences[] = {\n'
    for i,case in enumerate(cases()):
        code+='    {'+str(case['position'][0])+','+str(case['position'][1])+f',sequence_{i},sizeof(sequence_{i})/sizeof(Frame)'+'},\n'
    code+='};\nstruct Control {Vec2 input;int32_t threshold;Vec2 expected;};\nconstexpr Control controls[] = {\n'
    expected_controls=11*11*5
    if len(document['controls'])!=expected_controls:raise ValueError('Incomplete controls domain')
    values=[-4,-1,-0.71,-0.5,-0.49,0,0.49,0.5,0.71,1,4]
    domain=[(x,y,t) for x in values for y in values for t in [0,0.25,0.5,0.75,1]]
    for row,expected in zip(document['controls'],domain):
        if not isinstance(row,list) or len(row)!=5 or any(type(v) not in (int,float) or not math.isfinite(v) for v in row):
            raise ValueError('Invalid controls result')
        if tuple(row[:3])!=expected or row[3] not in (-1,0,1) or row[4] not in (-1,0,1):
            raise ValueError('Controls input/output domain changed/reordered')
        number=lambda n:format(float(n),'.9g')+('f' if '.' in format(float(n),'.9g') or 'e' in format(float(n),'.9g') else '.0f')
        # Match the reflected integer argument, not a new floating deadzone.
        code+='    {{'+number(row[0])+','+number(row[1])+'},'+str(int(row[2]))+',{'+number(row[3])+','+number(row[4])+'}},\n'
    return code+'};\n}\n'


def main() -> int:
    parser=argparse.ArgumentParser(description=__doc__);sub=parser.add_subparsers(dest='action',required=True)
    p=sub.add_parser('prepare');p.add_argument('--out',type=Path,required=True)
    p=sub.add_parser('fixture');p.add_argument('--reference',type=Path,required=True);p.add_argument('--out',type=Path,default=ROOT/'tests/fixtures/movement_v0410.hpp')
    args=parser.parse_args()
    try:
        review=read_json(REVIEW)
        if args.action=='prepare':prepare(ROOT/'upstream/MOTHER-Encore',args.out,review)
        else:args.out.write_text(fixture(read_json(args.reference),review),encoding='utf-8')
        return 0
    except (OSError,ValueError,KeyError) as error:
        print('MOVEMENT REFERENCE ERROR: '+str(error),file=sys.stderr);return 1


if __name__=='__main__':raise SystemExit(main())
