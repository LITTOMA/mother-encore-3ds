#!/usr/bin/env python3
"""Execute bounded original Lamp round methods in official Godot 3.6.2.

This is an extracted-source oracle, not a full-game/render/3DS reference. It
preserves native Variant arithmetic, global RNG, deferred calls and yields.
Unrelated systems are explicit adapters, and unreachable branches fail closed.
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

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from tools.extract_battle_entry import Extractor, require, animation

SYSTEM = 'Scripts/UI/Battle/BattleSystem.gd'
METHODS = ['_end_player_action_choices', '_new_round', '_reset_actions', '_cache_action',
 '_cache_enemy_and_npc_actions', '_choose_random_action', '_do_actions', '_start_action',
 '_do_skill', '_do_skill_with_screen_effect', '_check_ableness_for_action',
 '_retarget_action', '_retargeting', '_calculate_damage', '_apply_damage',
 '_do_attack_damage', '_chance_roll', '_sort_by_priority', '_sort_by_speed',
 '_get_conscious', '_get_targetables_for_action', '_get_miss_chance',
 '_try_apply_skill_costs', '_get_damage_modifiers', '_get_damage_color',
 '_compare_action_types', '_apply_confusion', '_do_pre_hit_effect',
 '_check_status_effect', '_do_status_hit_heal', '_do_status_transmission',
 '_check_buffered_player_defeat', '_play_battle_sprite_anim']


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def function(text, name):
    found = re.findall(r'^func ' + re.escape(name) + r'\([^\n]*\n.*?(?=^func |\Z)', text, re.M | re.S)
    require(len(found) == 1, 'Missing/ambiguous source method ' + name)
    return found[0].rstrip() + '\n'


def adapt(source, method, trace=True):
    # Engine/game class annotations are unavailable in the isolated project.
    # Native int/float/bool annotations and expression operators stay intact.
    source = re.sub(r':\s*(BattleParticipant|EnemySkill|Item|Status)(?=\s*[,)=\n])', '', source)
    source = re.sub(r'->\s*(BattleParticipant|SkillAction|EnemySkill)\s*:', ':', source)
    source = source.replace(':=', '=')
    source = source.replace('get_tree().create_timer(', 'runner.native_timer(\"' + method + '\", ')
    source = re.sub(r'\brandi\(\)', 'runner.draw_i("' + method + '")', source)
    source = re.sub(r'\brandf\(\)', 'runner.draw_f("' + method + '")', source)
    source = re.sub(r'\brand_range\(([^\n()]*)\)', r'runner.draw_range("' + method + r'", \1)', source)
    if not trace:
        return source
    source = re.sub(r'^(\s*)(if [^\n]+|else): (yield\([^\n]+)$', r'\1\2:\n\1\t\3', source, flags=re.M)
    lines = source.splitlines()
    result = [lines[0], '\trunner.event("enter", {"method":"' + method + '"})']
    if method == '_chance_roll':
        result.append('\trunner.event("chance_parameters", {"percentage":percentage,"multiplier":multiplier})')
    for lineno, line in enumerate(lines[1:], 2):
        indent = line[:len(line)-len(line.lstrip())]
        if 'yield(' in line and not line.lstrip().startswith('#'):
            result.append(indent + 'runner.event("before_yield", {"method":"' + method + '","line":' + str(lineno) + '})')
        if 'call_deferred(' in line and not line.lstrip().startswith('#'):
            result.append(indent + 'runner.event("before_deferred", {"method":"' + method + '","line":' + str(lineno) + '})')
        if method == '_calculate_damage' and line.strip().startswith('val = floor(val + (runner.draw_f'):
            result.append(indent + 'runner.event("damage_before_variance", {"value":"%.17f"%val,"double_le_hex":runner.double_bits(val),"variance":variance})')
        result.append(line)
        if 'yield(' in line and not line.lstrip().startswith('#'):
            result.append(indent + 'runner.event("after_yield", {"method":"' + method + '","line":' + str(lineno) + '})')
        if line.strip() == '_action_queue.sort_custom(self, "_sort_by_priority")':
            result.append(indent + 'runner.log_queue(_action_queue)')
        if line.strip() == 'action.targets = _retargeting(action, target_ok)':
            result.append(indent + 'runner.log_targets(action)')
        if line.strip() == 'damage = _calculate_damage(action, target, adrenaline, smashed)':
            result.append(indent + 'runner.event("damage", {"user":user.label,"target":target.label,"skill":action.skill.id,"damage":damage,"smash":smashed,"adrenaline":adrenaline})')
        if line.strip() == 'val = floor(val + (randf() * variance) - variance/2.0)':
            raise ValueError('RNG source interposition failed')
    return '\n'.join(result) + '\n'


PREFIX = '''extends Node
signal round_done(turn)
signal action_select_done
signal battle_paused
signal battle_unpaused
var runner
var global
var globaldata
class Character:
\tenum Type {PARTY_MEMBER, PARTY_NPC, ENEMY}
\tconst MAXHP="maxhp"
\tconst MAXPP="maxpp"
\tconst OFFENSE="offense"
\tconst DEFENSE="defense"
\tconst SPEED="speed"
\tconst IQ="iq"
\tconst GUTS="guts"
enum Result { LOSE=-1, FLEE, WIN }
enum Advantage { ENEMY=-1, NEUTRAL, PLAYER }
enum ActionType {DAMAGE, HEALING, STAT, AILMENT, OTHER}
enum TargetType {ENEMY, ALLY, ALLY_EXCEPT_SELF, RANDOM_ENEMY, RANDOM_ALLY, SELF, ALL_ENEMIES, ALL_ALLIES, ANY, RANDOM_ENEMIES_2, RANDOM_ENEMIES_UNTIL_MISS}
const MAX_ENEMY_COUNT=8
const ADRENALINE_MULT=1.5
const GUTS_MULT=500.0
const SMASH_MULT=4
const HEAL_BY_HIT_PROB=25
const TRANSMIT_PROB=10
const SP_TYPE={"ADRENALINE":10}
const EnemySkill=preload("res://enemy_skill.gd")
var _party_BPs=[]
var _enemy_BPs=[]
var _npc_BPs=[]
var _action_queue=[]
var _current_action=null
var _current_action_index=0
var _doing_actions=false
var _active=true
var _paused=false
var _lose_battle=false
var _buffer_reorganize=false
var _encore_activated=false
var _advantage=0
var _turns_count=1
var _curr_party_mem=-1
var _show_intro_outro=false
var _ongoing_npc_protection={}
var _special_npc_BPs={}
var _buffered_player_defeat=[]
var _sound_effects={}
var _exp_pool=0
var _received_exp_from_flee=false
var _can_run=false
var _context
func _set_fast_mode(enabled):
\trunner.event("set_fast_mode", {"enabled":enabled})
func _set_encore_active(enabled, _unused=false):
\tassert(!enabled)
func _next_active_member():
\trunner.event("next_menu", {"turn":_turns_count})
\trunner.boundary="next_menu"
func _win():
\trunner.event("win_boundary", {"current_action":_current_action.user.label,"index":_current_action_index})
\t_active=false
\trunner.boundary="win"
func _play_sfx(sound, _unused=0):
\trunner.event("sound", {"name":sound})
func _do_hit_effect(effect, sound, target):
\trunner.event("hit_effect_adapter", {"name":effect,"sound":sound,"target":target.label})
func _create_rising_num(value, target, _color=Color.white, flying=false):
\trunner.event("damage_number", {"value":value,"target":target.label,"flying":flying})
\tif flying:
\t\tvar n=load("res://flying.gd").new()
\t\tn.runner=runner
\t\tadd_child(n)
\t\tn.run()
func _create_smash_attack(target):
\trunner.event("smash_visual_adapter", {"target":target.label})
\treturn Node.new()
func _setup_npc_protection(_action, _targets, _output):
\tassert(_npc_BPs.empty())
\treturn null
func _use_passive_skill(data, _skill, _damage, _user, _target):
\tassert(data.empty())
\treturn null
func _try_add_attack_sp(action, target):
\trunner.event("sp_adapter", {"skill":action.skill.id,"target":target.label})
func _darken_bg():
\tassert(false)
func _undarken_bg():
\tassert(false)
'''

PARTICIPANT = '''extends Node
signal action_choice
signal before_action(action)
signal acted(action)
signal bp_hit
var runner
var label
var kind
var character
var stats={}
var sprite
var plate
var skills=[]
var defending=false
var immortal=false
var unconscious=false
var cur_scripted_skill=""
var cur_dialog={}
var _battle_passive_skills={}
func get_name():return label
func get_type():return kind
func is_type(t):return kind==t
func get_stat(s):return int(stats[s])
func get_target_hp():return int(stats.hp)
func get_target_pp():return int(stats.pp)
func get_current_hp():return int(stats.hp)
func get_sprite():return sprite
func get_plate():return plate
func is_unconscious():return unconscious
func is_incapacitated():return unconscious
func can_act():return !unconscious
func is_targetable_for_action(_a):return !unconscious
func get_passive_skills():return []
func get_passive_skill_for_attack(_a):return {}
func get_all_status_effects():return []
func get_affinity_multiplier(_a):return 1.0
func get_usable_skills():return skills
func get_combined_status_effect(key):
\tif key in ["turn_skip","confusion"]:return {}
\tif key=="miss_chance":return 0
\treturn []
func handle_battler_script():
\trunner.event("handle_battler_script", {"target":label})
func change_hp_by(amount):
\tvar old=stats.hp
\tstats.hp=clamp(int(stats.hp)+amount, 0, int(stats.maxhp))
\trunner.event("hp_target", {"target":label,"before":old,"after":stats.hp,"delta":amount})
func change_pp_by(amount):
\tstats.pp=clamp(int(stats.pp)+amount, 0, int(stats.maxpp))
func defeat():
\tunconscious=true
\trunner.event("defeat", {"target":label})
'''

SERVICES = '''extends Node
var runner
var skills={}
var stats={}
var SKILL_SPY="spy"
var user_fast_mode=false
var label=""
func get_battle_skill(key):return skills[key].duplicate(true)
func get_passive_skill(_key):
\tassert(false)
\treturn {}
func get_name():return label
func get_data():return stats
func get_status_ailments():return []
func get_passive_skills():return []
func start_joy_vibration(_a,_b,_c,_d):pass
func start_slowmo(a,b):
\trunner.event("slowmo_adapter", {"duration":a,"scale":b})
func set_actor(_a):return self
func set_targets(_a):return self
func set_item_or_skill(_a):return self
func append_formatted(text, _context, _third=null):
\trunner.event("dialog_append", {"text":text})
func start_from_appended():
\trunner.event("dialog_adapter_start")
\tyield(get_tree(), "idle_frame")
\trunner.event("dialog_adapter_done")
func start_from_formatted(text, _context):
\tappend_formatted(text, _context)
\tyield(start_from_appended(), "completed")
func start_from_string(text):
\tappend_formatted(text, null)
\tyield(start_from_appended(), "completed")
func quake(_a,_b=1.0):pass
func stop_scrolling():pass
'''

SPRITE = '''extends Control
signal apply_damage
var runner
var label
var _sprite
var _anim_player
var _waiting_pause=false
func _ready():
\t_sprite=Sprite.new()
\t_sprite.name="Sprite"
\t_sprite.hframes=10
\t_sprite.vframes=18
\tadd_child(_sprite)
\t_anim_player=AnimationPlayer.new()
\t_anim_player.name="AnimationPlayer"
\tadd_child(_anim_player)
\tvar clip=runner.read_json("res://bash_animation.json")
\tvar animation=Animation.new()
\tanimation.length=clip.length
\tfor track in clip.tracks:
\t\tvar index=animation.add_track(Animation.TYPE_METHOD if track.type=="method" else Animation.TYPE_VALUE)
\t\tanimation.track_set_path(index,NodePath(track.path))
\t\tif track.type=="value":animation.value_track_set_update_mode(index,track.keys.update)
\t\tfor key in track.keys.times.size():animation.track_insert_key(index,track.keys.times[key],track.keys.values[key])
\t_anim_player.add_animation("bash",animation)
func play(anim, _override=false):
\trunner.event("sprite_native_animation_play", {"target":label,"animation":anim})
\tif anim=="bash":_anim_player.play(anim)
\treturn anim=="bash"
func hide_away(_anim="", _override=false):
\trunner.event("sprite_hide_adapter", {"target":label})
func flash():
\trunner.event("sprite_flash_adapter", {"target":label})
func hit():
\trunner.event("sprite_hit_adapter", {"target":label})
func shake(amount):
\trunner.event("sprite_shake_adapter", {"target":label,"amount":amount})
func bounce_up_hit(_amount):
\tassert(false) # Outside first-round 62 HP Ninten damage threshold.
func pause():
\tassert(false)
'''

PROBE = '''extends SceneTree
var events=[]
var draws=[]
var cases=[]
var tests={}
var fixtures
var metadata
var output
var battle
var boundary=""
var case_name=""
var tick=0
var start_tick=0
var phase="setup"
var watch_frames=0
var mirror=RandomNumberGenerator.new()
var raw_count=0
var timer_serial=0
var frame_deltas=[]
var threshold_fired=false
func read_json(path):
\tvar f=File.new()
\tassert(f.open(path,File.READ)==OK)
\tvar x=JSON.parse(f.get_as_text())
\tf.close()
\tassert(x.error==OK)
\treturn x.result
func normalize_numbers(value):
\tif value is Dictionary:
\t\tfor k in value:value[k]=normalize_numbers(value[k])
\telif value is Array:
\t\tfor i in value.size():value[i]=normalize_numbers(value[i])
\telif value is float and value==floor(value):return int(value)
\treturn value
func _init():
\tfor arg in OS.get_cmdline_args():
\t\tif arg.begins_with("--encore-out="):output=arg.substr(13)
\tassert(output!=null)
\tfixtures=normalize_numbers(read_json("res://fixtures.json"))
\tmetadata=read_json("res://metadata.json")
\tcall_deferred("run")
func double_bits(x):
\tvar b=StreamPeerBuffer.new()
\tb.put_double(x)
\tvar encoded=""
\tfor byte in b.data_array:encoded+="%02x"%byte
\treturn encoded
func _idle(_dt):
\ttick+=1
\tframe_deltas.append({"tick":tick,"engine_idle":Engine.get_idle_frames(),"delta":"%.17f"%_dt,"double_le_hex":double_bits(_dt)})
\tphase="idle_callback"
\tif tick>3000:
\t\tpush_error("Battle round probe exceeded bounded observation window")
\t\tquit(3)
\treturn false
func event(name,data={}):
\tvar e={"event":name,"frame":tick-start_tick,"engine_idle":Engine.get_idle_frames()}
\tfor k in data:e[k]=data[k]
\tevents.append(e)
func native_timer(site,duration):
\tvar timer=create_timer(duration)
\ttimer_serial+=1
\tvar timer_id=timer_serial
\tevent("timer_create",{"site":site,"timer":timer_id,"duration":"%.17f"%duration,"native_time_left":"%.17f"%timer.time_left})
\ttimer.connect("timeout",self,"native_timer_timeout",[site,timer_id])
\treturn timer
func native_timer_timeout(site,timer_id):
\tevent("timer_timeout",{"site":site,"timer":timer_id})
func record_draw(site,op,value,raw,state,a=null,b=null):
\traw_count+=raw.size()
\tvar result={"site":site,"op":op,"value":value,"raw":raw,"raw_count":raw_count,"state_before":str(state),"state_after":str(mirror.state),"frame":tick-start_tick}
\tif a!=null:result.a=a
\tif b!=null:result.b=b
\tdraws.append(result)
func draw_i(site):
\tvar state=mirror.state
\tvar x=randi()
\tvar raw=mirror.randi()
\tassert(x==raw)
\trecord_draw(site,"randi",str(x),[str(raw)],state)
\treturn x
func draw_f(site):
\tvar state=mirror.state
\tvar x=randf()
\tvar raw=mirror.randi()
\trecord_draw(site,"randf","%.17f"%x,[str(raw)],state)
\treturn x
func draw_range(site,a,b):
\tvar state=mirror.state
\tvar x=rand_range(a,b)
\tvar raw=[str(mirror.randi())]
\tif raw[0]!="0":
\t\traw.append(str(mirror.randi()))
\t\traw.append(str(mirror.randi()))
\trecord_draw(site,"rand_range","%.17f"%x,raw,state,a,b)
\treturn x
func log_queue(q):
\tvar names=[]
\tfor a in q:names.append(a.user.label+":"+a.skill.id)
\tevent("queue_order", {"actions":names})
func log_targets(a):
\tvar names=[]
\tfor target in a.targets:names.append(target.label)
\tevent("targets", {"user":a.user.label,"skill":a.skill.id,"targets":names})
func signal_action(name, a):
\tevent(name,{"user":a.user.label,"skill":a.skill.id})
func round_done(turn):event("round_done",{"turn":turn})
func new_service():
\tvar s=load("res://services.gd").new()
\ts.runner=self
\tbattle.add_child(s)
\treturn s
func new_participant(label,kind,stats):
\tvar p=load("res://participant.gd").new()
\tp.runner=self
\tp.label=label
\tp.kind=kind
\tp.stats=stats.duplicate(true)
\tbattle.add_child(p)
\tp.character=new_service()
\tp.character.label=label
\tp.character.stats=stats
\tp.plate=new_service()
\tp.sprite=load("res://sprite.gd").new()
\tp.sprite.runner=self
\tp.sprite.label=label
\tbattle.add_child(p.sprite)
\tp.connect("before_action", self,"signal_action",["before_action"])
\tp.disconnect("before_action",self,"signal_action")
\tp.connect("before_action",self,"before_action_signal")
\tp.connect("acted",self,"acted_signal")
\treturn p
func before_action_signal(a):signal_action("before_action",a)
func acted_signal(a):signal_action("acted",a)
func new_battle():
\tbattle=load("res://battle.gd").new()
\tbattle.runner=self
\tget_root().add_child(battle)
\tbattle.global=new_service()
\tbattle.globaldata=new_service()
\tbattle.globaldata.skills=fixtures.skills
\tbattle._context=new_service()
\tvar dialog=new_service()
\tdialog.name="Dialoguebox"
\tvar menu=Control.new()
\tmenu.name="ActionMenuBox"
\tbattle.add_child(menu)
\tvar smash=Node.new()
\tsmash.name="SMASHBOX"
\tbattle.add_child(smash)
\tfor name in ["ScreenEffect","PreHitEffect"]:
\t\tvar fx=Control.new()
\t\tfx.name=name
\t\tbattle.add_child(fx)
\t\tvar player=AnimationPlayer.new()
\t\tplayer.name="AnimationPlayer"
\t\tfx.add_child(player)
\tvar n=new_participant("ninten",0,fixtures.ninten)
\tvar l=new_participant("lamp",2,fixtures.lamp)
\tfor data in fixtures.lamp.skills:
\t\tl.skills.append(load("res://enemy_skill.gd").new(data))
\tbattle._party_BPs=[n]
\tbattle._enemy_BPs=[l]
\tbattle.connect("round_done",self,"round_done")
\treturn [n,l]
func action_done(a):signal_action("action_done",a)
func attach_action_done(a):
\tif !a.is_connected("done",self,"action_done"):a.connect("done",self,"action_done",[a])
func run_case(name,random_seed,equal_speed=false,reverse=false):
\tyield(self,"idle_frame")
\tcase_name=name
\tevents=[]
\tdraws=[]
\tboundary=""
\tstart_tick=tick
\tvar p=new_battle()
\tif equal_speed:p[1].stats.speed=p[0].stats.speed
\tseed(random_seed)
\tmirror.seed=random_seed
\traw_count=0
\tvar a=battle.SkillAction.new(p[0])
\ta.skill=fixtures.skills.attack
\ta.targets=[p[1]]
\tattach_action_done(a)
\tbattle._cache_action(a)
\t# Original action-choice method caches enemy before the 0.3s wait.
\t# Queue listeners are attached immediately afterward, before action execution.
\tevent("invoke_round",{"seed":random_seed})
\tbattle._end_player_action_choices()
\tfor other in battle._action_queue:attach_action_done(other)
\tif reverse:battle._action_queue.invert()
\twhile boundary=="":
\t\tyield(self,"idle_frame")
\t\tphase="idle_signal"
\t# Observe result-boundary suspension rather than manufacturing action.done.
\tfor _i in range(2):yield(self,"idle_frame")
\tvar next_raw=str(randi())
\tassert(next_raw==str(mirror.randi()))
\tvar result={"name":name,"seed":random_seed,"boundary":boundary,"hp":{"ninten":p[0].get_target_hp(),"lamp":p[1].get_target_hp()},"turn":battle._turns_count,"action_index":battle._current_action_index,"events":events.duplicate(true),"draws":draws.duplicate(true),"next_randi":next_raw,"raw_draw_count":raw_count,"start_tick":start_tick,"end_tick":tick}
\tcases.append(result)
\tfor ac in battle._action_queue:ac.free()
\tp[1].skills.clear()
\tbattle.free()
\tyield(self,"idle_frame")
func threshold_timeout():
\tthreshold_fired=true
\tevent("exact_timer_timeout")
func timer_done(name,created):
\tevent("timer_timeout",{"name":name,"elapsed_frames":tick-created})
func run():
\t# Find representative seeds using the real native global stream, then reseed
\t# at each independent run. Probe draws are outside every reference case.
\tvar tackle_seed=-1
\tvar float_seed=-1
\tvar smash_seed=-1
\tfor s in range(1000):
\t\tseed(s)
\t\tvar choice=rand_range(0.0,3.0)
\t\tvar smash=randi()%100+1<=5
\t\tif !smash and choice<=2 and tackle_seed<0:tackle_seed=s
\t\tif !smash and choice>2 and float_seed<0:float_seed=s
\t\tif smash and smash_seed<0:smash_seed=s
\t\tif min(tackle_seed,min(float_seed,smash_seed))>=0:break
\ttests.representative_seeds={"tackle":tackle_seed,"float":float_seed,"smash":smash_seed}
\tfor s in [0,1,123]:yield(run_case("seed_"+str(s),s),"completed")
\tyield(run_case("ordinary_tackle",tackle_seed),"completed")
\tyield(run_case("ordinary_float",float_seed),"completed")
\tyield(run_case("smash_lethal",smash_seed),"completed")
\tyield(run_case("equal_speed_player_inserted_first",tackle_seed,true,false),"completed")
\tyield(run_case("equal_speed_lamp_inserted_first",tackle_seed,true,true),"completed")
\tvar ps=new_battle()
\tevents=[]
\tdraws=[]
\tseed(123)
\tvar expected=str(randi())
\tseed(123)
\tmirror.seed=123
\traw_count=0
\tvar false_chance=battle._chance_roll(0)
\tvar false_multiplier=battle._chance_roll(50,0)
\tvar a=battle.SkillAction.new(ps[0])
\ta.skill=fixtures.skills.attack
\tvar missed=battle._get_miss_chance(a)
\tvar observed=str(randi())
\ttests.zero_chance={"chance":false_chance,"zero_multiplier":false_multiplier,"miss":missed,"expected_next_randi":expected,"observed_next_randi":observed,"draws":draws.duplicate(true)}
\tassert(expected==observed and draws.empty() and !false_chance and !missed)
\ta.free()
\tps[1].skills.clear()
\tbattle.free()
\tevents=[]
\tstart_tick=tick
\tvar observed_timers=[]
\tfor time in [0.0,0.05,0.08,0.3,0.4,0.5]:
\t\tvar t=create_timer(time)
\t\tobserved_timers.append({"name":str(time),"timer":t})
\t\tt.connect("timeout",self,"timer_done",[str(time),tick])
\t\tevent("timer_created",{"name":str(time),"time_left":"%.17f"%t.time_left})
\tfor _i in range(33):
\t\tyield(self,"idle_frame")
\t\tfor observed_timer in observed_timers:
\t\t\tevent("timer_remaining",{"name":observed_timer.name,"time_left":"%.17f"%observed_timer.timer.time_left,"double_le_hex":double_bits(observed_timer.timer.time_left)})
\ttests.timer_threshold=events.duplicate(true)
\tEngine.time_scale=3.75
\tfor _i in range(2):yield(self,"idle_frame")
\tevents=[]
\tstart_tick=tick
\tvar exact=create_timer(0.125)
\texact.connect("timeout",self,"threshold_timeout")
\tevent("exact_timer_created",{"time_left":"%.17f"%exact.time_left,"bits":double_bits(exact.time_left)})
\tfor _i in range(4):
\t\tyield(self,"idle_frame")
\t\tevent("exact_timer_sample",{"time_left":"%.17f"%exact.time_left,"bits":double_bits(exact.time_left),"fired":threshold_fired})
\ttests.strict_zero_threshold=events.duplicate(true)
\tEngine.time_scale=1.0
\tvar f=File.new()
\tassert(f.open(output,File.WRITE)==OK)
\tf.store_string(JSON.print({"schema":1,"engine":Engine.get_version_info(),"metadata":metadata,"cases":cases,"tests":tests,"frame_deltas":frame_deltas},"  "))
\tf.close()
\tprint("BATTLE_ROUND_REFERENCE_COMPLETE")
\tquit()
'''


def prepare(work):
    ex = Extractor(ROOT)
    ir = ex.build()
    source = ex.text(SYSTEM)
    require(not work.exists() and work.is_relative_to(ROOT/'build/battle-round-reference'), 'Use fresh work directory under build/battle-round-reference')
    work.mkdir(parents=True)
    class_source = source[source.index('class Action extends Object:'):source.index('class ItemAction extends SkillAction:')]
    class_source = adapt(class_source, 'SkillAction.get_dialog', trace=False)
    # The nested source class has no runner property: source dialog in this scope
    # is always a String, so random dialog-array selection must fail closed.
    class_source = class_source.replace('runner.draw_i("SkillAction.get_dialog")', 'randi()')
    battle = PREFIX + '\n' + class_source
    battle += '\nclass ItemAction extends SkillAction:\n\tpass\nclass FleeAction extends Action:\n\tpass\n'
    for method in METHODS:
        battle += '\n' + adapt(function(source, method), method)
    # Compile-time references from unused source branches are visible and fail closed.
    known = set(re.findall(r'^func (\w+)', battle, re.M))
    calls = set(re.findall(r'(?<![.\w])(_[a-zA-Z_]\w*)\(', battle))
    stubs = sorted(calls-known-{'_init','_set_skill'})
    for name in stubs:
        # GDScript 3 has no variadic function parameters. Use enough optional slots.
        battle += '\nfunc '+name+'(_a=null,_b=null,_c=null,_d=null,_e=null):\n\tpush_error("Unexpected out-of-scope branch: '+name+'")\n\tassert(false)\n\treturn null\n'
    (work/'battle.gd').write_text(battle)
    enemy_source = ex.text('Scripts/UI/Battle/EnemySkill.gd').replace('class_name EnemySkill', 'extends Reference')
    (work/'enemy_skill.gd').write_text(enemy_source)
    flying_source = ex.text('Scripts/UI/Battle/FlyingNumber.gd')
    flying = 'extends Label\nsignal done\nvar runner\n'+adapt(function(flying_source,'run'),'FlyingNumber.run')
    (work/'flying.gd').write_text(flying)
    (work/'participant.gd').write_text(PARTICIPANT)
    (work/'services.gd').write_text(SERVICES)
    enemy_sprite=ex.text('Scripts/UI/Battle/EnemySprite.gd')
    party_sprite=ex.text('Scripts/UI/Battle/BattleSpriteParty.gd')
    (work/'sprite.gd').write_text(SPRITE+'\n'+adapt(function(enemy_sprite,'attack'),'EnemySprite.attack')+'\n'+adapt(function(party_sprite,'_apply_damage'),'BattleSpriteParty._apply_damage')+'\n'+adapt(function(party_sprite,'_try_pause'),'BattleSpriteParty._try_pause'))
    clip=animation(ex.text('Nodes/Ui/Battle/BattleSpriteNinten.tscn'),3,'Nodes/Ui/Battle/BattleSpriteNinten.tscn','bash')
    (work/'bash_animation.json').write_text(json.dumps(clip,indent=2)+'\n')
    (work/'probe.gd').write_text(PROBE)
    import yaml
    skills={name:dict(yaml.safe_load(ex.text('Data/BattleSkills/'+name+'.yaml')),id=name) for name in ['attack','tackle','float']}
    lamp=yaml.safe_load(ex.text('Data/Battlers/lamp.yaml'))
    fixtures={'ninten':ir['party']['effective_stats'],'lamp':lamp,'skills':skills}
    (work/'fixtures.json').write_text(json.dumps(fixtures,indent=2)+'\n')
    metadata={'commit':ir['commit'],'scope':'isolated original BattleSystem round methods; healthy first-round Ninten/Lamp; native RNG, double Variant arithmetic, deferred/yield/SceneTreeTimer semantics',
      'methods':METHODS,'sources':ex.sources,'source_method_sha256':{m:hashlib.sha256(function(source,m).encode()).hexdigest() for m in METHODS},
      'transformations':['remove unavailable custom type annotations; replace := with = (native scalar annotations retained)','insert event logging around entry/yield/deferred; forward global random calls through logging wrappers and native timer creation through observer wrapper','restore YAML integral numeric types after JSON fixture transport','source Action/SkillAction inner classes retained, unused ItemAction/FleeAction replaced with inert compile-time classes'],
      'adapters':['healthy participant stats/target HP with direct integer clamping; no original scrolling HP plate','dialogue completes after one native idle frame; not original glyph/text timing','Ninten bash native AnimationPlayer source tracks; source EnemySprite.attack native tween; nonblocking hide/hit/flash visual methods recorded only','sound/vibration/SP/hit-effect are recorded; no audio/GPU; bound text-only formatted dialogue has no pitch RNG','FlyingNumber.run is extracted source and consumes actual global RNG; its native tween is retained','SMASH visuals and global slow-motion recorded but not applied; source combat values remain unchanged','win records boundary and deactivates; no rewards, transition, world resume, or fabricated done','no statuses/passives/NPCs/encore/items; unexpected branches assert'],'unreachable_assert_stubs':stubs}
    (work/'metadata.json').write_text(json.dumps(metadata,indent=2)+'\n')
    (work/'project.godot').write_text('config_version=4\n[application]\nconfig/name="Encore source battle round reference"\n[logging]\nfile_logging/enable_logging=false\n[physics]\ncommon/physics_fps=60\n')
    return metadata


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--godot',type=Path,required=True)
    parser.add_argument('--work',type=Path,required=True)
    parser.add_argument('--reports',type=Path,required=True)
    args=parser.parse_args()
    work=args.work.resolve(); reports=args.reports.resolve()
    require(not reports.exists() and reports.is_relative_to(ROOT/'reports/battle-round-reference'),'Use fresh reports subdirectory')
    metadata=prepare(work)
    reports.mkdir(parents=True)
    env=dict(os.environ)
    for key in ['XDG_DATA_HOME','XDG_CONFIG_HOME','XDG_CACHE_HOME']:
        directory=work/key.lower();directory.mkdir();env[key]=str(directory)
    command=[str(args.godot.resolve()),'--path',str(work),'--fixed-fps','60','--script',str(work/'probe.gd'),'--encore-out='+str(reports/'reference.json')]
    with (reports/'godot.txt').open('wb') as logfile:
        run=subprocess.run(command,stdout=logfile,stderr=subprocess.STDOUT,env=env,timeout=45)
    log=(reports/'godot.txt').read_text()
    require(run.returncode==0 and 'SCRIPT ERROR' not in log and 'ERROR:' not in log and 'BATTLE_ROUND_REFERENCE_COMPLETE' in log,'Native reference failed; inspect retained godot.txt')
    result=json.loads((reports/'reference.json').read_text())
    require(result['engine']['string']=='3.6.2-stable (official)','Wrong native engine')
    require(len(result['cases'])==8,'Incomplete cases')
    for case in result['cases']:
        require(case['boundary'] in ['win','next_menu'],'Unresolved boundary')
    receipt={'schema':1,'commit':metadata['commit'],'scope':metadata['scope'],'engine_sha256':sha(args.godot),'tool_sha256':sha(__file__),'reference_sha256':sha(reports/'reference.json'),'generated_scripts_sha256':{p.name:sha(p) for p in work.glob('*.gd')},'full_game_reference':'not run','hardware':'not run','render_reference':'not run'}
    (reports/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
    summary={'scope':metadata['scope'],'timing_limits':metadata['adapters'],'cases':[{k:c[k] for k in ['name','seed','boundary','hp','turn','action_index','raw_draw_count','next_randi','start_tick','end_tick']} | {'events':[e for e in c['events'] if e['event'] in ['invoke_round','queue_order','targets','damage','hp_target','defeat','action_done','round_done','next_menu','win_boundary','timer_create','timer_timeout']],'draws':c['draws']} for c in result['cases']], 'tests':result['tests'],'frame_delta_sequence':'frame-deltas.json'}
    (reports/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    (reports/'frame-deltas.json').write_text(json.dumps(result['frame_deltas'],indent=2)+'\n')
    print(json.dumps([{'name':c['name'],'seed':c['seed'],'boundary':c['boundary'],'hp':c['hp'],'draws':c['draws']} for c in result['cases']],indent=2))
    print('Native source battle round reference complete:',reports)

if __name__=='__main__':
    try:main()
    except (ValueError,OSError,KeyError,subprocess.SubprocessError) as error:
        print('BATTLE ROUND REFERENCE ERROR: '+str(error),file=sys.stderr)
        sys.exit(1)
