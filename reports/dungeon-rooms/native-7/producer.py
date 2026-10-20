"""Original dungeon camera-room methods and native geometry/idle signals.

The eighteen native Area2D prefabs retain source sibling order and a hidden
parent. Entry/exit are explicitly delivered through the original body handler;
player movement and the inherited scene are bounded services, not full physics
or a playable map. Actual Camera2D integer setters and source offset/flag/room
methods run in the original engine, at both original and adapted viewport sizes.
"""
import argparse,hashlib,json,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import dungeon_rooms as d
from tools.run_battle_victory_reference import function
from tools.run_scene_reference import invoke

GLOBAL='''extends Node
signal flags_updated
var currentScene
var currentCamera
var player
var flag_signals=0
func _ready():
    player=Node2D.new()
    player.name="PlayerService"
    add_child(player)
    connect("flags_updated",self,"count_flag")
func count_flag():flag_signals+=1
func broadcast_flags_updated():emit_signal("flags_updated")
func get_player():return player
'''
PROBE='''extends Node
var config
var areas=[]
var rows=[]
var geometry=[]
var viewport_index=0
func _ready():call_deferred("run")
func vec(v):return ["%.17f"%v.x,"%.17f"%v.y]
func capture(action):
    var camera=global.currentCamera
    var current=global.currentScene._current_room
    var active=[]
    var flags=[]
    for i in areas.size():
        active.append(areas[i].inside)
        flags.append(globaldata.object_flags.get(config.model.rooms[i].flag,false))
    rows.append({"viewport":viewport_index,"action":action,"inside":active,"flags":flags,"flag_signals":global.flag_signals,"camareas":camera.camareas,"limits":[camera.limit_left,camera.limit_top,camera.limit_right,camera.limit_bottom],"camera_position":vec(camera.position),"offset_calls":camera.offset_calls.duplicate(true),"room":global.currentScene.get_current_room_id() if current else -1,"relative":vec(global.currentScene.get_player_position_relative_to_current_room()) if current else null})
    global.flag_signals=0
    camera.offset_calls.clear()
func make_scene():
    var scene=load("res://dungeon.gd").new()
    scene.name=config.model.name
    global.currentScene=scene
    var container=Node2D.new()
    container.name="Rooms"
    container.visible=config.proof.container_visible
    scene.add_child(container)
    areas=[]
    for i in config.model.rooms.size():
        var row=config.model.rooms[i]
        var instance=config.proof.native_instances[i]
        var area=load("res://"+config.paths.area).instance()
        area.set_script(load("res://"+config.paths.room_script))
        area.name=row.path.get_file()
        area.position=Vector2(instance.position[0],instance.position[1])
        area.collision_layer=row.layer
        area.collision_mask=row.mask
        area.monitoring=bool(int(row.bits)&1)
        area.monitorable=bool(int(row.bits)&2)
        var shape=area.get_node("CollisionShape2D")
        var rectangle=RectangleShape2D.new()
        rectangle.extents=Vector2(row.extents[0],row.extents[1])
        shape.shape=rectangle
        shape.position=Vector2(instance.shape_position[0],instance.shape_position[1])
        area.camera_offset=Vector2(row.offset[0],row.offset[1])
        container.add_child(area)
        areas.append(area)
    add_child(scene)
    var camera=load("res://camera.gd").new()
    camera.name="CameraService"
    add_child(camera)
    global.currentCamera=camera
    for area in areas:geometry.append({"viewport":viewport_index,"index":area.get_index(),"flag":area._flag,"center":vec(area.get_area_global_position()),"size":vec(area.get_size()),"layer":area.collision_layer,"mask":area.collision_mask,"monitoring":area.monitoring,"monitorable":area.monitorable,"visible":area.is_visible_in_tree()})
func enter(i):areas[i]._on_camarea_body_entered(global.player)
func leave(i):areas[i]._on_camarea_body_exited(global.player)
func run():
    var file=File.new()
    assert(file.open("res://input.json",File.READ)==OK)
    config=JSON.parse(file.get_as_text()).result
    file.close()
    for view in [Vector2(320,180),Vector2(400,240)]:
        get_tree().root.set_size_override(true,view)
        assert(get_viewport().get_visible_rect().size==view)
        global.player.position=Vector2(7,11)
        globaldata.object_flags={config.model.rooms[0].flag:true}
        global.flag_signals=0
        make_scene()
        capture("ready")
        areas[0]._on_camarea_body_entered(self)
        areas[0]._on_camarea_body_exited(self)
        capture("other_body_ignored")
        for i in areas.size():
            enter(i)
            capture("enter/"+str(i))
            yield(get_tree(),"idle_frame")
            capture("idle/"+str(i))
            leave(i)
            capture("exit/"+str(i))
        enter(0)
        capture("overlap_enter0")
        enter(1)
        capture("overlap_enter1")
        yield(get_tree(),"idle_frame")
        capture("overlap_idle")
        leave(1)
        capture("overlap_exit1_keeps_last_limits")
        leave(0)
        capture("overlap_exit0_resets_limits")
        enter(2)
        capture("cancel_enter")
        leave(2)
        capture("cancel_exit")
        yield(get_tree(),"idle_frame")
        capture("cancel_idle")
        enter(0)
        capture("reenter_visited")
        yield(get_tree(),"idle_frame")
        capture("reenter_idle")
        leave(0)
        capture("reenter_exit")
        enter(0)
        leave(0)
        enter(0)
        capture("same_frame_reenter")
        yield(get_tree(),"idle_frame")
        capture("same_frame_reenter_idle_twice")
        leave(0)
        capture("same_frame_reenter_exit")
        global.currentScene.queue_free()
        global.currentCamera.queue_free()
        yield(get_tree(),"idle_frame")
        yield(get_tree(),"idle_frame")
        viewport_index+=1
    assert(file.open("res://observations.json",File.WRITE)==OK)
    file.store_string(JSON.print({"engine":Engine.get_version_info(),"geometry":geometry,"rows":rows},"  "))
    file.close()
    print("DUNGEON_ROOMS_REFERENCE_COMPLETE")
    get_tree().quit()
'''

