#!/usr/bin/env python3
"""Native Godot 3.6.2 reference for the lamp bedroom camera subset.

Runs unchanged Camera2D.gd and Shaker.gd in a 400x240 Viewport. Actor/UI
services are minimal explicit stubs; this is not a full-game renderer test.
"""
from __future__ import annotations
import argparse, hashlib, json, math, sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from tools.upstream import git, read_json, write_json
from tools.run_scene_reference import invoke, digest
SOURCES=['Scripts/Main/Camera2D.gd','Scripts/misc/Shaker.gd','Scripts/Main/camarea.gd','Nodes/Ui/Camera.tscn','Nodes/Overworld/camarea.tscn','Maps/podunk/Nintens House.tscn','Scripts/Main/actor.gd','Scripts/UI/DialogueBox.gd','Data/Dialogue/Podunk/cutscenes/lamp_attack.yaml']
REVIEW=ROOT/'compatibility/reviews/cutscene-camera-v0410.json'

def cases():
    def a(t,op,**kw): return dict(tick=t,op=op,**kw)
    def c(name,ticks,actions,player=(432,397)):return dict(name=name,ticks=ticks,player=list(player),lamp=[496,390],actions=actions)
    return [
      c('lamp_move_from_screen_center',66,[a(0,'change'),a(1,'move',x=474,y=392,length=1)]),
      c('lamp_parent_local_follow',20,[a(0,'change'),a(1,'move',x=474,y=392,length=0),a(3,'lamp',x=460,y=456),a(8,'lamp',x=410,y=408),a(12,'lamp',x=480,y=328)]),
      c('global_tween_while_parent_moves',40,[a(0,'change'),a(1,'move',x=474,y=392,length=.5),a(4,'lamp',x=460,y=456),a(10,'lamp',x=410,y=408),a(20,'lamp',x=480,y=328)]),
      c('small_shake_physics_visibility',30,[a(0,'change'),a(1,'shake',length=.2)]),
      c('overlapping_small_shakes',45,[a(0,'change'),a(1,'shake',length=.2),a(10,'shake',length=.2)]),
      c('restore_during_shake',45,[a(0,'change'),a(1,'move',x=506,y=392,length=0),a(2,'shake',length=.2),a(5,'restore')]),
      c('restored_player_local_return',45,[a(0,'change'),a(1,'move',x=506,y=392,length=0),a(3,'restore')]),
      c('same_idle_restore_and_battle_pause',20,[a(0,'change'),a(1,'move',x=506,y=392,length=0),a(3,'restore'),a(3,'pause')]),
      c('pause_move_but_shake_continues',35,[a(0,'change'),a(1,'move',x=510,y=392,length=1),a(2,'shake',length=.2),a(3,'pause')]),
      c('pause_current_return_tween',20,[a(0,'change'),a(1,'move',x=506,y=392,length=0),a(3,'restore'),a(4,'pause')]),
      c('pause_then_new_move',50,[a(0,'change'),a(1,'move',x=510,y=400,length=.5),a(6,'pause'),a(10,'move',x=474,y=392,length=.5)]),
      c('limits_high_player_start',6,[a(0,'change')],player=(700,440)),
    ]

def prepare(source,out,review):
    if review.get('schema')!=1 or review.get('whole_file_approved') is not False:raise ValueError('Camera review scope mismatch')
    if git(source,'rev-parse','HEAD')!=review['commit'] or git(source,'status','--porcelain'):raise ValueError('Requires pristine pinned upstream')
    if set(review['sources'])!=set(SOURCES):raise ValueError('Camera source domain mismatch')
    for path in SOURCES:
        if digest(source/path)!=review['sources'][path]:raise ValueError('Camera source changed: '+path)
    if out.exists() or not out.resolve().is_relative_to(ROOT/'build'):raise ValueError('Requires fresh work directory under build')
    out.mkdir(parents=True)
    for src,name in [('Scripts/Main/Camera2D.gd','camera.gd'),('Scripts/misc/Shaker.gd','shaker.gd'),('Scripts/Main/camarea.gd','camarea.gd')]: (out/name).write_bytes((source/src).read_bytes())
    (out/'global.gd').write_text('extends Node\nvar currentCamera\nvar player\nfunc get_player(): return player\n')
    (out/'ui.gd').write_text('extends Node\nsignal battle_to_ov\nfunc is_in_battle(): return false\n')
    (out/'controls.gd').write_text('extends Node\nfunc get_controls_vector(): return Vector2.ZERO\n')
    (out/'player.gd').write_text('extends Node2D\nsignal paused\nconst CAMERA=99\nconst ATTACK=98\nfunc get_state(): return 0\nfunc is_being_damaged(): return false\n')
    (out/'arrows.gd').write_text('extends Node2D\nfunc handle_input_events(): pass\n')
    (out/'project.godot').write_text('''config_version=4
_global_script_classes=[{"base":"Node","class":"Shaker","language":"GDScript","path":"res://shaker.gd"}]
_global_script_class_icons={"Shaker":""}
[application]
config/name="Encore isolated cutscene camera reference"
[logging]
file_logging/enable_logging=false
[autoload]
global="*res://global.gd"
uiManager="*res://ui.gd"
controlsManager="*res://controls.gd"
[display]
window/size/width=400
window/size/height=240
[physics]
common/physics_fps=60
''')
    (out/'probe.gd').write_bytes((ROOT/'tools/godot_exporter/cutscene_camera_probe.gd').read_bytes())
    write_json(out/'cases.json',cases());write_json(out/'metadata.json',review)

