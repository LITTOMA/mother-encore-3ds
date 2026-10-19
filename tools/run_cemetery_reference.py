"""Native source deferred Grow and actor erase semantics; isolated services."""
import argparse,hashlib,json,re,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.field_story import read
from tools.run_prop_clip_reference import run as prepare
from tools.run_scene_reference import invoke,digest

def method(code,name):
 m=re.search(r'^func '+re.escape(name)+r'[(].*?(?=^func |\Z)',code,re.M|re.S)
 if not m:raise ValueError('Source method missing: '+name)
 return m[0]

def block(code,name,end):
 a=code.index('\tif _curr_phrase.has("'+name+'"):',0)
 z=code.index('\t# '+end,a)
 return code[a:z]

def run(out,engine):
 p,ex=read();out=prepare(out,engine);project=out/'project'
 source=ex.text('Scripts/UI/DialogueBox.gd');actor=ex.text('Scripts/Main/actor.gd')
 blocks={name:block(source,name,end)for name,end in [('objectsfunction','tweenpos:'),('eraseactors','playeremote:'),('setflags','unsetflags:')]}
 methods={name:method(actor,name)for name in ('erase','unmake_persistent','_replaced_exists')}
 methods['_change_flags']=method(source,'_change_flags')
 (project/'eraser.gd').write_text('extends Node2D\nsignal enemy_erased\nvar _replaced=null\n'+''.join(methods[n]for n in ('erase','unmake_persistent','_replaced_exists')),encoding='utf8')
 dialog='extends Node\nvar _curr_phrase={}\nvar _actors={}\n'+methods['_change_flags']
 for name,code in blocks.items():dialog+='\nfunc apply_'+name+'():\n'+code
 (project/'dialog.gd').write_text(dialog,encoding='utf8')
 (project/'global.gd').write_text('extends Node\nvar currentScene=null\nvar talker=null\nvar player=null\nvar persistent=[]\nvar grows=[]\nvar grown_order=[]\nfunc get_player():\n    return player\nfunc remove_persistent(obj):\n    persistent.erase(obj)\nfunc start_joy_vibration(a,b,c,d):\n    grows.append([a,b,c,d,globaldata.flags.cem_roots_grown])\n',encoding='utf8')
 (project/'grow-observer.gd').write_text('extends "res://Scripts/Main/Dead Bush.gd"\nfunc grow():\n    global.grown_order.append(get_meta("source_id"))\n    .grow()\n',encoding='utf8')
 (project/'globaldata.gd').write_text('extends Node\nvar flags={"bat":false,"cem_roots_grown":false,"got_shovel":false}\nfunc set_flag(name,value):\n    flags[name]=value\n',encoding='utf8')
 doc=next(p['documents'][r['source']]for r in p['continuations'])
 (project/'cemetery-input.json').write_text(json.dumps(doc),encoding='utf8')
 (project/'cemetery-probe.gd').write_text(PROBE,encoding='utf8')
 invoke(Path(engine).resolve(),project,['--debug','--script',str(project/'cemetery-probe.gd')],out/'cemetery-godot.log')
 result=json.loads((out/'cemetery-native.json').read_text())
 expected={'erase_immediate':[True,True,True,True,True,True],'erase_flushed':[False,False,False,False], 'persistent_remaining':0,'flag_before_flush':True,'grows_before_flush':0,'grown_flags':[True]*5,'grown_order':[3,4,6,7,5],'finished':[[True,True,True,True]]*5}
 if result!=expected:raise ValueError('Native cemetery semantics differ: '+str(result))
 receipt=dict(schema=1,commit=p['commit'],engine_sha256=digest(engine),source_blocks={k:hashlib.sha256(v.encode()).hexdigest()for k,v in blocks.items()},source_methods={k:hashlib.sha256(v.encode()).hexdigest()for k,v in methods.items()},result_sha256=digest(out/'cemetery-native.json'),scope='Unchanged source objectsfunction/eraseactors/setflags blocks and Actor erase helpers, actual native deferred calls, queue_free and five source prop Grow coroutines. UI, persistent registry and flag services are explicit stubs; an inherited Grow observer records identity then forwards to the unchanged source method. Does not validate the full cutscene timeline, rendering, audio, emulator or hardware.')
 (out/'cemetery-receipt.json').write_text(json.dumps(receipt,indent=2)+'\n');print('Native cemetery semantics: 2 actor erasures, 5 ordered deferred Grow calls, immediate flag and final collision/interaction state')

