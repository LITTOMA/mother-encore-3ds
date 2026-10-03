#!/usr/bin/env python3
"""Execute reviewed DialogueBox command branches with native Timer/yield dispatch.

External actors, audio, camera and battle are recording boundaries. Their actual
movement/rendering/battle implementations are NOT validated by this probe.
Selected source blocks are copied byte-for-byte after whole-file hash checking;
the one sound-resource load is substituted by an explicit recording boundary.
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
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.lamp_dialogue import ROOT,REVIEW,verify_sources

KEYS=['actors','autowait','objectsfunction','ovbattlemusic','musicloop','soundeffect','talker','actorsmove','actorsturn','actorsshake','actorsjump','actorsanim','actorsemote','shakecam','changecam','movecam','startbattle']

def command_block(source,key):
    lines=source.splitlines(keepends=True);needle='\tif _curr_phrase.has("'+key+'"):'
    indices=[i for i,l in enumerate(lines) if l.rstrip()==needle]
    if len(indices)!=1:raise ValueError('Missing/ambiguous command block '+key)
    first=indices[0];end=first+1
    while end<len(lines):
        line=lines[end]
        if line.strip() and not line.lstrip().startswith('#') and not line.startswith('\t\t') and not line.startswith('\telse:') and not line.startswith('\telif '):break
        end+=1
    return ''.join(lines[first:end]).rstrip()+'\n'

def tabs(text):
    return re.sub(r'(?m)^( +)',lambda m:'\t'*(len(m.group(1))//4),text)

def prepare(source,work):
    review=json.loads(REVIEW.read_text());verify_sources(source,review);work.mkdir(parents=True,exist_ok=True)
    raw=(source/'Scripts/UI/DialogueBox.gd').read_text()
    blocks={k:command_block(raw,k) for k in KEYS}
    symbols={k:hashlib.sha256(v.encode()).hexdigest() for k,v in blocks.items()}
    handler=tabs(HANDLER_PREFIX)
    for key,block in blocks.items():
        if key=='soundeffect':block=block.replace('load(_curr_phrase["soundeffect"])','_reference_sound(_curr_phrase["soundeffect"])')
        handler+=block
        if key=='autowait':handler+='\tTrace.record("StartWait", "None", [0,0], 0, _curr_phrase["wait"])\n'
        if key=='objectsfunction':handler+='\tif _curr_phrase.has("objectsfunction"):\n\t\tfor key in _curr_phrase["objectsfunction"]:\n\t\t\tTrace.record("CallObjectDeferred", "None", [0,0], 0, 0, key, _curr_phrase["objectsfunction"][key])\n'
        if key=='talker':handler+='\tif _curr_phrase.has("talker"): Trace.record("SetTalker", "Lamp")\n'
        if key=='startbattle':handler+='\tif _queued_battle: Trace.record("QueueBattle", "Lamp", [0,0], 0, 0, "lamp", _battle_win_flag)\n'
    # Exact production next/goto branch and timer callback, excluding preceding
    # unsupported conditional-expression implementations which are absent here.
    start=raw.index('\tif _curr_phrase.has("redirect") or _curr_phrase.has("goto"):')
    end=raw.index('\nfunc get_actors()',start)
    handler+='\nfunc _next_phrase(with_sound := false):\n'+raw[start:end]+'\n'
    start=raw.index('func _on_WaitTimer_timeout():');end=raw.index('\nfunc ',start+5)
    handler+='\n'+raw[start:end]+'\n'
    start=raw.index('\tif _actors.size() != 0:',raw.index('func _end_dialogue():'));end=raw.index('\nfunc _clear_dialogue():',start)
    interaction_start=raw.index('\tif is_instance_valid(global.talker)',raw.index('func _end_dialogue():'))
    interaction_end=raw.index('\tif _name_label.text',interaction_start)
    handler+='\nfunc _end_dialogue():\n'+raw[interaction_start:interaction_end]+'\tTrace.record("SetTalker")\n'+raw[start:end]+'\n'
    handler+=tabs(HANDLER_SUFFIX)
    (work/'dialogue.gd').write_text(handler)
    (work/'project.godot').write_text('''config_version=4
[application]
config/name="Lamp scheduler command reference"
[autoload]
Trace="*res://trace.gd"
[logging]
file_logging/enable_logging=false
''')
    actor_source=(source/'Scripts/Main/actor.gd').read_text();classes=actor_source[actor_source.index('class MoveAction:'):actor_source.index('export var allow_debug_echo')]
    (work/'actor.gd').write_text(tabs(ACTOR_PREFIX)+classes+tabs(ACTOR_SUFFIX))
    for name,body in [('trace.gd',TRACE),('probe.gd',PROBE),('camera.gd',CAMERA),('audio.gd',AUDIO),('global.gd',GLOBAL),('ui.gd',UI),('object.gd',OBJECT),('enemy.gd',ENEMY)]: (work/name).write_text(body)
    (work/'actor.tscn').write_text('[gd_scene load_steps=2 format=2]\n[ext_resource path="res://actor.gd" type="Script" id=1]\n[node name="Actor" type="Node2D"]\nscript = ExtResource( 1 )\n')
    (work/'yaml_parser.gd').write_bytes((source/'Scripts/global/yaml_parser.gd').read_bytes())
    (work/'lamp.yaml').write_bytes((source/'Data/Dialogue/Podunk/cutscenes/lamp_attack.yaml').read_bytes())
    return review,symbols

HANDLER_PREFIX='''extends Node
signal done(result)
const Actor = preload("res://actor.gd")
const Enemy = preload("res://enemy.gd")
const YAMLParser = preload("res://yaml_parser.gd")
var ActorChar := preload("res://actor.tscn")
var global
var uiManager
var audioManager
var _dialog := {}
var _curr_phrase := {}
var _phrase_num = "0"
var _actors := {}
var _camera
var _can_input = false
var _auto_advance = false
var _finished = false
var _cursor_down_sprite = Node2D.new()
var _queued_battle = false
var _post_battle_cutscenes := {}
var _battle_win_flag = ""
var _dialog_response = 0
var _set_respawn = false
func _handle_phrase() -> void:
    $WaitTimer.stop()
    _can_input = true
    _auto_advance = false
    _curr_phrase = _dialog.get(str(_phrase_num), {})
    Trace.phrase = int(_phrase_num)
'''
HANDLER_SUFFIX='''
func _actor_strings_to_node(path) -> Node2D:
    var node = global.currentScene.get_node("Ninten" if path == "leader" else "lamp")
    Trace.record("BindActor", node.name, [0,0], 0, 0, path)
    return node
func _reference_sound(path):
    return path
func _close_dialog_box():
    pass
'''
TRACE='''extends Node
var events := []
var phrase := 0
var frame := 0
var frame_delta := 0.0
func _ready():
    get_tree().connect("idle_frame", self, "_idle")
func _idle():
    frame += 1
func record(kind, actor="None", vector=[0,0], value=0, duration=0, text="", detail=""):
    if actor == "lamp": actor = "Lamp"
    events.append({"frame":frame,"phrase":phrase,"kind":kind,"actor":actor,"vector":vector,"value":value,"duration":duration,"text":text,"detail":detail})
func _process(delta):
    frame_delta = delta
    if frame > 700:
        push_error("Probe did not finish")
        get_tree().quit(2)
'''
ACTOR_PREFIX='''extends Node2D
signal actor_ready
var actor_name := ""
var camera
var emotes
var drafted = false
var talking = false
var keepAfterBattle = false
var replaced
class EmotePlayer:
    func play(anim):
        Trace.record("EmoteActor", "Ninten", [0,0], 0, 0, anim)
'''
ACTOR_SUFFIX='''
func init(npc, _disable):
    replaced = npc
    actor_name = npc.name
func _ready():
    camera = load("res://camera.gd").new()
    add_child(camera)
    camera.actor_name = actor_name
    camera.global = get_tree().root.get_node("Probe").global
    emotes = {"animaPlayer":EmotePlayer.new()}
    Trace.record("ActorReady", actor_name)
    emit_signal("actor_ready")
func make_persistent():
    Trace.record("ActorPersistent", actor_name)
func unmake_persistent():
    Trace.record("ReleaseBattleActor", actor_name)
func stop_interaction():
    Trace.record("StopInteraction", actor_name)
func update_npcs():
    Trace.record("RestoreActor", actor_name)
func move_queue(moves, _animation, speed, type, _moonwalk, _loop, _queue):
    var point = moves[0].movement
    Trace.record("MoveActor", actor_name, [point.x,point.y], speed, 0, type)
func turn_to(direction, speed, _queue):
    Trace.record("TurnActor", actor_name, [direction.x,direction.y], 0, speed)
func shake(offset, length, _queue):
    Trace.record("ShakeActor", actor_name, [offset.x,offset.y], 0, length)
func jump(height, length, _times, _queue, _shadow, _crouch):
    Trace.record("JumpActor", actor_name, [0,0], height, length)
func play_anim(anim, speed, _queue, _type, _newidle):
    Trace.record("AnimateActor", actor_name, [0,0], speed, 0, anim)
func add_battle(_battler):
    drafted = true
'''
CAMERA='''extends Node2D
var actor_name = "None"
var global
var tween = null
func set_current():
    global.currentCamera = self
    Trace.record("ChangeCamera", actor_name)
func move_camera(pos, time, trans, easing):
    Trace.record("MoveCamera", "None", [pos.x,pos.y], 0, time, "sine" if trans == 1 else "bad", "out" if easing == 1 else "bad")
func shake_camera(size, length, direction):
    Trace.record("ShakeCamera", "None", [direction.x,direction.y], size, length, "small")
func return_camera(_duration):
    push_error("Unexpected camera return")
func return_offset(_duration):
    pass
'''
AUDIO='''extends Node
var overworldBattleMusic = false setget set_ov
func set_ov(value):
    overworldBattleMusic = value
    Trace.record("OverworldBattleMusic", "None", [0,0], 1 if value else 0)
func music_fadeout(index, duration):
    Trace.record("MusicFadeOut", "None", [0,0], index, duration)
func get_audio_player(_index):
    return null
func add_audio_player():
    pass
func play_music_on_latest_player(_music, _loop):
    push_error("Unexpected music")
func play_sfx(path, channel):
    Trace.record("PlaySound", "None", [0,0], 0, 0, path, channel)
'''
GLOBAL='''extends Node
signal cutscene_ended
var currentScene
var currentCamera
var talker
var partySpace := []
var partyObjects := []
var in_cutscene = true
func start_joy_vibration(_a, _b, _c, _d):
    pass
func set_respawn():
    pass
'''
UI='''extends Node
func update_key_indicator():
    pass
func start_battle(_a, _b, _c, _d, flag):
    Trace.record("RequestBattle", "Lamp", [0,0], 0, 0, "lamp", flag)
    get_tree().root.get_node("Probe").call_deferred("finish")
'''
OBJECT='''extends Node
func play_music():
    Trace.record("ObjectCalled", "None", [0,0], 0, 0, "Poltergeist/MusicArea", "play_music")
func delayed_start():
    Trace.record("ObjectCalled", "None", [0,0], 0, 0, "Room Shaker", "delayed_start")
'''
ENEMY='''extends Reference
var id
func _init(enemy):
    id = enemy
'''
PROBE='''extends SceneTree
var global
var controller
var step_deltas := []
func _init():
    call_deferred("run")
func run():
    var probe = Node.new()
    probe.name = "Probe"
    probe.set_script(load("res://probe_node.gd"))
    root.add_child(probe)
'''
# Node callback context retains global for actor/camera recording stubs.
PROBE_NODE='''extends Node
var global
var controller
func _ready():
    global = load("res://global.gd").new()
    add_child(global)
    global.currentScene = Node2D.new()
    add_child(global.currentScene)
    for actor_name in ["Ninten","lamp"]:
        var npc = Node2D.new()
        npc.name = actor_name
        global.currentScene.add_child(npc)
    for object_name in ["Poltergeist","Room Shaker"]:
        var object = Node.new()
        object.name = object_name
        global.currentScene.add_child(object)
        if object_name == "Poltergeist":
            var music = Node.new()
            music.name = "MusicArea"
            music.set_script(load("res://object.gd"))
            object.add_child(music)
        else:
            object.set_script(load("res://object.gd"))
    global.connect("cutscene_ended", self, "on_cutscene_end")
    controller = load("res://dialogue.gd").new()
    controller.global = global
    controller.uiManager = load("res://ui.gd").new()
    add_child(controller.uiManager)
    controller.audioManager = load("res://audio.gd").new()
    add_child(controller.audioManager)
    controller._camera = load("res://camera.gd").new()
    add_child(controller._camera)
    global.currentCamera = controller._camera
    var timer = Timer.new()
    timer.name = "WaitTimer"
    timer.one_shot = true
    controller.add_child(timer)
    add_child(controller)
    controller.add_child(controller._cursor_down_sprite)
    timer.connect("timeout", controller, "_on_WaitTimer_timeout")
    controller.connect("done", self, "on_done")
    controller._dialog = load("res://yaml_parser.gd").parse_file("res://lamp.yaml")
    Trace.record("BeginCutscene")
    controller._handle_phrase()
func on_cutscene_end():
    Trace.record("CutsceneEnded")
func on_done(_result):
    Trace.record("DialogueDone")
func finish():
    var file = File.new()
    file.open("res://trace.json", File.WRITE)
    file.store_string(JSON.print({"godot":Engine.get_version_info(),"delta":"%.17f" % Trace.frame_delta,"events":Trace.events}))
    file.close()
    get_tree().quit()
'''

def main():
    p=argparse.ArgumentParser();p.add_argument('--source',type=Path,default=ROOT/'upstream/MOTHER-Encore');p.add_argument('--godot',type=Path,required=True);p.add_argument('--work',type=Path,default=ROOT/'build/lamp-dialogue-reference');p.add_argument('--output',type=Path,default=ROOT/'reports/m5-lamp-dialogue-reference');args=p.parse_args()
    review,symbols=prepare(args.source,args.work)
    (args.work/'probe_node.gd').write_text(PROBE_NODE)
    env=dict(os.environ,XDG_DATA_HOME=str(args.work/'data'),XDG_CONFIG_HOME=str(args.work/'config'),XDG_CACHE_HOME=str(args.work/'cache'))
    result=subprocess.run([str(args.godot),'--path',str(args.work),'--fixed-fps','60','--script','probe.gd'],capture_output=True,text=True,timeout=30,env=env)
    args.output.mkdir(parents=True,exist_ok=True);(args.output/'native-trace.log').write_text(result.stdout+result.stderr)
    if result.returncode or 'ERROR' in result.stdout+result.stderr: raise ValueError('Native command reference failed; see native-trace.log')
    data=json.loads((args.work/'trace.json').read_text());data.update(schema=1,commit=review['commit'],sources=review['sources'],command_blocks=symbols,
        scope='Exact reviewed command blocks with recording external boundaries and native Timer/yield dispatch',
        overrides={'soundeffect.load':'Replaced resource loading by recording path; no audio playback validated','external_boundaries':'Actor, camera, object, audio and battle method bodies are recording stubs; production command branches copied verbatim','ui':'Only non-text command path, wait callback and end-dialogue lifecycle tail are executed'})
    generated_fixture=fixture(data,review)
    (args.output/'scheduler.json').write_text(json.dumps(data,indent=2)+'\n')
    (ROOT/'tests/fixtures/lamp_dialogue_v0410.hpp').write_text(generated_fixture)
    print('Native command trace:',len(data['events']),'events; final frame',data['events'][-1]['frame'])



def fixture(data,review):
    from tools.lamp_dialogue import compile_receipt,RECEIPT,number,canonical
    if data.get('schema')!=1 or data.get('commit')!=review['commit'] or data.get('sources')!=review['sources'] or data.get('command_blocks')!=review['command_blocks']:
        raise ValueError('Unreviewed command reference provenance')
    if data.get('scope')!='Exact reviewed command blocks with recording external boundaries and native Timer/yield dispatch' or set(data.get('overrides',{}))!={'soundeffect.load','external_boundaries','ui'}:
        raise ValueError('Unreviewed command reference scope')
    receipt=json.loads(RECEIPT.read_text())
    if data.get('godot')!=receipt['godot'] or data.get('delta')!='0.01666666753590110':
        raise ValueError('Unreviewed command reference engine/delta')
    if hashlib.sha256(canonical({'delta':data.get('delta'),'events':data.get('events')})).hexdigest()!=review.get('native_trace_sha256'):
        raise ValueError('Changed unreviewed native command trace')
    generated=[a for a in compile_receipt(receipt,review) if a['kind'] not in ['YieldIdle','AwaitTimer']]
    events=data.get('events')
    if not isinstance(events,list) or len(events)!=59:raise ValueError('Missing native command events')
    actions=[e for e in events if e['kind'] not in ['ActorReady','ObjectCalled']]
    if len(actions)!=len(generated):raise ValueError('Native/generated command count mismatch')
    for event,command in zip(actions,generated):
        if set(event)!=set(command)|{'frame'}:raise ValueError('Unknown native command field')
        if {k:v for k,v in event.items() if k!='frame'}!=command:raise ValueError('Native/generated command mismatch: '+str(event))
        if type(event['frame']) is not int or event['frame']<0 or event['frame']>1000:raise ValueError('Invalid native frame')
    if any(a['frame']>b['frame'] for a,b in zip(events,events[1:])):raise ValueError('Nonmonotonic native frame')
    if [(e['frame'],e['actor']) for e in events if e['kind']=='ActorReady']!=[(0,'Ninten'),(0,'Lamp')]:raise ValueError('Changed actor-ready ordering')
    calls=[e for e in events if e['kind']=='ObjectCalled']
    scheduled=[e for e in events if e['kind']=='CallObjectDeferred']
    for called,scheduled in zip(calls,scheduled):
        if any(called[k]!=scheduled[k] for k in ['frame','phrase','text','detail']):raise ValueError('Changed deferred object dispatch')
    if len(calls)!=2:raise ValueError('Missing deferred object execution')
    # Preserve independently recorded frames; never calculate native expectations
    # from the C++ timer implementation.
    lines=['// Generated from the actual Godot command trace. Do not edit.','#pragma once','#include "encore/dialogue.hpp"','#include <array>',
        'namespace encore::upstream::reference {','struct DialogueEvent { unsigned frame; DialogueAction action; };',f'inline constexpr std::array<DialogueEvent,{len(actions)}> lamp_trace{{{{']
    for event in actions:
        # Test-golden serialization only; production data never uses C++ headers.
        a=event
        num=lambda n:format(float(n),'.17g')
        coord=lambda n:num(n)+('.0' if float(n)%1==0 else '')+'f'
        entry='{DialogueActionKind::%s,DialogueActor::%s,%d,{%s,%s},%s,%s,%s,%s}' % (
            a['kind'],a['actor'],a['phrase'],coord(a['vector'][0]),coord(a['vector'][1]),
            num(a['value']),num(a['duration']),json.dumps(a['text']),json.dumps(a['detail']))
        lines.append('    {%d,%s},'%(event['frame'],entry))
    lines+=['}};','}']
    return '\n'.join(lines)+'\n'

if __name__=='__main__':main()
