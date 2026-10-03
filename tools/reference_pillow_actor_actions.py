#!/usr/bin/env python3
"""Bounded unchanged-source Godot oracle for Pillow/Minnie actor additions."""
from __future__ import annotations
import argparse
import math
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from tools.reference_actor_actions import ACTOR, REVIEW, prepare as prepare_base
from tools.reference_movement import extract
from tools.run_scene_reference import digest, invoke
from tools.upstream import read_json, write_json

EXTRA_SOURCES = {
    'Data/Dialogue/Podunk/cutscenes/pillow_attack.yaml': '6e100c4ed8dc5cfd1fc6f88bc79ef9198d57c32721c5a9690d38e2e9c59c2f17',
    'Data/Dialogue/Podunk/cutscenes/minnie_leave.yaml': '6cc6d9c3c8f7eb2517dd945369b272b315e344e0af5afd0721a6b485894d053a',
    'Data/Animations/4dir.yaml': 'f9c45d6ce16f93cc1065b60caf4374aa0b170a73a9c4d87ab4caec88692257b4',
    'Data/Animations/Floater.yaml': '880a7d6662f5dbf53c313124f0dbdaff5f4b0806a2f5f032a1366142b748d047',
}
EXTRA_SYMBOLS = {'stop_loop': 'f63c9fdb4e405cb3ccf2d722e2a54c78612638e6e042ecf92f349ce2b01fc77f'}
SCOPE = ('Pillow original fourteen-point looping position path, stop_loop and queued retreat; '
         'Minnie indefinite shake, repeated jump including final wait, queued leave; '
         'bounded shared _looping interactions. Unchanged source functions and native '
         'automatic physics/idle/tween/timer processing, isolated game services.')


def cases():
    def a(tick, op, **kwargs): return dict(tick=tick, op=op, **kwargs)
    def m(x, y): return dict(kind=0, x=x, y=y, duration=0)
    def w(seconds): return dict(kind=1, x=0, y=0, duration=seconds)
    def path(tick, entries, speed, loop=False, queue=False, animation=''):
        return a(tick, 'path', entries=entries, speed=speed, loop=loop, queue=queue, animation=animation)
    def c(name, kind, start, ticks, actions): return dict(name=name, kind=kind, start=start, ticks=ticks, actions=actions)
    orbit = [m(x,y) for x,y in [(468,81),(480,74),(492,81),(500,95),(512,102),(524,95),(528,88),
                               (524,81),(512,74),(500,81),(492,95),(480,102),(468,95),(464,88)]]
    leave = [w(.3),m(488,156),m(464,156),m(464,200)]
    return [
        c('pillow_orbit_stop_queued_retreat', 'Pillow', [825,25], 260, [
            a(0,'teleport',x=464,y=80), path(0,orbit,200,loop=True), a(0,'anim',animation='Idle'),
            a(173,'stop'), path(173,[m(464,72),m(464,56)],160), a(173,'shake',x=1,length=.8)]),
        c('minnie_indefinite_shake_stop_turn', 'Minnie', [472,88], 175, [
            a(0,'teleport',x=560,y=80), a(0,'turn',x=0,y=-1), a(0,'shake',x=1,length=-1),
            a(120,'stop'), a(120,'turn_to',x=0,y=1,length=.15)]),
        c('minnie_repeated_jump', 'Minnie', [560,80], 50, [
            a(0,'turn',x=0,y=1),a(0,'jump',height=8,length=.2,times=2)]),
        c('repeat_final_wait_releases_queue', 'Minnie', [488,144], 150, [
            a(0,'jump',height=8,length=.2,times=2),path(0,leave,64,queue=True,animation='Walk')]),
        c('minnie_leave_queued_path', 'Minnie', [560,80], 280, [
            path(0,[m(560,144),m(488,144)],64,animation='Walk'),
            path(140,leave,64,queue=True,animation='Walk'),a(140,'turn_to',x=0,y=1,length=.1)]),
        c('idle_queue_waits_for_signal', 'Minnie', [488,144], 35, [
            path(0,leave,64,queue=True,animation='Walk'),a(10,'turn_to',x=0,y=1,length=.1)]),
        c('movement_loop_extends_finite_shake', 'Pillow', [0,0], 110, [
            path(0,[m(12,0),m(0,0)],60,loop=True),a(0,'shake',x=1,length=.2),a(64,'stop')]),
        c('shake_loop_repeats_nonloop_path_once', 'Pillow', [0,0], 65, [
            path(0,[m(12,0),m(0,0)],60),a(0,'shake',x=1,length=-1)]),
    ]


def prepare(source, out):
    review = read_json(REVIEW)
    for name, expected in EXTRA_SOURCES.items():
        if digest(source/name) != expected:
            raise ValueError('Pillow actor source changed: '+name)
    functions, hashes = extract((source/ACTOR).read_bytes(),review['sources'][ACTOR],list(EXTRA_SYMBOLS))
    if hashes != EXTRA_SYMBOLS:
        raise ValueError('Pillow actor function changed')
    prepare_base(source,out,review)
    for name in ['4dir','Floater']:
        (out/(name+'.yaml')).write_bytes((source/f'Data/Animations/{name}.yaml').read_bytes())
    wrapper = (out/'actor.gd').read_text().replace('character_sprite.set_animation(animation_name)',
        'character_sprite.set_animation(animation_name, [["Talk","Idle",2]] if animation_name == "4dir" else [])')
    (out/'actor.gd').write_text(wrapper+'\n'+functions)
    (out/'probe.gd').write_bytes((ROOT/'tools/godot_exporter/pillow_actor_actions_probe.gd').read_bytes())
    metadata = read_json(out/'metadata.json')
    metadata.update(scope=SCOPE, additional_sources=EXTRA_SOURCES, additional_symbols=EXTRA_SYMBOLS)
    write_json(out/'metadata.json',metadata)
    write_json(out/'cases.json',cases())


