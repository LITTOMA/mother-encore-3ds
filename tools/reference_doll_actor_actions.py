#!/usr/bin/env python3
"""Native unchanged Actor function oracle for the bounded doll_attack additions."""
from __future__ import annotations
import argparse, json, math, re, sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from tools.reference_actor_actions import prepare as prepare_base,REVIEW,SYMBOLS
from tools.run_scene_reference import invoke,digest
from tools.upstream import read_json,write_json

EXTRA_SOURCES={
 'Data/Dialogue/Podunk/cutscenes/doll_attack.yaml':'b04e1ba848ee5c83f60781f723fb04f051c9be8242d8e68789104bd11f87a44b',
 'Data/Animations/4dir.yaml':'f9c45d6ce16f93cc1065b60caf4374aa0b170a73a9c4d87ab4caec88692257b4',
 'Data/Animations/Floater.yaml':'880a7d6662f5dbf53c313124f0dbdaff5f4b0806a2f5f032a1366142b748d047',
}
SCOPE='doll_attack bounded vector/wait position and STEP queues, concurrent teleport, moonwalk, source Walk/Talk/Float and exclamation; unchanged source action methods, isolated game services'

def cases():
 def c(name,kind,start,ticks,actions):return dict(name=name,kind=kind,start=start,ticks=ticks,actions=actions)
 def a(tick,op,**kw):return dict(tick=tick,op=op,**kw)
 def path(tick,entries,speed,step=False,moonwalk=False,animation=''):return a(tick,'path',entries=entries,speed=speed,step=step,moonwalk=moonwalk,animation=animation)
 def m(x,y):return dict(kind=0,x=x,y=y,duration=0)
 def w(t):return dict(kind=1,x=0,y=0,duration=t)
 return [
  c('doll_initial_float','Doll',[128,74],220,[path(0,[m(0,16),w(1),m(0,16),w(1)],32,True),a(0,'anim',anim='Float')]),
  c('mimmie_moonwalk','Mimmie',[128,90],230,[a(0,'turn',x=0,y=-1),path(0,[m(0,20),w(1),m(0,20),w(1)],32,True,True,'Walk')]),
  c('mimmie_exit','Mimmie',[128,130],65,[a(0,'turn',x=0,y=-1),path(0,[m(136,144),m(136,160),m(128,160)],128,False,False,'Walk')]),
  c('ninten_walk','Ninten',[128,169],45,[a(0,'turn',x=0,y=-1),path(0,[m(128,144)],64,False,False,'Walk')]),
  c('doll_retreat_shake','Doll',[128,106],80,[a(0,'anim',anim='Float'),path(0,[m(0,-16)],64,True),a(0,'shake',x=2,length=.8)]),
  c('doll_battle_charge','Doll',[128,90],24,[a(0,'anim',anim='Float'),path(0,[m(0,48)],200,True)]),
  c('teleport_keeps_step_target','Doll',[128,74],100,[path(0,[m(0,16),w(.1),m(16,0)],32,True),a(8,'teleport',x=160,y=90)]),
  c('zero_wait_new_timer_phase','Doll',[10.25,12.25],12,[path(0,[w(0),w(0),m(3,0),w(0)],60,True)]),
  c('wait_allows_new_motion','Mimmie',[0,0],55,[path(0,[w(.2)],64),path(2,[m(100,0)],64,False,False,'Walk')]),
  c('wait_allows_turn','Mimmie',[0,0],45,[a(0,'turn',x=-1,y=0),path(0,[w(.2)],64),a(2,'turn_to',x=1,y=0,length=.08)]),
  c('mimmie_talk_exclamation','Mimmie',[128,130],110,[a(0,'turn',x=0,y=1),a(0,'talk',enabled=True),a(0,'emote'),a(35,'talk',enabled=False),a(65,'turn',x=1,y=0),a(65,'talk',enabled=True),a(86,'talk',enabled=False)]),
  c('mimmie_turn_full','Mimmie',[128,130],35,[a(0,'turn',x=0,y=-1),a(0,'turn_to',x=0,y=1,length=.05)]),
  c('step_diagonal_arithmetic','Doll',[1.25,-3.5],30,[path(0,[m(10,16),m(-9,3)],64,True)]),
 ]

