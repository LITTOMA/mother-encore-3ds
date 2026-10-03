#!/usr/bin/env python3
"""Native Godot reference for audited lamp_attack actor actions only."""
from __future__ import annotations
import argparse, hashlib, json, math, re, sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from tools.reference_movement import extract
from tools.upstream import git,read_json,write_json
from tools.run_scene_reference import invoke,digest
REVIEW=ROOT/'compatibility/reviews/actor-actions-v0410.json'
ACTOR='Scripts/Main/actor.gd'
SYMBOLS=['_physics_process','move_queue','_move_to','jump','shake','set_direction','blend_position','play_anim','_set_special_sprite','set_shadow','turn_to']

def cases():
    def case(name,kind,start,ticks,actions):return dict(name=name,kind=kind,start=start,ticks=ticks,actions=actions)
    def a(tick,op,**kw):return dict(tick=tick,op=op,**kw)
    return [
      case('lamp_first_move','Lamp',[496,390],15,[a(0,'move',x=460,y=456,speed=600)]),
      case('lamp_move_queue','Lamp',[460,456],40,[a(0,'move',x=410,y=408,speed=500),a(3,'move',x=480,y=328,speed=500),a(8,'move',x=496,y=392,speed=500)]),
      case('lamp_move_jump','Lamp',[496,392],26,[a(0,'move',x=432,y=392,speed=200),a(0,'jump',height=24,length=.35)]),
      case('snap_axis_threshold','Lamp',[0,0],8,[a(0,'move',x=10,y=10,speed=600)]),
      case('ceil_real_delta','Lamp',[0,0],4,[a(0,'move',x=10.5,y=0,speed=600)]),
      case('free_through_wall','Lamp',[0,0],20,[a(0,'move',x=100,y=0,speed=600)]),
      case('idle_timer_born_shake','Lamp',[496,390],40,[a(0,'timer_shake',x=2,length=.5)]),
      case('ninten_jump','Ninten',[432,397],20,[a(0,'jump',height=3,length=.2)]),
      case('ninten_overlapping_jump','Ninten',[432,397],30,[a(0,'jump',height=3,length=.2),a(9,'jump',height=3,length=.2)]),
      case('lamp_shake_half','Lamp',[496,390],50,[a(0,'shake',x=2,length=.5)]),
      case('lamp_shake_full','Lamp',[496,390],90,[a(0,'shake',x=2,length=1)]),
      case('lamp_open','Lamp',[496,390],130,[a(0,'anim',anim='Idle'),a(2,'anim',anim='Open')]),
      case('ninten_turn_left_right','Ninten',[432.25,397.25],25,[a(0,'turn',x=-1,y=0),a(0,'turn_to',x=1,y=0,length=.08)]),
      case('ninten_turn_up_right','Ninten',[432.25,397.25],15,[a(0,'turn',x=0,y=-1),a(0,'turn_to',x=1,y=0,length=.08)]),
      case('ninten_turn_down_right','Ninten',[432.25,397.25],15,[a(0,'turn',x=0,y=1),a(0,'turn_to',x=1,y=0,length=.08)]),
      case('ninten_turn_same','Ninten',[432.25,397.25],5,[a(0,'turn',x=1,y=0),a(0,'turn_to',x=1,y=0,length=.08)]),
      case('idle_timer_born_turn','Ninten',[432,397],25,[a(0,'turn',x=-1,y=0),a(0,'timer_turn',x=1,y=0,length=.08)]),
      case('move_waits_for_turn','Ninten',[432,397],35,[a(0,'turn',x=-1,y=0),a(0,'turn_to',x=1,y=0,length=.08),a(2,'move',x=496,y=390,speed=500)]),
      case('ninten_surprise','Ninten',[432,397],90,[a(0,'turn',x=1,y=0),a(0,'emote')]),
      case('jump_releases_waiting_move','Lamp',[0,0],40,[a(0,'move',x=200,y=200,speed=200),a(0,'jump',height=3,length=.2),a(2,'move',x=30,y=30,speed=200)]),
    ]

