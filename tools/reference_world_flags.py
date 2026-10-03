#!/usr/bin/env python3
"""Run exact reviewed flag functions in an isolated Godot 3.6.2 project."""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess

ROOT=Path(__file__).resolve().parents[1]
REVIEW=ROOT/'compatibility/reviews/world-flags-v0410.json'

def digest(path): return hashlib.sha256(path.read_bytes()).hexdigest()
def functions(source,names):
    # Whole source hash is checked before this bounded extraction. This is a
    # harness builder, not a general semantic or compatibility approval parser.
    result=[]
    for name in names:
        pattern=r'^func '+re.escape(name)+r'\([^\n]*\n.*?(?=^func |\Z)'
        found=re.search(pattern,source,re.M|re.S)
        if not found: raise ValueError('Missing reviewed function '+name)
        result.append(found.group(0).rstrip()+'\n')
    return '\n'.join(result)

def prepare(source,work):
    review=json.loads(REVIEW.read_text())
    for name,expected in review['sources'].items():
        if digest(source/name)!=expected: raise ValueError('Changed unreviewed source: '+name)
    runtime=(ROOT/'runtime/world_flags.cpp').read_text()
    gd=(source/'Scripts/global/globalData.gd').read_text()
    init=functions(gd,['_init_flags'])
    names=re.findall(r'^\s*"([^"\n]+)"\s*,?\s*(?:#.*)?$',init,re.M)
    actual=re.findall(r'"([^"\n]+)"',runtime.split('names{{\n',1)[1].split('\n}};',1)[0])
    if names!=actual or len(names)!=178: raise ValueError('Native flag registry differs from reviewed _init_flags')
    work.mkdir(parents=True)
    (work/'project.godot').write_text('''config_version=4
[application]
config/name="Encore isolated world flag reference"
[logging]
file_logging/enable_logging=false
''')
    (work/'global.gd').write_text('''extends Node
signal flags_updated
var count := 0
var currentScene: Node
func _ready():
    connect("flags_updated", self, "_count")
    currentScene = Node.new()
    currentScene.name = "Ninten's House"
    add_child(currentScene)
func _count():
    count += 1
''')
    (work/'globaldata.gd').write_text('extends Node\nvar global\nvar flags := {}\nvar object_flags := {}\nfunc _init():\n\t_init_flags()\n'+functions(gd,['_init_flags','check_appear_disappear_flags','set_flag','set_object_flag']))
    (work/'landmark.gd').write_text((source/'Scripts/Main/Flag Landmarks.gd').read_text()+'\nvar global\nvar globaldata\n')
    (work/'flaggable.gd').write_text('''extends Sprite
var global
var globaldata
var flag := ""
var is_object_flag := false
var emit_flag_updated_signal := false
var reset_when_leaving_area := false
var reset_when_leaving_region := false
'''+functions((source/'Scripts/Main/FlaggableObject.gd').read_text(),['_get_flag_status','_set_flag_status','_on_leave_area']))
    (work/'npc.gd').write_text('''extends Node2D
var collisions := CollisionShape2D.new()
var interact_area := CollisionShape2D.new()
var dialogue := true
func _init():
    add_child(collisions)
    add_child(interact_area)
func has_dialog():
    return dialogue
'''+functions((source/'Scripts/Main/npc.gd').read_text(),['update_visibility_changed']))
    (work/'prompt.gd').write_text('extends Node\nvar enabled := false\n')
    (work/'door.gd').write_text('''extends Sprite
var globaldata
var key := ""
var blocked := false
var one_way := false
var locked := false
var flag := ""
var _unlocked := true
'''+functions((source/'Scripts/Main/Openable Door.gd').read_text(),['_update_door_state','unlock','lock']))
    (work/'probe.gd').write_text(PROBE)
    return review