def prepare(source,out):
 review=read_json(REVIEW);prepare_base(source,out,review)
 extra=list(EXTRA_SOURCES)
 for name,expected in EXTRA_SOURCES.items():
  if digest(source/name)!=expected:raise ValueError('Doll actor source changed: '+name)
 for name in ['4dir','Floater']:(out/(name+'.yaml')).write_bytes((source/f'Data/Animations/{name}.yaml').read_bytes())
 wrapper=(out/'actor.gd').read_text().replace('character_sprite.set_animation(animation_name)','character_sprite.set_animation(animation_name, [["Talk","Idle",2]] if animation_name == "4dir" else [])')
 (out/'actor.gd').write_text(wrapper)
 probe=(ROOT/'tools/godot_exporter/doll_actor_actions_probe.gd').read_bytes();(out/'probe.gd').write_bytes(probe)
 emotes=(source/'Nodes/Ui/emotes.tscn').read_text();block=re.search(r'\[sub_resource type="Animation" id=8\]\n(.*?)(?=\n\[sub_resource)',emotes,re.S).group(1)
 (out/'exclamation.tres').write_text('[gd_resource type="Animation" format=2]\n[resource]\n'+block)
 metadata=read_json(out/'metadata.json');metadata['scope']=SCOPE;metadata['additional_sources']={p:digest(source/p) for p in extra};write_json(out/'metadata.json',metadata);write_json(out/'cases.json',cases());return metadata

