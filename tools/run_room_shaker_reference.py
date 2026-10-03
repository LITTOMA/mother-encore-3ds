#!/usr/bin/env python3
"""Native source RoomShaker delayed/repeating timer and shared RNG oracle.

No world rendering/audio or production edits. Camera/audio/vibration effects are
observable adapters; original source control flow and native Timer are retained.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from tools.extract_battle_entry import Extractor,require,node
from tools.run_battle_victory_reference import function,adapted,sha

SERVICE='''extends Node
var runner
var in_battle=true
var game_over=false
var state=0
const CAMERA=1
var currentCamera
var stream
func is_in_battle():return in_battle
func is_game_over():return game_over
func get_state():return state
func get_player():return self
func shake_camera(magnitude,length,direction):
\trunner.event("camera_adapter",{"magnitude":magnitude,"length":length,"direction":[direction.x,direction.y]})
func play():runner.event("audio_adapter")
func start_joy_vibration(device,weak,strong,length):
\trunner.vibrations+=1
\trunner.event("joy_adapter",{"device":device,"weak":weak,"strong":strong,"length":length})
'''

PROBE='''extends SceneTree
var tick=0
var start_tick=0
var shaker
var service
var events=[]
var samples=[]
var draws=[]
var cases=[]
var tests={}
var vibrations=0
var mirror=RandomNumberGenerator.new()
var output
var metadata
var fixtures
var last_timer
var raw_count=0
func _init():
\tfor arg in OS.get_cmdline_args():
\t\tif arg.begins_with("--encore-out="):output=arg.substr(13)
\tmetadata=read_json("res://metadata.json")
\tfixtures=read_json("res://fixtures.json")
\tcall_deferred("run")
func read_json(path):
\tvar f=File.new()
\tassert(f.open(path,File.READ)==OK)
\treturn JSON.parse(f.get_as_text()).result
func bits(value):
\tvar b=StreamPeerBuffer.new()
\tb.big_endian=false
\tb.put_double(value)
\treturn b.data_array.hex_encode()
func _idle(delta):
\ttick+=1
\tif shaker:
\t\tvar timer=shaker.get_node("Timer")
\t\tsamples.append({"frame":tick-start_tick,"delta":"%.17f"%delta,"timer_left":"%.17f"%timer.time_left,"wait_time":"%.17f"%timer.wait_time,"stopped":timer.is_stopped(),"in_battle":service.in_battle,"vibrations":vibrations})
\treturn false
func event(name,data={}):
\tvar row=data.duplicate(true)
\trow.event=name
\trow.frame=tick-start_tick
\trow.order=events.size()
\tif shaker:
\t\tvar timer=shaker.get_node("Timer")
\t\trow.timer_left="%.17f"%timer.time_left
\t\trow.wait_time="%.17f"%timer.wait_time
\t\trow.stopped=timer.is_stopped()
\t\trow.in_battle=service.in_battle
\t\trow.game_over=service.game_over
\t\trow.player_state=service.state
\tevents.append(row)
func native_timer(label,duration):
\tvar timer=create_timer(duration)
\tlast_timer=timer
\tevent("delay_created",{"label":label,"duration":duration,"stored":"%.17f"%timer.time_left})
\ttimer.connect("timeout",self,"delay_finished",[label])
\treturn timer
func delay_finished(label):event("delay_finished",{"label":label,"stored":"%.17f"%last_timer.time_left})
func random_range(label,low,high):
\tvar before=str(mirror.state)
\tvar result=rand_range(low,high)
\tvar raw=[str(mirror.randi())]
\tif raw[0]!="0":
\t\traw.append(str(mirror.randi()))
\t\traw.append(str(mirror.randi()))
\traw_count+=raw.size()
\tdraws.append({"frame":tick-start_tick,"label":label,"low":low,"high":high,"result":"%.17f"%result,"result_double_hex":bits(result),"raw":raw,"state_before":before,"state_after":str(mirror.state)})
\tevent("interval_rng",{"result":"%.17f"%result,"raw_count":raw.size()})
\treturn result
func new_case():
\tstart_tick=tick
\tevents=[]
\tsamples=[]
\tdraws=[]
\tvibrations=0
\traw_count=0
\tservice=load("res://service.gd").new()
\tservice.runner=self
\tservice.currentCamera=service
\tget_root().add_child(service)
\tshaker=load("res://shaker.gd").new()
\tshaker.runner=self
\tshaker.global=service
\tshaker.uiManager=service
\tshaker.direction=Vector2(fixtures.direction[0],fixtures.direction[1])
\tvar timer=Timer.new()
\ttimer.name="Timer"
\tshaker.add_child(timer)
\ttimer.connect("timeout",shaker,"_on_Timer_timeout")
\tvar audio=load("res://service.gd").new()
\taudio.runner=self
\taudio.name="AudioStreamPlayer"
\tshaker.add_child(audio)
\tget_root().add_child(shaker)
\tseed(123)
\tmirror.seed=123
\tevent("ready",{"one_shot":timer.one_shot,"autostart":timer.autostart,"process_mode":timer.process_mode,"pause_mode":timer.pause_mode})
func finish_case(name):
\tvar actual=str(randi())
\tvar expected=str(mirror.randi())
\tassert(actual==expected)
\tcases.append({"name":name,"events":events.duplicate(true),"samples":samples.duplicate(true),"draws":draws.duplicate(true),"raw_count":raw_count,"vibrations":vibrations,"next_randi":actual,"expected_next_randi":expected})
\tshaker.free()
\tshaker=null
\tservice.free()
func tiny_timeout(timer):
\ttests.strict_threshold.events.append({"tick":tick,"left":"%.17f"%timer.time_left})
func run():
\tyield(self,"idle_frame")
\tnew_case()
\tshaker.delayed_start()
\twhile tick-start_tick<620:yield(self,"idle_frame")
\tassert(draws.empty() and vibrations==0)
\tevent("battle_finished")
\tservice.in_battle=false
\twhile vibrations<4:yield(self,"idle_frame")
\tfinish_case("delay_and_repeat_continue_during_battle")
\tyield(self,"idle_frame")
\tnew_case()
\tservice.in_battle=false
\tshaker.start_shake()
\tassert(vibrations==1 and draws.size()==1)
\tfor _i in range(10):yield(self,"idle_frame")
\tevent("start_again_while_running")
\tvar old=shaker.timer.time_left
\tshaker.start_shake()
\tassert(shaker.timer.time_left==old and vibrations==1)
\twhile vibrations<4:yield(self,"idle_frame")
\tshaker.stop_shake()
\tassert(shaker.timer.time_left==0)
\tfinish_case("start_outside_battle_and_no_restart")
\tyield(self,"idle_frame")
\tnew_case()
\tservice.in_battle=false
\tservice.game_over=true
\tshaker.start_shake()
\tassert(vibrations==0 and !shaker.timer.is_stopped())
\tservice.game_over=false
\tservice.state=service.CAMERA
\tshaker._on_Timer_timeout()
\tassert(vibrations==0 and draws.empty())
\tfinish_case("gameover_and_camera_guards")
\tEngine.time_scale=3.75
\tfor _i in range(2):yield(self,"idle_frame")
\ttests.strict_threshold={"start_tick":tick,"samples":[],"events":[]}
\tvar timer=Timer.new()
\ttimer.wait_time=.125
\tget_root().add_child(timer)
\ttimer.connect("timeout",self,"tiny_timeout",[timer])
\ttimer.start()
\tfor _i in range(7):
\t\ttests.strict_threshold.samples.append({"tick":tick,"left":"%.17f"%timer.time_left})
\t\tyield(self,"idle_frame")
\ttimer.free()
\tEngine.time_scale=1.0
\tvar f=File.new()
\tassert(f.open(output,File.WRITE)==OK)
\tf.store_string(JSON.print({"schema":1,"engine":Engine.get_version_info(),"metadata":metadata,"cases":cases,"tests":tests},"  "))
\tf.close()
\tprint("ROOM_SHAKER_REFERENCE_COMPLETE")
\tquit()
'''

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--godot',type=Path,required=True)
    ap.add_argument('--work',type=Path,required=True)
    ap.add_argument('--reports',type=Path,required=True)
    args=ap.parse_args();work=args.work.resolve();reports=args.reports.resolve()
    require(not work.exists() and work.is_relative_to(ROOT/'build/battle-victory-reference'),'Fresh build/battle-victory-reference work required')
    require(not reports.exists() and reports.is_relative_to(ROOT/'reports/battle-victory-reference'),'Fresh reports/battle-victory-reference reports required')
    ex=Extractor(ROOT)
    source=ex.text('Scripts/Main/roomshaker.gd')
    scene=ex.text('Nodes/Reusables/roomshaker.tscn')
    props=node(ex.text('Maps/podunk/Nintens House.tscn'),'Room Shaker')
    require(node(scene,'Timer')=={},'Unreviewed Timer property override')
    methods=['_ready','delayed_start','start_shake','stop_shake','vibrate','_on_Timer_timeout']
    code=source[:source.index('func _ready')]+ '\nvar runner\nvar global\nvar uiManager\n'
    for method in methods:
        text=function(source,method)
        if method=='_ready':text=text.replace('load("res://Audio/Sound effects/" + sound)','"res://Audio/Sound effects/" + sound')
        code+='\n'+adapted(text,'RoomShaker.'+method)
    fixtures={'direction':props['direction']}
    metadata={'commit':ex.lock['commit'],'scope':'bounded source RoomShaker delayed_start/start_shake/vibrate and native repeating Timer; camera/audio/vibration adapters only','sources':ex.sources,'method_sha256':{m:hashlib.sha256(function(source,m).encode()).hexdigest() for m in methods},'adapters':['original _ready audio resource load replaced by path string on fake audio Node','camera shake/audio/joy effects logged; existing camera-shaker math is not reimplemented or tested here','global rand_range forwarded to native built-in and mirrored raw stream; observer only','whole-game not run; player/battle/game-over states explicitly driven'],'source_methods':methods}
    work.mkdir(parents=True);reports.mkdir(parents=True)
    for name,text in {'shaker.gd':code,'service.gd':SERVICE,'probe.gd':PROBE}.items():(work/name).write_text(text)
    for name,obj in [('fixtures',fixtures),('metadata',metadata)]:(work/(name+'.json')).write_text(json.dumps(obj,indent=2)+'\n')
    (work/'project.godot').write_text('config_version=4\n[application]\nconfig/name="RoomShaker source oracle"\n[logging]\nfile_logging/enable_logging=false\n[physics]\ncommon/physics_fps=60\n')
    env=dict(os.environ)
    for key in ['XDG_DATA_HOME','XDG_CONFIG_HOME','XDG_CACHE_HOME']:
        d=work/key.lower();d.mkdir();env[key]=str(d)
    command=[str(args.godot.resolve()),'--path',str(work),'--fixed-fps','60','--script',str(work/'probe.gd'),'--encore-out='+str(reports/'reference.json')]
    with (reports/'godot.txt').open('wb') as f:result=subprocess.run(command,stdout=f,stderr=subprocess.STDOUT,env=env,timeout=35)
    log=(reports/'godot.txt').read_text()
    require(result.returncode==0 and 'ERROR:' not in log and 'SCRIPT ERROR' not in log and 'ROOM_SHAKER_REFERENCE_COMPLETE' in log,'Native shaker failed; inspect retained log')
    ref=json.loads((reports/'reference.json').read_text());require(ref['engine']['string']=='3.6.2-stable (official)','Wrong native engine')
    receipt={'commit':ex.lock['commit'],'engine_sha256':sha(args.godot),'tool_sha256':sha(__file__),'reference_sha256':sha(reports/'reference.json'),'generated_script_sha256':{p.name:sha(p) for p in work.glob('*.gd')},'scope':metadata['scope']}
    (reports/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
    summary={'cases':[{k:c[k] for k in ['name','events','draws','raw_count','vibrations','next_randi','expected_next_randi']} for c in ref['cases']],'tests':ref['tests']}
    (reports/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    print(json.dumps(summary,indent=2))
if __name__=='__main__':
    try:main()
    except (ValueError,OSError,KeyError,subprocess.SubprocessError) as e:
        print('ROOM SHAKER REFERENCE ERROR: '+str(e),file=sys.stderr);sys.exit(1)