def fixture(doc):
    review = read_json(REVIEW)
    for key, expected in dict(schema=1,commit=review['commit'],sources=review['sources'],symbols=review['function_sha256'],
            scope=SCOPE,additional_sources=EXTRA_SOURCES,additional_symbols=EXTRA_SYMBOLS).items():
        if doc.get(key) != expected: raise ValueError('Pillow reference provenance mismatch: '+key)
    if doc.get('godot',{}).get('string') != '3.6.2-stable (official)': raise ValueError('Wrong Godot engine')
    if [c['definition'] for c in doc['cases']] != cases(): raise ValueError('Reference domain changed')
    def num(n):
        if type(n) not in (int,float) or not math.isfinite(n): raise ValueError('Invalid numeric result')
        value=format(float(n),'.9g')
        return value+('f' if '.' in value or 'e' in value else '.0f')
    def vec(v):
        if not isinstance(v,list) or len(v)!=2: raise ValueError('Invalid vector')
        return '{'+','.join(num(n) for n in v)+'}'
    code=['// Generated from unchanged source functions in official Godot3.6.2. Do not edit.',
          '#pragma once','#include "encore/actor_actions.hpp"','namespace pillow_actor_actions_reference {',
          'using namespace encore::upstream;',
          'struct Action { unsigned tick; const char* op; float x,y,speed,height; double length; unsigned times; bool loop,queue; const char* animation; const ActorPathEntry* entries; unsigned count; };',
          'struct Frame { Vec2 position,direction,blend,velocity,sprite_position,sprite_offset; unsigned frame; bool moving,rotating,looping; unsigned movements,actions; };',
          'struct Case { const char* name; const char* actor; Vec2 start; const Action* actions; unsigned action_count; const Frame* frames; unsigned frame_count; };']
    for i,row in enumerate(doc['cases']):
        if len(row['frames'])!=row['definition']['ticks']: raise ValueError('Incomplete reference')
        for j,a in enumerate(row['definition']['actions']):
            if a['op']=='path': code.append('constexpr ActorPathEntry entries_%d_%d[] = {'%(i,j)+','.join(
                '{ActorPathEntryKind::'+('Move' if e['kind']==0 else 'Wait')+','+vec([e['x'],e['y']])+','+repr(e['duration'])+'}' for e in a['entries'])+'};')
        code.append('constexpr Action actions_%d[] = {'%i)
        for j,a in enumerate(row['definition']['actions']):
            code.append('{'+str(a['tick'])+',"'+a['op']+'",'+','.join(num(a.get(k,0)) for k in ['x','y','speed','height'])+','+
                repr(a.get('length',0))+','+str(a.get('times',1))+','+','.join(str(a.get(k,False)).lower() for k in ['loop','queue'])+',"'+
                a.get('animation','')+'",'+('entries_%d_%d,%d'%(i,j,len(a['entries'])) if a['op']=='path' else 'nullptr,0')+'},')
        code.append('};\nconstexpr Frame frames_%d[] = {'%i)
        vectors=['position','direction','blend','velocity','sprite_position','sprite_offset']
        flags=['moving','rotating','looping']
        counts=['frame','movements','actions']
        for f in row['frames']:
            if set(f)!=set(vectors+flags+counts) or any(type(f[k])!=bool for k in flags) or any(type(f[k])!=int or f[k]<0 for k in counts):
                raise ValueError('Invalid reference fields')
            if f['frame'] >= {'Minnie':20,'Pillow':4}[row['definition']['kind']]: raise ValueError('Frame outside source sprite')
            code.append('{'+','.join(vec(f[k]) for k in vectors)+','+str(f['frame'])+','+','.join(str(f[k]).lower() for k in flags)+','+
                str(f['movements'])+','+str(f['actions'])+'},')
        code.append('};')
    code.append('constexpr Case cases[] = {')
    for i,row in enumerate(doc['cases']):
        c=row['definition']
        code.append('{"'+c['name']+'","'+c['kind']+'",'+vec(c['start'])+',actions_%d,sizeof(actions_%d)/sizeof(Action),frames_%d,sizeof(frames_%d)/sizeof(Frame)},'%(i,i,i,i))
    return '\n'.join(code+['};','}'])+'\n'


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--godot',type=Path,required=True)
    parser.add_argument('--work',type=Path,required=True)
    parser.add_argument('--reports',type=Path,required=True)
    args=parser.parse_args()
    work,reports=args.work.resolve(),args.reports.resolve()
    if reports.exists() or not reports.is_relative_to(ROOT/'reports'): raise ValueError('Requires fresh report directory')
    prepare(ROOT/'upstream/MOTHER-Encore',work)
    reports.mkdir(parents=True)
    invoke(args.godot.resolve(),work,['--fixed-fps','60','--script',str(work/'probe.gd'),'--encore-out='+str(reports/'reference.json')],reports/'godot.txt')
    (reports/'pillow_actor_actions_v0410.hpp').write_text(fixture(read_json(reports/'reference.json')))
    write_json(reports/'receipt.json',dict(engine_sha256=digest(args.godot),reference_sha256=digest(reports/'reference.json'),
        fixture_sha256=digest(reports/'pillow_actor_actions_v0410.hpp'),scope=SCOPE,
        tools_sha256={str(p.relative_to(ROOT)):digest(p) for p in [Path(__file__).resolve(),ROOT/'tools/godot_exporter/pillow_actor_actions_probe.gd']},
        emulator='not run',hardware='not run'))
    print('Pillow actor native reference complete:',reports)

if __name__=='__main__': main()