PROBE=r'''extends SceneTree
var rows={}
func _init():
    call_deferred("run")
func run():
    yield(self,"idle_frame")
    var services=get_root().get_node("global")
    var store=get_root().get_node("globaldata")
    var scene=Node2D.new()
    get_root().add_child(scene)
    services.currentScene=scene
    var objects=Node2D.new()
    objects.name="Objects"
    scene.add_child(objects)
    var bushes=Node2D.new()
    bushes.name="Dead Bushes"
    objects.add_child(bushes)
    var roots=[]
    for id in [3,4,6,7,5]:
        var prop=load("res://prop-probe.tscn").instance()
        prop.set_script(load("res://grow-observer.gd"))
        prop.set_meta("source_id",id)
        prop.name="Dead Bush"+str({3:4,4:5,6:6,7:7,5:8}[id])
        prop.flag="cem_roots_grown"
        bushes.add_child(prop)
        prop.get_node("AnimationPlayer").playback_process_mode=2
        roots.append(prop)
        prop.get_node("AnimationPlayer").advance(0)
    var dialog=load("res://dialog.gd").new()
    scene.add_child(dialog)
    var actors=[]
    var originals=[]
    for name in ["zombie1","zombie2"]:
        var original=Node2D.new()
        scene.add_child(original)
        originals.append(original)
        var actor=load("res://eraser.gd").new()
        actor._replaced=original
        scene.add_child(actor)
        actors.append(actor)
        dialog._actors[name]=actor
        services.persistent.append(original)
        services.persistent.append(actor)
    var f=File.new()
    assert(f.open("res://cemetery-input.json",File.READ)==OK)
    var phrases=JSON.parse(f.get_as_text()).result
    f.close()
    rows.erase_immediate=[]
    for index in range(2):
        var name="zombie"+str(index+1)
        dialog._curr_phrase=phrases[str(index+6)]
        dialog.apply_eraseactors()
        rows.erase_immediate.append(actors[index].is_queued_for_deletion())
        rows.erase_immediate.append(originals[index].is_queued_for_deletion())
        rows.erase_immediate.append(not dialog._actors.has(name))
    rows.persistent_remaining=services.persistent.size()
    dialog._curr_phrase=phrases["9"]
    dialog.apply_objectsfunction()
    dialog.apply_setflags()
    rows.flag_before_flush=store.flags.cem_roots_grown
    rows.grows_before_flush=services.grows.size()
    yield(self,"idle_frame")
    rows.erase_flushed=[]
    for index in range(2):
        rows.erase_flushed.append(is_instance_valid(actors[index]))
        rows.erase_flushed.append(is_instance_valid(originals[index]))
    rows.grown_flags=[]
    rows.grown_order=services.grown_order.duplicate()
    rows.finished=[]
    for index in range(roots.size()):
        rows.grown_flags.append(services.grows[index][4])
        var a=roots[index].get_node("AnimationPlayer")
        a.advance(0)
        a.advance(1)
        a.advance(0)
        var prop=roots[index]
        rows.finished.append([prop.get_node("Sprite").visible,not prop.get_node("StaticBody2D/CollisionShape2D").disabled,not prop.get_node("Hitbox/CollisionShape2D").disabled,not prop.get_node("interact/CollisionShape2D").disabled])
    assert(f.open("res://../cemetery-native.json",File.WRITE)==OK)
    f.store_string(JSON.print(rows))
    f.close()
    scene.queue_free()
    yield(self,"idle_frame")
    quit()
'''
if __name__=='__main__':
 a=argparse.ArgumentParser();a.add_argument('--out',type=Path,required=True);a.add_argument('--godot',type=Path,required=True);v=a.parse_args();run(v.out,v.godot)