PROBE='''extends SceneTree
var global
var globaldata
var results := {"appearance": [], "doors": [], "npc": [], "landmarks": [], "writes": [], "objects": []}
func _init():
    call_deferred("run")
func reset_flags():
    globaldata._init_flags()
    globaldata.object_flags.clear()
func run():
    global = load("res://global.gd").new()
    root.add_child(global)
    globaldata = load("res://globaldata.gd").new()
    globaldata.global = global
    root.add_child(globaldata)
    reset_flags()
    results["flag_names"] = globaldata.flags.keys()
    for bits in 4:
        globaldata.flags["poltergeist"] = bool(bits & 1)
        globaldata.flags["doll_melody"] = bool(bits & 2)
        for appear in ["", "poltergeist", "doll_melody", "missing"]:
            for disappear in ["", "poltergeist", "doll_melody", "missing"]:
                results.appearance.append([bits, appear, disappear, globaldata.check_appear_disappear_flags(appear, disappear)])
    for object in [false, true]:
        for name in ["poltergeist", "missing", ""]:
            for value in [false, true]:
                for emit in [false, true]:
                    reset_flags()
                    var before = global.count
                    if object: globaldata.set_object_flag(name, value, emit)
                    else: globaldata.set_flag(name, value, emit)
                    var dict = globaldata.object_flags if object else globaldata.flags
                    results.writes.append([object, name, value, emit, dict.has(name), dict.get(name, false), global.count - before])
    for parent_shown in [false, true]:
        for shown in [false, true]:
            for dialogue in [false, true]:
                var parent = Node2D.new()
                parent.visible = parent_shown
                var npc = load("res://npc.gd").new()
                parent.add_child(npc)
                npc.visible = shown
                npc.dialogue = dialogue
                root.add_child(parent)
                npc.update_visibility_changed()
                results.npc.append([parent_shown, shown, dialogue, npc.collisions.disabled, npc.interact_area.disabled, npc.is_physics_processing()])
                parent.free()
    for blocker in 16:
        for flag in ["", "poltergeist", "missing"]:
            for value in [false, true]:
                reset_flags()
                globaldata.flags["poltergeist"] = value
                var door = load("res://door.gd").new()
                door.globaldata = globaldata
                door.key = "key" if blocker & 1 else ""
                door.blocked = bool(blocker & 2)
                door.one_way = bool(blocker & 4)
                door.locked = bool(blocker & 8)
                door.flag = flag
                var body = StaticBody2D.new()
                body.name = "StaticBody2D"
                var shape = CollisionShape2D.new()
                shape.name = "CollisionShape2D"
                shape.disabled = true
                body.add_child(shape)
                door.add_child(body)
                var interact = Node.new()
                interact.name = "interact"
                var prompt = load("res://prompt.gd").new()
                prompt.name = "ButtonPrompt"
                interact.add_child(prompt)
                door.add_child(interact)
                root.add_child(door)
                door._update_door_state()
                yield(self, "idle_frame")
                results.doors.append([blocker, flag, value, door._unlocked, shape.disabled, prompt.enabled])
                door.free()
    reset_flags()
    for delete_if_hidden in [false, true]:
        var node = load("res://landmark.gd").new()
        node.global = global
        node.globaldata = globaldata
        node.disappear_flag = "poltergeist"
        node.delete_if_hidden = delete_if_hidden
        root.add_child(node)
        globaldata.set_flag("poltergeist", true)
        var hidden_visible = node.visible
        var hidden_queued = node.is_queued_for_deletion()
        globaldata.set_flag("poltergeist", false)
        var restored_visible = node.visible
        var restored_queued = node.is_queued_for_deletion()
        yield(self, "idle_frame")
        var remains = is_instance_valid(node)
        results.landmarks.append([delete_if_hidden, hidden_visible, hidden_queued, restored_visible, restored_queued, remains])
        if remains: node.free()
    for explicit in ["", "poltergeist", "missing"]:
        for object in [false, true]:
            for emit in [false, true]:
                reset_flags()
                var node = load("res://flaggable.gd").new()
                node.global = global
                node.globaldata = globaldata
                node.name = "Present2"
                node.flag = explicit
                node.is_object_flag = object
                node.emit_flag_updated_signal = emit
                root.add_child(node)
                var before = global.count
                node._set_flag_status(true)
                results.objects.append([explicit, object, emit, node._get_flag_status(), global.count-before, globaldata.object_flags.keys()])
                node.free()
    var output = ""
    for arg in OS.get_cmdline_args():
        if arg.begins_with("--encore-out="): output = arg.trim_prefix("--encore-out=")
    var f = File.new()
    assert(f.open(output, File.WRITE) == OK)
    f.store_string(JSON.print(results, "  "))
    f.close()
    print("ENCORE_WORLD_FLAGS_REFERENCE_COMPLETE")
    quit()
'''

def validate_result(data):
    counts={'flag_names':178,'appearance':64,'doors':96,'npc':8,'landmarks':2,'writes':24,'objects':12}
    if not isinstance(data,dict) or set(data)!=set(counts): raise ValueError('Incomplete or unknown world flag reference fields')
    for name,count in counts.items():
        if not isinstance(data[name],list) or len(data[name])!=count: raise ValueError('Wrong reference case count: '+name)
    if any(not isinstance(n,str) or not n for n in data['flag_names']) or len(set(data['flag_names']))!=178:
        raise ValueError('Invalid native flag registry')
    types={'appearance':(int,str,str,bool),'doors':(int,str,bool,bool,bool,bool),
           'npc':(bool,)*6,'landmarks':(bool,)*6,'writes':(bool,str,bool,bool,bool,bool,int),
           'objects':(str,bool,bool,bool,int,list)}
    for name,row_types in types.items():
        for row in data[name]:
            if not isinstance(row,list) or len(row)!=len(row_types) or any(type(value) is not kind for value,kind in zip(row,row_types)):
                raise ValueError('Invalid typed reference row: '+name)
    if any(not 0<=row[0]<4 for row in data['appearance']) or any(not 0<=row[0]<16 for row in data['doors']):
        raise ValueError('Out-of-range input reference')
    if any(row[-1] not in (0,1) for row in data['writes']) or any(row[4] not in (0,1) or len(row[5])>1 or any(not isinstance(k,str) for k in row[5]) for row in data['objects']):
        raise ValueError('Invalid signal count or object key reference')

