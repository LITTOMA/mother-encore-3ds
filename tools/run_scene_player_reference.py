"""Native single-party scene handoff with unchanged source and bounded adapters."""
import argparse,hashlib,json,math,shutil,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.reference_movement import extract
from tools.extract_battle_entry import Extractor,require,PIN
from tools import podunk_field as f,world_scene_routes as routes
from tools.godot_text import instantiate
from tools.run_scene_reference import invoke
CASES=[dict(scene=1,position=[136,809],direction=[0,1]),dict(scene=2,position=[8,9],direction=[0,-1]),dict(scene=0,position=[520,397],direction=[0,0]),dict(scene=1,position=[1144,263],direction=[-1,0]),dict(scene=2,position=[8,-39],direction=[1,0]),dict(scene=0,position=[0,0],direction=[0,0])]
PROBE='''extends SceneTree
func _init():call_deferred("run")
func point(value):return Vector2(value[0],value[1])
func pair(value):return [value.x,value.y]
func state(actor):
    return {"tap":actor._tap_run,"running":actor._running,"speed":actor._speed,"velocity":pair(actor._velocity),"substantial":actor._substantial_movement,"previous_toggle":actor.previous_toggle,"run_sound":actor.run_sound,"camera_offset":pair(actor.camera.offset),"shaker":actor.camera.get_node("Shaker").get_meta("token")}
func run():
    var file=File.new()
    var open_error=file.open("res://input.json",File.READ)
    assert(open_error==OK)
    var parsed=JSON.parse(file.get_as_text())
    var config=parsed.result
    file.close()
    var world_script=load("res://world.gd")
    var world=world_script.new()
    world.name="World"
    var ui=load("res://ui.gd").new()
    var transition=load("res://transition.gd").new()
    transition.global=world
    transition.uiManager=ui
    get_root().add_child(world)
    get_root().add_child(ui)
    get_root().add_child(transition)
    world.currentScene=load("res://scene0.tscn").instance()
    get_root().add_child(world.currentScene)
    var actor=load("res://player.gd").new()
    world.partyObjects=[actor]
    world.get_current_scene_player_node().add_child(actor)
    var camera=actor.camera
    var shaker=camera.get_node("Shaker")
    var persistent=Node2D.new()
    persistent.position=Vector2(17,-9)
    world.get_current_scene_player_node().add_child(persistent)
    world._persist_array=[persistent]
    var rows=[]
    for index in config.cases.size():
        var input=config.cases[index]
        actor._tap_run=index%2==0
        actor._running=index%3!=0
        actor._speed=64.0+index
        actor._velocity=Vector2(index+.25,-index-.5)
        actor._substantial_movement=index%2==1
        actor.previous_toggle=index%3==0
        actor.run_sound="wood" if index%2==0 else "stone"
        camera.offset=Vector2(index+1,-index-2)
        shaker.set_meta("token",index+40)
        var before=state(actor)
        var old_scene=world.currentScene
        var direction=actor._direction if point(input.direction)==Vector2.ZERO else point(input.direction)
        var pending=transition._deferred_goto_scene("res://scene%d.tscn"%input.scene,point(input.position),point(input.direction),[index])
        assert(pending is GDScriptFunctionState)
        var disabled=actor._collision.disabled
        var event=Node.new()
        get_root().call_deferred("add_child",event)
        print("handoff waiting ",index)
        yield(world,"scene_changed")
        event.free()
        rows.append({"case":input,"before":before,"after":state(actor),"same_player":world.get_player()==actor,"same_camera":actor.camera==camera,"same_shaker":camera.get_node("Shaker")==shaker,"old_freed":not is_instance_valid(old_scene),"disabled_during":disabled,"enabled_after":not actor._collision.disabled,"local_position":pair(actor.position),"global_position":pair(actor.global_position),"direction":pair(actor._direction),"expected_direction":pair(direction),"parent":actor.get_parent().name,"persistent_retained":persistent.get_parent()==world.get_current_scene_player_node() and persistent.position==Vector2(17,-9),"params":world.currentScene.parameter,"ui_updates":ui.updates,"left":world.left.duplicate(true)})
    assert(file.open("res://../reference.json",File.WRITE)==OK)
    file.store_string(JSON.print({"schema":1,"godot":Engine.get_version_info(),"rows":rows},"  "))
    file.close()
    yield(self,"idle_frame")
    world.currentScene.free()
    transition.free()
    ui.free()
    world.free()
    quit()
'''
PLAYER='''extends KinematicBody2D
var _tap_run=false
var _running=false
var _speed=0.0
var _velocity=Vector2.ZERO
var _substantial_movement=false
var previous_toggle=false
var run_sound="wood"
var _direction=Vector2(0,1)
var _input_vector=Vector2.ZERO
var _collision=CollisionShape2D.new()
var eventRayCaster=Node2D.new()
var camera=Camera2D.new()
func _init():
    var shape=RectangleShape2D.new()
    shape.extents=Vector2(1,1)
    _collision.shape=shape
    add_child(_collision)
    add_child(eventRayCaster)
    add_child(camera)
    var shaker=Node.new()
    shaker.name="Shaker"
    camera.add_child(shaker)
func blend_position(_value):pass
'''
AREA='''extends Node2D
var parameter=-1
var global
func _ready():global=get_tree().get_root().get_node("World")
func init_params(value):parameter=value
func leave_for(other):global.left.append([get_meta("index"),other.get_meta("index")])
'''

