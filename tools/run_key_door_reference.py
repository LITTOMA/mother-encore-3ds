#!/usr/bin/env python3
"""Run the pinned key-door methods in official Godot with explicit service adapters."""
import argparse, hashlib, json, re, shutil, subprocess, sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.extract_battle_entry import Extractor, ROOT, require
from tools.house_key_doors import texts, BINDING
from tools.run_battle_victory_reference import function

DOOR='''extends Node
var Inventory
var global
var globaldata
var uiManager
var key=""
var flag=""
var activates_flag=""
var deactivates_flag=""
var blocked=false
var locked=false
var one_way=false
var remove_key=false
var sound="None"
var _unlocked=false
var interact_doorblocked=""
var interact_locklocked=""
var interact_lockopened=""
'''
INVENTORY='''extends Reference
var global
var globaldata
static func drop_item_from_party(_item):
\tassert(false) # consuming keys is outside this fixture and admitted native scope
\treturn false
'''
INV='''extends Node
var item=null
func find_item(name):return item if item and item.item_name==name else null
'''
SERVICE='''extends Node
signal flags_updated
var flags={}
var key_items
var storage
var party=[]
var entering_door=false
var item=null
var events=[]
var inv
func open_dialogue_box(path):
\tget_node("/root/global").events.append("text")
\tget_node("/root/global").events.append(path)
'''
PROMPT='''extends Control
var enabled=true
'''
SHAPE='''extends Node
var disabled=false
'''
PROBE='''extends SceneTree
var global
var globaldata
var uiManager
func _init():call_deferred("run")
func run():
\tvar f=File.new()
\tassert(f.open("res://fixture.json",File.READ)==OK)
\tvar data=JSON.parse(f.get_as_text()).result
\tf.close()
\tglobal=load("res://service.gd").new()
\tglobal.name="global"
\tget_root().add_child(global)
\tglobaldata=load("res://service.gd").new()
\tget_root().add_child(globaldata)
\tuiManager=load("res://service.gd").new()
\tget_root().add_child(uiManager)
\tglobaldata.flags[data.flag]=false
\tglobaldata.key_items=load("res://inv.gd").new()
\tglobaldata.storage=load("res://inv.gd").new()
\tvar member=load("res://service.gd").new()
\tmember.inv=load("res://inv.gd").new()
\tglobal.party=[member]
\tvar door=load("res://door.gd").new()
\tdoor.global=global
\tdoor.globaldata=globaldata
\tdoor.uiManager=uiManager
\tdoor.Inventory=load("res://inventory.gd").new()
\tdoor.Inventory.global=global
\tdoor.Inventory.globaldata=globaldata
\tdoor.key=data.key
\tdoor.flag=data.flag
\tdoor.interact_locklocked=data.locked
\tdoor.interact_lockopened=data.opened
\tvar body=Node.new()
\tbody.name="StaticBody2D"
\tdoor.add_child(body)
\tvar shape=load("res://shape.gd").new()
\tshape.name="CollisionShape2D"
\tbody.add_child(shape)
\tvar interact=Node.new()
\tinteract.name="interact"
\tdoor.add_child(interact)
\tvar prompt=load("res://prompt.gd").new()
\tprompt.name="ButtonPrompt"
\tinteract.add_child(prompt)
\tvar ap=AnimationPlayer.new()
\tap.name="AnimationPlayer"
\tap.add_animation("Action",Animation.new())
\tdoor.add_child(ap)
\tget_root().add_child(door)
\tdoor.lock()
\tyield(self,"idle_frame")
\tglobal.events=[]
\tdoor.interact()
\tvar cases={}
\tcases.no_key={"flag":globaldata.flags[data.flag],"unlocked":door._unlocked,"disabled":shape.disabled,"events":global.events.duplicate()}
\tassert(!cases.no_key.flag and !cases.no_key.unlocked and !cases.no_key.disabled)
\tassert(cases.no_key.events==["text",data.locked])
\tvar held=Node.new()
\theld.set_script(load("res://item.gd"))
\theld.item_name=data.key
\theld.uid=1234567
\theld.doses=1
\tglobaldata.key_items.item=held
\tglobal.events=[]
\tdoor.interact()
\tcases.with_key={"flag":globaldata.flags[data.flag],"unlocked":door._unlocked,"disabled_before_flush":shape.disabled,"retained":globaldata.key_items.item==held,"uid":held.uid,"doses":held.doses,"item_context":global.item==held,"events":global.events.duplicate()}
\tassert(cases.with_key.flag and cases.with_key.unlocked and !cases.with_key.disabled_before_flush and cases.with_key.retained and cases.with_key.item_context)
\tassert(cases.with_key.events==["flag","open","unlock","text",data.opened])
\tyield(self,"idle_frame")
\tcases.deferred={"disabled":shape.disabled,"prompt_enabled":prompt.enabled,"animation":ap.current_animation}
\tassert(shape.disabled and !prompt.enabled and ap.current_animation=="Action")
\tglobal.events=[]
\tdoor.interact()
\tcases.repeat={"events":global.events.duplicate(),"retained":globaldata.key_items.item==held,"uid":held.uid,"doses":held.doses}
\tassert(cases.repeat.events.empty() and cases.repeat.retained and held.uid==1234567 and held.doses==1)
\tdoor._update_door_state()
\tcases.restore={"unlocked":door._unlocked,"prompt_enabled":prompt.enabled}
\tassert(door._unlocked and !prompt.enabled)
\tassert(f.open("res://../reference.json",File.WRITE)==OK)
\tf.store_string(JSON.print({"engine":Engine.get_version_info(),"cases":cases},"  "))
\tf.close()
\tprint("KEY_DOOR_REFERENCE_COMPLETE")
\tquit()
'''


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--godot',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
    a=p.parse_args();out=a.out.resolve()
    require(not out.exists() and out.is_relative_to(ROOT/'build'),'Fresh project-owned reference output required')
    out.mkdir(parents=True);work=out/'temporary';work.mkdir()
    ex=Extractor(ROOT);house=json.loads((ROOT/'content/native-house.json').read_text(encoding='utf8'))
    rows=texts(ex,house);b=json.loads((ROOT/BINDING).read_text(encoding='utf8'));door=b['doors'][0]
    require(len(b['doors'])==1,'Reference admits one key door')
    methods={}
    def extract(path,names):
        source=ex.text(path);parts=[]
        for name in names:
            code=function(source,name);methods[path+':'+name]=hashlib.sha256(code.encode()).hexdigest()
            # Item is an original project class; the fixture uses the same
            # properties without loading the unrelated complete item system.
            code=re.sub(r':\s*Item\b','',code).replace(' -> Item','').replace(':=','=').replace('static func ','func ')
            if name=='_use_key':code=code.replace('globaldata.flags[flag] = true','globaldata.flags[flag] = true\n\tglobal.events.append("flag")')
            if name in ('open','unlock'):code=code.replace('\n','\n\tglobal.events.append("'+name+'")\n',1)
            parts.append(code)
        return '\n'.join(parts)
    generated={
        'door.gd':DOOR+extract(b['door_script'],['_update_door_state','lock','unlock','open','_use_key','interact']),
        'inventory.gd':INVENTORY+extract(b['inventory_script'],['_get_all_inventories','find_item_for_all']),
        'inv.gd':INV,'service.gd':SERVICE,'prompt.gd':PROMPT,'shape.gd':SHAPE,
        'item.gd':'extends Node\nvar item_name=""\nvar uid=0\nvar doses=1\n', 'probe.gd':PROBE,
        'project.godot':'config_version=4\n[application]\nconfig/name="Key-door source oracle"\n',
        'fixture.json':json.dumps(dict(key=door['key'],flag=door['flag'],locked=rows[0]['source_path'].removeprefix('Data/Dialogue/').removesuffix('.yaml'),opened=rows[1]['source_path'].removeprefix('Data/Dialogue/').removesuffix('.yaml')))}
    for name,body in generated.items():(work/name).write_text(body,encoding='utf8')
    with (out/'godot.log').open('w',encoding='utf8') as log:
        result=subprocess.run([str(a.godot.resolve()),'--path',str(work),'--no-window','-s','probe.gd'],stdout=log,stderr=subprocess.STDOUT,timeout=40)
    require(result.returncode==0 and (out/'reference.json').is_file(),'Original key-door reference failed; retain work and log')
    reference=json.loads((out/'reference.json').read_text());engine=reference['engine']
    require((engine.get('major'),engine.get('minor'),engine.get('patch'))==(3,6,2) and engine.get('build')=='official','Unreviewed reference engine')
    receipt=dict(commit=ex.lock['commit'],sources=ex.sources,methods_sha256=methods,godot_sha256=hashlib.sha256(a.godot.read_bytes()).hexdigest(),reference_sha256=hashlib.sha256((out/'reference.json').read_bytes()).hexdigest(),generated_sha256={k:hashlib.sha256(v.encode()).hexdigest() for k,v in generated.items()},
                 adapters=['Item type annotation erased; fixed source item identity with test UID/doses', 'Original static inventory methods use injected service instances; method bodies/search order retained; Inventory.find_item and containers are property adapters', 'UI records dialogue paths; text/layout/voice and full original save loading are outside this oracle', 'AnimationPlayer and deferred shape property write use native Godot; no audio resource loads because fixture sound None; actual source sound identity is checked separately', 'Flag/open/unlock event observation inserted without changing source statements', 'Consuming key branch is excluded and rejects if reached'],temporary_removed=False)
    shutil.rmtree(work);receipt['temporary_removed']=True
    (out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
    print('Original key-door reference: 5 cases passed; native deferred property and AnimationPlayer; temporary removed')


if __name__=='__main__':main()