def prepare(source,out,review):
    if review.get('schema')!=1 or review.get('whole_file_approved') is not False or review.get('symbols')!=SYMBOLS:raise ValueError('Actor action review scope changed')
    if git(source,'rev-parse','HEAD')!=review['commit'] or git(source,'status','--porcelain'):raise ValueError('Requires pristine pinned upstream')
    for path,expected in review['sources'].items():
        if digest(source/path)!=expected:raise ValueError('Actor action source changed; semantic review required: '+path)
    if out.exists() or not out.resolve().is_relative_to(ROOT/'build'):raise ValueError('Requires fresh work subdirectory under build/')
    out.mkdir(parents=True)
    original=(source/ACTOR).read_bytes()
    functions,hashes=extract(original,review['sources'][ACTOR],SYMBOLS)
    classes=original.decode().split('class MoveAction:')[1].split('export var allow_debug_echo')[0]
    wrapper=(ROOT/'tools/godot_exporter/actor_actions_wrapper.gd').read_text()
    (out/'actor.gd').write_text(wrapper+'\nclass MoveAction:'+classes+'\n'+functions)
    (out/'character_sprite.gd').write_bytes((source/'Scripts/Main/character_sprite.gd').read_bytes())
    (out/'yaml_parser.gd').write_bytes((source/'Scripts/global/yaml_parser.gd').read_bytes())
    for name in ['Lamp','PartyMember']:
        (out/(name+'.yaml')).write_bytes((source/f'Data/Animations/{name}.yaml').read_bytes())
    (out/'globaldata.gd').write_text('extends Node\nfunc get_json_data(path):\n    return load("res://yaml_parser.gd").parse_file("res://"+path+".yaml")\n')
    (out/'global.gd').write_text('extends Node\nvar talker = null\n')
    (out/'audio.gd').write_text('extends Node\nfunc play_sfx(_a,_b): assert(false)\n')
    (out/'editor.gd').write_text('extends Node\nfunc get_json_data(_a): assert(false)\n')
    (out/'project.godot').write_text('config_version=4\n[application]\nconfig/name="Encore actor actions reference"\n[logging]\nfile_logging/enable_logging=false\n[autoload]\nglobal="*res://global.gd"\nglobaldata="*res://globaldata.gd"\naudioManager="*res://audio.gd"\nEditorTools="*res://editor.gd"\n[physics]\ncommon/physics_fps=60\n')
    emotes=(source/'Nodes/Ui/emotes.tscn').read_text()
    block=re.search(r'\[sub_resource type="Animation" id=5\]\n(.*?)(?=\n\[sub_resource)',emotes,re.S).group(1)
    (out/'surprise.tres').write_text('[gd_resource type="Animation" format=2]\n[resource]\n'+block)
    (out/'probe.gd').write_bytes((ROOT/'tools/godot_exporter/actor_actions_probe.gd').read_bytes())
    write_json(out/'cases.json',cases())
    write_json(out/'metadata.json',dict(schema=1,commit=review['commit'],sources=review['sources'],symbols=hashes,scope=review['scope']))