def source():
    ex=Extractor(ROOT)
    def methods(path,names):
        raw=ex.data(path);return extract(raw,hashlib.sha256(raw).hexdigest(),names)
    transition,t_hash=methods('Scripts/global/SceneTransition.gd',['_deferred_goto_scene'])
    world,w_hash=methods('Scripts/global/global.gd',['get_current_scene_player_node','set_party_position','create_party_followers','get_player','get_persistent','is_persistent','remove_persistent'])
    player,p_hash=methods('Scripts/Main/party/Player.gd',['set_direction_and_input','set_direction','get_direction'])
    _,blend_hash=methods('Scripts/Main/party/Player.gd',['blend_position'])
    collision,c_hash=methods('Scripts/Main/party/party_object.gd',['set_collisions','has_collisions'])
    transition='extends Node\nconst AreaRoom=preload("res://area.gd")\nvar global\nvar uiManager\n'+transition
    world='extends Node\nconst PartyMemberPlayer=preload("res://player.gd")\nconst PartyFollower=preload("res://follower.gd")\nsignal party_changed\n# Emitted by the unchanged transition service on this injected global.\n# warning-ignore:unused_signal\nsignal scene_changed\nvar currentScene: Node = null\nvar party := [null]\nvar partyNpcs := []\nvar partySpace := []\nvar partyObjects := []\nvar _persist_array := []\nvar left=[]\n'+world
    player=PLAYER+'\n'+player+'\n'+collision
    declarations=['var party := []','var partyNpcs := []','var partySpace := []','var partyObjects := []','var _persist_array := []','var currentScene: Node = null']
    raw_global=ex.data('Scripts/global/global.gd').decode().splitlines()
    require(all(raw_global.count(line)==1 for line in declarations),'Source scene global declaration types changed')
    sources=f.Sources();parents=[]
    for entry in routes.load()['scenes']:
        nodes=instantiate(sources.doc,entry['source']);names={n.path:n for n in nodes};parent='YSort'if'YSort'in names else'Objects';require(parent in names,'Scene player parent absent');t=routes.transforms(nodes)[parent];require((t.xx,t.xy,t.yx,t.yy,t.ox,t.oy)==(1,0,0,1,0,0),'Scene Player parent transform changed; local/world conversion review required');parents.append(dict(id=entry['id'],source=entry['source'],parent=parent,transform=[t.xx,t.xy,t.yx,t.yy,t.ox,t.oy]))
    proof=dict(commit=PIN,whole_gameplay_approved=False,sources={**ex.sources,**sources.digests()},methods=dict(transition=t_hash,world=w_hash,player=p_hash,collision=c_hash,blend_position_reviewed_not_executed=blend_hash),scene_parents=parents,source_declarations=declarations,cases=CASES,probe_sha256=hashlib.sha256(PROBE.encode()).hexdigest(),adapters_sha256={k:hashlib.sha256(v.encode()).hexdigest()for k,v in dict(transition=transition,world=world,player=player,area=AREA).items()},scope='Player blend_position reviewed as direction blend parameters only, not executed; unchanged _deferred_goto_scene, single-party global helpers, Player direction and PartyObject collision methods; fixture scene roots and UI/AreaRoom leave_for adapters; opaque movement and Camera2D/Shaker identity retained, not their full scripts; driver triggers tree_changed after the original coroutine yields; no door fades, music, run playback, follower expansion, map physics, GPU or full gameplay claim')
    return transition,world,player,proof