def fixture(doc):
 review=read_json(REVIEW)
 if doc.get('schema')!=1 or doc.get('commit')!=review['commit'] or doc.get('sources')!=review['sources'] or doc.get('symbols')!=review['function_sha256'] or doc.get('scope')!=SCOPE or doc.get('additional_sources')!=EXTRA_SOURCES:raise ValueError('Doll actor provenance mismatch')
 if doc.get('godot',{}).get('string')!='3.6.2-stable (official)':raise ValueError('Wrong native Godot version')
 if [c['definition'] for c in doc['cases']]!=cases():raise ValueError('Doll actor reference domain changed')
 def num(n):
  if type(n) not in (int,float) or not math.isfinite(n):raise ValueError('Invalid numeric result')
  return format(float(n),'.9g')+('f' if '.' in format(float(n),'.9g') or 'e' in format(float(n),'.9g') else '.0f')
 def vec(v):
  if not isinstance(v,list) or len(v)!=2:raise ValueError('Invalid vector')
  return '{'+','.join(num(n) for n in v)+'}'
 lines=['// Native official Godot3.6.2 unchanged source Actor oracle. Generated, do not edit.','#pragma once','#include "encore/actor_actions.hpp"','namespace doll_actor_actions_reference {','using namespace encore::upstream;','struct Action { unsigned tick; const char* op; float x,y,speed; double length; bool step,moonwalk,enabled; const char* animation; const ActorPathEntry* entries; unsigned count; };','struct Frame { Vec2 position,direction,blend,velocity; unsigned frame,emote; bool moving,rotating,moonwalk; unsigned movements,actions; };','struct Case { const char* name; const char* actor; Vec2 start; const Action* actions; unsigned action_count; const Frame* frames; unsigned frame_count; };']
 for i,row in enumerate(doc['cases']):
  if len(row['frames'])!=row['definition']['ticks']:raise ValueError('Incomplete reference frames')
  for j,a in enumerate(row['definition']['actions']):
   if a['op']=='path':lines.append('constexpr ActorPathEntry entries_%d_%d[] = {'%(i,j)+','.join('{ActorPathEntryKind::'+('Move' if e['kind']==0 else 'Wait')+','+vec([e['x'],e['y']])+','+repr(e['duration'])+'}' for e in a['entries'])+'};')
  lines.append('constexpr Action actions_%d[] = {'%i)
  for j,a in enumerate(row['definition']['actions']):lines.append('{'+str(a['tick'])+',"'+a['op']+'",'+','.join(num(a.get(k,0)) for k in ['x','y','speed'])+','+repr(a.get('length',0))+','+','.join(str(a.get(k,False)).lower() for k in ['step','moonwalk','enabled'])+',"'+a.get('animation',a.get('anim',''))+'",'+('entries_%d_%d,%d'%(i,j,len(a['entries'])) if a['op']=='path' else 'nullptr,0')+'},')
  lines.append('};\nconstexpr Frame frames_%d[] = {'%i)
  expected={'position','direction','blend','velocity','frame','emote','moving','rotating','moonwalk','movements','actions'}
  for f in row['frames']:
   if set(f)!=expected or any(type(f[k])!=bool for k in ['moving','rotating','moonwalk']) or any(type(f[k])!=int or f[k]<0 for k in ['frame','emote','movements','actions']):raise ValueError('Invalid frame fields')
   if f['frame']>={'Ninten':200,'Mimmie':20,'Doll':4}[row['definition']['kind']] or f['emote']>=144:raise ValueError('Frame outside source sprite')
   lines.append('{'+','.join(vec(f[k]) for k in ['position','direction','blend','velocity'])+','+str(f['frame'])+','+str(f['emote'])+','+','.join(str(f[k]).lower() for k in ['moving','rotating','moonwalk'])+','+str(f['movements'])+','+str(f['actions'])+'},')
  lines.append('};')
 lines.append('constexpr Case cases[] = {')
 for i,row in enumerate(doc['cases']):
  c=row['definition'];lines.append('{"'+c['name']+'","'+c['kind']+'",'+vec(c['start'])+',actions_%d,sizeof(actions_%d)/sizeof(Action),frames_%d,sizeof(frames_%d)/sizeof(Frame)},'%(i,i,i,i))
 return '\n'.join(lines+['};','}'])+'\n'

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--godot',type=Path,required=True);p.add_argument('--work',type=Path,required=True);p.add_argument('--reports',type=Path,required=True);a=p.parse_args();work=a.work.resolve();reports=a.reports.resolve()
 if reports.exists() or not reports.is_relative_to(ROOT/'reports'):raise ValueError('Requires fresh report directory')
 prepare(ROOT/'upstream/MOTHER-Encore',work);reports.mkdir(parents=True)
 invoke(a.godot.resolve(),work,['--fixed-fps','60','--script',str(work/'probe.gd'),'--encore-out='+str(reports/'reference.json')],reports/'godot.txt')
 (reports/'doll_actor_actions_v0410.hpp').write_text(fixture(read_json(reports/'reference.json')))
 timer=ROOT/'tools/godot_exporter/doll_actor_timer_probe.gd'
 (work/'timer_probe.gd').write_bytes(timer.read_bytes())
 invoke(a.godot.resolve(),work,['--fixed-fps','60','--script',str(work/'timer_probe.gd'),'--encore-out='+str(reports/'timer.json')],reports/'timer-godot.txt')
 write_json(reports/'receipt.json',dict(engine_sha256=digest(a.godot),reference_sha256=digest(reports/'reference.json'),timer_sha256=digest(reports/'timer.json'),fixture_sha256=digest(reports/'doll_actor_actions_v0410.hpp'),scope=SCOPE,tools_sha256={str(p.relative_to(ROOT)):digest(p) for p in [Path(__file__).resolve(),ROOT/'tools/godot_exporter/doll_actor_actions_probe.gd',timer]},emulator='not run',hardware='not run'))
 print('Doll actor native reference complete:',reports)
if __name__=='__main__':main()