def fixture(data):
    validate_result(data)
    quote=lambda value: json.dumps(value,ensure_ascii=False)
    val=lambda value: 'true' if value else 'false'
    out=['// Generated by tools/reference_world_flags.py from native Godot results.\n#pragma once\nnamespace world_flags_reference {']
    out.append('inline constexpr const char* names[] = {'+','.join(map(quote,data['flag_names']))+'};')
    out.append('struct Appearance { unsigned bits; const char* appear; const char* disappear; bool expected; };\ninline constexpr Appearance appearance[]={')
    out += ['{%s,%s,%s,%s},'%(r[0],quote(r[1]),quote(r[2]),val(r[3])) for r in data['appearance']];out.append('};')
    out.append('struct Write { bool object; const char* name; bool value,emit,present,result; unsigned signals; };\ninline constexpr Write writes[]={')
    out += ['{%s,%s,%s,%s,%s,%s,%s},'%(val(r[0]),quote(r[1]),val(r[2]),val(r[3]),val(r[4]),val(r[5]),r[6]) for r in data['writes']];out.append('};')
    out.append('struct Door { unsigned blocker; const char* flag; bool flag_value,unlocked,disabled,prompt; };\ninline constexpr Door doors[]={')
    out += ['{%s,%s,%s,%s,%s,%s},'%(r[0],quote(r[1]),val(r[2]),val(r[3]),val(r[4]),val(r[5])) for r in data['doors']];out.append('};')
    out.append('struct Npc { bool parent_shown,shown,dialogue,collision_disabled,interaction_disabled,processing; };\ninline constexpr Npc npcs[]={')
    out += ['{'+','.join(map(val,r))+'},' for r in data['npc']];out.append('};')
    out.append('struct Landmark { bool delete_hidden,hidden_visible,hidden_queued,restored_visible,restored_queued,remains; };\ninline constexpr Landmark landmarks[]={')
    out += ['{'+','.join(map(val,r))+'},' for r in data['landmarks']];out.append('};')
    out.append('struct Object { const char* flag; bool object,emit,status; unsigned signals; const char* object_key; };\ninline constexpr Object objects[]={')
    out += ['{%s,%s,%s,%s,%s,%s},'%(quote(r[0]),val(r[1]),val(r[2]),val(r[3]),r[4],quote(r[5][0] if r[5] else '')) for r in data['objects']];out.append('};\n}')
    return '\n'.join(out)+'\n'

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--godot',type=Path,required=True)
    parser.add_argument('--work',type=Path,required=True)
    parser.add_argument('--reports',type=Path,required=True)
    args=parser.parse_args()
    work=args.work.resolve();reports=args.reports.resolve();engine=args.godot.resolve()
    if not work.is_relative_to(ROOT/'build') or work.exists(): raise ValueError('Use fresh build/ work directory')
    if not reports.is_relative_to(ROOT/'reports') or reports.exists(): raise ValueError('Use fresh reports/ directory')
    version=subprocess.check_output([str(engine),'--version'],text=True).strip()
    if not version.startswith('3.6.2.stable.official.3cd3caab6'): raise ValueError('Expected official Godot3.6.2')
    review=prepare(ROOT/'upstream/MOTHER-Encore',work)
    reports.mkdir(parents=True)
    command=[str(engine),'--path',str(work),'--script',str(work/'probe.gd'),'--encore-out='+str(reports/'world-flags.json')]
    environment=dict(os.environ)
    for key,child in [('XDG_DATA_HOME','user-data'),('XDG_CONFIG_HOME','user-config')]:
        directory=work/child;directory.mkdir();environment[key]=str(directory)
    result=subprocess.run(command,capture_output=True,text=True,timeout=45,env=environment)
    (reports/'godot.txt').write_text(result.stdout+result.stderr)
    if result.returncode or 'ENCORE_WORLD_FLAGS_REFERENCE_COMPLETE' not in result.stdout or 'ERROR:' in result.stderr or 'WARNING:' in result.stderr:
        raise ValueError('Godot reference failed; see '+str(reports/'godot.txt'))
    data=json.loads((reports/'world-flags.json').read_text())
    registry=re.findall(r'\"([^\"\n]+)\"',(ROOT/'runtime/world_flags.cpp').read_text().split('names{{\n',1)[1].split('\n}};',1)[0])
    if data['flag_names']!=registry: raise ValueError('Native registry differs from C++ registry')
    (reports/'world_flags_v0410.hpp').write_text(fixture(data))
    receipt={'schema':1,'commit':review['commit'],'game_version':review['game_version'],'engine_version':version,'engine_sha256':digest(engine),'sources':review['sources'],'scope':review['scope'],'outputs_sha256':{p.name:digest(p) for p in sorted(reports.iterdir()) if p.is_file()},'harness_sha256':{p.name:digest(p) for p in sorted(work.iterdir()) if p.is_file()},'tool_sha256':digest(Path(__file__)),'hardware':'not run','emulator':'not run'}
    (reports/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
    print('Exact-source world-flag reference recorded; compare generated fixture in C++ tests.')
if __name__=='__main__': main()
