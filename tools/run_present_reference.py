#!/usr/bin/env python3
"""Execute pinned ItemHolder/FlaggableObject/Present methods in Godot 3.6.2.

This oracle covers key-item dispatch, source flag namespace, animation dispatch,
repeat and reconstructed object state. It does not run the complete game/UI/save.
"""
import argparse, hashlib, json, shutil, subprocess, sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.extract_battle_entry import Extractor, ROOT, require
from tools.house_presents import load
from tools.run_battle_victory_reference import function

BASE='''extends Node2D
var global
var globaldata
var uiManager
var Inventory
var Item
var flag=""
var is_object_flag=false
var emit_flag_updated_signal=false
var item=""
var dialog=""
var dialog_full=""
var dialog_empty=""
var can_pickup=true
func _update_sprite():pass # texture loading excluded; original sprite state methods below
'''
SERVICE='''extends Node
var flags={}
var object_flags={}
var currentScene
var item=null
var events=[]
var keys=[]
var present
func get_item_data(_name):return {"keyitem":true}
func set_flag(key,value,_emit):
\tflags[key]=value
\tevents.append("story-flag")
func set_object_flag(key,value,_emit):
\tobject_flags[key]=value
\tevents.append("object-flag")
func has_inventory_space():return false # deliberately full normal inventory; key items bypass it
func add_item_available(name):
\tvar value={"item_name":name,"uid":2748,"doses":1}
\tkeys.append(value)
\tevents.append("grant")
\treturn value
func open_dialogue_box(path):
\tevents.append(path)
func animation_started(clip):events.append("animation:"+clip)
'''.replace('\\t','\t')
SPARKLE='''extends Node2D
var playing=true
func play():playing=true
func stop():playing=false
'''
PROBE='''extends SceneTree
var service
var result={}
var fixture
func _init():call_deferred("run")
func create_present(name):
\tvar box=load("res://present.gd").new()
\tbox.name=name
\tbox.global=service
\tbox.globaldata=service
\tbox.uiManager=service
\tbox.Inventory=service
\tbox.item=fixture.item
\tvar sprite=Sprite.new()
\tsprite.name="Sprite"
\tsprite.hframes=5
\tbox.add_child(sprite)
\tvar sparkles=load("res://sparkles.gd").new()
\tsparkles.name="Sparkles"
\tbox.add_child(sparkles)
\tvar player=AnimationPlayer.new()
\tplayer.name="AnimationPlayer"
\tfor row in fixture.clips:
\t\tvar animation=Animation.new()
\t\tanimation.length=row.length
\t\tvar track=animation.add_track(Animation.TYPE_VALUE)
\t\tanimation.track_set_path(track,"Sprite:frame")
\t\tanimation.value_track_set_update_mode(track,Animation.UPDATE_DISCRETE)
\t\tfor key in row.frames:animation.track_insert_key(track,key[0],key[1])
\t\tplayer.add_animation(row.role,animation)
\tplayer.connect("animation_started",service,"animation_started")
\tbox.add_child(player)
\tservice.currentScene.add_child(box)
\treturn box
func run():
\tprint("PRESENT_ORACLE run")
\tvar f=File.new()
\tassert(f.open("res://fixture.json",File.READ)==OK)
\tfixture=JSON.parse(f.get_as_text()).result
\tf.close()
\tservice=load("res://service.gd").new()
\tget_root().add_child(service)
\tservice.currentScene=Node2D.new()
\tservice.currentScene.name=fixture.scene
\tget_root().add_child(service.currentScene)
\tprint("PRESENT_ORACLE create")
\tvar box=create_present(fixture.name)
\tprint("PRESENT_ORACLE ready")
\tassert(not box._get_flag_status())
\tassert(box.get_node("Sparkles").playing)
\tbox.interact()
\tprint("PRESENT_ORACLE events ",service.events)
\tassert(service.events==["animation:Unwrapped","grant","object-flag","ItemDialogue/presentcheck"])
\tassert(service.keys.size()==1 and service.keys[0].item_name==fixture.item)
\tprint("PRESENT_ORACLE flags ",service.object_flags," expected ",fixture.identity)
\tassert(service.object_flags.size()==1 and service.object_flags.get(fixture.identity,false) and service.flags.empty())
\tassert(not box.get_node("Sparkles").playing and not box.get_node("Sparkles").visible)
\tresult.grant={"events":service.events.duplicate(),"object_flags":service.object_flags.duplicate(),"item":service.keys[0].duplicate()}
\tbox.get_node("AnimationPlayer").advance(0.4)
\tprint("PRESENT_ORACLE animation advanced")
\tassert(box.get_node("Sprite").frame==4)
\tservice.events=[]
\tbox.interact()
\tassert(service.events==["ItemDialogue/presentempty"] and service.keys.size()==1)
\tresult.repeat={"events":service.events.duplicate(),"count":service.keys.size()}
\tvar serialized=JSON.print({"object_flags":service.object_flags,"keys":service.keys})
\tbox.free()
\tvar restored=JSON.parse(serialized).result
\tservice.object_flags=restored.object_flags
\tservice.keys=restored.keys
\tservice.events=[]
\tbox=create_present(fixture.name)
\tassert(box._get_flag_status() and box.get_node("Sprite").frame==4)
\tassert(not box.get_node("Sparkles").playing and not box.get_node("Sparkles").visible)
\tbox.interact()
\tassert(service.events==["ItemDialogue/presentempty"] and service.keys.size()==1 and service.keys[0].uid==2748)
\tresult.reconstructed={"frame":box.get_node("Sprite").frame,"events":service.events.duplicate(),"item":service.keys[0].duplicate()}
\tvar other=create_present("OtherObject")
\tassert(not other._get_flag_status())
\tresult.namespace_isolated=true
\tresult.engine=Engine.get_version_info()
\tassert(f.open("res://../reference.json",File.WRITE)==OK)
\tf.store_string(JSON.print(result))
\tf.close()
\tquit()
'''.replace('\\t','\t')

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--godot',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
    args=p.parse_args();out=args.out.resolve()
    require(not out.exists() and out.is_relative_to(ROOT/'build'),'Fresh owned build output required')
    out.mkdir(parents=True);work=out/'temporary';work.mkdir()
    ex=Extractor(ROOT);ir=load();obj=next(o for o in ir['objects'] if o['item']=='MapPodunk')
    for source,digest in ir['sources'].items():
        ex.data(source);require(ex.sources[source]==digest,'Changed present oracle source: '+source)
    methods={}
    def extract(path,names):
        source=ex.text(path);parts=[]
        for name in names:
            code=function(source,name);methods[path+':'+name]=hashlib.sha256(code.encode()).hexdigest();parts.append(code)
        return '\n'.join(parts)
    script=BASE+extract('Scripts/Main/FlaggableObject.gd',['_get_flag_status','_set_flag_status'])
    script+=extract('Scripts/Main/ItemHolder.gd',['interact','_check_item','_warn_empty'])
    script+=extract('Scripts/Main/Present.gd',['_ready','_update_state','_play_interact','_play_collect_item','_play_revert'])
    generated={'present.gd':script,'service.gd':SERVICE,'sparkles.gd':SPARKLE,'probe.gd':PROBE,
               'project.godot':'config_version=4\n[application]\nconfig/name="Original present method oracle"\n',
               'fixture.json':json.dumps(dict(scene=obj['flag'].split('/')[0],name=obj['source_path'].split('/')[-1],identity=obj['flag'],item=obj['item'],clips=ir['clips']))}
    for name,body in generated.items():(work/name).write_bytes(body.encode('utf8'))
    try:
        with (out/'godot.log').open('w',encoding='utf8') as log:
            proc=subprocess.run([str(args.godot.resolve()),'--path',str(work),'--no-window','--verbose','-s','probe.gd'],stdout=log,stderr=subprocess.STDOUT,timeout=35)
        require(proc.returncode==0 and (out/'reference.json').is_file(),'Original present oracle failed; inspect retained work/log')
        result=json.loads((out/'reference.json').read_bytes());engine=result['engine']
        require((engine.get('major'),engine.get('minor'),engine.get('patch'))==(3,6,2) and engine.get('build')=='official','Unreviewed reference engine')
        receipt=dict(commit=ex.lock['commit'],sources=ex.sources,methods_sha256=methods,tool_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                     godot_sha256=hashlib.sha256(args.godot.read_bytes()).hexdigest(),
                     reference_sha256=hashlib.sha256((out/'reference.json').read_bytes()).hexdigest(),
                     generated_sha256={k:hashlib.sha256(v.encode()).hexdigest() for k,v in generated.items()},
                     adapters=['Original methods retained verbatim; texture-loading helper excluded',
                               'Injected singleton properties; fixed key-item definition and UID inventory adapter; normal inventory deliberately full',
                               'Native AnimationPlayer manually advanced 0.4 seconds and Sprite; Sparkles play/stop property adapter; audio tracks excluded',
                               'UI records source dialogue path; phrase printing, voice and Item Received playback excluded',
                               'Reconstruction transfers JSON object/key state; this is not the original full save loader'],temporary_removed=False)
        shutil.rmtree(work);receipt['temporary_removed']=True
        (out/'receipt.json').write_bytes((json.dumps(receipt,indent=2)+'\n').encode('utf8'))
        print('Original present: grant order, repeat, reconstructed state and object namespace passed; temporary removed')
    except Exception:
        print('Reference failure retained at '+str(out),file=sys.stderr);raise
if __name__=='__main__':main()
