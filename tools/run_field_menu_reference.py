#!/usr/bin/env python3
"""Bounded original Pause/PSI methods in official Godot 3.6.2, with named adapters."""
import argparse, hashlib, json, re, shutil, subprocess, sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from tools.extract_battle_entry import Extractor, require
from tools.run_battle_victory_reference import function, sha
from tools.field_equipment import load

SERVICE='''extends Node
var paused=true
var cash=0
var skills={}
var events=[]
var telepathy_calls=0
var SKILL_TELEPATHY="telepathy"
var SKILL_TELEPORT_A="teleportA"
func get_battle_skill(id):return skills.get(id,{})
func get_player():return self
func use_telepathy():
	telepathy_calls+=1
	events.append("telepathy")
func close_commands_menu(keep_paused=false,_bars=false,_silent=false,_from=false):
	paused=keep_paused
	events.append("close_commands")
func play_sfx_by_name(_a,_b):pass
func info_plates_update():pass
func info_plates_show():pass
func info_plates_hide():pass
func toggle_black_bars(_a):pass
func music_muffle(_a,_b):pass
func close_key_indicator():pass
func update_key_indicator():pass
'''
ARROW='''extends Node2D
var on=false
var index=3
func set_cursor_from_index(i,_sound):index=i
'''
MEMBER='''extends Node
var pp=26
var active=false
var forgetful=false
func get_pp():return pp
func set_pp(value):pp=value
func has_status(status):return status=="forgetful" and forgetful
'''
PAUSE='''extends Node
enum CloseFlags {SILENT=1,UNPAUSE=2,HIDE_BARS=4}
var globaldata
var uiManager
var audioManager
var _arrow
var _cash_box
var _animation_player
var _active=false
var _visible=false
'''
SELECT='''extends Node
enum PSIType {Overworld=-1,Both,Battle}
var _psi_type=-1
var globaldata
var user
var _skills=[]
var _skills_condensed=[]
var _lines=[]
var _index=0
var page=0
var _cursor_to_0=false
var cursor
var _active=false
signal using_skill_failed(skill)
signal use(skill)
signal selected(skill)
func _update_page(_a):pass
func _set_line_active(_a,_b):pass
func _on_box_sort():pass
'''
LINE='''extends Node
var box
func get_hbox():return box
'''
PSI='''extends Node
signal back
var _skill={}
var _current_character
var _psi_select
var _character_tab
var globaldata
var uiManager
var global
var audioManager
var active=true
'''
PROBE='''extends SceneTree
var output
var fixture
var result={}
func _init():
	for arg in OS.get_cmdline_args():
		if arg.begins_with("--out="):output=arg.substr(6)
	var f=File.new()
	assert(f.open("res://fixture.json",File.READ)==OK)
	fixture=JSON.parse(f.get_as_text()).result
	call_deferred("run")
func ap(parent,open_length,close_length):
	var n=AnimationPlayer.new()
	n.name="AnimationPlayer"
	parent.add_child(n)
	for pair in [["open",open_length],["Open",open_length],["Close",close_length]]:
		var a=Animation.new()
		a.length=pair[1]
		n.add_animation(pair[0],a)
	return n
func run():
	var service=load("res://service.gd").new()
	service.skills=fixture.skills
	get_root().add_child(service)
	var pause=load("res://pause.gd").new()
	pause.globaldata=service
	pause.uiManager=service
	pause.audioManager=service
	pause._arrow=load("res://arrow.gd").new()
	pause.add_child(pause._arrow)
	pause._cash_box=Node.new()
	pause.add_child(pause._cash_box)
	var amount=Label.new()
	amount.name="Amount"
	pause._cash_box.add_child(amount)
	get_root().add_child(pause)
	pause._animation_player=ap(pause,fixture.pause_open,fixture.pause_close)
	for kind in ["select","cancel"]:
		service.paused=true
		pause.open()
		for _i in range(15):yield(self,"idle_frame")
		assert(pause._active and pause._visible and pause._arrow.on)
		if kind=="select":
			if !InputMap.has_action("ui_select"):InputMap.add_action("ui_select")
			var event=InputEventAction.new()
			event.action="ui_select"
			event.pressed=true
			pause._input(event)
		else:pause._on_Arrow_cancel()
		for _i in range(10):yield(self,"idle_frame")
		result["pause_"+kind+"_close"]={"paused":service.paused,"active":pause._active,"visible":pause._visible,"cursor":pause._arrow.index}
		assert(!service.paused and !pause._active and !pause._visible and pause._arrow.index==0)
	var select=load("res://select.gd").new()
	select.globaldata=service
	var line=load("res://line.gd").new()
	line.box=HBoxContainer.new()
	line.add_child(line.box)
	select._lines=[line]
	select.add_child(line)
	select.cursor=pause._arrow
	get_root().add_child(select)
	var member=load("res://member.gd").new()
	member.pp=fixture.pp
	get_root().add_child(member)
	select.user=member
	select.update_skills(fixture.unlearned)
	select._cursor_selected(0)
	result.psi_unlearned={"skills":select._skills.size(),"pp":member.pp,"telepathy_calls":service.telepathy_calls}
	assert(select._skills.empty() and member.pp==fixture.pp and service.telepathy_calls==0)
	select.update_skills(fixture.learned)
	assert(select._skills.size()==1 and select._skills[0].id=="telepathy")
	assert(select._does_it_do_anything(select._skills[0]))
	member.pp=0
	assert(select._does_it_do_anything(select._skills[0]))
	member.forgetful=true
	assert(!select._does_it_do_anything(select._skills[0]))
	member.forgetful=false
	var psi=load("res://psi.gd").new()
	psi._psi_select=select
	psi._character_tab=member
	psi._current_character=member
	psi.global=service
	psi.globaldata=service
	psi.audioManager=service
	psi.uiManager=service
	get_root().add_child(psi)
	ap(psi,fixture.psi_open,fixture.psi_close)
	psi.connect("back",pause,"_on_submenu_back")
	service.paused=true
	psi._close()
	result.psi_back={"pause_active":pause._active,"paused":service.paused,"psi_active":psi.active}
	assert(pause._active and service.paused and !psi.active)
	psi.active=true
	service.events=[]
	psi._use_skill(select._skills[0])
	result.telepathy_use={"pp":member.pp,"paused":service.paused,"calls":service.telepathy_calls,"events":service.events}
	assert(member.pp==0 and !service.paused and service.telepathy_calls==1 and service.events==["close_commands","telepathy"])
	var f=File.new()
	assert(f.open(output,File.WRITE)==OK)
	f.store_string(JSON.print({"engine":Engine.get_version_info(),"cases":result},"  "))
	f.close()
	print("FIELD_MENU_REFERENCE_COMPLETE")
	quit()
'''

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--godot',type=Path,required=True)
    p.add_argument('--out',type=Path,required=True)
    a=p.parse_args();out=a.out.resolve()
    require(not out.exists() and out.is_relative_to(ROOT/'build/emulator-regression'),'Fresh reference output required')
    out.mkdir(parents=True);work=out/'temporary';work.mkdir()
    ex=Extractor(ROOT);ir=load(ROOT);methods={}
    def extract(path,names):
        source=ex.text(path);parts=[]
        for name in names:
            code=function(source,name);methods[path+':'+name]=hashlib.sha256(code.encode()).hexdigest()
            parts.append(code.replace(':=','='))
        return '\n'.join(parts)
    pause=PAUSE+extract('Scripts/UI/Pausemenu.gd',['open','_input','_deactivate','_activate','_on_submenu_back','_on_Arrow_cancel','_close_view','close'])
    select=SELECT+extract('Scripts/UI/PSISelect.gd',['update_skills','_condense_skills','is_empty','set_active','refresh_selectable','_cursor_selected','_does_it_do_anything'])
    # Only the SELF arm is in this bounded fixture. The real numeric identity is
    # checked in the source skill; no other target behavior is admitted here.
    target_source=ex.text('Scripts/UI/Battle/BattleSystem.gd')
    target_match=re.search(r'enum TargetType\s*\{([^}]+)\}',target_source)
    require(target_match is not None,'Missing source TargetType')
    targets=[v.strip() for v in target_match[1].split(',')]
    require(targets.index('SELF')==5,'Unreviewed SELF enum identity')
    constants=ex.text('Scripts/global/globalData.gd')
    require('const SKILL_TELEPATHY := "telepathy"' in constants and 'const SKILL_TELEPORT_A := "teleportA"' in constants,'Unreviewed PSI source constants')
    select=select.replace('BattleSystem.TargetType.SELF',str(targets.index('SELF')))
    psi=PSI+extract('Scripts/UI/PSIMenuUI.gd',['_close','_use_skill'])
    skills={}
    for name in ['strike','splitShot','telepathy']:
        skill=ex.yaml('Data/BattleSkills/'+name+'.yaml');skill['id']=name;skills[name]=skill
    require(skills['telepathy']['pp_cost']==0 and skills['telepathy']['target_type']==5,'Unreviewed telepathy target/cost')
    session=json.loads((ROOT/'content/native-session.json').read_text())
    character=session['startup']['characters'][0]
    fixture={'skills':skills,'pp':character['pp'],'unlearned':character['learned_skills'],'learned':character['learned_skills']+['telepathy']}
    for key,role in [('pause_open','PauseOpen'),('pause_close','PauseClose'),('psi_open','PsiOpen'),('psi_close','PsiClose')]:
        fixture[key]=next(c['duration'] for c in ir['clips'] if c['role']==role)
    scripts={'service.gd':SERVICE,'arrow.gd':ARROW,'member.gd':MEMBER,'line.gd':LINE,'pause.gd':pause,'select.gd':select,'psi.gd':psi,'probe.gd':PROBE}
    for name,code in scripts.items():(work/name).write_text(code,encoding='utf-8')
    (work/'fixture.json').write_text(json.dumps(fixture),encoding='utf-8')
    (work/'project.godot').write_text('config_version=4\n[application]\nconfig/name="Bounded original menu oracle"\n[logging]\nfile_logging/enable_logging=false\n')
    command=[str(a.godot.resolve()),'--no-window','--path',str(work),'--fixed-fps','60','--script',str(work/'probe.gd'),'--out='+str(out/'reference.json')]
    with (out/'godot.log').open('wb') as log:
        result=subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,timeout=45)
    log=(out/'godot.log').read_text(encoding='utf-8',errors='replace')
    require(result.returncode==0 and 'SCRIPT ERROR' not in log and 'ERROR:' not in log and 'FIELD_MENU_REFERENCE_COMPLETE' in log,'Original menu oracle failed; retained log')
    data=json.loads((out/'reference.json').read_text())
    require(data['engine']['string']=='3.6.2-stable (official)','Wrong reference engine')
    receipt={'commit':ex.lock['commit'],'engine_sha256':sha(a.godot),'tool_sha256':sha(__file__),'sources':ex.sources,'methods_sha256':methods,'reference_sha256':sha(out/'reference.json'),'generated_scripts_sha256':{name:sha(work/name) for name in scripts},'fixture_sha256':sha(work/'fixture.json'),
             'adapters':['Native AnimationPlayer uses checked source durations, without rendering tracks; glyphs, texture geometry and timing equivalence are not tested',
                         'UI/audio/world service records pause and call order; no real sound or world movement',
                         'Character adapter stores PP/status; source PSI selection and PP subtraction execute unchanged',
                         'Skill dictionaries read from checked source YAML and identity appended as the original lookup does',
                         'PSI line layout/page refresh and cursor containers are stubs; filtering/empty selection logic executes original methods',
                         'TargetType.SELF is replaced by verified value 5; no other targeting path exercised'],
             'scope':'Bounded extracted original Pause open/cancel/select close, PSI filtering/back and zero-PP telepathy dispatch; not full original game, input device, graphics, or hardware'}
    (out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
    shutil.rmtree(work)  # this fresh, owned source work copy; original logs/receipts remain
    print(json.dumps(data['cases'],indent=2))
if __name__=='__main__':main()