def fixture(doc,review):
    if doc.get('schema')!=1 or review.get('schema')!=1 or review.get('whole_file_approved') is not False:raise ValueError('Actor schema/scope mismatch')
    if doc.get('commit')!=review['commit'] or doc.get('sources')!=review['sources'] or doc.get('scope')!=review['scope']:raise ValueError('Actor provenance mismatch')
    if doc.get('symbols')!=review['function_sha256']:raise ValueError('Actor function provenance mismatch')
    if doc.get('godot',{}).get('string')!='3.6.2-stable (official)':raise ValueError('Requires native official Godot 3.6.2')
    if [c['definition'] for c in doc['cases']]!=cases():raise ValueError('Actor reference domain changed')
    def number(n):
        v=format(float(n),'.9g');return v+('f' if '.' in v or 'e' in v else '.0f')
    def vec(v):return '{'+','.join(map(number,v))+'}'
    code=['// Generated from unchanged upstream functions run by official Godot 3.6.2. Do not edit.','#pragma once','#include "encore/actor_actions.hpp"','namespace actor_actions_reference {','using namespace encore::upstream;','struct Action { unsigned tick; const char* op; float x,y,speed,height; double length; const char* animation; };','struct Frame { Vec2 position,direction,velocity,sprite_position,sprite_offset; unsigned frame,emote; bool moving,rotating; unsigned movement_signals,action_signals; };','struct Case { const char* name; ActorKind kind; Vec2 start; const Action* actions; unsigned action_count; const Frame* frames; unsigned frame_count; };']
    for i,row in enumerate(doc['cases']):
        if len(row['frames'])!=row['definition']['ticks']:raise ValueError('Incomplete actor frames')
        code.append(f'constexpr Action actions_{i}[] = {{')
        for a in row['definition']['actions']:
            code.append('{'+str(a['tick'])+',"'+a['op']+'",'+','.join(number(a.get(k,0)) for k in ['x','y','speed','height'])+','+repr(a.get('length',0))+',"'+a.get('anim','')+'"},')
        code.append('};\nconstexpr Frame frames_'+str(i)+'[] = {')
        for f in row['frames']:
            if set(f)!={'position','direction','velocity','sprite_position','sprite_offset','frame','emote','moving','rotating','movement_signals','action_signals'}:raise ValueError('Unknown actor frame field')
            for k in ['position','direction','velocity','sprite_position','sprite_offset']:
                if not isinstance(f[k],list) or len(f[k])!=2 or any(type(n) not in (int,float) or not math.isfinite(n) for n in f[k]):raise ValueError('Invalid actor numeric result')
            for k in ['frame','emote','movement_signals','action_signals']:
                if type(f[k])!=int or f[k]<0:raise ValueError('Invalid actor discrete result')
            if type(f['moving'])!=bool or type(f['rotating'])!=bool or f['frame']>=(200 if row['definition']['kind']=='Ninten' else 4) or f['emote']>=144:raise ValueError('Actor result out of domain')
            code.append('{'+','.join(vec(f[k]) for k in ['position','direction','velocity','sprite_position','sprite_offset'])+','+str(f['frame'])+','+str(f['emote'])+','+str(f['moving']).lower()+','+str(f['rotating']).lower()+','+str(f['movement_signals'])+','+str(f['action_signals'])+'},')
        code.append('};')
    code.append('constexpr Case cases[] = {')
    for i,row in enumerate(doc['cases']):
        c=row['definition'];code.append('{"'+c['name']+'",ActorKind::'+c['kind']+','+vec(c['start'])+f',actions_{i},sizeof(actions_{i})/sizeof(Action),frames_{i},sizeof(frames_{i})/sizeof(Frame)'+'},')
    return '\n'.join(code+['};','}'])+'\n'

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--godot',type=Path,required=True);p.add_argument('--work',type=Path,required=True);p.add_argument('--reports',type=Path,required=True);args=p.parse_args()
    review=read_json(REVIEW);work=args.work.resolve();reports=args.reports.resolve()
    if reports.exists() or not reports.is_relative_to(ROOT/'reports'):raise ValueError('Requires fresh reports subdirectory')
    prepare(ROOT/'upstream/MOTHER-Encore',work,review);reports.mkdir(parents=True)
    invoke(args.godot.resolve(),work,['--fixed-fps','60','--script',str(work/'probe.gd'),'--encore-out='+str(reports/'reference.json')],reports/'godot.txt')
    doc=read_json(reports/'reference.json')
    (reports/'actor_actions_v0410.hpp').write_text(fixture(doc,review))
    write_json(reports/'receipt.json',dict(engine_sha256=digest(args.godot),reference_sha256=digest(reports/'reference.json'),fixture_sha256=digest(reports/'actor_actions_v0410.hpp'),commit=review['commit'],scope=review['scope'],tools_sha256={str(p.relative_to(ROOT)):digest(p) for p in [Path(__file__).resolve(),ROOT/'tools/godot_exporter/actor_actions_wrapper.gd',ROOT/'tools/godot_exporter/actor_actions_probe.gd']},hardware='not run',emulator='not run'))
    print('Actor action native reference complete:',reports)
if __name__=='__main__':
    try:main()
    except (ValueError,OSError,KeyError,TypeError) as e:print('ACTOR ACTION REFERENCE ERROR: '+str(e),file=sys.stderr);sys.exit(1)
