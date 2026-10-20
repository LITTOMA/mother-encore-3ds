"""Original key/lock methods in Godot, with explicit scene/player/UI/audio services.

No source scene, physical contact, animation timeline, audible output or native
gameplay pass. One separate negative signal case records the original absent
handler; every other engine error remains fatal to this reference run.
"""
import argparse,hashlib,json,os,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import dungeon_keys as d
from tools.run_battle_victory_reference import function
from tools.run_scene_reference import invoke
from tools.godot_text import Document

GLOBAL='''extends Node
signal flags_updated
class SceneService:
    extends Node2D
    signal area_left(region_changed)
    func get_region_name():return name
    func leave(region_changed):emit_signal("area_left",region_changed)
class PlayerService:
    extends Node
    var paused=false
    func is_paused():return paused
var currentScene
var player
func _ready():
    currentScene=SceneService.new()
    currentScene.name="Catacombs"
    add_child(currentScene)
    player=PlayerService.new()
    add_child(player)
func get_player():return player
func broadcast_flags_updated():emit_signal("flags_updated")
'''
STATE='''extends Node
var disabled=false
var enabled=true
var playing=false
'''
PROBE='''extends SceneTree
var key
var door
var rows=[]
var notifications=[]
var config
func _init():call_deferred("run")
func describe(target,is_key):
    if not is_instance_valid(target):return {"alive":false}
    var value={"alive":true,"queued":target.is_queued_for_deletion(),"visible":target.visible}
    if is_key:
        value.audio=target.get_node("AudioStreamPlayer").playing
        value.disabled=target.get_node("Area2D/CollisionShape2D").disabled
    else:
        value.opened=target.opened
        value.disabled=target.get_node("StaticBody2D/CollisionShape2D").disabled
        value.prompt=target.get_node("interact/ButtonPrompt").enabled
        value.animation=target.get_node("AnimationPlayer").current_animation
    return value
func flag_notification():
    notifications.append({"count":uiManager.get_key_count(),"flags":globaldata.object_flags.duplicate(),"key":describe(key,true),"lock":describe(door,false)})
func capture(action):
    rows.append({"action":action,"count":uiManager.get_key_count(),"flags":globaldata.object_flags.duplicate(),"key":describe(key,true),"lock":describe(door,false),"notifications":notifications.duplicate(true),"ui":uiManager.events.duplicate(true)})
    notifications.clear()
    uiManager.events.clear()
func child(parent,name):
    var node=load("res://state.gd").new()
    node.name=name
    parent.add_child(node)
    return node
func make_key():
    var target=load("res://"+config.key_script).new()
    target.name=config.key_name
    target.is_object_flag=true
    target.emit_flag_updated_signal=true
    var area=Node.new()
    area.name="Area2D"
    target.add_child(area)
    child(area,"CollisionShape2D")
    child(target,"AudioStreamPlayer")
    global.currentScene.add_child(target)
    return target
func make_lock():
    var target=load("res://"+config.lock_script).new()
    target.name=config.lock_name
    target.is_object_flag=true
    target.emit_flag_updated_signal=true
    var body=Node.new()
    body.name="StaticBody2D"
    target.add_child(body)
    child(body,"CollisionShape2D")
    var interact=Node.new()
    interact.name="interact"
    target.add_child(interact)
    child(interact,"ButtonPrompt")
    var animation=AnimationPlayer.new()
    animation.name="AnimationPlayer"
    animation.playback_process_mode=AnimationPlayer.ANIMATION_PROCESS_MANUAL
    for name in ["Open","Opened"]:
        var value=Animation.new()
        value.length=0.01 # Explicit request service, not the source timeline.
        animation.add_animation(name,value)
    target.add_child(animation)
    global.currentScene.add_child(target)
    return target
func run():
    yield(self,"idle_frame")
    var file=File.new()
    assert(file.open("res://input.json",File.READ)==OK)
    config=JSON.parse(file.get_as_text()).result
    file.close()
    global.connect("flags_updated",self,"flag_notification")
    key=make_key()
    capture("key_ready")
    var other=Node.new()
    key._on_Area2D_body_entered(other)
    other.free()
    capture("nonplayer_ignored")
    global.player.paused=true
    key._on_Area2D_body_entered(global.player)
    capture("paused_ignored")
    global.player.paused=false
    key._on_Area2D_body_entered(global.player)
    capture("key_pickup_before_deferred")
    assert(uiManager.get_key_count()==1 and key.visible==false)
    key._on_Area2D_body_entered(global.player)
    capture("duplicate_audio_busy_ignored")
    yield(self,"idle_frame")
    yield(self,"idle_frame")
    capture("key_collision_deferred")
    assert(key.get_node("Area2D/CollisionShape2D").disabled)
    key._on_AudioStreamPlayer_finished()
    capture("key_audio_finish_queued")
    yield(self,"idle_frame")
    yield(self,"idle_frame")
    capture("key_deleted")
    assert(not is_instance_valid(key))
    key=make_key()
    capture("saved_key_ready_queued")
    assert(key.is_queued_for_deletion() and uiManager.get_key_count()==1)
    yield(self,"idle_frame")
    yield(self,"idle_frame")
    capture("saved_key_deleted")
    globaldata.keys["Catacombs"]=0 # Explicit counter service input for no-key.
    door=make_lock()
    capture("lock_ready")
    door.interact()
    capture("lock_no_key")
    assert(not door.opened)
    assert(uiManager.try_alter_key_count(1))
    door.interact()
    capture("lock_open_requested")
    assert(door.opened and uiManager.get_key_count()==0)
    door.open() # Explicit source animation method boundary, no timeline claim.
    capture("lock_open_method")
    assert(door.get_node("StaticBody2D/CollisionShape2D").disabled)
    door.interact()
    capture("opened_lock_repeat_ignored")
    var animation=door.get_node("AnimationPlayer")
    var connection=animation.connect(config.connection.signal,door,config.connection.method)
    capture("negative_missing_handler_connected")
    animation.emit_signal(config.connection.signal,"Open")
    capture("negative_missing_handler_after_emit")
    assert(is_instance_valid(door) and door.opened and uiManager.get_key_count()==0)
    door.queue_free()
    yield(self,"idle_frame")
    yield(self,"idle_frame")
    door=make_lock()
    capture("saved_lock_ready")
    assert(door.opened and door.get_node("StaticBody2D/CollisionShape2D").disabled and not door.get_node("interact/ButtonPrompt").enabled)
    door.queue_free()
    yield(self,"idle_frame")
    yield(self,"idle_frame")
    assert(file.open("res://observations.json",File.WRITE)==OK)
    file.store_string(JSON.print({"engine":Engine.get_version_info(),"rows":rows,"connection_return":connection},"  "))
    file.close()
    quit(17) # Intentional expected-negative signal fixture, not gameplay exit.
'''

