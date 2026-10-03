#!/usr/bin/env python3
"""Run bounded original Lamp victory methods in official Godot 3.6.2.

Extracted-source oracle with native timers, signals, AnimationPlayers and jump
Tween. Dialogue glyph printing, audio, world actors/camera and missing gameplay
systems have explicit, observable adapters. Not a full-game or GPU reference.
"""
from __future__ import annotations
import argparse
import csv
import hashlib
import io
import json
import os
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from tools.extract_battle_entry import Extractor, require

SYSTEM = 'Scripts/UI/Battle/BattleSystem.gd'
METHODS = ['_win', '_reset_actions', '_get_conscious', '_activate_on_screen_enemies',
 '_set_bp_end_battle', '_get_exp_dialog', '_give_exp', '_do_rewards', '_give_cash',
 '_end_battle_to_overworld', '_turn_party_to_overworld', '_hide_battle_BG',
 '_hide_enemies', '_jump_to_overworld', '_jump_npcs_to_overworld',
 '_rotate_party_to_original_direction']


def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def function(source, name):
    found = re.findall(r'^(?:static )?func ' + re.escape(name) + r'\([^\n]*\n.*?(?=^(?:static )?func |\Z)', source, re.M | re.S)
    require(len(found) == 1, 'Missing/ambiguous source method ' + name)
    return found[0].rstrip() + '\n'


def adapted(source, label, trace=True):
    source = re.sub(r':\s*(BattleParticipant|Item|FormatContext)(?=\s*[,)=\n])', '', source)
    source = re.sub(r'->\s*(BattleParticipant|FormatContext)\s*:', ':', source)
    source = source.replace(':=', '=')
    # Static progression methods cannot reference an instance logger.
    if source.startswith('static '): return source
    source = source.replace('get_tree().create_timer(', 'runner.native_timer("' + label + '", ')
    source = re.sub(r'\brandi\(\)', 'runner.random_i("' + label + '")', source)
    source = re.sub(r'\brand_range\(([^\n()]*)\)', r'runner.random_range("' + label + r'", \1)', source)
    if not trace: return source
    source = re.sub(r'^(\s*)(if [^\n]+|else): (yield\([^\n]+)$', r'\1\2:\n\1\t\3', source, flags=re.M)
    lines = source.splitlines()
    result = [lines[0], '\trunner.event("enter", {"method":"' + label + '"})']
    for lineno, line in enumerate(lines[1:], 2):
        indent = line[:len(line)-len(line.lstrip())]
        if 'yield(' in line and not line.lstrip().startswith('#'):
            result.append(indent+'runner.event("before_yield", {"method":"'+label+'","line":'+str(lineno)+'})')
        result.append(line)
        if 'yield(' in line and not line.lstrip().startswith('#'):
            result.append(indent+'runner.event("after_yield", {"method":"'+label+'","line":'+str(lineno)+'})')
    return '\n'.join(result)+'\n'


BATTLE = '''extends Node
signal battle_to_ov
signal battle_ended(result)
var runner
var global
var globaldata
var audioManager
var uiManager
var _party_BPs=[]
var _npc_BPs=[]
var _party_orig_objects=[]
var _party_orig_dirs=[]
var _party_orig_positions=[]
var _party_info
var _battle_bg
var _current_action=null
var _current_action_index=1
var _doing_actions=true
var _active=true
var _is_boss=false
var _lose_battle=false
var _exp_pool=0
var _cash_pool=0
var _received_exp_from_flee=false
var _item_pool
var _stolen_item=null
var _stolen_item_definitive=false
var _win_flag=""
var _context
var _post_battle_cutscenes={}
const PartyMember=preload("res://character.gd")
const DroppedItemNode=preload("res://unused.tscn")
enum Result { LOSE=-1, FLEE, WIN }
func _set_encore_active(a,b):
\tassert(!a and !b)
\trunner.event("encore_off")
func _level_up_or_learn_skills(_a,_b,_c):
\tassert(false)
func _remove_battle_music():
\trunner.event("remove_battle_music_adapter")
func _drop_item_to_overworld(items):
\tassert(items.empty())
\trunner.event("empty_drop_coroutine_adapter")
func _revive_party():
\tassert(false)
'''

CHARACTER = '''extends Reference
var runner
var _exp=0
var _level=1
var hp=62
var pp=26
var data={}
var _learned_skills=[]
var PLAYER_STAT_TARGET_TABLE={"ninten":{}}
var PLAYER_LEARN_SKILL_TABLE={}
var PLAYER_LEARN_SKILL_TABLE_FLAGS={}
var globaldata
func get_name():return "ninten"
func get_level():return _level
func get_data():return data
func get_hp():return hp
func get_pp():return pp
func set_hp(value):
\thp=int(value)
\trunner.event("character_hp",{"value":hp})
func set_pp(value):
\tpp=int(value)
\trunner.event("character_pp",{"value":pp})
func get_status_ailments():return []
func _get_stat_for_level(_a,_b):
\tassert(false)
\treturn 0
func get_base_stat(_a):
\tassert(false)
\treturn 0
func set_stat(_a,_b):assert(false)
func _learn_new_skills(_a,_b):assert(false)
'''

PARTICIPANT = '''extends Reference
var runner
var character
var _party_info
var sprite
var _hp_stopped_scrolling_cb=null
var _stat_mods={"offense":3}
var kind=0
class Character:
\tenum Type { PARTY_MEMBER, PARTY_NPC, ENEMY }
func is_type(t):return kind==t
func is_unconscious():return false
func can_act():return true
func is_boss():return false
func get_id():return "lamp" if kind==2 else "ninten"
func get_name():return "Lamp" if kind==2 else "Ninten"
func get_sprite():return sprite
func get_plate():return _party_info
func set_status(_a,_b):assert(false)
'''

