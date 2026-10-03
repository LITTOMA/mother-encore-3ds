#!/usr/bin/env python3
"""Bounded original house Door/NPC/dialogue oracle in official Godot 3.6.2.

Uses extracted source functions, native physics, scene transforms, AnimationPlayer,
RichTextLabel/font layout, Timer, signals and RNG. No encounter implementation or
emulator. Resource manifests identify every source and explicit adapter.
"""
from __future__ import annotations
import argparse,csv,hashlib,io,json,os,re,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from tools.extract_battle_entry import Extractor,require,node,animation
from tools.run_battle_victory_reference import function,sha

DOORS=['Ninten_upstairs','Upstairs_Ninten','Upstairs_Living','Living_Upstairs']
SERVICE='''extends Node
signal cutscene_ended
var runner
var player
var scene_transition
var currentScene
var currentCamera
var entering_door=false
var in_cutscene=false
var talker=null
var partySpace=[]
var partyObjects=[]
var musicChangers=[]
var flags={"poltergeist":true}
var seen_dialogue_flags={}
var text_speed=0.02
var fade
var stack=false
func get_player():return player
func get_fade():return fade
func is_in_cutscene():return in_cutscene
func is_in_battle():return false
func is_stack_empty():return !stack
func party_call(method,arg=null):
\trunner.event("party_call",{"method":method})
func close_commands_menu(_a,_b):runner.event("close_commands_menu")
func open_dialogue_box(id,_a=null,npc=null):
\tstack=true
\trunner.event("open_dialogue_box",{"id":id,"npc":str(npc.get_path()) if npc else ""})
func info_plates_hide():pass
func set_phone_location(_a):pass
func set_telepathy_effect(_a):pass
func update_key_indicator():pass
func get_cash_box():return self
func close():pass
func set_respawn():assert(false)
'''
AUDIO='''extends Node
var runner
var stream
func play():runner.event("audio_adapter",{"path":str(stream)})
func stop():pass
'''
SPRITE='''extends Node2D
var last_direction=Vector2.ZERO
var state="Idle"
func blend_position(dir):last_direction=dir
func travel(anim):state=anim
'''
PLAYER='''extends KinematicBody2D
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
func pause(_stop_running=false,_start_idle=true):
\t_paused=true
\t_input_vector=Vector2.ZERO
\t_set_collision_masks(false)
\trunner.event("player_pause_adapter")
func unpause():
\t_paused=false
\t_set_collision_masks(true)
\trunner.event("player_unpause_adapter")
func is_paused():return _paused
func _set_event_collider(_a):pass
func blend_position(_a):pass
func use_telepathy():assert(false)
func _physics_process(_delta):
\tif runner.trace_physics_order:runner.physics_order.append({"who":"player","physics_tick":runner.physics_tick})
\tcheck_normal_interaction()
'''
DIALOGUE='''extends "res://abstract.gd"
signal done(result)
var global
var uiManager
var _can_input=true
var _options=[]
var _options_count=0
var _options_grid
var _dialog_response=0
var _name_label
var ended=false
func _handle_phrase():assert(false)
func _add_dialog_options():assert(!_curr_phrase.has("options"))
func _next_phrase(_sound=false):
\tassert(!_curr_phrase.has("goto") and !_curr_phrase.has("redirect") and !_curr_phrase.has("options"))
\t_end_dialogue()
func _end_dialogue():
\tInput.action_release("ui_cancel")
\tInput.action_release("ui_accept")
\t_clear_dialogue()
\tif is_instance_valid(global.talker) and global.talker!=null:global.talker.stop_interaction()
\tglobal.talker=null
\tglobal.in_cutscene=false
\tuiManager.stack=false
\tended=true
\trunner.event("dialogue_end_adapter")
\temit_signal("done",0)
'''
PROBE='''extends SceneTree
var tick=0
var physics_tick=0
var start_tick=0
var events=[]
var cases=[]
var geometry=[]
var geometry_sweep=[]
var ap_samples=[]
var ray_cases=[]
var output
var fixtures
var metadata
var service
var player
var npc
var doors=[]
var fade
var root_world
var draws=[]
var mirror=RandomNumberGenerator.new()
var raw_count=0
var voice_count=0
var dialogue
var snapshots=[]
var frame_samples=[]
var trace_frames=false
var trace_physics_order=false
var physics_order=[]
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
func _idle(_delta):
\ttick+=1
\tif fade:
\t\tvar ap=fade._anim_player
\t\tap_samples.append({"frame":tick-start_tick,"physics_tick":physics_tick,"delta":"%.17f"%_delta,"position":"%.17f"%ap.current_animation_position if ap.assigned_animation!="" else "unset","animation":ap.assigned_animation,"playing":ap.is_playing()})
\tif trace_frames and dialogue:frame_samples.append(dialogue_state("idle"))
\treturn false
func _iteration(_delta):
\tphysics_tick+=1
\treturn false
func vec(v):return [v.x,v.y]
func event(name,data={}):
\tvar row=data.duplicate(true)
\trow.event=name
\trow.order=events.size()
\trow.frame=tick-start_tick
\trow.physics_tick=physics_tick
\tif fade and fade._anim_player.assigned_animation!="":row.animation_position="%.17f"%fade._anim_player.current_animation_position
\tif player:
\t\trow.player_position=vec(player.global_position)
\t\trow.player_direction=vec(player._direction)
\t\trow.paused=player._paused
\t\trow.player_collision_mask=player.collision_mask
\t\trow.entering=service.entering_door
\t\trow.player_camera_current=player.camera.current
\tevents.append(row)
func signal_event(name):event(name)
func new_node(type,name,parent):
\tvar n=ClassDB.instance(type)
\tn.name=name
\tparent.add_child(n)
\treturn n
func rectangle(parent,name,position,extents):
\tvar shape=new_node("CollisionShape2D",name,parent)
\tshape.position=position
\tshape.shape=RectangleShape2D.new()
\tshape.shape.extents=extents
\treturn shape
func setup_world():
\tservice=load("res://service.gd").new()
\tservice.runner=self
\tget_root().add_child(service)
\troot_world=new_node("Node2D",fixtures.scene_name,get_root())
\tservice.currentScene=root_world
\tvar objects=new_node("Node2D","Objects",root_world)
\tplayer=load("res://player.gd").new()
\tplayer.name="player"
\tplayer.runner=self
\tplayer.global=service
\tplayer.uiManager=service
\tplayer.collision_layer=1
\tplayer.collision_mask=4353
\tobjects.add_child(player)
\tvar poly=new_node("CollisionPolygon2D","CollisionShape2D",player)
\tpoly.position=Vector2(0,3)
\tpoly.polygon=PoolVector2Array([Vector2(6,4),Vector2(4,6),Vector2(-5,6),Vector2(-7,4),Vector2(-7,2),Vector2(-5,0),Vector2(4,0),Vector2(6,2)])
\tplayer.camera=new_node("Camera2D","Camera2D",player)
\tvar old_camera=new_node("Camera2D","OldCamera",root_world)
\told_camera.current=true
\tservice.currentCamera=old_camera
\tservice.player=player
\tvar ray=new_node("RayCast2D","EventDetector",player)
\tray.position=Vector2(0,6)
\tray.scale=Vector2(0.3,1)
\tray.enabled=true
\tray.cast_to=Vector2(0,16)
\tray.collide_with_areas=true
\tplayer.eventRayCaster=ray
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
\tget_root().add_child(service.scene_transition)
\tvar door_parent=new_node("Node2D","Doors",root_world)
\tfor f in fixtures.doors:
\t\tvar d=load("res://door.gd").new()
\t\td.name=f.name
\t\td.global=service
\t\td.uiManager=service
\t\td.globaldata=service
\t\td.runner=self
\t\td.position=Vector2(f.position[0],f.position[1])
\t\td.scale=Vector2(f.scale[0],f.scale[1])
\t\td.dir=Vector2(f.dir[0],f.dir[1])
\t\td.sound=f.sound
\t\td.end_sound=f.end_sound
\t\td.pause_mode=2
\t\td.collision_layer=256
\t\td.collision_mask=0
\t\td.monitorable=false
\t\tvar c=CollisionShape2D.new()
\t\tc.name="CollisionShape2D"
\t\tc.position=Vector2(f.shape_position[0],f.shape_position[1])
\t\tc.shape=RectangleShape2D.new()
\t\tc.shape.extents=Vector2(4,4)
\t\td.add_child(c)
\t\tvar pos=Position2D.new()
\t\tpos.name="Position2D"
\t\tpos.position=Vector2(f.destination_local[0],f.destination_local[1])
\t\td.add_child(pos)
\t\tvar aud=load("res://audio.gd").new()
\t\taud.runner=self
\t\taud.name="AudioStreamPlayer"
\t\td.add_child(aud)
\t\tdoor_parent.add_child(d)
\t\td.connect("body_entered",self,"door_enter",[d])
\t\td.connect("body_entered",d,"_on_Door_body_entered")
\t\td.connect("body_exited",self,"door_exit",[d])
\t\tfor sig in ["entered","moved_player","done"]:d.connect(sig,self,"signal_event",[d.name+":"+sig])
\t\tdoors.append(d)
\t\tgeometry.append({"door":d.name,"trigger_center":vec(c.global_position),"trigger_half_extents":vec(c.shape.extents*d.scale),"destination_marker":vec(pos.global_position),"player_destination":vec(pos.global_position-Vector2(0,7)),"direction":vec(d.dir)})
\tnpc=load("res://npc.gd").new()
\tnpc.name="npc"
\tnpc.global=service
\tnpc.globaldata=service
\tnpc.uiManager=service
\tnpc.runner=self
\tnpc.position=Vector2(192,704)
\tnpc.staring=true
\tnpc.initial_dir=Vector2.DOWN
\tnpc._input_vector=Vector2.DOWN
\tnpc._all_dialog=fixtures.dialogues
\tnpc.collision_layer=573
\tnpc.collision_mask=1597
\tnpc.character_sprite=load("res://sprite.gd").new()
\tnpc.add_child(npc.character_sprite)
\tobjects.add_child(npc)
\trectangle(npc,"CollisionShape2D",Vector2(-0.5,6),Vector2(7.5,3))
\tvar interact=new_node("Area2D","interact",npc)
\tinteract.position=Vector2(0,-10)
\trectangle(interact,"CollisionShape2D",Vector2(-0.5,15),Vector2(12.3,14))
\tvar view=new_node("Area2D","ViewArea",npc)
\tview.collision_layer=4096
\tview.collision_mask=4096
\tview.monitorable=false
\tvar view_shape=new_node("CollisionShape2D","CollisionShape2D2",view)
\tview_shape.position=Vector2(0,9)
\tview_shape.shape=CircleShape2D.new()
\tview_shape.shape.radius=44
\tview.connect("body_entered",npc,"_on_ViewArea_body_entered")
\tview.connect("body_exited",npc,"_on_ViewArea_body_exited")
\tview.connect("body_entered",self,"view_signal",["entered"])
\tview.connect("body_exited",self,"view_signal",["exited"])
\tobjects.move_child(player,objects.get_child_count()-1)
func view_signal(body,kind):
\tif body==player:event("npc_view_"+kind)
func door_enter(body,d):
\tif body==player:event(d.name+":body_entered")
func door_exit(body,d):
\tif body==player:event(d.name+":body_exited")
func finish_case(name):
\tcases.append({"name":name,"events":events.duplicate(true),"animation_samples":ap_samples.duplicate(true)})
\tap_samples=[]
\tevents=[]
\tstart_tick=tick
func random_range(label,low,high):
\tvar result=rand_range(low,high)
\tvar raw=[str(mirror.randi())]
\tif raw[0]!="0":
\t\traw.append(str(mirror.randi()))
\t\traw.append(str(mirror.randi()))
\traw_count+=raw.size()
\tdraws.append({"frame":tick-start_tick,"label":label,"value":"%.17f"%result,"raw":raw,"visible":dialogue._dialogue_label.visible_characters,"last_char":dialogue._get_last_visible_char()})
\treturn result
func dialogue_state(label):
\tvar d=dialogue
\tvar rich=d._dialogue_label
\treturn {"label":label,"frame":tick-start_tick,"visible":rich.visible_characters,"text":rich.text,"bbcode":rich.bbcode_text,"bullets":d._bullet_label.text,"segment":d._segment_num,"stopped":d._stopped,"finished":d._finished,"ended":d.ended,"speed_input":d._speed_multiplier_from_input,"t":"%.17f"%d._t,"talking":npc.talking,"line_count":rich.get_line_count(),"content_height":rich.get_content_height(),"scroll":rich.get_v_scroll().value,"bullet_scroll":d._bullet_label.get_v_scroll().value,"label_size":vec(rich.rect_size),"label_global_position":vec(rich.rect_global_position),"voice_rng":draws.size()}
func snap(label):snapshots.append(dialogue_state(label))
func action(name,pressed=true,echo=false):
\tvar ev=InputEventAction.new()
\tev.action=name
\tev.pressed=pressed
\tInput.parse_input_event(ev)
\tdialogue._input(ev)
func create_dialogue(with_voice):
\tdialogue=load("res://dialogue.gd").new()
\tdialogue.runner=self
\tdialogue.global=service
\tdialogue.globaldata=service
\tdialogue.uiManager=service
\tdialogue._bullet_string="[right]@ [/right]"
\tvar box=Control.new()
\tbox.name="Dialoguebox"
\tbox.rect_position=Vector2(28,120)
\tbox.rect_size=Vector2(264,60)
\tdialogue.add_child(box)
\tvar clip=new_node("Control","ClipBox",box)
\tclip.anchor_right=1
\tclip.margin_left=5
\tclip.margin_top=6
\tclip.margin_bottom=53
\tclip.rect_clip_content=true
\tvar hbox=new_node("HBoxContainer","HBoxContainer",clip)
\thbox.anchor_right=1
\thbox.margin_left=2
\thbox.margin_top=-13
\thbox.margin_right=-7
\thbox.margin_bottom=28
\thbox.add_constant_override("separation",0)
\tfor name in ["DippinDots","Dialogue"]:
\t\tvar rich=new_node("RichTextLabel",name,hbox)
\t\trich.rect_min_size=Vector2(10 if name=="DippinDots" else 235,60)
\t\tif name=="Dialogue":rich.size_flags_horizontal=3
\t\trich.bbcode_enabled=true
\t\trich.scroll_active=false
\t\trich.scroll_following=true
\t\trich.add_constant_override("line_separation",3)
\t\trich.add_font_override("normal_font",load("res://Fonts/EBMain_la.tres"))
\t\trich.add_font_override("mono_font",load("res://Fonts/EBMain_la.tres"))
\t\tif name=="Dialogue":dialogue._dialogue_label=rich
\t\telse:dialogue._bullet_label=rich
\tdialogue._cursor_down_sprite=new_node("AnimatedSprite","Cursor_Down",box)
\tvar arrow=load("res://arrow.gd").new()
\tarrow.name="Arrow"
\tbox.add_child(arrow)
\tdialogue._options_grid=new_node("GridContainer","Options",box)
\tvar timer=new_node("Timer","WaitTimer",dialogue)
\ttimer.one_shot=true
\tvar anim=new_node("AnimationPlayer","AnimationPlayer",dialogue)
\tvar opened=Animation.new()
\topened.length=.3
\tanim.add_animation("Open",opened)
\tvar input_audio=new_node("AudioStreamPlayer","InputSound",dialogue)
\tvar audio=new_node("AudioStreamPlayer","AudioStreamPlayer",dialogue)
\tif with_voice:
\t\tvar file=File.new()
\t\tassert(file.open("res://female.mp3",File.READ)==OK)
\t\tvar mp3=AudioStreamMP3.new()
\t\tmp3.data=file.get_buffer(file.get_len())
\t\taudio.stream=mp3
\tget_root().add_child(dialogue)
\tdialogue.set_physics_process(false)
\tdialogue.set_process_input(false)
\tservice.talker=npc
\tservice.in_cutscene=true
\tservice.stack=true
\tInput.action_release("ui_cancel")
\tInput.action_release("ui_accept")
\tdialogue._curr_phrase={"text":load("res://text.gd").add_line_breaks(fixtures.replaced_text,dialogue._dialogue_label)}
\tdialogue._finished=false
\tdialogue._print_dialogue_segment(true)
\tdialogue._dialogue_label.visible_characters=0
\tanim.play("Open")
func run():
\tyield(self,"idle_frame")
\tInputMap.add_action("ui_toggle")
\tsetup_world()
\tvar translation=Translation.new()
\ttranslation.locale="en"
\ttranslation.add_message("WORD_SEPARATOR"," ")
\tTranslationServer.add_translation(translation)
\tTranslationServer.set_locale("en")
\tplayer.position=Vector2(450,390)
\tfor _i in range(4):yield(self,"physics_frame")
\tfor idx in range(doors.size()):
\t\tvar d=doors[idx]
\t\tplayer._paused=false
\t\tplayer._set_collision_masks(true)
\t\tplayer.position=Vector2(600,900)
\t\tfor _i in range(3):yield(self,"physics_frame")
\t\tstart_tick=tick
\t\tevents=[]
\t\tap_samples=[]
\t\tvar target=d.get_node("CollisionShape2D").global_position-Vector2(0,6)
\t\tplayer.position=target
\t\tvar start=tick
\t\twhile !service.entering_door and tick-start<20:yield(self,"idle_frame")
\t\tassert(service.entering_door)
\t\td.enter(player)
\t\tevent("reentry_attempt_while_guarded")
\t\twhile service.entering_door:yield(self,"idle_frame")
\t\tfor _i in range(30):yield(self,"idle_frame")
\t\tassert(player.position==d.get_node("Position2D").global_position-Vector2(0,7))
\t\tfinish_case(d.name)

\t# Geometry-only overlap boundary probe; source callback disconnected explicitly.
\tfor d in doors:d.disconnect("body_entered",d,"_on_Door_body_entered")
\tfor sample in fixtures.boundaries:
\t\tvar d=doors[int(sample.door)]
\t\tplayer.position=Vector2(600,900)
\t\tfor _i in range(3):yield(self,"physics_frame")
\t\tplayer.position=Vector2(sample.position[0],sample.position[1])
\t\tfor _i in range(3):yield(self,"physics_frame")
\t\tgeometry_sweep.append({"door":d.name,"player":vec(player.position),"overlap":d.overlaps_body(player)})
\tfinish_case("geometry_boundary_sweep_callback_disconnected")
\t# Direct enter in an existing cutscene exhibits source latch-before-guard.
\tservice.in_cutscene=true
\tdoors[0].enter(player)
\tassert(service.entering_door)
\tevent("cutscene_guard_latches_entering")
\tservice.in_cutscene=false
\tservice.entering_door=false
\tfinish_case("cutscene_guard")
\t# Actual physics ray, body and Area2D interaction geometry.
\tfor sample in fixtures.rays:
\t\tplayer.position=Vector2(sample.position[0],sample.position[1])
\t\tplayer.set_direction_and_input(Vector2(sample.direction[0],sample.direction[1]))
\t\tfor _i in range(2):yield(self,"physics_frame")
\t\tplayer.eventRayCaster.force_raycast_update()
\t\tvar collider=player.eventRayCaster.get_collider()
\t\tray_cases.append({"name":sample.name,"player":vec(player.position),"direction":vec(player._direction),"origin":vec(player.eventRayCaster.global_position),"endpoint":vec(player.eventRayCaster.to_global(player.eventRayCaster.cast_to)),"collider":str(collider.get_path()) if collider else "","hit":vec(player.eventRayCaster.get_collision_point()) if collider else []})
\t# Normal A source gate, held A does not create new just-press.
\tplayer.position=Vector2(192,730)
\tplayer.set_direction_and_input(Vector2.UP)
\tfor _i in range(3):yield(self,"physics_frame")
\tplayer.eventRayCaster.force_raycast_update()
\tassert(npc.has_dialog())
\tassert(service.seen_dialogue_flags.empty())
\ttrace_physics_order=true
\tInput.action_press("ui_accept")
\tfor _i in range(2):yield(self,"physics_frame")
\tevent("normal_a_state",{"seen":service.seen_dialogue_flags.duplicate(),"npc_direction":vec(npc._input_vector),"held":Input.is_action_pressed("ui_accept")})
\ttrace_physics_order=false
\tevent("npc_before_dynamic_player",{"native_order":physics_order.duplicate(true)})
\tassert(service.seen_dialogue_flags.size()==1)
\tInput.action_release("ui_accept")
\tnpc.stop_interaction()
\tservice.stack=false
\tfinish_case("normal_a_and_seen_tracking")
\t# Staring callbacks and original native return timer.
\tnpc._on_ViewArea_body_entered(player)
\tplayer.position=Vector2(202,734)
\tnpc._physics_process(1.0/60.0)
\tevent("staring_direction",{"direction":vec(npc._input_vector)})
\tnpc._on_ViewArea_body_exited(player)
\tfor _i in range(63):yield(self,"idle_frame")
\tevent("staring_return",{"direction":vec(npc._input_vector)})
\tassert(npc._input_vector==Vector2.DOWN)
\tfinish_case("staring_native_timer")
\tfor with_voice in [true,false]:
\t\tstart_tick=tick
\t\tdraws=[]
\t\traw_count=0
\t\tsnapshots=[]
\t\tframe_samples=[]
\t\tseed(123)
\t\tmirror.seed=123
\t\tcreate_dialogue(with_voice)
\t\tfor _i in range(2):yield(self,"idle_frame")
\t\ttrace_frames=true
\t\tsnap("started")
\t\taction("ui_cancel")
\t\tsnap("cancel_during_open_ignored")
\t\tassert(dialogue._speed_multiplier_from_input==1)
\t\twhile dialogue.get_node("AnimationPlayer").is_playing():
\t\t\tdialogue._advance_printing(1.0/60.0)
\t\t\tyield(self,"idle_frame")
\t\t# B speeds printing; its held state alone cannot release a WAIT.
\t\taction("ui_cancel")
\t\tsnap("cancel_speeds_printing")
\t\tdialogue._advance_printing(1.0/60.0)
\t\tsnap("wait_1")
\t\tassert(dialogue._stopped)
\t\tfor _i in range(4):
\t\t\tdialogue._advance_printing(1.0/60.0)
\t\t\tyield(self,"idle_frame")
\t\tsnap("held_b_at_wait")
\t\tassert(dialogue._stopped)
\t\t# ui_toggle never confirms WAIT, A and B do. No cancellation of dialogue.
\t\taction("ui_toggle")
\t\tassert(dialogue._stopped)
\t\taction("ui_cancel",false)
\t\taction("ui_accept")
\t\tsnap("accept_resumes_wait")
\t\tassert(!dialogue._stopped and dialogue._speed_multiplier_from_input==1)
\t\taction("ui_accept",false)
\t\taction("ui_accept")
\t\tsnap("accept_speeds_printing")
\t\tassert(dialogue._speed_multiplier_from_input==3)
\t\twhile !dialogue._stopped:
\t\t\tdialogue._advance_printing(1.0/60.0)
\t\t\tyield(self,"idle_frame")
\t\tsnap("wait_2")
\t\taction("ui_cancel")
\t\tassert(!dialogue._stopped)
\t\taction("ui_cancel",false)
\t\taction("ui_cancel")
\t\tdialogue._advance_printing(1.0/60.0)
\t\tsnap("wait_3")
\t\tassert(dialogue._stopped)
\t\taction("ui_accept")
\t\taction("ui_cancel")
\t\tdialogue._advance_printing(1.0/60.0)
\t\tfor _i in range(2):yield(self,"idle_frame")
\t\tsnap("finished_waiting_confirmation")
\t\tassert(dialogue._finished and !dialogue.ended)
\t\taction("ui_toggle")
\t\tassert(!dialogue.ended)
\t\taction("ui_cancel",false)
\t\taction("ui_cancel")
\t\tsnap("cancel_confirms_completion")
\t\tassert(dialogue.ended and !Input.is_action_pressed("ui_accept") and !Input.is_action_pressed("ui_cancel"))
\t\ttrace_frames=false
\t\tvar next_actual=str(randi())
\t\tvar next_expected=str(mirror.randi())
\t\tassert(next_actual==next_expected)
\t\tcases.append({"name":"carol_native_dialogue_voice_present" if with_voice else "carol_native_dialogue_null_stream_control","snapshots":snapshots.duplicate(true),"frames":frame_samples.duplicate(true),"draws":draws.duplicate(true),"raw_rng_count":raw_count,"next_randi":next_actual,"expected_next_randi":next_expected,"events":events.duplicate(true)})
\t\tdialogue.free()
\t\tdialogue=null
\t\tevents=[]
\tvar f=File.new()
\tassert(f.open(output,File.WRITE)==OK)
\tf.store_string(JSON.print({"schema":1,"engine":Engine.get_version_info(),"metadata":metadata,"geometry":geometry,"geometry_sweep":geometry_sweep,"ray_cases":ray_cases,"cases":cases},"  "))
\tf.close()
\tnpc.get_node("ViewArea").disconnect("body_entered",self,"view_signal")
\tnpc.get_node("ViewArea").disconnect("body_exited",self,"view_signal")
\tprint("HOUSE_INTERACTION_REFERENCE_COMPLETE")
\tquit()
'''

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--godot',type=Path,required=True);ap.add_argument('--work',type=Path,required=True);ap.add_argument('--reports',type=Path,required=True)
    a=ap.parse_args();work=a.work.resolve();reports=a.reports.resolve()
    require(not work.exists() and work.is_relative_to(ROOT/'build/house-interaction-reference'),'Fresh build/house-interaction-reference work required')
    require(not reports.exists() and reports.is_relative_to(ROOT/'reports/house-interaction-reference'),'Fresh reports/house-interaction-reference output required')
    ex=Extractor(ROOT);methods={};scripts={}
    def extract(path,names,trace=False):
        src=ex.text(path);result=''
        for name in names:
            fn=function(src,name);methods[path+':'+name]=hashlib.sha256(fn.encode()).hexdigest()
            fn=fn.replace(':=','=')
            fn=re.sub(r':\s*(Door|PartyMemberPlayer)(?=\s*[,)=\n])','',fn)
            if trace:fn=fn.replace('\n','\n\trunner.event("source_enter",{"method":'+json.dumps(path+':'+name)+'})\n',1)
            result+='\n'+fn
        return result
    ds=ex.text('Scripts/Main/Door.gd');scripts['door.gd']=ds[:ds.index('func _on_Door_body_entered')].replace('class_name Door','')+'\nvar runner\nvar global\nvar globaldata\nvar uiManager\n'+extract('Scripts/Main/Door.gd',['_on_Door_body_entered','enter','goto_player','_set_flag'],True)+'\nfunc change_scene():assert(false)\n'
    scripts['transition.gd']='extends Node\nvar global\nvar uiManager\nvar audioManager\nvar runner\n'+extract('Scripts/global/SceneTransition.gd',['start_door_transition','_update_party_from_door'],True)
    scripts['transition.gd']=scripts['transition.gd'].replace('load("res://Audio/Sound effects/" + door.sound)','"res://Audio/Sound effects/" + door.sound').replace('load("res://Audio/Sound effects/" + door.end_sound)','"res://Audio/Sound effects/" + door.end_sound')
    fs=ex.text('Nodes/Ui/effects/Fade.gd');scripts['fade.gd']=fs[:fs.index('func toggle_spin')]+ '\nvar global\nvar runner\n'+extract('Nodes/Ui/effects/Fade.gd',['toggle_spin','set_spin','set_cut','focus_object','_recenter','set_color','init_cut','fade_in','fade_out','_on_fade_in_mostly_done','_on_fade_out_mostly_done'],True)
    ns=ex.text('Scripts/Main/npc.gd');decl=ns[:ns.index('func _ready')].replace('tool\n','')
    decl=re.sub(r'onready var .*=.*\n','',decl).replace(' setget _set_engine_sprite_offset','').replace(' setget _set_engine_sprite','')
    scripts['npc.gd']=decl+'\nvar character_sprite\nvar global\nvar globaldata\nvar uiManager\nvar runner\n'+extract('Scripts/Main/npc.gd',['_physics_process','has_dialog','_get_right_dialog','interact','stop_interaction','_on_ViewArea_body_entered','_on_ViewArea_body_exited','return_to_init_dir'])
    scripts['npc.gd']=scripts['npc.gd'].replace('func _physics_process(delta: float):','func _physics_process(delta: float):\n\tif runner.trace_physics_order:runner.physics_order.append({"who":"npc","physics_tick":runner.physics_tick})').replace('func return_to_init_dir():','func return_to_init_dir():\n\trunner.event("npc_return_timer")')
    ex.text('Scripts/global/global.gd')
    ps=ex.text('Scripts/Main/party/Player.gd');gate=ps[ps.index('\tif Input.is_action_just_pressed("ui_accept")'):ps.index('\n#emits a signal')]
    methods['Player.normal_interaction_gate']=hashlib.sha256(gate.encode()).hexdigest()
    scripts['player.gd']=PLAYER+extract('Scripts/Main/party/Player.gd',['_set_collision_masks','set_direction_and_input','set_direction','interact_with'])+'\nfunc check_normal_interaction():\n'+gate
    abstract=ex.text('Scripts/UI/AbstractDialogueBox.gd').replace('class_name AbstractDialogueBox\n','')
    abstract=abstract.replace('extends CanvasLayer','extends CanvasLayer\nconst TextTools=preload("res://text.gd")\nvar globaldata\nvar runner')
    abstract=abstract.replace('rand_range(0.85, 1.0)','runner.random_range("AbstractDialogueBox.voice",0.85,1.0)')
    scripts['abstract.gd']=abstract
    scripts['dialogue.gd']=DIALOGUE+extract('Scripts/UI/DialogueBox.gd',['_action_press','_advance_printing','_finish_phrase','_clear_dialogue'])
    textsrc=ex.text('Scripts/global/text_tools.gd');scripts['text.gd']='extends Reference\n'+ '\n'.join(x for x in textsrc.splitlines() if x.startswith('const CHAR_'))+'\n'+extract('Scripts/global/text_tools.gd',['add_line_breaks','strip_bbcode','_tr'])
    scripts.update({'service.gd':SERVICE,'audio.gd':AUDIO,'sprite.gd':SPRITE,'probe.gd':PROBE,'arrow.gd':'extends Node2D\nvar cursor_index=0\n'})
    house=ex.text('Maps/podunk/Nintens House.tscn');door_scene=ex.text('Nodes/Overworld/Door.tscn');npc_scene=ex.text('Nodes/Reusables/npc.tscn');player_scene=ex.text('Nodes/Reusables/Player.tscn');dialogue_scene=ex.text('Nodes/Ui/DialogueBox.tscn')
    doors=[]
    for name in DOORS:
        p=node(house,'Doors/'+name);dest=node(house,'Doors/'+name+'/Position2D')
        shape=node(house,'Doors/'+name+'/CollisionShape2D') if name=='Upstairs_Ninten' else node(door_scene,'CollisionShape2D')
        doors.append(dict(name=name,position=p['position'],scale=p['scale'],dir=p['dir'],sound=p.get('sound','None'),end_sound=p.get('end_sound','None'),shape_position=shape['position'],destination_local=dest['position']))
    carol=node(house,'Objects/npc');rows=list(csv.DictReader(io.StringIO(ex.text('Translations/TranslatedText/dialogue_Podunk - sheet.csv'))));row=next(r for r in rows if list(r.values())[0]=='DIALOGUE_PODUNK_CAROL_PANIC_0');english=row['en']
    require(english.count('[WAIT@]')==3,'Unexpected Carol WAIT count')
    replaced=english.replace('[Ninten]','Ninten').replace('[WAIT@]','\u2063\n\u2064').replace('[@]','\u2064')
    require('[' not in replaced,'Unreviewed Carol text tag')
    ys=ex.yaml('Data/Dialogue/Podunk/carol_panic.yaml');require(set(ys)=={'0'} and set(ys['0'])=={'name','sound','text'} and ys['0']['sound']=='Female','Unreviewed Carol phrase')
    scene_name=re.search(r'^\[node name="([^"]+)" type="Node2D"\]',house,re.M)[1].replace("\\'","'")
    fixtures={'scene_name':scene_name,'boundaries':[{'door':i,'position':xy} for i,positions in enumerate([[[x,397] for x in [429,428,427,426]],[[x,390] for x in [222,223,224,225]],[[x,409] for x in [153,152,151,150]],[[32,y] for y in [679,678,677,676]]]) for xy in positions],'doors':doors,'dialogues':[['',carol['dialog']]]+carol['_all_dialog'],'english':english,'replaced_text':replaced,'rays':[]}
    for name,pos,di in [('south_toward',[192,732],[0,-1]),('south_away',[192,732],[0,1]),('south_edge',[192,733],[0,-1]),('south_outside',[192,734],[0,-1]),('north_toward',[192,674],[0,1]),('east_toward',[219,704],[-1,0]),('west_toward',[164,704],[1,0]),('diagonal',[216,728],[-1,-1])]:fixtures['rays'].append(dict(name=name,position=pos,direction=di))
    fade_scene=ex.text('Nodes/Ui/effects/Fade.tscn').replace('res://Nodes/Ui/effects/Fade.gd','res://fade.gd')
    files={'Fade.tscn':fade_scene.encode(),'Shaders/Fade.shader':ex.data('Shaders/Fade.shader'),'female.mp3':ex.data('Audio/Sound effects/text/Female.mp3')}
    for name in ['EBMain_la.tres','EBMain_ko.ttf','EBMain_fw9.ttf','EBMain_jaM3.ttf','EBMain.ttf','EBMain_zh_cn.ttf']:files['Fonts/'+name]=ex.data('Fonts/'+name)
    metadata={'commit':ex.lock['commit'],'sources':ex.sources,'methods_sha256':methods,'scope':'Four same-scene house warps, Carol NPC interaction and one carol_panic phrase; no encounters','adapters':['Player pause/unpause records state and executes exact collision-mask function; player movement, animation and party follower systems excluded','Door AudioStream resource loading/playback records paths; door source ordering preserved','Carol NPC declaration/setup initializes source scene data; sprite blend/travel recording node; wandering false; ViewArea return timer source method is native','Dialogue setup limited to Carol one-phrase no-options schema; translation/tag substitution is a checked English/Ninten fixture; add_line_breaks and RichTextLabel/font layout are native','Dialogue _action_press/printing/finish are extracted source; normal no-goto end path is adapted for isolated UI cleanup; scene removal/camera-return tween omitted','Dialogue physical processing manually stepped at 1/60 while real idle AnimationPlayer controls initial input gate; native input events forwarded once explicitly','Female MP3 is real AudioStreamMP3 with native play calls and native rand_range; headless audio has no audible hardware output. Null-stream case is a missing-resource control, not a missing-hardware simulation','Root scene uses exact source node names; unrelated game systems including blink/shaker RNG excluded, source voice RNG stream seeded123 and checked against native RandomNumberGenerator','Player is moved after Carol under Objects as global._init_player/SceneTransition append it in source; native callback order logged. ViewArea/Timer are native; explicit callback timer case is separately labeled'],'fade_in':animation(ex.text('Nodes/Ui/effects/Fade.tscn'),7,'Nodes/Ui/effects/Fade.tscn','Fade In'),'fade_out':animation(ex.text('Nodes/Ui/effects/Fade.tscn'),8,'Nodes/Ui/effects/Fade.tscn','Fade Out')}
    work.mkdir(parents=True);reports.mkdir(parents=True)
    for name,code in scripts.items():(work/name).write_text(code)
    for name,data in files.items():p=work/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(data)
    for name,data in [('fixtures',fixtures),('metadata',metadata)]:(work/(name+'.json')).write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n')
    (work/'project.godot').write_text('config_version=4\n[application]\nconfig/name="House native oracle"\n[display]\nwindow/size/width=320\nwindow/size/height=180\n[logging]\nfile_logging/enable_logging=false\n[physics]\ncommon/physics_fps=60\n')
    env=dict(os.environ)
    for key in ['XDG_DATA_HOME','XDG_CONFIG_HOME','XDG_CACHE_HOME']:
        d=work/key.lower();d.mkdir();env[key]=str(d)
    command=[str(a.godot.resolve()),'--path',str(work),'--fixed-fps','60','--script',str(work/'probe.gd'),'--encore-out='+str(reports/'reference.json')]
    with (reports/'godot.txt').open('wb') as f:result=subprocess.run(command,stdout=f,stderr=subprocess.STDOUT,env=env,timeout=45)
    log=(reports/'godot.txt').read_text();known='ERROR: VisualServer attempted to free a NULL RID.\n   at: free (servers/visual/visual_server_raster.cpp:69)\n';clean=log.replace(known,'');require(result.returncode==0 and 'ERROR:' not in clean and 'SCRIPT ERROR' not in log and 'HOUSE_INTERACTION_REFERENCE_COMPLETE' in log,'Native house oracle failed; inspect retained log')
    ref=json.loads((reports/'reference.json').read_text());require(ref['engine']['string']=='3.6.2-stable (official)','Wrong native engine')
    receipt={'commit':ex.lock['commit'],'engine_sha256':sha(a.godot),'tool_sha256':sha(__file__),'reference_sha256':sha(reports/'reference.json'),'generated_scripts_sha256':{p.name:sha(p) for p in work.glob('*.gd')},'scope':metadata['scope'],'headless_visual_null_rid_diagnostic_count':log.count(known),'headless_visual_diagnostic':'Official headless renderer emits null-RID free diagnostics for visual resources; exact diagnostic retained and counted. Script/assert/other engine errors fail.'}
    (reports/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
    summary={'geometry_sweep':ref['geometry_sweep'],'geometry':ref['geometry'],'ray_cases':ref['ray_cases'],'cases':[{k:v for k,v in c.items() if k not in ['frames','draws','animation_samples']} for c in ref['cases']]}
    (reports/'summary.json').write_text(json.dumps(summary,ensure_ascii=False,indent=2)+'\n')
    print(json.dumps({'reports':str(reports),'cases':len(ref['cases']),'geometry':ref['geometry']},indent=2))
if __name__=='__main__':main()