def run(out,engine):
 out=Path(out).resolve();engine=Path(engine).resolve();d.require(out.is_relative_to((ROOT/'build').resolve())and out!=ROOT/'build'and not out.exists()and engine.is_file(),'Fresh dungeon-room reference and engine required')
 review,s=d.read();model,raw,proof=d.extract();paths=review['paths'];project=out/'project';project.mkdir(parents=True);(project/'.owned-dungeon-rooms-reference').write_text('source-dungeon-rooms-v1',encoding='utf8');copied={}
 for key in ('area','base_script','room_script'):
  name=paths[key];value=s.data(name);target=project/name;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(value);copied[name]=hashlib.sha256(value).hexdigest()
 dungeon='extends Node2D\nclass_name DungeonAreaRoom\nvar _current_room=null\n'+'\n'.join(function(s.text(paths['dungeon_script']),name)for name in ('set_current_room','get_current_room_id','get_player_position_relative_to_current_room'))
 camera='extends Camera2D\nvar camareas=0\nvar _camarea_offset=Vector2.ZERO\nvar offset_calls=[]\n'+function(s.text(paths['camera_script']),'set_camarea_offset')+'\toffset_calls.append(["%.17f"%off.x,"%.17f"%off.y])\n'
 data='extends Node\nvar object_flags={}\n'+function(s.text(paths['data_script']),'set_object_flag')
 adapters={'dungeon.gd':dungeon,'camera.gd':camera,'data.gd':data,'global.gd':GLOBAL,'probe.gd':PROBE}
 for name,value in adapters.items():(project/name).write_text(value,encoding='utf8')
 (project/'input.json').write_text(json.dumps(dict(model=model,proof=proof,paths=paths),indent=2)+'\n',encoding='utf8')
 (project/'probe.tscn').write_text('[gd_scene load_steps=2 format=2]\n[ext_resource path="res://probe.gd" type="Script" id=1]\n[node name="Probe" type="Node"]\nscript = ExtResource( 1 )\n',encoding='utf8')
 (project/'project.godot').write_text('config_version=4\n[application]\nconfig/name="Source dungeon camera rooms"\nrun/main_scene="res://probe.tscn"\n[autoload]\nglobal="*res://global.gd"\nglobaldata="*res://data.gd"\n[debug]\ngdscript/warnings/unused_argument=false\n',encoding='utf8')
 invoke(engine,project,['--debug','--editor','--quit'],out/'import.log')
 for name,value in adapters.items():(project/name).write_text(value,encoding='utf8')
 invoke(engine,project,['--debug'],out/'engine.log');doc=json.loads((project/'observations.json').read_bytes());v=doc['engine']
 d.require((v['major'],v['minor'],v['patch'],v['hash'])==(3,6,2,'3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8')and len(doc['geometry'])==36 and len(doc['rows'])==140,'Dungeon-room native engine/domain coverage')
 for name,digest in copied.items():d.require(hashlib.sha256((project/name).read_bytes()).hexdigest()==digest,'Dungeon-room source copy changed')
 for name,value in adapters.items():d.require((project/name).read_text(encoding='utf8')==value,'Dungeon-room adapter changed')
 for name in ('input.json','observations.json'):(out/name).write_bytes((project/name).read_bytes())
 (out/'source.encdrooms').write_bytes(raw)
 for name in adapters:(out/name).write_bytes((project/name).read_bytes())
 sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
 receipt=dict(commit=d.PIN,scope=__doc__,source=proof,review_sources=s.digests(),engine_sha256=sha(engine),producer_sha256=sha(Path(__file__)),copied_sha256=copied,adapter_sha256={name:sha(project/name)for name in adapters},observations=140,native_geometry=36,instrumentation='Camera offset setter retains original statements, then appends a read-only observation of the argument. Scene inheritance and body entry are explicit services.',outputs_sha256={name:sha(out/name)for name in ('input.json','observations.json','source.encdrooms','engine.log','import.log')})
 (out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf8');print('Source dungeon rooms:140 method/idle observations,36 native geometry records,two viewport sizes; no complete-scene/physics admission');return receipt
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',type=Path,required=True);p.add_argument('--godot',type=Path,required=True);a=p.parse_args();run(a.out,a.godot)