PLATE = '''extends Control
signal hp_scroll_done
signal pp_scroll_done
var runner
var _cur_hp=62
var _target_hp=62
var _dhp_frame=0
var _cur_pp=26
var _target_pp=26
var _dpp_frame=0
var _max_hp=62
var _are_hp_scrolling=true
var _are_hp_increasing=false
var _ones_digit_hp
var _tens_digit_hp
var _huns_digit_hp
func _hide_leading_zeros(_a,_b):pass
'''

DIGIT = '''extends Reference
func set_value(_a,_b=0):pass
'''

SPRITE = '''extends Node2D
var runner
enum States { HIDDEN, SHOWN }
var state=States.HIDDEN
func show_in():
\tstate=States.SHOWN
\trunner.event("sprite_show_in_adapter")
func play(anim,_loop):
\trunner.event("sprite_animation_adapter",{"animation":anim})
func hide_away():
\tstate=States.HIDDEN
\trunner.event("sprite_hide_away_adapter")
'''

DIALOG_BASE = '''extends Node
var runner
var _finished=true
var _stopped=false
var _auto_advance=true
var _curr_phrase={}
var _phrase_num="0"
var _speed_multiplier_from_input=1
const SPEED_UP_FROM_PRESS_A=2
const SPEED_UP_FROM_PRESS_B=4
func _handle_phrase():assert(false)
func _end_dialogue():assert(false)
'''

DIALOG = '''extends "res://dialog_base.gd"
signal done
var _dialogue_box_node
var _cursor_down_sprite
var _is_waiting_between_phrases=false
var _t=0
var pending=""
var fixture_text=""
var context
func start_from_string(text):
\t$AnimationPlayer.play("RESET")
\t_dialogue_box_node.show()
\tpending="exp"
\t_finished=false
\t_curr_phrase={"text":text}
\trunner.event("dialogue_started_adapter",{"text":text,"auto_advance":_auto_advance})
\tyield(self,"done")
func format_battle_text(key,ctx):
\tassert(key=="BATTLE_MSG_EXP_ONE_ALLY")
\tvar out=fixture_text.format({"name":ctx.actor.get_name(),"value":ctx.value})
\trunner.event("format_exp_dialogue",{"key":key,"value":ctx.value,"text":out})
\treturn out
func _end_dialogue():
\trunner.event("dialogue_done_adapter",{"pending":pending})
\tpending=""
\t_finished=true
\t_dialogue_box_node.hide()
\temit_signal("done")
func append(_text):assert(false)
func append_formatted(_a,_b,_c=""):assert(false)
func start_from_appended():assert(false)
'''

CONTEXT = '''extends Reference
var actor
var value=0
func set_actor(a):
\tactor=a
\treturn self
func set_value(v):
\tvalue=v
\treturn self
func set_targets(_a):assert(false)
func set_item_or_skill(_a):assert(false)
'''

GLOBAL = '''extends Node
signal flags_updated
var runner
var party=[]
var player
var currentCamera
func get_player():return player
'''

GLOBALDATA = '''extends Reference
var runner
var global
var bank=0
var cash=0
var earned_cash=0
var flags={"poltergeist":false,"earned_cash":false}
var rare_drops={}
'''

UI = '''extends Reference
signal battle_to_ov
var runner
var globaldata
var _in_battle=true
var _battle_ui
func get_on_screen_enemies():return []
func remove_ui(node):
\trunner.event("remove_ui_adapter",{"node":node.name})
func open_dialogue_box_and_unpause(_a):assert(false)
'''

AUDIO = '''extends Reference
var runner
var overworldBattleMusic=true
func pause_all_music():runner.event("audio_pause_all_adapter")
func add_audio_player():runner.event("audio_add_player_adapter")
func play_music_on_latest_player(intro,main):runner.event("audio_play_adapter",{"intro":intro,"main":main})
func resume_all_music():runner.event("audio_resume_all_adapter")
func get_audio_player(_index):return null
func music_fadein(_a,_b,_c):assert(false)
'''

ACTOR = '''extends Node2D
var runner
var sprite
var paused=true
var _direction=Vector2(1,0)
const MOVE=0
var _state=MOVE
func unpause():
\tpaused=false
\trunner.event("player_unpause",{"direction":[_direction.x,_direction.y]})
func set_direction(value):
\t_direction=value
\trunner.event("world_direction_adapter",{"direction":[value.x,value.y]})
func set_idle():runner.event("world_idle_adapter")
func set_anim_state(state):runner.event("world_animation_state_adapter",{"state":state})
func blend_position(value):
\trunner.event("world_blend_adapter",{"direction":[value.x,value.y]})
'''

CAMERA = '''extends Node2D
var runner
var tween=null
var _base_offset=Vector2(7,8)
var _shake_offset=Vector2(2,3)
var _camarea_offset=Vector2.ZERO
var _scope_arrows
var global
'''

