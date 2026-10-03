#!/usr/bin/env python3
"""Original Actor movement/jump/turn concurrency for post-win command variants.

This oracle isolates authored action arguments at explicit physics ticks. It
does not claim full DialogueBox timing or automatically acknowledge any text.
"""
import argparse,json,math,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.reference_doll_actor_actions import prepare
from tools.run_scene_reference import invoke,digest
from tools.upstream import write_json
from tools.doll_postwin import SOURCE

def cases(doc):
 def path(tick,actor,phrase):
  p=doc[str(phrase)]['actorsmove'][actor]
  return dict(tick=tick,op='path',entries=[dict(kind=0,x=e['x'],y=e['y'],duration=0)for e in p['movement']],speed=p['speed'],step=p['type']=='step',moonwalk=False,animation=p.get('animation',''))
 def jump(tick,actor,phrase):
  p=doc[str(phrase)]['actorsjump'][actor];return dict(tick=tick,op='jump',height=p['height'],length=p.get('length',p.get('speed')))
 def direction(tick,phrase):return dict(tick=tick,op='turn',**doc[str(phrase)]['actorsdir']['mimmie'])
 def turn(tick,phrase):
  p=doc[str(phrase)]['actorsturn']['ninten'];return dict(tick=tick,op='turn_to',x=p['x'],y=p['y'],length=p['speed'])
 result=[
  dict(name='doll_postwin_concurrent_bounces',kind='Doll',start=[128,138],ticks=100,actions=[path(0,'doll',0),jump(0,'doll',0),dict(tick=0,op='anim',anim='Idle'),path(30,'doll',1),jump(30,'doll',1)]),
  dict(name='mimmie_postwin_walk_jumps',kind='Mimmie',start=[128,160],ticks=180,actions=[path(0,'mimmie',3),direction(90,4),jump(114,'mimmie',5),jump(132,'mimmie',6),direction(150,7)]),
  dict(name='ninten_postwin_long_turns',kind='Ninten',start=[128,144],ticks=220,actions=[dict(tick=0,op='turn',x=0,y=-1),turn(0,0),turn(61,2),turn(120,3)])]
 for tick in [30,59,60,61,62]:
  result.append(dict(name='ninten_overlapping_turn_at_'+str(tick),kind='Ninten',start=[128,144],ticks=300,actions=[dict(tick=0,op='turn',x=0,y=-1),turn(0,0),turn(tick,2)]))
 result.append(dict(name='ninten_same_target_overlap',kind='Ninten',start=[128,144],ticks=150,actions=[dict(tick=0,op='turn',x=0,y=-1),turn(0,0),dict(tick=10,op='turn_to',x=-1,y=-1,length=.5)]))
 return result

def fixture(result):
 def num(v):
  if type(v)not in(int,float)or not math.isfinite(v):raise ValueError('Invalid native reference scalar')
  text=format(float(v),'.9g');return text+('f'if'.'in text or'e'in text else '.0f')
 def vec(v):
  if not isinstance(v,list)or len(v)!=2:raise ValueError('Invalid native reference vector')
  return '{'+','.join(num(x)for x in v)+'}'
 lines=['// Generated only from original native Godot3.6.2 post-win Actor oracle.','#pragma once','#include "encore/actor_actions.hpp"','namespace doll_postwin_actions_reference {','using namespace encore::upstream;','struct Action { unsigned tick; const char* op; float x,y,speed,height; double length; bool step,moonwalk; const char* animation; const ActorPathEntry* entries; unsigned count; };','struct Frame { Vec2 position,direction,blend,velocity; unsigned frame,emote; bool moving,rotating,moonwalk; unsigned movements,actions; };','struct Case { const char* name; const char* actor; Vec2 start; const Action* actions; unsigned action_count; const Frame* frames; unsigned frame_count; };']
 for i,row in enumerate(result['cases']):
  if len(row['frames'])!=row['definition']['ticks']:raise ValueError('Incomplete native post-win frames')
  for j,a in enumerate(row['definition']['actions']):
   if a['op']=='path':lines.append('constexpr ActorPathEntry entries_%d_%d[] = {'%(i,j)+','.join('{ActorPathEntryKind::Move,'+vec([e['x'],e['y']])+',0}'for e in a['entries'])+'};')
  lines.append('constexpr Action actions_%d[] = {'%i)
  for j,a in enumerate(row['definition']['actions']):
   lines.append('{'+str(a['tick'])+',"'+a['op']+'",'+','.join(num(a.get(k,0))for k in ['x','y','speed','height'])+','+repr(a.get('length',0))+','+','.join(str(a.get(k,False)).lower()for k in ['step','moonwalk'])+',"'+a.get('animation',a.get('anim',''))+'",'+('entries_%d_%d,%d'%(i,j,len(a['entries']))if a['op']=='path'else'nullptr,0')+'},')
  lines.append('};\nconstexpr Frame frames_%d[] = {'%i)
  for f in row['frames']:
   if set(f)!={'position','direction','blend','velocity','frame','emote','moving','rotating','moonwalk','movements','actions'}:raise ValueError('Unknown native frame fields')
   lines.append('{'+','.join(vec(f[k])for k in ['position','direction','blend','velocity'])+','+str(f['frame'])+','+str(f['emote'])+','+','.join(str(f[k]).lower()for k in ['moving','rotating','moonwalk'])+','+str(f['movements'])+','+str(f['actions'])+'},')
  lines.append('};')
 lines.append('constexpr Case cases[] = {')
 for i,row in enumerate(result['cases']):
  c=row['definition'];lines.append('{"'+c['name']+'","'+c['kind']+'",'+vec(c['start'])+',actions_%d,sizeof(actions_%d)/sizeof(Action),frames_%d,sizeof(frames_%d)/sizeof(Frame)},'%(i,i,i,i))
 return '\n'.join(lines+['};','}'])+'\n'

