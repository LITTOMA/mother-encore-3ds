#!/usr/bin/env python3
"""Bounded official Godot 3.6.2 Mimmie door and Doll trigger oracle.

Runs pinned extracted source methods, native scaled physics, AnimationPlayers,
Timer and deferred UI ready signals. Stops at the dialogue request boundary:
Doll YAML, actor actions and battle are deliberately never executed.
"""
from __future__ import annotations
import argparse, hashlib, json, os, re, subprocess, sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from tools.extract_battle_entry import Extractor, require, node, animation
from tools.run_battle_victory_reference import function, sha

SERVICE='''extends Node
signal flags_updated
signal battle_to_ov
signal cutscene_ended
const DialogueBoxRes=preload("res://dialogue_stub.tscn")
var runner
var global
var player
var scene_transition
var currentScene
var currentCamera
var entering_door=false
var partySpace=[]
var partyObjects=[]
var musicChangers=[]
var flags={"poltergeist":true,"mimmie_door_opened":false,"doll_defeated":false,"doll_melody":false,"doll_attack":false}
var fade
var _ui_stack=[]
var _stable_canvas_layer
var _dialogue_box
var _cutscene=false
var _pause_menu_active=false
var in_battle=false
var item
func get_player():return player
func get_fade():return fade
func is_in_battle():return in_battle
func party_call(method,arg=null):runner.event("party_adapter",{"method":method})
func close_commands_menu(_a,_b):runner.event("close_commands_menu")
func close_key_indicator():pass
func toggle_black_bars(_a):pass
func set_respawn():assert(false)
func shake_camera(magnitude,length,direction):runner.event("camera_shake",{"magnitude":magnitude,"length":length,"direction":[direction.x,direction.y]})
func find_item_for_all(_key):return null
func drop_item_from_party(_item):assert(false)
'''
PLAYER='''extends "res://party.gd"
const AreaRoom=preload("res://room.gd")
signal paused
signal unpaused
var runner
var global
var uiManager
var camera
var eventRayCaster
var _paused=false
var _input_vector=Vector2.ZERO
var _direction=Vector2.DOWN
var can_interact=true
var _crouch=false
var _walk=false
var _running=false
var _tap_run=false
var _state=0
const MOVE=0
const TELEPORTING=5
var _tp_take_off_timer_done=false
var _tp_take_off_timer=null
var motion=Vector2.ZERO
var input_check=false
var last_collision=""
func _anim_play_pause(_a,_b=false):pass
func _set_running(enabled):_running=enabled
func _check_event_collider():pass
func _start_landing():assert(false)
func _set_event_collider(_a):pass
func blend_position(_a):pass
func use_telepathy():assert(false)
func _physics_process(delta):
\tif !_paused and motion!=Vector2.ZERO:
\t\tvar collision=move_and_collide(motion*delta)
\t\tif collision:
\t\t\tlast_collision=str(collision.collider.get_path())
\t\t\trunner.event("motion_collision",{"collider":last_collision})
\tif input_check:check_normal_interaction()
'''
AUDIO='''extends Node
var runner
var stream
var playing=false
var stream_paused=false
func play():
\tplaying=true
\tif runner:runner.event("audio_adapter",{"path":str(stream)})
func stop():playing=false
'''
PROBE='''extends SceneTree
var tick=0
var physics_tick=0
var start_tick=0
var events=[]
var samples=[]
var cases=[]
var sweep=[]
var output
var fixtures
var metadata
var service
var player
var root_world
var openable
var doors=[]
var story
var fade
var sample_enabled=false
var flag_notifications=0
var boundary_samples=[]
func _init():
\tfor arg in OS.get_cmdline_args():
\t\tif arg.begins_with("--encore-out="):output=arg.substr(13)
\tfixtures=read_json("res://fixtures.json")
\tmetadata=read_json("res://metadata.json")
\tcall_deferred("run")
func read_json(path):
\tvar f=File.new()
\tassert(f.open(path,File.READ)==OK)
\treturn JSON.parse(f.get_as_text()).result
func vec(v):return [v.x,v.y]
func _idle(delta):
\ttick+=1
\tif sample_enabled:samples.append(snapshot("idle",delta))
\treturn false
func _iteration(_delta):
\tphysics_tick+=1
\treturn false
func snapshot(label,delta=0):
\tvar row={"label":label,"frame":tick-start_tick,"physics_tick":physics_tick,"delta":"%.17f"%delta}
\tif player:
\t\trow.player_position=vec(player.position)
\t\trow.direction=vec(player.get_direction())
\t\trow.paused=player.is_paused()
\t\trow.running=player.is_running()
\t\trow.player_mask=player.collision_mask
\t\trow.entering=service.entering_door
\t\trow.cutscene=service.is_in_cutscene()
\t\trow.flags=service.flags.duplicate()
\t\trow.flag_notifications=flag_notifications
\tif openable:
\t\trow.blocked=openable.blocked
\t\trow.unlocked=openable._unlocked
\t\trow.sprite_visible=openable.get_node("Sprite").visible
\t\trow.player_collider_disabled=openable.get_node("StaticBody2D/CollisionShape2D").disabled
\t\trow.nonplayer_collider_disabled=openable.get_node("NonPlayerStaticBody2D/CollisionShape2D").disabled
\t\trow.animation=openable.get_node("AnimationPlayer").current_animation
\t\trow.animation_position="%.17f"%openable.get_node("AnimationPlayer").current_animation_position if openable.get_node("AnimationPlayer").assigned_animation!="" else "unset"
\t\trow.timer_left="%.17f"%openable.get_node("Timer").time_left
\t\trow.timer_wait_time="%.17f"%openable.get_node("Timer").wait_time
\t\trow.timer_stopped=openable.get_node("Timer").is_stopped()
\t\trow.timer_one_shot=openable.get_node("Timer").one_shot
\t\trow.timer_process_mode=openable.get_node("Timer").process_mode
\t\trow.one_way=openable.one_way
\t\trow.prompt_enabled=openable.get_node("interact/ButtonPrompt").enabled
\t\trow.overlapping=[]
\t\tfor body in openable.get_node("Area2D").get_overlapping_bodies():row.overlapping.append(str(body.get_path()))
\tif fade:
\t\trow.fade_animation=fade._anim_player.assigned_animation
\t\trow.fade_position="%.17f"%fade._anim_player.current_animation_position if fade._anim_player.assigned_animation!="" else "unset"
\t\trow.fade_playing=fade._anim_player.is_playing()
\tif story:row.story_processing=story.is_processing()
\treturn row
func event(name,data={}):
\tvar row=snapshot(name)
\tfor key in data:row[key]=data[key]
\trow.event=name
\trow.order=events.size()
\tevents.append(row)
func signal_event(name):
\tevent(name)
\tif name=="player_paused":player.motion=Vector2.ZERO
func flags_event():
\tflag_notifications+=1
\tevent("flags_updated")
func area_event(body,name,kind):event(name+":"+kind,{"body":str(body.get_path())})
func new_node(type,name,parent):
\tvar n=ClassDB.instance(type)
\tn.name=name
\tparent.add_child(n)
\treturn n
func setup(with_warps=false,with_story=false):
\tstart_tick=tick
\tevents=[]
\tsamples=[]
\tflag_notifications=0
\tservice=load("res://service.gd").new()
\tservice.runner=self
\tservice.global=service
\tget_root().add_child(service)
\tservice._stable_canvas_layer=new_node("Node","UI",service)
\tservice.connect("flags_updated",self,"flags_event")
\troot_world=load("res://room.gd").new()
\troot_world.name=fixtures.scene_name
\tservice.currentScene=root_world
\tget_root().add_child(root_world)
\tvar below=new_node("Node2D","Below",root_world)
\tvar objects=new_node("Node2D","Objects",root_world)
\tplayer=load("res://player.gd").new()
\tplayer.name="player"
\tplayer.runner=self
\tplayer.global=service
\tplayer.uiManager=service
\tplayer.collision_layer=1
\tplayer.collision_mask=4353
\tplayer.position=Vector2(64,400)
\tvar poly=CollisionPolygon2D.new()
\tpoly.name="CollisionShape2D"
\tpoly.position=Vector2(0,3)
\tpoly.polygon=PoolVector2Array([Vector2(6,4),Vector2(4,6),Vector2(-5,6),Vector2(-7,4),Vector2(-7,2),Vector2(-5,0),Vector2(4,0),Vector2(6,2)])
\tplayer.add_child(poly)
\tvar aud=load("res://audio.gd").new()
\taud.name="AudioStreamPlayer"
\tplayer.add_child(aud)
\tnew_node("Timer","MiscTimer",player)
\tplayer.camera=new_node("Camera2D","Camera2D",player)
\tvar ray=new_node("RayCast2D","EventDetector",player)
\tray.position=Vector2(0,6)
\tray.scale=Vector2(.3,1)
\tray.enabled=true
\tray.cast_to=Vector2(0,16)
\tray.collide_with_areas=true
\tplayer.eventRayCaster=ray
\tservice.player=player
\tservice.currentCamera=service
\tobjects.add_child(player)
\tfor sig in ["paused","unpaused"]:player.connect(sig,self,"signal_event",["player_"+sig])
\topenable=load("res://Openable.tscn").instance()
\topenable.name="Openable Door2"
\topenable.runner=self
\topenable.global=service
\topenable.globaldata=service
\topenable.uiManager=service
\topenable.Inventory=service
\topenable.position=Vector2(fixtures.openable.position[0],fixtures.openable.position[1])
\topenable.blocked=fixtures.openable.blocked
\topenable.flag=fixtures.openable.flag
\topenable.get_node("AudioStreamPlayer").runner=self
\tfor sig in ["body_entered","body_exited"]:openable.get_node("Area2D").connect(sig,self,"area_event",["openable",sig])
\tbelow.add_child(openable)
\tfade=load("res://Fade.tscn").instance()
\tfade.runner=self
\tfade.global=service
\tget_root().add_child(fade)
\tservice.fade=fade
\tfor sig in ["fade_in_done","fade_in_mostly_done","fade_out_mostly_done","fade_out_done"]:fade.connect(sig,self,"signal_event",[sig])
\tservice.scene_transition=load("res://transition.gd").new()
\tservice.scene_transition.global=service
\tservice.scene_transition.uiManager=service
\tservice.scene_transition.audioManager=service
\tservice.scene_transition.runner=self
\tservice.add_child(service.scene_transition)
\tif with_warps:
\t\tvar parent=new_node("Node2D","Doors",root_world)
\t\tfor f in fixtures.doors:
\t\t\tvar d=load("res://Door.tscn").instance()
\t\t\td.name=f.name
\t\t\td.runner=self
\t\t\td.global=service
\t\t\td.globaldata=service
\t\t\td.uiManager=service
\t\t\td.position=Vector2(f.position[0],f.position[1])
\t\t\td.scale=Vector2(f.scale[0],f.scale[1])
\t\t\td.dir=Vector2(f.dir[0],f.dir[1])
\t\t\td.sound=f.sound
\t\t\td.end_sound=f.end_sound
\t\t\td.get_node("Position2D").position=Vector2(f.destination_local[0],f.destination_local[1])
\t\t\td.get_node("AudioStreamPlayer").runner=self
\t\t\tparent.add_child(d)
\t\t\tfor sig in ["body_entered","body_exited"]:d.connect(sig,self,"area_event",[d.name,sig])
\t\t\tfor sig in ["entered","moved_player","done"]:d.connect(sig,self,"signal_event",[d.name+":"+sig])
\t\t\tdoors.append(d)
\tif with_story:
\t\tvar parent=new_node("Node2D","Poltergeist",root_world)
\t\tstory=load("res://Cutscene.tscn").instance()
\t\tstory.name="Cutscene Area2"
\t\tstory.runner=self
\t\tstory.global=service
\t\tstory.globaldata=service
\t\tstory.uiManager=service
\t\tstory.position=Vector2(fixtures.story.position[0],fixtures.story.position[1])
\t\tstory.scale=Vector2(fixtures.story.scale[0],fixtures.story.scale[1])
\t\tstory.dialog=fixtures.story.dialog
\t\tstory.disappear_flag=fixtures.story.disappear_flag
\t\tparent.add_child(story)
\t\tfor sig in ["body_entered","body_exited"]:story.connect(sig,self,"area_event",["story",sig])
\tsample_enabled=true
func finish_case(name):
\tevent("case_final")
\tcases.append({"name":name,"events":events.duplicate(true),"samples":samples.duplicate(true)})
\tsample_enabled=false
\troot_world.free()
\tservice.free()
\tfade.free()
\tplayer=null
\topenable=null
\tstory=null
\tservice=null
\tfade=null
\tdoors=[]
func settle(count=4):
\tfor _i in range(count):yield(self,"idle_frame")
func count_event(name):
\tvar n=0
\tfor e in events:
\t\tif e.event==name:n+=1
\treturn n
func run():
\tyield(self,"idle_frame")
\tsetup()
\tyield(settle(),"completed")
\tevent("initial_locked")
\tassert(openable.blocked and !openable._unlocked)
\tassert(!openable.get_node("StaticBody2D/CollisionShape2D").disabled)
\t# Actual Player normal-A gate, source ray/interaction and UI deferred ready.
\tplayer.position=Vector2(64,382)
\tplayer.set_direction_and_input(Vector2.UP)
\tyield(settle(),"completed")
\tplayer.eventRayCaster.force_raycast_update()
\tevent("a_ray",{"collider":str(player.eventRayCaster.get_collider().get_path()) if player.eventRayCaster.is_colliding() else "none"})
\tplayer.input_check=true
\tInput.action_press("ui_accept")
\tyield(settle(2),"completed")
\tInput.action_release("ui_accept")
\tplayer.input_check=false
\tassert(count_event("dialogue_start_from_id")==1)
\tassert(!service.flags.mimmie_door_opened and !service.flags.doll_attack)
\tfinish_case("initial_blocked_a_interaction")
\t# Exact native scaled-area/octagon boundaries with behavior callbacks disconnected.
\tsetup(true,true)
\tfor d in doors:d.disconnect("body_entered",d,"_on_Door_body_entered")
\tstory.disconnect("body_entered",story,"_on_Cutscene_Area_body_entered")
\tstory.disconnect("body_exited",story,"_on_Cutscene_Area_body_exited")
\tyield(settle(),"completed")
\tfor pos in [Vector2(64,371),Vector2(64,370),Vector2(64,369.999),Vector2(64,369),Vector2(64,326),Vector2(64,325),Vector2(42,350),Vector2(42.001,350),Vector2(87,350),Vector2(86.999,350),Vector2(64,365),Vector2(64,364),Vector2(64,363.999),Vector2(64,363),Vector2(128,174),Vector2(128,175),Vector2(128,175.001),Vector2(128,176),Vector2(128,159),Vector2(128,159.001),Vector2(128,181),Vector2(128,180.999),Vector2(128,169)]:
\t\tplayer.position=pos
\t\tyield(settle(),"completed")
\t\tboundary_samples.append({"position":vec(pos),"openable":openable.get_node("Area2D").overlaps_body(player),"upstair_sister":doors[0].overlaps_body(player),"sister_upstair":doors[1].overlaps_body(player),"doll_area":story.overlaps_body(player)})
\tfinish_case("source_scaled_geometry_sweep")
\t# Native collision walk stops on player-only static body; all wrong directions fail.
\tfor dir in [Vector2.UP,Vector2.DOWN,Vector2.LEFT,Vector2.RIGHT,Vector2(-.70710678,-.70710678)]:
\t\tsetup()
\t\tyield(settle(),"completed")
\t\tplayer._running=dir!=Vector2.UP
\t\tplayer.set_direction_and_input(dir)
\t\tplayer.motion=Vector2(0,-64)
\t\tyield(settle(45),"completed")
\t\tplayer.motion=Vector2.ZERO
\t\tevent("movement_stopped")
\t\tassert(openable.blocked and !openable._unlocked and player.position.y>364)
\t\tassert(player.last_collision.ends_with("/StaticBody2D"))
\t\tfinish_case("walk_up_blocked" if dir==Vector2.UP else "running_direction_"+str(dir))
\t# Digital source directions have signed, non-normalized components: both upper diagonals pass.
\tfor dir in [Vector2(-1,-1),Vector2(1,-1)]:
\t\tsetup()
\t\tyield(settle(),"completed")
\t\tplayer._running=true
\t\tplayer.set_direction_and_input(dir)
\t\tplayer.position=Vector2(64,369)
\t\tyield(settle(),"completed")
\t\tassert(!openable.blocked and openable._unlocked and service.flags.mimmie_door_opened)
\t\tfinish_case("running_signed_diagonal_"+str(dir))
\t# Running alone is not polled: enter walking, change running while still inside, no bash.
\tsetup()
\tyield(settle(),"completed")
\tplayer.set_direction_and_input(Vector2.UP)
\tplayer.position=Vector2(64,369)
\tyield(settle(),"completed")
\tassert(!openable._unlocked)
\tplayer._running=true
\tyield(settle(),"completed")
\tassert(!openable._unlocked)
\tevent("running_without_reentry_stays_blocked")
\tfinish_case("requires_body_enter_event")
\t# Native Action/Normal timing, pause retains this Area, exit timer and notifications.
\tsetup()
\tyield(settle(),"completed")
\tplayer._running=true
\tplayer._tap_run=true
\tplayer.set_direction_and_input(Vector2.UP)
\tplayer.position=Vector2(64,369)
\tyield(settle(),"completed")
\tassert(!openable.blocked and openable._unlocked and service.flags.mimmie_door_opened)
\tassert(flag_notifications==0)
\tassert(!openable.get_node("Sprite").visible)
\tplayer.pause()
\tyield(settle(),"completed")
\tassert(openable.get_node("Area2D").overlaps_body(player))
\tevent("pause_keeps_openable_overlap")
\tplayer.position=Vector2(64,400)
\tyield(settle(),"completed")
\tassert(openable.get_node("Sprite").visible and openable.get_node("Timer").is_stopped())
\tassert(openable.get_node("StaticBody2D/CollisionShape2D").disabled)
\tassert(!openable.get_node("NonPlayerStaticBody2D/CollisionShape2D").disabled)
\tplayer.unpause()
\tplayer.position=Vector2(64,369)
\tyield(settle(),"completed")
\tassert(!openable.get_node("Sprite").visible)
\tplayer.position=Vector2(64,400)
\tyield(settle(25),"completed")
\tassert(openable.get_node("Timer").is_stopped() and openable.get_node("Sprite").visible)
\tassert(count_event("timer_timeout")==1)
\tassert(flag_notifications==0)
\tassert(count_event("audio_adapter")==4) # bash, reopen, two close calls
\tservice.emit_signal("flags_updated")
\tyield(settle(),"completed")
\tassert(flag_notifications==1 and openable.get_node("StaticBody2D/CollisionShape2D").disabled)
\tfinish_case("bash_pause_exit_close_timer_and_notifications")
\t# One-shot timeout while a PartyObject has returned: expiry does not close.
\tsetup()
\tyield(settle(),"completed")
\tplayer._running=true
\tplayer.set_direction_and_input(Vector2.UP)
\tplayer.position=Vector2(64,369)
\tyield(settle(),"completed")
\tplayer.position=Vector2(64,400)
\tyield(settle(8),"completed")
\tplayer.position=Vector2(64,369)
\tyield(settle(25),"completed")
\tassert(count_event("timer_timeout")==1 and openable.get_node("Timer").is_stopped())
\tassert(!openable.get_node("Sprite").visible and count_event("audio_adapter")==1)
\tfinish_case("timer_reentry_preserves_open_door")
\t# Pausing outside after the timer starts suppresses the guarded close but not the trailing semicolon close.
\tsetup()
\tyield(settle(),"completed")
\tplayer._running=true
\tplayer.set_direction_and_input(Vector2.UP)
\tplayer.position=Vector2(64,369)
\tyield(settle(),"completed")
\tplayer.position=Vector2(64,400)
\tyield(settle(8),"completed")
\tplayer.pause()
\tyield(settle(25),"completed")
\tassert(count_event("timer_timeout")==1 and count_event("audio_adapter")==2)
\tassert(openable.get_node("Sprite").visible and openable.get_node("Timer").is_stopped())
\tfinish_case("timer_paused_outside_trailing_close")
\t# Native nonplayer blocker remains solid in Normal while the unlocked player blocker is disabled.
\tsetup()
\tyield(settle(),"completed")
\topenable.blocked=false
\tservice.flags.mimmie_door_opened=true
\topenable._update_door_state()
\tplayer.position=Vector2(200,400)
\tyield(settle(),"completed")
\tvar npc_body=new_node("KinematicBody2D","nonplayer_collision_probe",root_world)
\tnpc_body.collision_layer=573
\tnpc_body.collision_mask=1597
\tvar npc_shape=new_node("CollisionShape2D","CollisionShape2D",npc_body)
\tnpc_shape.position=Vector2(-.5,6)
\tnpc_shape.shape=RectangleShape2D.new()
\tnpc_shape.shape.extents=Vector2(7.5,3)
\tnpc_body.position=Vector2(64,400)
\tyield(settle(),"completed")
\tvar npc_collision=npc_body.move_and_collide(Vector2(0,-50))
\tassert(npc_collision and str(npc_collision.collider.get_path()).ends_with("/NonPlayerStaticBody2D"))
\tevent("nonplayer_collision_mapping",{"collider":str(npc_collision.collider.get_path()),"npc_mask":npc_body.collision_mask,"npc_layer":npc_body.collision_layer,"position":vec(npc_body.position)})
\tassert((player.collision_mask & openable.get_node("NonPlayerStaticBody2D").collision_layer)==0)
\tfinish_case("native_nonplayer_blocker_mapping")
\t# The source blocked branch intentionally has no PartyObject predicate.
\tsetup()
\tyield(settle(),"completed")
\tplayer._running=true
\tplayer.set_direction_and_input(Vector2.UP)
\tvar other=new_node("KinematicBody2D","non_party_body",root_world)
\tother.collision_mask=4096
\tvar shape=new_node("CollisionShape2D","CollisionShape2D",other)
\tshape.shape=CircleShape2D.new()
\tshape.shape.radius=1
\tother.position=Vector2(64,350)
\tyield(settle(),"completed")
\tassert(!openable.blocked and service.flags.mimmie_door_opened)
\tfinish_case("nonparty_body_source_branch_mapping")
\t# Forward warp with original CutsceneArea processing enabled during fade.
\tsetup(true,true)
\tyield(settle(),"completed")
\tplayer._running=true
\tplayer._tap_run=true
\tplayer.set_direction_and_input(Vector2.UP)
\tplayer.motion=Vector2(0,-96)
\tvar deadline=tick+160
\twhile count_event("Upstair_Sister:done")==0 and tick<deadline:yield(self,"idle_frame")
\tassert(count_event("Upstair_Sister:done")==1)
\tplayer.motion=Vector2.ZERO
\tyield(settle(10),"completed")
\tassert(player.position==Vector2(128,169))
\tassert(count_event("dialogue_start_from_id")==1 and service.is_in_cutscene() and player.is_paused())
\tassert(!service.flags.doll_attack and !service.flags.doll_defeated and !service.flags.doll_melody)
\tassert(count_event("player_unpaused")==0)
\tfinish_case("forward_warp_doll_trigger_during_fade")
\t# Suppressed story flag allows both physical warp directions without an encounter.
\tsetup(true,true)
\tservice.flags.doll_defeated=true
\tyield(settle(),"completed")
\tplayer._running=true
\tplayer._tap_run=true
\tplayer.set_direction_and_input(Vector2.UP)
\tplayer.motion=Vector2(0,-96)
\tdeadline=tick+160
\twhile count_event("Upstair_Sister:done")==0 and tick<deadline:yield(self,"idle_frame")
\tplayer.motion=Vector2.ZERO
\tassert(player.position==Vector2(128,169) and !player.is_paused())
\tassert(count_event("dialogue_start_from_id")==0)
\tyield(settle(10),"completed")
\tplayer._running=false
\tplayer.set_direction_and_input(Vector2.DOWN)
\tplayer.motion=Vector2(0,64)
\tdeadline=tick+160
\twhile count_event("Sister_Upstair:done")==0 and tick<deadline:yield(self,"idle_frame")
\tplayer.motion=Vector2.ZERO
\tassert(count_event("Sister_Upstair:done")==1)
\tassert(player.position==Vector2(64,369) and !player.is_paused())
\tyield(settle(10),"completed")
\tfinish_case("paired_warps_story_disappear_flag")
\tvar f=File.new()
\tassert(f.open(output,File.WRITE)==OK)
\tf.store_string(JSON.print({"schema":1,"engine":Engine.get_version_info(),"metadata":metadata,"fixtures":fixtures,"boundary_samples":boundary_samples,"cases":cases},"  "))
\tf.close()
\tprint("SISTER_DOOR_REFERENCE_COMPLETE")
\tquit()
'''

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--godot',type=Path,required=True)
    ap.add_argument('--work',type=Path,required=True)
    ap.add_argument('--reports',type=Path,required=True)
    args=ap.parse_args();work=args.work.resolve();reports=args.reports.resolve()
    require(not work.exists() and work.is_relative_to(ROOT/'build/sister-door-reference'),'Fresh scoped work directory required')
    require(not reports.exists() and reports.is_relative_to(ROOT/'reports/sister-door-reference'),'Fresh scoped report directory required')
    ex=Extractor(ROOT);methods={};scripts={}
    def extract(path,names,trace=True):
        src=ex.text(path);out=''
        for name in names:
            fn=function(src,name);methods[path+':'+name]=hashlib.sha256(fn.encode()).hexdigest()
            fn=fn.replace(':=','=')
            fn=re.sub(r':\s*(Door|PartyMemberPlayer|Item)(?=\s*[,)=\n])','',fn)
            if trace and name not in ['is_in_cutscene','is_stack_empty','is_pause_menu_active']:
                label='timer_timeout' if path.endswith('Openable Door.gd') and name=='_on_Timer_timeout' else 'source_enter'
                fn=fn.replace('\n','\n\tif runner:runner.event('+json.dumps(label)+',{"method":'+json.dumps(path+':'+name)+'})\n',1)
            out+='\n'+fn
        return out
    od='Scripts/Main/Openable Door.gd';src=ex.text(od)
    decl=src[:src.index('func _set_texture')].replace('tool\n','')
    scripts['openable.gd']=decl+'\nconst PartyObject=preload("res://party.gd")\nvar runner\nvar global\nvar globaldata\nvar uiManager\nvar Inventory\n'+extract(od,re.findall(r'^func (\w+)\(',src,re.M))
    scripts['openable.gd']=scripts['openable.gd'].replace('\t\t\tglobaldata.flags[flag] = true','\t\t\tglobaldata.flags[flag] = true\n\t\t\trunner.event("bash_direct_flag_write")').replace('load("Audio/Sound effects/" + end_sound)','"Audio/Sound effects/" + end_sound').replace('load("Audio/Sound effects/" + sound)','"Audio/Sound effects/" + sound').replace('load("res://Audio/Sound effects/bash.mp3")','"res://Audio/Sound effects/bash.mp3"')
    # Read the source identity predicates and player body layout as provenance.
    ex.text('Scripts/Main/party/party_object.gd');ex.text('Nodes/Reusables/Player.tscn');ex.text('Nodes/Reusables/npc.tscn');ex.text('Scripts/global/controlsManager.gd')
    ds=ex.text('Scripts/Main/Door.gd')
    scripts['door.gd']=ds[:ds.index('func _on_Door_body_entered')].replace('class_name Door','')+'\nvar runner\nvar global\nvar globaldata\nvar uiManager\n'+extract('Scripts/Main/Door.gd',['_on_Door_body_entered','enter','goto_player','_set_flag'])+'\nfunc change_scene():assert(false)\n'
    scripts['transition.gd']='extends Node\nvar global\nvar uiManager\nvar audioManager\nvar runner\n'+extract('Scripts/global/SceneTransition.gd',['start_door_transition','_update_party_from_door'])
    scripts['transition.gd']=scripts['transition.gd'].replace('load("res://Audio/Sound effects/" + door.sound)','"res://Audio/Sound effects/" + door.sound').replace('load("res://Audio/Sound effects/" + door.end_sound)','"res://Audio/Sound effects/" + door.end_sound')
    fs=ex.text('Nodes/Ui/effects/Fade.gd')
    scripts['fade.gd']=fs[:fs.index('func toggle_spin')]+'\nvar global\nvar runner\n'+extract('Nodes/Ui/effects/Fade.gd',['toggle_spin','set_spin','set_cut','focus_object','_recenter','set_color','init_cut','fade_in','fade_out','_on_fade_in_mostly_done','_on_fade_out_mostly_done'])
    cs=ex.text('Scripts/Main/CutsceneArea.gd')
    scripts['cutscene.gd']=cs[:cs.index('func _ready')]+'\nvar global\nvar globaldata\nvar uiManager\nvar runner\n'+extract('Scripts/Main/CutsceneArea.gd',re.findall(r'^func (\w+)\(',cs,re.M),False)
    scripts['cutscene.gd']=scripts['cutscene.gd'].replace('func _start_cutscene():','func _start_cutscene():\n\trunner.event("cutscene_request")')
    ps=ex.text('Scripts/Main/party/Player.gd');gate=ps[ps.index('\tif Input.is_action_just_pressed("ui_accept")'):ps.index('\n#emits a signal')]
    methods['Player.normal_interaction_gate']=hashlib.sha256(gate.encode()).hexdigest()
    scripts['player.gd']=PLAYER+extract('Scripts/Main/party/Player.gd',['pause','unpause','on_dialogue_done','is_paused','is_running','get_direction','_set_collision_masks','set_direction_and_input','set_direction','interact_with'],False)+'\nfunc check_normal_interaction():\n'+gate
    scripts['service.gd']=SERVICE+extract('Scripts/global/globalData.gd',['check_appear_disappear_flags'],False)+extract('Scripts/global/uiManager.gd',['add_ui','is_stack_empty','is_pause_menu_active','is_in_cutscene','open_dialogue_box','open_dialogue_box_and_unpause'])
    scripts.update({'probe.gd':PROBE,'audio.gd':AUDIO,'party.gd':'extends KinematicBody2D\n','room.gd':'extends Node2D\n','prompt.gd':'extends Sprite\nvar enabled=false\nfunc press_button():pass\n','dialogue_stub.gd':'extends Node\nsignal done(result)\nfunc start_from_id(id,_npc):\n\tget_tree().event("dialogue_start_from_id",{"id":id,"yaml_executed":false})\n'})
    house=ex.text('Maps/podunk/Nintens House.tscn')
    fixtures={'scene_name':re.search(r'^\[node name="([^"]+)" type="Node2D"\]',house,re.M)[1].replace("\\'","'"),'openable':node(house,'Below/Openable Door2'),'story':node(house,'Poltergeist/Cutscene Area2'),'doors':[]}
    for name in ['Upstair_Sister','Sister_Upstair']:
        p=node(house,'Doors/'+name);dest=node(house,'Doors/'+name+'/Position2D')
        fixtures['doors'].append(dict(name=name,position=p['position'],scale=p['scale'],dir=p['dir'],sound=p.get('sound','None'),end_sound=p.get('end_sound','None'),destination_local=dest['position']))
    ex.yaml('Data/Dialogue/Reusable/doorblocked.yaml');ex.yaml('Data/Dialogue/Podunk/cutscenes/doll_attack.yaml')
    oscene=ex.text('Nodes/Overworld/Objects/Openable Door.tscn').replace('res://Scripts/Main/Openable Door.gd','res://openable.gd').replace('res://Nodes/Ui/ButtonPrompt.tscn','res://prompt.tscn')
    oscene=oscene.replace('[gd_scene load_steps=12','[gd_scene load_steps=13').replace('[sub_resource type="RectangleShape2D" id=7]','[ext_resource path="res://audio.gd" type="Script" id=3]\n\n[sub_resource type="RectangleShape2D" id=7]')
    oscene=oscene.replace('[node name="AudioStreamPlayer" type="AudioStreamPlayer" parent="."]\nbus = "SFX"','[node name="AudioStreamPlayer" type="Node" parent="."]\nscript = ExtResource( 3 )')
    dscene=ex.text('Nodes/Overworld/Door.tscn').replace('res://Scripts/Main/Door.gd','res://door.gd')
    dscene=re.sub(r'\[ext_resource path="res://Audio/[^\n]+','[ext_resource path="res://audio.gd" type="Script" id=2]',dscene)
    dscene=dscene.replace('[node name="AudioStreamPlayer" type="AudioStreamPlayer" parent="."]\nstream = ExtResource( 2 )\nbus = "SFX"','[node name="AudioStreamPlayer" type="Node" parent="."]\nscript = ExtResource( 2 )')
    cscene=ex.text('Nodes/Reusables/CutsceneArea.tscn').replace('res://Scripts/Main/CutsceneArea.gd','res://cutscene.gd')
    fscene=ex.text('Nodes/Ui/effects/Fade.tscn').replace('res://Nodes/Ui/effects/Fade.gd','res://fade.gd')
    files={'Openable.tscn':oscene.encode(),'Door.tscn':dscene.encode(),'Cutscene.tscn':cscene.encode(),'Fade.tscn':fscene.encode(),'Shaders/Fade.shader':ex.data('Shaders/Fade.shader'),'prompt.tscn':b'[gd_scene load_steps=2 format=2]\n[ext_resource path="res://prompt.gd" type="Script" id=1]\n[node name="ButtonPrompt" type="Sprite"]\nscript = ExtResource( 1 )\n','dialogue_stub.tscn':b'[gd_scene load_steps=2 format=2]\n[ext_resource path="res://dialogue_stub.gd" type="Script" id=1]\n[node name="DialogueBoundary" type="Node"]\nscript = ExtResource( 1 )\n'}
    metadata={'commit':ex.lock['commit'],'sources':ex.sources,'methods_sha256':methods,'scope':'Mimmie blocked openable door, sister paired same-scene warps and Doll CutsceneArea request only; no Doll YAML execution/battle','adapters':['Source source-method tracing and local service variables; type annotations removed only for isolated project classes','PartyObject identity represented by minimal KinematicBody2D base; source Player octagon/ray/masks and pause/unpause functions retained','Movement stimulus uses native move_and_collide at selected walking/running speeds and is released on native paused signal; no claim to full Player input/run acceleration','Player animation, party follower, miscellaneous sound/timer side effects stubbed; native MiscTimer and pause collision masks retained','Openable, Door and Cutscene scene geometry/properties and Openable/Fade AnimationPlayer resources retained; ButtonPrompt UI substituted for enabled flag and press observation','Audio resource loads record source path; no sound hardware playback','uiManager extracted deferred add_ui, open_dialogue_box, open_dialogue_box_and_unpause methods; stub native Node ready logs start_from_id and never executes YAML or emits done','Unrelated house bodies excluded; source-specific player/nonplayer blocker mapping and native area callbacks independently checked'],'openable_action':animation(ex.text('Nodes/Overworld/Objects/Openable Door.tscn'),1,'Openable Door.tscn','Action'),'openable_normal':animation(ex.text('Nodes/Overworld/Objects/Openable Door.tscn'),2,'Openable Door.tscn','Normal')}
    work.mkdir(parents=True);reports.mkdir(parents=True)
    for name,code in scripts.items():(work/name).write_text(code)
    for name,data in files.items():
        p=work/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(data)
    for name,data in [('fixtures',fixtures),('metadata',metadata)]:(work/(name+'.json')).write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n')
    (work/'project.godot').write_text('config_version=4\n[application]\nconfig/name="Sister door native oracle"\n[display]\nwindow/size/width=320\nwindow/size/height=180\n[logging]\nfile_logging/enable_logging=false\n[physics]\ncommon/physics_fps=60\n')
    env=dict(os.environ)
    for key in ['XDG_DATA_HOME','XDG_CONFIG_HOME','XDG_CACHE_HOME']:
        d=work/key.lower();d.mkdir();env[key]=str(d)
    command=[str(args.godot.resolve()),'--path',str(work),'--fixed-fps','60','--script',str(work/'probe.gd'),'--encore-out='+str(reports/'reference.json')]
    with (reports/'godot.txt').open('wb') as f:result=subprocess.run(command,stdout=f,stderr=subprocess.STDOUT,env=env,timeout=45)
    log=(reports/'godot.txt').read_text();known='ERROR: VisualServer attempted to free a NULL RID.\n   at: free (servers/visual/visual_server_raster.cpp:69)\n';clean=log.replace(known,'')
    require(result.returncode==0 and 'ERROR:' not in clean and 'SCRIPT ERROR' not in log and 'SISTER_DOOR_REFERENCE_COMPLETE' in log,'Native oracle failed; inspect retained log')
    ref=json.loads((reports/'reference.json').read_text());require(ref['engine']['string']=='3.6.2-stable (official)','Wrong native engine')
    receipt={'commit':ex.lock['commit'],'engine_sha256':sha(args.godot),'tool_sha256':sha(__file__),'reference_sha256':sha(reports/'reference.json'),'generated_scripts_sha256':{p.name:sha(p) for p in work.glob('*.gd')},'scope':metadata['scope'],'headless_visual_null_rid_diagnostic_count':log.count(known),'headless_visual_diagnostic':'Exact official headless visual-resource null RID free diagnostic retained and counted; all other errors fail'}
    (reports/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
    (reports/'summary.json').write_text(json.dumps({'boundary_samples':ref['boundary_samples'],'cases':[{'name':c['name'],'events':c['events']} for c in ref['cases']]},indent=2)+'\n')
    print(json.dumps({'reports':str(reports),'cases':len(ref['cases'])},indent=2))
if __name__=='__main__':main()