PROBE = '''extends SceneTree
var fixtures
var metadata
var output
var tick=0
var start_tick=0
var events=[]
var samples=[]
var cases=[]
var tests={}
var battle
var dialog
var participant
var lamp
var actor
var services=[]
var completed=false
var next_expected=""
var rng_calls=0
func _init():
\tfor arg in OS.get_cmdline_args():
\t\tif arg.begins_with("--encore-out="):output=arg.substr(13)
\tfixtures=read_json("res://fixtures.json")
\tmetadata=read_json("res://metadata.json")
\tcall_deferred("run")
func _idle(_delta):
\ttick+=1
\tif battle and is_instance_valid(battle):
\t\tvar ap=battle.get_node("AnimScene")
\t\tvar win=dialog.get_node("Dialoguebox/ClipBox/YouWin")
\t\tvar transition=battle.get_node("PlayerTransitions").get_child(0)
\t\tsamples.append({"frame":tick-start_tick,"you_win_frame":win.frame,"you_win_visible":win.visible,"you_win_time":"%.17f"%animation_time(dialog.get_node("AnimationPlayer")),"top":[battle.get_node("top").rect_position.x,battle.get_node("top").rect_position.y],"bottom":[battle.get_node("bottom").rect_position.x,battle.get_node("bottom").rect_position.y],"plate_y":battle.get_node("PlayerInfo").rect_position.y,"transition_time":"%.17f"%animation_time(ap),"transition_position":[transition.position.x,transition.position.y],"transition_scale":[transition.scale.x,transition.scale.y],"transition_visible":transition.visible,"world_visible":actor.visible,"paused":actor.paused,"camera_position":[battle.global.currentCamera.position.x,battle.global.currentCamera.position.y],"camera_base_offset":[battle.global.currentCamera._base_offset.x,battle.global.currentCamera._base_offset.y]})
\treturn false
func animation_time(player):
\treturn player.current_animation_position if player.current_animation!="" else -1
func read_json(path):
\tvar f=File.new()
\tassert(f.open(path,File.READ)==OK)
\tvar result=JSON.parse(f.get_as_text())
\tassert(result.error==OK)
\treturn result.result
func event(name,data={}):
\tvar out=data.duplicate(true)
\tout.event=name
\tout.frame=tick-start_tick
\tout.order=events.size()
\tif battle:
\t\tif battle.has_node("AnimScene"):
\t\t\tout.animation_position="%.17f"%animation_time(battle.get_node("AnimScene"))
\t\tif participant and participant._party_info:
\t\t\tvar pos=participant._party_info.rect_global_position
\t\t\tout.plate_global=[pos.x,pos.y]
\t\tout.state={"xp":participant.character._exp if participant else -1,"bank":battle.globaldata.bank,"cash":battle.globaldata.cash,"earned_cash":battle.globaldata.earned_cash,"poltergeist":battle.globaldata.flags.poltergeist,"in_battle":battle.uiManager._in_battle,"paused":actor.paused if actor else true}
\tevents.append(out)
func native_timer(label,duration):
\tvar timer=create_timer(duration)
\tevent("timer_create",{"label":label,"duration":duration,"time_left":"%.17f"%timer.time_left})
\ttimer.connect("timeout",self,"timer_done",[label,tick])
\treturn timer
func timer_done(label,created):event("timer_timeout",{"label":label,"elapsed_frames":tick-created})
func random_i(label):
\trng_calls+=1
\tevent("unexpected_rng",{"label":label})
\treturn randi()
func random_range(label,a,b):
\trng_calls+=1
\tevent("unexpected_rng",{"label":label})
\treturn rand_range(a,b)
func service(path):
\tvar s=load(path).new()
\ts.runner=self
\tservices.append(s)
\treturn s
func control(parent,name):
\tvar n=Control.new()
\tn.name=name
\tparent.add_child(n)
\treturn n
func sprite(parent,name):
\tvar n=Sprite.new()
\tn.name=name
\tn.hframes=8
\tn.vframes=20
\tparent.add_child(n)
\treturn n
func flags_updated():event("flags_updated")
func battle_to_ov():event("battle_to_ov")
func battle_ended(result):
\tevent("battle_ended",{"result":result})
\tcompleted=true
func animation_started(name,who):event("animation_started",{"name":name,"who":who})
func animation_finished(name,who):event("animation_finished",{"name":name,"who":who})
func new_case():
\tparticipant=null
\tlamp=null
\tactor=null
\tbattle=load("res://battle.gd").new()
\tbattle.name="Battle"
\tbattle.runner=self
\tbattle.globaldata=service("res://globaldata.gd")
\tbattle.globaldata.bank=int(fixtures.new_game.bank)
\tbattle.globaldata.cash=int(fixtures.new_game.cash)
\tbattle.globaldata.earned_cash=int(fixtures.new_game.earned_cash)
\tbattle.uiManager=service("res://ui.gd")
\tbattle.uiManager.globaldata=battle.globaldata
\tbattle.uiManager._battle_ui=battle
\tbattle.audioManager=service("res://audio.gd")
\tbattle.global=service("res://global.gd")
\tbattle.globaldata.global=battle.global
\tbattle.global.connect("flags_updated",self,"flags_updated")
\tbattle.global.currentCamera=service("res://camera.gd")
\tbattle.global.currentCamera.position=Vector2(13,14)
\tget_root().add_child(battle)
\tactor=service("res://actor.gd")
\tactor.name="WorldPlayer"
\tactor.position=Vector2(430,397)
\tactor.sprite=sprite(actor,"Sprite")
\tbattle.add_child(actor)
\tactor.hide()
\tbattle.global.player=actor
\tvar camera=battle.global.currentCamera
\tcamera.global=battle.global
\tactor.add_child(camera)
\tcamera._scope_arrows=control(camera,"ScopeArrows")
\tvar arrows=AnimationPlayer.new()
\tarrows.name="ArrowsAnim"
\tarrows.add_animation("Come Out",load("res://scope_out.tres"))
\tcamera.add_child(arrows)
\tbattle.uiManager.connect("battle_to_ov",camera,"_scoping_stop")
\tbattle._party_orig_objects=[actor]
\tbattle._party_orig_dirs=[Vector2(1,0)]
\tcontrol(battle,"top")
\tcontrol(battle,"bottom")
\tcontrol(battle,"Enemies")
\tcontrol(battle,"NpcTransitions")
\tcontrol(battle,"ActionMenuBox")
\tvar pi=load("res://return_layout.tscn").instance()
\tbattle.add_child(pi)
\tpi.rect_position.y=fixtures.initial_player_info_y
\tbattle._party_info=pi.get_node("PlayerInfoVbox/PartyInfo")
\tvar pt=control(battle,"PlayerTransitions")
\tvar ps=sprite(pt,"ReturnSprite")
\tps.hide()
\tbattle._battle_bg=control(battle,"BattleBackground")
\tparticipant=service("res://participant.gd")
\tparticipant.character=service("res://character.gd")
\tparticipant.character._exp=int(fixtures.new_game.ninten.exp)
\tparticipant.character._level=int(fixtures.new_game.ninten.level)
\tparticipant.character.hp=int(fixtures.ninten.hp)
\tparticipant.character.pp=int(fixtures.ninten.pp)
\tparticipant._party_info=service("res://plate.gd")
\tvar plate=participant._party_info
\tplate.rect_position=Vector2(128,20)
\tplate.rect_size=Vector2(65,49)
\tplate.name="NintenPlate"
\tbattle._party_info.add_child(plate)
\tplate._cur_hp=int(fixtures.ninten.hp)
\tplate._target_hp=plate._cur_hp
\tplate._max_hp=int(fixtures.ninten.maxhp)
\tplate._target_pp=int(fixtures.ninten.pp)
\tplate._cur_pp=plate._target_pp
\tfor key in ["_ones_digit_hp","_tens_digit_hp","_huns_digit_hp"]:plate.set(key,load("res://digit.gd").new())
\tvar exclamation=control(plate,"HPExclamation")
\tvar exp_anim=AnimationPlayer.new()
\texp_anim.name="AnimationPlayer"
\texclamation.add_child(exp_anim)
\tplate.connect("hp_scroll_done",participant,"hp_stopped_scrolling")
\tplate.connect("pp_scroll_done",participant,"pp_stopped_scrolling")
\tparticipant.sprite=service("res://sprite.gd")
\tbattle.add_child(participant.sprite)
\tbattle._party_BPs=[participant]
\tbattle.global.party=[participant.character]
\tlamp=service("res://participant.gd")
\tlamp.kind=2
\tlamp.character=service("res://character.gd")
\tlamp.character.data=fixtures.lamp
\tbattle._item_pool=service("res://pool.gd")
\tbattle._item_pool.globaldata=battle.globaldata
\tbattle._exp_pool=int(fixtures.lamp.exp)
\tbattle._cash_pool=int(fixtures.lamp.cash)
\tbattle._win_flag="poltergeist"
\tbattle._context=load("res://context.gd").new()
\tdialog=service("res://dialog.gd")
\tdialog.name="Dialoguebox"
\tdialog.fixture_text=fixtures.exp_text
\tbattle.add_child(dialog)
\tvar db=control(dialog,"Dialoguebox")
\tdialog._dialogue_box_node=db
\tvar clip=control(db,"ClipBox")
\tcontrol(clip,"HBoxContainer")
\tvar win=sprite(clip,"YouWin")
\twin.hframes=1
\twin.vframes=6
\twin.hide()
\tdialog._cursor_down_sprite=control(db,"Cursor_Down")
\tvar timer=Timer.new()
\ttimer.name="Timer"
\ttimer.wait_time=1.25
\ttimer.one_shot=true
\ttimer.connect("timeout",dialog,"_next_phrase")
\tdialog.add_child(timer)
\tvar dap=AnimationPlayer.new()
\tdap.name="AnimationPlayer"
\tdap.root_node=NodePath("../Dialoguebox")
\tdap.add_animation("YouWin",load("res://you_win.tres"))
\tdap.add_animation("RESET",load("res://dialog_reset.tres"))
\tdialog.add_child(dap)
\tdap.connect("animation_started",self,"animation_started",["dialog"])
\tdap.connect("animation_finished",self,"animation_finished",["dialog"])
\tvar ap=AnimationPlayer.new()
\tap.name="AnimScene"
\tap.add_animation("transitionOut",load("res://transition_out.tres"))
\tbattle.add_child(ap)
\tap.connect("animation_started",self,"animation_started",["battle"])
\tap.connect("animation_finished",self,"animation_finished",["battle"])
\tbattle.connect("battle_to_ov",self,"battle_to_ov")
\tbattle.connect("battle_to_ov",battle.uiManager,"emit_signal",["battle_to_ov"])
\tbattle.connect("battle_ended",self,"battle_ended")
\tbattle.connect("battle_ended",battle.uiManager,"_on_battle_ended",[battle])
func cleanup():
\tbattle.free()
\tbattle=null
\tfor s in services:
\t\tif is_instance_valid(s) and s is Node:s.free()
\tservices.clear()
\tparticipant=null
\tlamp=null
\tactor=null
\tdialog=null
func run_case(name,initial_finished,cur_hp,hp_frame,target_hp,cancel_ack=false):
\tyield(self,"idle_frame")
\tstart_tick=tick
\tevents=[]
\tsamples=[]
\tcompleted=false
\trng_calls=0
\tnew_case()
\tparticipant._party_info._cur_hp=cur_hp
\tparticipant._party_info._dhp_frame=hp_frame
\tparticipant._party_info._target_hp=target_hp
\tparticipant._party_info._are_hp_increasing=target_hp>cur_hp
\tseed(123)
\tvar expected=str(randi())
\tseed(123)
\tbattle._item_pool.roll_item([lamp])
\tevent("pool_rolled",{"item":battle._item_pool.item,"candidates":battle._item_pool._pool.size()})
\tdialog._finished=initial_finished
\tdialog.pending="" if initial_finished else "prior"
\tevent("invoke_win",{"initial_finished":initial_finished,"cur_hp":cur_hp,"hp_frame":hp_frame,"target_hp":target_hp})
\tbattle._win()
\tif !initial_finished:
\t\tfor _i in range(3):yield(self,"idle_frame")
\t\tdialog._finish_phrase()
\t\tevent("prior_print_complete_adapter")
\t\tfor _i in range(80):yield(self,"idle_frame")
\t\tassert(dialog.pending=="prior" and !dialog._auto_advance)
\t\tevent("prior_ack_input")
\t\tdialog._action_press(true,false)
\twhile dialog.pending!="exp":yield(self,"idle_frame")
\tassert(participant.character._exp==0 and !battle.globaldata.flags.poltergeist)
\tevent("exp_print_complete_adapter")
\tdialog._finish_phrase()
\tfor _i in range(80):yield(self,"idle_frame")
\tassert(dialog.pending=="exp" and participant.character._exp==0 and !battle.globaldata.flags.poltergeist)
\tevent("exp_waited_without_ack",{"frames":80})
\tevent("exp_ack_input")
\tdialog._action_press(true,cancel_ack)
\twhile !completed:yield(self,"idle_frame")
\tfor _i in range(2):yield(self,"idle_frame")
\tvar observed=str(randi())
\tassert(expected==observed and rng_calls==0)
\tassert(participant.character._exp==3 and participant.character._level==1)
\tassert(battle.globaldata.bank==5 and battle.globaldata.earned_cash==5 and battle.globaldata.cash==0)
\tassert(battle.globaldata.flags.poltergeist and battle.globaldata.flags.earned_cash)
\tassert(!actor.paused and !battle.uiManager._in_battle)
\tcases.append({"name":name,"events":events.duplicate(true),"samples":samples.duplicate(true),"state":{"xp":participant.character._exp,"level":participant.character._level,"hp":participant.character.hp,"pp":participant.character.pp,"plate_raw_hp":participant._party_info._cur_hp,"plate_hp_frame":participant._party_info._dhp_frame,"plate_current_hp":participant.get_current_hp(),"plate_target_hp":participant.get_target_hp(),"bank":battle.globaldata.bank,"cash":battle.globaldata.cash,"earned_cash":battle.globaldata.earned_cash,"flags":battle.globaldata.flags.duplicate(),"rare_drops":battle.globaldata.rare_drops.duplicate(),"paused":actor.paused,"in_battle":battle.uiManager._in_battle},"expected_next_randi":expected,"actual_next_randi":observed,"rng_calls":rng_calls})
\tcleanup()
func run():
\tyield(run_case("fresh_healthy",true,62,0,62),"completed")
\tyield(run_case("unfinished_prior_dialogue",false,62,0,62),"completed")
\tyield(run_case("damage_rolling_mid_digit",true,61,7,59),"completed")
\tyield(run_case("exp_cancel_ack",true,62,0,62,true),"completed")
\t# Isolated HP boundary probes, not reachable fresh-Lamp progression scenarios.
\ttests.hp_cases=[]
\tfor config in [[61,0,59],[61,7,59],[59,0,62],[0,7,0],[0,0,0]]:
\t\tevents=[]
\t\tsamples=[]
\t\tnew_case()
\t\tvar p=participant._party_info
\t\tp._cur_hp=int(config[0]);p._dhp_frame=int(config[1]);p._target_hp=int(config[2]);p._are_hp_increasing=p._target_hp>p._cur_hp
\t\tvar before={"raw":p._cur_hp,"frame":p._dhp_frame,"current":participant.get_current_hp(),"target":participant.get_target_hp()}
\t\tbattle._set_bp_end_battle()
\t\ttests.hp_cases.append({"before":before,"after":{"raw":p._cur_hp,"frame":p._dhp_frame,"current":participant.get_current_hp(),"target":participant.get_target_hp(),"character_hp":participant.character.hp,"mods":participant._stat_mods,"scrolling":p._are_hp_scrolling,"increasing":p._are_hp_increasing},"events":events.duplicate(true)})
\t\tcleanup()
\tvar c=load("res://character.gd")
\tvar camera=load("res://camera.gd").new()
\tcamera.runner=self
\tcamera.position=Vector2(13,14)
\tcamera.reset()
\ttests.camera_reset={"position":[camera.position.x,camera.position.y],"base_offset":[camera._base_offset.x,camera._base_offset.y],"shake_offset":[camera._shake_offset.x,camera._shake_offset.y]}
\tassert(camera.position==Vector2(13,14) and camera._base_offset==Vector2(7,8) and camera._shake_offset==Vector2(2,3))
\tcamera.free()
\ttests.progression={"level_cap":c.LEVEL_CAP,"level2_exp":c._level_to_exp(2),"cap_exp":c._level_to_exp(c.LEVEL_CAP),"fresh_result_level":c._exp_to_level(3)}
\tvar f=File.new()
\tassert(f.open(output,File.WRITE)==OK)
\tf.store_string(JSON.print({"schema":1,"engine":Engine.get_version_info(),"metadata":metadata,"cases":cases,"tests":tests},"  "))
\tf.close()
\tprint("BATTLE_VICTORY_REFERENCE_COMPLETE")
\tquit()
'''