def fixture(doc,review):
    if doc.get('sources')!=review['sources'] or doc.get('commit')!=review['commit'] or doc.get('scope')!=review['scope']:raise ValueError('Camera provenance mismatch')
    if doc.get('godot',{}).get('string')!='3.6.2-stable (official)':raise ValueError('Native official Godot 3.6.2 required')
    if doc.get('viewport')!=[400,240] or [r['definition'] for r in doc.get('cases',[])]!=cases():raise ValueError('Camera reference domain mismatch')
    def n(x):
        if type(x) not in (int,float) or not math.isfinite(x):raise ValueError('Invalid camera numeric result')
        v=format(float(x),'.9g');return v+('f' if '.' in v or 'e' in v else '.0f')
    def v(x):
        if not isinstance(x,list) or len(x)!=2:raise ValueError('Invalid camera vector')
        return '{'+','.join(map(n,x))+'}'
    code=['// Generated from unchanged Camera2D.gd/Shaker.gd in official Godot 3.6.2. Do not edit.','#pragma once','#include "encore/movement.hpp"','namespace cutscene_camera_reference {','using encore::upstream::Vec2;','struct Action { unsigned tick; const char* op; Vec2 target; double length; };','struct Frame { Vec2 global,center,offset,shake,physics_global,physics_center; bool lamp; };','struct Case { const char* name; Vec2 player,lamp; const Action* actions; unsigned action_count; const Frame* frames; unsigned frame_count; };']
    for i,row in enumerate(doc['cases']):
        if len(row['frames'])!=row['definition']['ticks']:raise ValueError('Incomplete camera trace')
        code.append(f'constexpr Action actions_{i}[] = {{')
        for a in row['definition']['actions']:code.append('{'+str(a['tick'])+',"'+a['op']+'",'+v([a.get('x',0),a.get('y',0)])+','+repr(a.get('length',0))+'},')
        code.append('};\nconstexpr Frame frames_'+str(i)+'[] = {')
        for f in row['frames']:
            if f['limits']!=[296,256,712,496] or type(f['lamp'])!=bool:raise ValueError('Native bedroom inherited limits changed')
            code.append('{'+','.join(v(f[k]) for k in ['global','center','offset','shake','physics_global','physics_center'])+','+str(f['lamp']).lower()+'},')
        code.append('};')
    code.append('constexpr Case cases[] = {')
    for i,row in enumerate(doc['cases']):
        c=row['definition'];code.append('{"'+c['name']+'",'+v(c['player'])+','+v(c['lamp'])+f',actions_{i},sizeof(actions_{i})/sizeof(Action),frames_{i},sizeof(frames_{i})/sizeof(Frame)'+'},')
    return '\n'.join(code+['};','}'])+'\n'

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--godot',type=Path,required=True);p.add_argument('--work',type=Path,required=True);p.add_argument('--reports',type=Path,required=True);args=p.parse_args()
    work=args.work.resolve();reports=args.reports.resolve();review=read_json(REVIEW)
    if reports.exists() or not reports.is_relative_to(ROOT/'reports'):raise ValueError('Requires fresh reports directory')
    prepare(ROOT/'upstream/MOTHER-Encore',work,review);reports.mkdir(parents=True)
    invoke(args.godot.resolve(),work,['--fixed-fps','60','--script',str(work/'probe.gd'),'--encore-out='+str(reports/'reference.json')],reports/'godot.txt')
    doc=read_json(reports/'reference.json');(reports/'cutscene_camera_v0410.hpp').write_text(fixture(doc,review))
    write_json(reports/'receipt.json',dict(engine_sha256=digest(args.godot),reference_sha256=digest(reports/'reference.json'),fixture_sha256=digest(reports/'cutscene_camera_v0410.hpp'),tools_sha256={str(p.relative_to(ROOT)):digest(p) for p in [Path(__file__).resolve(),ROOT/'tools/godot_exporter/cutscene_camera_probe.gd']},hardware='not run',emulator='not run'))
    print('Camera reference complete:',reports)
if __name__=='__main__':
    try:main()
    except (ValueError,OSError,KeyError,TypeError) as e:print('CAMERA REFERENCE ERROR: '+str(e),file=sys.stderr);sys.exit(1)