def validate_result(doc):
    require(type(doc)is dict and set(doc)=={'schema','godot','rows'}and type(doc['schema'])is int and doc['schema']==1 and type(doc['godot'])is dict and doc['godot'].get('string')=='3.6.2-stable (official)'and type(doc['rows'])is list and len(doc['rows'])==len(CASES),'Scene player result schema/count')
    keys=set('case before after same_player same_camera same_shaker old_freed disabled_during enabled_after local_position global_position direction expected_direction parent persistent_retained params ui_updates left'.split())
    state_keys=set('tap running speed velocity substantial previous_toggle run_sound camera_offset shaker'.split())
    previous_direction=[0,1];left=[];old_scene=0
    for index,(row,case)in enumerate(zip(doc['rows'],CASES)):
        require(type(row)is dict and set(row)==keys and row['case']==case and all(type(row[k])is bool and row[k]for k in ['same_player','same_camera','same_shaker','old_freed','disabled_during','enabled_after','persistent_retained']),'Scene player identity or lifecycle mismatch')
        for key in ['local_position','global_position','direction','expected_direction']:
            require(type(row[key])is list and len(row[key])==2 and all(type(v)in(int,float)and math.isfinite(v)for v in row[key]),'Scene player pose vector malformed')
        require(type(row['case'])is dict and type(row['case']['scene'])is int and all(type(row['case'][key])is list and all(type(v)is int for v in row['case'][key])for key in ['position','direction']),'Scene player input type changed')
        require(type(row['left'])is list and all(type(pair)is list and len(pair)==2 and all(type(v)is int for v in pair)for pair in row['left']),'Scene player leave order malformed')
        expected_state=dict(tap=index%2==0,running=index%3!=0,speed=64+index,velocity=[index+.25,-index-.5],substantial=index%2==1,previous_toggle=index%3==0,run_sound='wood'if index%2==0 else'stone',camera_offset=[index+1,-index-2],shaker=index+40)
        for state in (row['before'],row['after']):
            require(type(state)is dict and set(state)==state_keys and state==expected_state and all(type(state[k])is bool for k in ['tap','running','substantial','previous_toggle'])and type(state['shaker'])is int and type(state['speed'])in(int,float),'Scene player transient changed or malformed')
            for k in ['velocity','camera_offset']:require(type(state[k])is list and len(state[k])==2 and all(type(v)in(int,float)and math.isfinite(v)for v in state[k]),'Scene player vector malformed')
        parent='YSort'if case['scene']==1 else'Objects';offset=[100,-50]if case['scene']==1 else[0,0]
        if case['direction']!=[0,0]:previous_direction=case['direction']
        left.append([old_scene,case['scene']]);old_scene=case['scene']
        require(row['local_position']==case['position']and row['global_position']==[v+o for v,o in zip(case['position'],offset)]and row['direction']==previous_direction and row['expected_direction']==previous_direction and row['parent']==parent and type(row['params'])is int and row['params']==index and type(row['ui_updates'])is int and row['ui_updates']==index+1 and row['left']==left,'Scene player pose, routing or service order mismatch')
    return doc

def fixture(doc):
    validate_result(doc);lines=['# case x y input_dir_x input_dir_y dir_x dir_y tap running speed vel_x vel_y substantial previous_toggle run_sound camera_x camera_y shaker']
    for index,row in enumerate(doc['rows']):
        s=row['after'];lines.append('\t'.join(str(v)for v in [index,*row['local_position'],*row['case']['direction'],*row['direction'],int(s['tap']),int(s['running']),s['speed'],*s['velocity'],int(s['substantial']),int(s['previous_toggle']),s['run_sound'],*s['camera_offset'],s['shaker']]))
    return '\n'.join(lines)+'\n'

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--godot',type=Path,required=True);ap.add_argument('--out',type=Path,required=True);o=ap.parse_args();out=o.out.resolve();require(out.is_relative_to(ROOT/'build')and out!=ROOT/'build'and not out.exists(),'Fresh owned scene player probe required');transition,world,player,proof=source();require(proof==json.loads((ROOT/'compatibility/reviews/scene-player-v0410.json').read_bytes()),'Scene player source review changed');out.mkdir(parents=True);project=out/'temporary';project.mkdir();(project/'.owned-scene-player').write_bytes(b'scene-player-v1')
    files={'transition.gd':transition,'world.gd':world,'player.gd':player,'area.gd':AREA,'follower.gd':'extends Node2D\nfunc init_with_follower_idx(_value):assert(false)\n','ui.gd':'extends Node\nvar updates=0\nfunc update_key_indicator():updates+=1\n','probe.gd':PROBE,'input.json':json.dumps(dict(cases=CASES)),'project.godot':'config_version=4\n[application]\nconfig/name="Native persistent player handoff"\n[logging]\nfile_logging/enable_logging=false\n[rendering]\nquality/driver/driver_name="GLES2"\n'}
    for i in range(3):
        parent='YSort'if i==1 else'Objects';offset='position = Vector2(100, -50)\n'if i==1 else''
        files['scene%d.tscn'%i]='[gd_scene load_steps=2 format=2]\n[ext_resource path="res://area.gd" type="Script" id=1]\n[node name="Scene%d" type="Node2D"]\nscript = ExtResource(1)\n__meta__ = {"index": %d}\n[node name="%s" type="Node2D" parent="."]\n%s'%(i,i,parent,offset)
    try:
        for name,value in files.items():(project/name).write_bytes(value.encode())
        invoke(o.godot.resolve(),project,['--debug','--script',str(project/'probe.gd')],out/'godot.log');doc=validate_result(json.loads((out/'reference.json').read_bytes()));(out/'handoffs.tsv').write_bytes(fixture(doc).encode())
        (out/'receipt.json').write_bytes((json.dumps(dict(source=proof,producer_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),engine_sha256=hashlib.sha256(o.godot.read_bytes()).hexdigest(),generated_sha256={name:hashlib.sha256((project/name).read_bytes()).hexdigest()for name in files},result_sha256=hashlib.sha256((out/'reference.json').read_bytes()).hexdigest(),scope=proof['scope']),indent=2)+'\n').encode());print('Scene player:',len(doc['rows']),'native single-party handoffs')
    finally:
        if project.resolve().is_relative_to(out)and(project/'.owned-scene-player').read_bytes()==b'scene-player-v1':shutil.rmtree(project)
if __name__=='__main__':main()