def animation_resource(source, number):
    found = re.findall(r'^\[sub_resource type="Animation" id='+str(number)+r'\]\n(.*?)(?=^\[|\Z)', source, re.M|re.S)
    require(len(found)==1, 'Animation resource missing')
    return '[gd_resource type="Animation" format=2]\n\n[resource]\n'+found[0]


def prepare(work):
    require(not work.exists() and work.is_relative_to(ROOT/'build/battle-victory-reference'), 'Use fresh work directory under build/battle-victory-reference')
    ex=Extractor(ROOT)
    ir=ex.build()
    system=ex.text(SYSTEM)
    pm=ex.text('Scripts/global/PartyMember.gd')
    bp=ex.text('Scripts/UI/Battle/BattleParticipant.gd')
    plate=ex.text('Scripts/UI/Battle/PartyInfoPlate.gd')
    dialog=ex.text('Scripts/UI/Battle/BattleDialogueBox.gd')
    abstract=ex.text('Scripts/UI/AbstractDialogueBox.gd')
    ui=ex.text('Scripts/global/uiManager.gd')
    gd=ex.text('Scripts/global/globalData.gd')
    work.mkdir(parents=True)
    battle=BATTLE
    for field in ['SPRITE_FRAMES','_musical_effects']:
        match=re.search(r'^(?:const|var) '+field+r'\s*:?=\s*\{.*?^\}',system,re.M|re.S)
        require(match is not None, 'Source field missing '+field)
        battle+='\n'+match[0]+'\n'
    for method in METHODS:battle+='\n'+adapted(function(system,method),method)
    character=CHARACTER+'\n'+re.search(r'^const LEVEL_CAP.*$',pm,re.M)[0]+'\n'
    for name in ['_level_to_exp','_exp_to_level','give_exp','_set_exp','_update_level_from_exp']:
        character+='\n'+adapted(function(pm,name),'PartyMember.'+name)
    participant=PARTICIPANT
    for name in ['get_current_hp','get_target_hp','get_target_pp','set_target_hp','hp_stopped_scrolling','pp_stopped_scrolling','reset_all_stat_mods']:
        participant+='\n'+adapted(function(bp,name),'BattleParticipant.'+name)
    plate_out=PLATE
    for name in ['get_current_hp','get_target_hp','get_target_pp','set_instant_hp','set_target_hp','_hide_exclamation','stop_scrolling']:
        plate_out+='\n'+adapted(function(plate,name),'PartyInfoPlate.'+name)
    base=DIALOG_BASE
    for name in ['_action_press','_next_phrase','set_auto_advance']:
        base+='\n'+adapted(function(abstract,name),'AbstractDialogueBox.'+name)
    dlg=DIALOG
    for name in ['play_win','did_finish','_finish_phrase','_action_press']:
        dlg+='\n'+adapted(function(dialog,name),'BattleDialogueBox.'+name)
    pool=ex.text('Scripts/UI/Battle/BattleItemPool.gd')
    pool=pool.replace('extends Object\nclass_name BattleItemPool','extends Reference\nvar runner\nvar globaldata\nconst Item=preload("res://unused_item.gd")')
    pool=re.sub(r':\s*(BattleParticipant|Item)(?=\s*[,)=\n])','',pool)
    pool=re.sub(r'->\s*BattleParticipant\s*:',':',pool).replace(':=','=')
    pool=re.sub(r'\brandi\(\)', 'runner.random_i("BattleItemPool.roll_item")', pool)
    pool=re.sub(r'\brand_range\(([^\n()]*)\)', r'runner.random_range("BattleItemPool._roll", \1)', pool)
    scripts={'battle':battle,'character':character,'participant':participant,'plate':plate_out,
     'digit':DIGIT,'sprite':SPRITE,'dialog_base':base,'dialog':dlg,'context':CONTEXT,
     'global':GLOBAL,'globaldata':GLOBALDATA+'\n'+adapted(function(gd,'set_flag'),'globalData.set_flag'),
     'ui':UI+'\n'+adapted(function(ui,'give_cash_to_bank'),'uiManager.give_cash_to_bank')+'\n'+adapted(function(ui,'_on_battle_ended'),'uiManager._on_battle_ended'),
     'audio':AUDIO,'actor':ACTOR+'\n'+adapted(function(ex.text('Scripts/Main/party/Player.gd'),'rotate_to'),'Player.rotate_to')+'\n'+adapted(function(ex.text('Scripts/Main/party/Player.gd'),'exit_camera'),'Player.exit_camera'),'camera':CAMERA+'\n'+adapted(function(ex.text('Scripts/Main/Camera2D.gd'),'reset'),'GameCamera.reset')+'\n'+adapted(function(ex.text('Scripts/Main/Camera2D.gd'),'_scoping_stop'),'GameCamera._scoping_stop')+'\n'+adapted(function(ex.text('Scripts/Main/Camera2D.gd'),'return_offset'),'GameCamera.return_offset'),'pool':pool,'probe':PROBE,
     'unused_item':'extends Reference\nfunc _init(_id):assert(false)\n'}
    for name,content in scripts.items():(work/(name+'.gd')).write_text(content)
    (work/'unused.tscn').write_text('[gd_scene format=2]\n[node name="UnreachableDrop" type="Node"]\n')
    layout_source=ex.text('Nodes/Ui/Battle/Battle.tscn')
    def node_body(name, parent):
        matches=re.findall(r'^\[node name="'+re.escape(name)+r'"[^\n]* parent="'+re.escape(parent)+r'"[^\n]*\]\n(.*?)(?=^\[|\Z)',layout_source,re.M|re.S)
        require(len(matches)==1,'Missing native layout node '+name)
        return matches[0]
    layout='[gd_scene format=2]\n[node name="PlayerInfo" type="Control"]\n'+node_body('PlayerInfo','.')
    layout+='[node name="PlayerInfoVbox" type="VBoxContainer" parent="."]\n'+node_body('PlayerInfoVbox','PlayerInfo')
    layout+='[node name="PartyInfo" type="Control" parent="PlayerInfoVbox"]\n'+node_body('PartyInfo','PlayerInfo/PlayerInfoVbox')
    sp=ex.text('Nodes/Ui/Battle/SpMeter.tscn')
    minimum=re.search(r'^rect_min_size = Vector2\( 320, 8 \)$',sp,re.M)
    require(minimum is not None,'Unexpected SpMeter minimum')
    layout+='[node name="SpMeter" type="Control" parent="PlayerInfoVbox"]\n'+node_body('SpMeter','PlayerInfo/PlayerInfoVbox')+'visible = false\n'+minimum[0]+'\n'
    (work/'return_layout.tscn').write_text(layout)
    (work/'transition_out.tres').write_text(animation_resource(ex.text('Nodes/Ui/Battle/Battle.tscn'),14))
    (work/'you_win.tres').write_text(animation_resource(ex.text('Nodes/Ui/Battle/BattleDialogueBox.tscn'),2))
    (work/'scope_out.tres').write_text(animation_resource(ex.text('Nodes/Ui/Camera.tscn'),276))
    (work/'dialog_reset.tres').write_text(animation_resource(ex.text('Nodes/Ui/Battle/BattleDialogueBox.tscn'),1))
    translations=list(csv.reader(io.StringIO(ex.text('Translations/TranslatedText/battletext - sheet.csv'))))
    exp_text=next(r[1] for r in translations if r[0]=='BATTLE_MSG_EXP_ONE_ALLY')
    fixtures={'new_game':ex.yaml('Data/save_new_game.yaml'),'ninten':ir['party']['effective_stats'],'lamp':ex.yaml('Data/Battlers/lamp.yaml'),'exp_text':exp_text,'lamp_dialogue':ex.yaml('Data/Dialogue/Podunk/cutscenes/lamp_attack.yaml'),'initial_player_info_y':next(t['keys']['values'][-1] for a in ir['animations'] if a['name']=='scene.transitionIn' for t in a['tracks'] if t['path']=='PlayerInfo:rect_position:y')}
    require(fixtures['lamp_dialogue']['12']['ovbattlemusic'] is True,'Lamp music flag changed')
    require(fixtures['lamp'].get('items',[])==[] and fixtures['lamp']['boss'] is False,'Lamp item pool changed')
    require(fixtures['new_game']['ninten']['exp']==0 and fixtures['new_game']['ninten']['level']==1,'Fresh scope changed')
    (work/'fixtures.json').write_text(json.dumps(fixtures,indent=2)+'\n')
    audit_sources=['Maps/podunk/Nintens House.tscn','Nodes/Reusables/npc.tscn','Scripts/Main/npc.gd','Scripts/Main/actor.gd','Scripts/Main/Flag Landmarks.gd','Scripts/Main/party/party_object.gd','Nodes/Reusables/Player.tscn','Scripts/Main/CutsceneArea.gd','Scripts/Main/camarea.gd','Nodes/Overworld/camarea.tscn']
    for path in audit_sources:ex.data(path)
    metadata={'commit':ir['commit'],'scope':'healthy fresh single Ninten/Lamp victory, reward and source transition callbacks; separate synthetic HP commit boundary probes',
     'methods':METHODS,'sources':ex.sources,'source_method_sha256':{m:hashlib.sha256(function(system,m).encode()).hexdigest() for m in METHODS},
     'transformations':['extract source methods; erase unavailable custom class annotations and replace := with =; retain native int/float annotations and arithmetic','event instrumentation around method entry/yield; native global RNG forwarded only to observe accidental consumption','native .tres Animation resources preserve exact original serialized property text; no hand-entered keyframes','transported JSON integral fixtures explicitly restored to native int'],
     'adapters':['dialogue printing is explicitly completed by probe; source action-press, phrase-finish, next-phrase and manual advance behavior execute, but glyph timing and layout do not','English EXP_ONE_ALLY format adapter substitutes only source name/value; translation grammar/wrapping not proven','audio calls observed only; actual Lamp ovbattlemusic=true and boss=false skips win music/removal/fade branches; resume call logged','world objects record callbacks; ui battle_to_ov relay executes source camera _scoping_stop/return_offset with native Tween from explicit synthetic local offsets, and source Player.exit_camera; original single-player jump Tween executes with native minimal source Control/VBox layout and identity viewport transform; original Player.rotate_to executes native Vector2 math and timers; its blend_position visual side effect is logged','party victory/show/hide sprites observed only; actual source YouWin/RESET and transitionOut AnimationPlayers execute natively','HP plate source stop/set-instant/set-target/get and participant signal commit execute; counter glyph writes are inert; tests initialize explicit mid-scroll fields and do not run the scrolling process','empty item drop coroutine is recorded and skipped because there are no items; no full-world object lifecycle or whole-game proof','healthy status list and NPC list empty; level-up or skill-learn branches assert; LEVEL_CAP policy recorded from source but no artificial capped-character gameplay scenario'],
     'not_proven':['whole-game victory timing','GPU framebuffer parity','audio playback','hardware','full initial world lifecycle']}
    (work/'metadata.json').write_text(json.dumps(metadata,indent=2)+'\n')
    (work/'project.godot').write_text('config_version=4\n[application]\nconfig/name="Encore source victory reference"\n[logging]\nfile_logging/enable_logging=false\n[physics]\ncommon/physics_fps=60\n')
    return metadata