def run(out,engine):
 out=Path(out).resolve();engine=Path(engine).resolve();d.require(out.is_relative_to((ROOT/'build').resolve())and not out.exists(),'Dungeon key reference needs a fresh build directory')
 config,ex=d.read();data,proof=d.extract();paths=config['paths'];out.mkdir(parents=True);project=out/'project';project.mkdir();(project/'.owned-key-reference').write_text('source-key-method-reference-v1',encoding='utf8')
 copied={}
 for source in (paths['flag_script'],paths['key_script'],paths['door_script']):
  raw=ex.data(source);dest=project/source;dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(raw);copied[source]=hashlib.sha256(raw).hexdigest()
 document=Document(paths['door_scene'],ex.text(paths['door_scene']));connections=[row for row in document.connections if row.get('from')=='AnimationPlayer'and row.get('to')=='.']
 d.require(len(connections)==1 and connections[0]['signal']=='animation_finished'and connections[0]['method']=='_on_AnimationPlayer_animation_finished','Dungeon key expected source signal changed')
 missing=connections[0]['method'];d.require(all('func '+missing+'('not in ex.text(paths[p])for p in ('door_script','flag_script')),'Dungeon key formerly absent callback now exists')
 # Preserve the original setting for unchanged callback argument names;
 # all other diagnostics remain admitted individually by the strict runner.
 d.require('gdscript/warnings/unused_argument=false'in ex.text('project.godot'),'Dungeon key source callback warning profile changed')
 (project/'project.godot').write_text('config_version=4\n[application]\nconfig/name="Source key methods"\nrun/main_scene="res://probe.tscn"\n[autoload]\nglobal="*res://global.gd"\nglobaldata="*res://data.gd"\nuiManager="*res://ui.gd"\n[debug]\ngdscript/warnings/unused_argument=false\n',encoding='utf8')
 # Use the normal application SceneTree so source autoload names are available
 # when the original class scripts compile; --script replaces that main loop.
 node_probe=PROBE.replace('extends SceneTree','extends Node',1).replace('yield(self,"idle_frame")','yield(get_tree(),"idle_frame")').replace('quit(17)','get_tree().quit(17)')
 (project/'global.gd').write_text(GLOBAL,encoding='utf8');(project/'state.gd').write_text(STATE,encoding='utf8');(project/'probe.gd').write_text(node_probe,encoding='utf8')
 (project/'probe.tscn').write_text('[gd_scene load_steps=2 format=2]\n[ext_resource path="res://probe.gd" type="Script" id=1]\n[node name="Probe" type="Node"]\nscript = ExtResource( 1 )\n',encoding='utf8')
 global_methods='\n'.join(function(ex.text(paths['globaldata_script']),name)for name in ('set_flag','set_object_flag'))
 (project/'data.gd').write_text('extends Node\nvar flags={}\nvar object_flags={}\nvar keys={"Catacombs":0}\n'+global_methods,encoding='utf8')
 ui_methods='\n'.join(function(ex.text(paths['ui_script']),name)for name in ('get_key_count','try_alter_key_count'))
 (project/'ui.gd').write_text('extends Node\nvar events=[]\nfunc update_key_indicator():events.append({"kind":"indicator","value":get_key_count()})\nfunc open_dialogue_box(path):events.append({"kind":"text","path":path})\n'+ui_methods,encoding='utf8')
 key=next(row for row in data['nodes']if row['kind']==1);lock=next(row for row in data['nodes']if row['kind']==2)
 fixture=dict(key_script=paths['key_script'],lock_script=paths['door_script'],key_name=key['path'].rsplit('/',1)[-1],lock_name=lock['path'].rsplit('/',1)[-1],connection=connections[0]);(project/'input.json').write_text(json.dumps(fixture,indent=2),encoding='utf8')
 invoke(engine,project,['--debug','--editor','--quit'],out/'import.log')
 environment=dict(os.environ)
 for variable,child in [('APPDATA','user-roaming'),('LOCALAPPDATA','user-local'),('XDG_DATA_HOME','user-data'),('XDG_CONFIG_HOME','user-config')]:
  environment[variable]=str(project/child)
 command=[str(engine),'--path',str(project)]
 if os.name=='nt':command+=['--no-window','--audio-driver','Dummy']
 with(out/'engine.log').open('wb')as log_file:
  result=subprocess.run(command+['--debug'],stdout=log_file,stderr=subprocess.STDOUT,env=environment,timeout=55)
 d.require(result.returncode==17,'Dungeon key expected-negative fixture did not reach its intentional exit17')
 log=(out/'engine.log').read_text(encoding='utf8',errors='replace');errors=[line for line in log.splitlines()if 'ERROR:'in line]
 d.require(len(errors)==1 and missing in errors[0]and 'WARNING:'not in log,'Dungeon key reference has an unexpected engine diagnostic')
 observations=json.loads((project/'observations.json').read_bytes());version=observations['engine']
 d.require((version['major'],version['minor'],version['patch'],version['hash'])==(3,6,2,'3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8'),'Dungeon key reference engine version changed')
 rows=observations['rows'];actions=['key_ready','nonplayer_ignored','paused_ignored','key_pickup_before_deferred','duplicate_audio_busy_ignored','key_collision_deferred','key_audio_finish_queued','key_deleted','saved_key_ready_queued','saved_key_deleted','lock_ready','lock_no_key','lock_open_requested','lock_open_method','opened_lock_repeat_ignored','negative_missing_handler_connected','negative_missing_handler_after_emit','saved_lock_ready']
 d.require([row['action']for row in rows]==actions and [row['count']for row in rows]==[0,0,0,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0],'Dungeon key reference observations/counter order incomplete')
 d.require([len(row['notifications'])for row in rows]==[0,0,0,1,0,0,0,0,0,0,0,0,1,0,0,0,0,0],'Dungeon key source flag-notification order differs')
 notice=rows[3]['notifications'][0];d.require(notice['count']==1 and notice['flags'].get(key['flag'])is True and notice['key']['visible']is True and notice['key']['audio']is False and notice['key']['disabled']is False,'Dungeon key flag observers must precede audio/hide/deferred collision')
 notice=rows[12]['notifications'][0];d.require(notice['count']==0 and notice['flags'].get(lock['flag'])is True and notice['lock']['opened']is True and notice['lock']['animation']=='Open'and notice['lock']['disabled']is False,'Dungeon key lock observer/callback order differs')
 d.require(rows[11]['ui']==[dict(kind='text',path=data['locked'])]and rows[5]['key']['disabled']and rows[6]['key']['queued']and not rows[7]['key']['alive']and rows[8]['key']['queued'],'Dungeon key denial/deferred lifetime differs')
 before=dict(rows[15]);after=dict(rows[16]);before.pop('action');after.pop('action');d.require(observations['connection_return']==0 and before==after,'Dungeon key missing callback unexpectedly mutated live state')
 for source,digest in copied.items():d.require(hashlib.sha256((project/source).read_bytes()).hexdigest()==digest,'Dungeon key original script was modified')
 (out/'observations.json').write_bytes((project/'observations.json').read_bytes());proof.update(scope=__doc__,engine_sha256=hashlib.sha256(engine.read_bytes()).hexdigest(),copied_scripts=copied,source_project_sha256=hashlib.sha256(ex.data('project.godot')).hexdigest(),source_warning_profile='Only original gdscript/warnings/unused_argument=false retained',expected_negative_exit=result.returncode,expected_negative_error=errors[0],adapter_sha256={p.name:hashlib.sha256(p.read_bytes()).hexdigest()for p in project.glob('*.gd')},rows=18)
 (out/'receipt.json').write_text(json.dumps(proof,indent=2)+'\n',encoding='utf8');print('Original key/lock methods:',18,'observations; explicit services and one isolated absent-handler diagnostic')
if __name__=='__main__':
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--out',type=Path,required=True);parser.add_argument('--godot',type=Path,required=True);args=parser.parse_args();run(args.out,args.godot)