def main():
 p=argparse.ArgumentParser();p.add_argument('--godot',type=Path,required=True);p.add_argument('--work',type=Path,required=True);p.add_argument('--reports',type=Path,required=True);p.add_argument('--fixture',type=Path);a=p.parse_args()
 work=a.work.resolve();reports=a.reports.resolve();source=ROOT/'upstream/MOTHER-Encore'
 if work.exists()or reports.exists()or not work.is_relative_to(ROOT/'build')or not reports.is_relative_to(ROOT/'reports'):raise ValueError('Fresh build/report paths required')
 meta=prepare(source,work);reports.mkdir(parents=True)
 from tools.doll_dialogue import decode
 doc=decode(json.loads((ROOT/'reports/doll-postwin/native-parser-animation.json').read_text()))['yaml'][0]
 write_json(work/'cases.json',cases(doc));meta['scope']='Original post-win action arguments at explicit physics ticks; shared movement/action signals, speed-alias jumps and slow timed turns; no full dialogue schedule, rendering or audio claim'
 meta['additional_sources'][SOURCE]=digest(source/SOURCE);write_json(work/'metadata.json',meta)
 invoke(a.godot.resolve(),work,['--fixed-fps','60','--script',str(work/'probe.gd'),'--encore-out='+str(reports/'reference.json')],reports/'godot.txt')
 result=json.loads((reports/'reference.json').read_text());summaries=[]
 for row in result['cases']:
  changes=[];before=None
  for i,frame in enumerate(row['frames']):
   value=(frame['movements'],frame['actions'])
   if value!=before:changes.append(dict(tick=i,movements=value[0],actions=value[1],position=frame['position'],moving=frame['moving'],rotating=frame['rotating']));before=value
  summaries.append(dict(name=row['definition']['name'],last_frame=row['frames'][-1],signal_changes=changes))
 write_json(reports/'summary.json',summaries)
 generated=fixture(result)
 if a.fixture:
  if not a.fixture.resolve().is_relative_to(ROOT/'tests/fixtures'):raise ValueError('Oracle C++ data is test-only')
  a.fixture.parent.mkdir(parents=True,exist_ok=True);a.fixture.write_text(generated)
 write_json(reports/'receipt.json',dict(engine_sha256=digest(a.godot),reference_sha256=digest(reports/'reference.json'),tool_sha256=digest(Path(__file__)),probe_sha256=digest(ROOT/'tools/godot_exporter/doll_actor_actions_probe.gd'),fixture_sha256=__import__('hashlib').sha256(generated.encode()).hexdigest(),scope=meta['scope'],emulator='not run',hardware='not run'))
 print('Recorded',sum(len(c['frames'])for c in result['cases']),'original post-win action physics samples:',reports)
if __name__=='__main__':main()