def world_audit():
    from tools.world_geometry import geometry
    ex=Extractor(ROOT)
    house_text=ex.text('Maps/podunk/Nintens House.tscn')
    review=json.loads((ROOT/'compatibility/reviews/opening-world-v0410.json').read_text())
    docs={}
    for path in ['reports/cloud-world/house-exact.json','reports/cloud-world/player-exact.json']:
        require(sha(ROOT/path)==review['inputs'][path],'Unreviewed native source export '+path)
        docs[path]=json.loads((ROOT/path).read_text())
    house=docs['reports/cloud-world/house-exact.json']
    _,polygons=geometry(house,docs['reports/cloud-world/player-exact.json'])
    bodies=[n['path'] for n in house['nodes'] if n['class'] in ['StaticBody2D','KinematicBody2D']]
    lamp=[p for p in polygons if p['body']=='Objects/lamp']
    require(len(lamp)==1,'Lamp collision namespace changed')
    flag_refs=[]
    for match in re.finditer(r'^\[node ([^\n]+)\]\n(.*?)(?=^\[|\Z)',house_text,re.M|re.S):
        attrs=dict(re.findall(r'(\w+)="([^"]*)"',match[1]))
        parent=attrs.get('parent')
        path='.' if parent is None else attrs['name'] if parent=='.' else parent+'/'+attrs['name']
        refs=[line for line in match[2].splitlines() if 'poltergeist' in line or 'earned_cash' in line]
        if refs:flag_refs.append({'path':path,'source_lines':refs})
    for p in ['Nodes/Reusables/npc.tscn','Scripts/Main/npc.gd','Scripts/Main/actor.gd','Scripts/Main/CutsceneArea.gd','Scripts/Main/Flag Landmarks.gd','Scripts/UI/Battle/BattleParticipant.gd','Scripts/Main/Camera2D.gd']:
        ex.data(p)
    return {'schema':1,'commit':ex.lock['commit'],'kind':'source/static dependency audit; not full-world native runtime proof',
      'native_export_sha256':{p:sha(ROOT/p) for p in docs},'sources':ex.sources,
      'blocking_body_count':len(bodies),'blocking_polygon_count':len(polygons),
      'lamp_body_id':bodies.index('Objects/lamp')+1,'lamp_polygons':lamp,
      'house_references_to_reward_flags':flag_refs,
      'findings':['Lamp body is included in the 105-polygon scene export and must be disabled/removed when that bound enemy is defeated; its actor visual removal alone leaves a stale obstacle',
      'BattleParticipant.defeat ENEMY calls _kill_overworld before defeated signal; _kill_overworld calls actor.remove_battle and actor.die; actor.erase queues both replaced non-player NPC and actor for deletion',
      'poltergeist disables Poltergeist/Cutscene Area and enables Poltergeist/MusicArea; these are nonblocking Area2D nodes, outside the StaticBody2D/KinematicBody2D solver set',
      'Other house poltergeist references select interaction text; no serialized static-body flag/ancestor condition depends on either reward flag',
      'earned_cash has no serialized reference in this house; source bank award suppresses flags_updated for this flag',
      'House Poltergeist landmark depends on doll_melody, DoorBlock landmark on talked_to_dad, and locked-door flags remain unchanged',
      'Camera.reset only rebinds a for-loop local; native oracle proves position/base/shake fields unchanged. _hide_battle_BG separately resumes an existing valid camera tween']}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--godot',type=Path,required=True)
    parser.add_argument('--work',type=Path,required=True)
    parser.add_argument('--reports',type=Path,required=True)
    args=parser.parse_args()
    work=args.work.resolve();reports=args.reports.resolve()
    require(not reports.exists() and reports.is_relative_to(ROOT/'reports/battle-victory-reference'),'Use fresh report subdirectory')
    metadata=prepare(work)
    reports.mkdir(parents=True)
    env=dict(os.environ)
    for key in ['XDG_DATA_HOME','XDG_CONFIG_HOME','XDG_CACHE_HOME']:
        directory=work/key.lower();directory.mkdir();env[key]=str(directory)
    command=[str(args.godot.resolve()),'--path',str(work),'--fixed-fps','60','--script',str(work/'probe.gd'),'--encore-out='+str(reports/'reference.json')]
    with (reports/'godot.txt').open('wb') as log:
        run=subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,env=env,timeout=45)
    log=(reports/'godot.txt').read_text()
    require(run.returncode==0 and 'SCRIPT ERROR' not in log and 'ERROR:' not in log and 'BATTLE_VICTORY_REFERENCE_COMPLETE' in log,'Native reference failed; inspect retained godot.txt')
    result=json.loads((reports/'reference.json').read_text())
    require(result['engine']['string']=='3.6.2-stable (official)','Wrong native engine')
    require(len(result['cases'])==4,'Incomplete native cases')
    receipt={'schema':1,'commit':metadata['commit'],'scope':metadata['scope'],'engine_sha256':sha(args.godot),'tool_sha256':sha(__file__),'reference_sha256':sha(reports/'reference.json'),'generated_scripts_sha256':{p.name:sha(p) for p in work.glob('*.gd')},'generated_animations_sha256':{p.name:sha(p) for p in work.glob('*.tres')},'generated_scenes_sha256':{p.name:sha(p) for p in work.glob('*.tscn')},'full_game_reference':'not run','hardware':'not run','render_reference':'not run'}
    (reports/'world-audit.json').write_text(json.dumps(world_audit(),indent=2)+'\n')
    receipt['world_audit_sha256']=sha(reports/'world-audit.json')
    (reports/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
    summary={'cases':[{k:c[k] for k in ['name','state','expected_next_randi','actual_next_randi','rng_calls']}|{'events':[e for e in c['events'] if e['event'] not in ['enter','before_yield','after_yield']]} for c in result['cases']],'tests':result['tests']}
    (reports/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    print(json.dumps(summary,indent=2))
    print('Native battle victory reference complete:',reports)

if __name__=='__main__':
    try:main()
    except (ValueError,OSError,KeyError,subprocess.SubprocessError) as error:
        print('BATTLE VICTORY REFERENCE ERROR: '+str(error),file=sys.stderr)
        sys.exit(1)
