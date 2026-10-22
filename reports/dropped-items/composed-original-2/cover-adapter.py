"""Original Breakable/Flaggable scripts and prefab animations, bounded providers.

The content holder, player pause, coin factory and voice request recorder are
explicit services. This does not execute the contents' AI or admit a full room.
"""
import argparse,hashlib,json,posixpath,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import catacombs_breakables as b
from tools.reference_movement import extract
from tools.run_scene_reference import invoke

PROBE=r'''extends Node
var rows=[]
var config
var current_case=0
func _ready():call_deferred("run")
func bits(value):
 var stream=StreamPeerBuffer.new()
 stream.put_float(value)
 stream.seek(0)
 return stream.get_u32()
func vector(value):return [bits(value.x),bits(value.y)]
func path(node):return str(global.currentScene.get_path_to(node)) if is_instance_valid(node) and node.is_inside_tree() else ""
func capture(frame,object,item):
 var animation=object.get_node("AnimationPlayer")
 var row={"case":current_case,"frame":frame,"root_visible":object.visible,"parent":path(object.get_parent()),"position":vector(object.position),"frame_index":object.get_node("Sprite").frame,"sprite_visible":object.get_node("Sprite").visible,"assigned":animation.assigned_animation,"playing":animation.is_playing(),"time":bits(animation.current_animation_position) if animation.assigned_animation!="" else null,"body_disabled":object.get_node("StaticBody2D/CollisionShape2D").disabled,"body_layer":object.get_node("StaticBody2D").collision_layer,"hit_layer":object.get_node("Hurtbox").collision_layer,"hit_mask":object.get_node("Hurtbox").collision_mask,"flag":object._get_flag_status(),"paused":global.paused,"rumble":global.rumble.duplicate(true),"audio":object.get_node("AudioStreamPlayer").requests.duplicate(true),"coins":object.coinSpawner.calls.duplicate(true) if object.coinSpawner else [],"has_content":is_instance_valid(item)}
 if is_instance_valid(item):
  row.content={"parent":path(item.get_parent()),"position":vector(item.position),"visible":item.visible,"physics":item.is_physics_processing(),"changing":item.changingParents,"start":vector(item.start_pos),"interactions":item.interactions,"shapes":[item.get_node("CollisionShape2D").disabled,item.get_node("Area2D/CollisionShape2D").disabled]}
 rows.append(row)
func run():
 var file=File.new()
 if file.open("res://input.json",File.READ)!=OK:
  get_tree().quit(2)
  return
 config=JSON.parse(file.get_as_text()).result
 file.close()
 for ci in config.cases.size():
  current_case=ci
  var test=config.cases[ci]
  var owner=config.owners[test.owner]
  var scene=Node2D.new()
  scene.name=config.name
  scene.set_script(load("res://scene.gd"))
  global.currentScene=scene
  global.paused=false
  global.rumble=[]
  globaldata.flags={}
  globaldata.object_flags={}
  if test.flag:
   if owner.flag_kind=="object":globaldata.object_flags[owner.flag]=true
   else:globaldata.flags[owner.flag]=true
  for parent in config.parents:
   var node=Node2D.new()
   node.name=parent.path
   scene.add_child(node)
  var item=null
  if owner.content!="":
   item=load("res://content.gd").new()
   item.name=owner.content.get_file()
   item.position=Vector2(19,23)
   var collider=CollisionShape2D.new()
   collider.name="CollisionShape2D"
   var shape=RectangleShape2D.new()
   shape.extents=Vector2(2,2)
   collider.shape=shape
   item.add_child(collider)
   var area=Area2D.new()
   area.name="Area2D"
   area.monitoring=false
   area.monitorable=false
   area.collision_layer=0
   area.collision_mask=0
   var nested=collider.duplicate()
   area.add_child(nested)
   item.add_child(area)
   scene.get_node(owner.content.get_base_dir()).add_child(item)
   item.set_physics_process(true)
  var family=config.families[0] if config.families[0].id==owner.family else config.families[1]
  var object=load("res://"+family.source).instance()
  object.name=owner.path.get_file()
  object.position=Vector2(owner.position[0],owner.position[1])
  var original_audio=object.get_node("AudioStreamPlayer")
  object.remove_child(original_audio)
  original_audio.free()
  var voice=load("res://voice.gd").new()
  voice.name="AudioStreamPlayer"
  object.add_child(voice)
  for key in ["appear","disappear","flag"]:
   object.set(key+"_flag" if key in ["appear","disappear"] else key,owner[key])
  object.is_object_flag=owner.flag_kind=="object"
  object.emit_flag_updated_signal=owner.emit_flag
  object.reset_when_leaving_area=owner.reset_area
  object.reset_when_leaving_region=owner.reset_region
  object.Drop_item_if_hidden=owner.Drop_item_if_hidden
  object.interact_content=owner.interact_content
  object.pause_on_break=owner.pause_on_break
  object.coins_spawned=owner.coins
  var parent=scene.get_node(owner.parent)
  object.contents=NodePath(owner.content_relative)
  object.Drop_where=NodePath(owner.drop_literal)
  object.New_parent=NodePath(owner.new_parent_relative)
  parent.add_child(object)
  add_child(scene)
  yield(get_tree(),"idle_frame")
  voice.base=Engine.get_idle_frames()
  if object.coinSpawner:object.coinSpawner.base=voice.base
  global.base=voice.base
  capture(-1,object,item)
  var emitter=Node2D.new()
  scene.add_child(emitter)
  emitter.global_position=object.global_position+Vector2(test.distance,0)
  var pending=object._on_Hurtbox_area_entered(emitter)
  capture(0,object,item)
  for frame in range(1,46):
   yield(get_tree(),"idle_frame")
   capture(frame,object,item)
  rows[-1].pending=is_instance_valid(pending) and pending.is_valid()
  emitter.free()
  scene.free()
  global.currentScene=null
  yield(get_tree(),"idle_frame")
 if file.open("res://../reference.json",File.WRITE)!=OK:
  get_tree().quit(3)
  return
 file.store_string(JSON.print({"schema":1,"godot":Engine.get_version_info(),"rows":rows}))
 file.close()
 get_tree().quit()
'''

def source():
 p,m,proof=b.source();proof['content_producer_sha256']=proof.pop('producer_sha256');selected=['Objects/Coffin3','Objects/Coffin','Objects/Barrel2','Objects/Coffin13'];cases=[dict(owner=next(i for i,v in enumerate(m['owners'])if v['path']==name),distance=distance,flag=flag)for name,distance,flag in [(v,100 if v=='Objects/Coffin'else 0,False)for v in selected]+[('Objects/Barrel2',0,True),('Objects/Barrel2',10,False)]]
 files={'project.godot':'config_version=4\n[application]\nconfig/name="Breakable script providers"\nrun/main_scene="res://main.tscn"\n[autoload]\nglobal="*res://global.gd"\nglobaldata="*res://flags.gd"\n[logging]\nfile_logging/enable_logging=false\n[rendering]\nquality/driver/driver_name="GLES2"\n','main.tscn':'[gd_scene load_steps=2 format=2]\n[ext_resource path="res://probe.gd" type="Script" id=1]\n[node name="Reference" type="Node"]\nscript=ExtResource(1)\n','scene.gd':'extends Node2D\nsignal area_left(region_changed)\n','probe.gd':PROBE,'global.gd':'extends Node\nsignal flags_updated\nvar currentScene\nvar paused=false\nvar rumble=[]\nvar base=0\nfunc get_player():return self\nfunc pause():paused=true\nfunc start_joy_vibration(device,weak,strong,duration):rumble.append([Engine.get_idle_frames()-base,device,weak,strong,duration])\n','content.gd':'extends Node2D\nvar start_pos=Vector2(7,11)\nvar changingParents=false\nvar interactions=0\nfunc interact():interactions+=1\n','voice.gd':'extends Node\nvar base=0\nvar requests=[]\nvar playing=false setget set_playing\nfunc set_playing(value):\n playing=value\n requests.append([Engine.get_idle_frames()-base,value])\n','factory.gd':'class_name CoinSpawner\nextends Node2D\nvar base=0\nvar calls=[]\nfunc spawn_coins(count):calls.append([Engine.get_idle_frames()-base,count])\n'}
 classes=[dict(base='Sprite',**{'class':'FlaggableObject'},language='GDScript',path='res://'+p['paths']['flag_script']),dict(base='Node2D',**{'class':'CoinSpawner'},language='GDScript',path='res://factory.gd')]
 files['project.godot']=files['project.godot'].replace('config_version=4\n','config_version=4\n_global_script_classes='+json.dumps(classes)+'\n_global_script_class_icons={"FlaggableObject":"","CoinSpawner":""}\n')
 files['scene.gd']+='func leave(region_changed):emit_signal("area_left",region_changed)\n'
 files['global.gd']+='func notify_flags():emit_signal("flags_updated")\n'
 flags,_=extract(pth:=b.f.Sources().data(p['paths']['global_data']),hashlib.sha256(pth).hexdigest(),['check_appear_disappear_flags','set_flag','set_object_flag']);files['flags.gd']='extends Node\nvar flags={}\nvar object_flags={}\n'+flags
 copied={name:b.f.Sources().data(name)for name in [p['paths'][key]for key in ['script','flag_script','coffin','barrel','hurtbox','coffin_texture','barrel_texture','sound']]}
 # One explicit prefab dependency replacement: the same CoinSpawner class API,
 # recording factory calls instead of admitting unported coin physics/RNG.
 files[p['paths']['coin_spawner']]='[gd_scene load_steps=2 format=2]\n[ext_resource path="res://factory.gd" type="Script" id=1]\n[node name="CoinSpawner" type="Node2D"]\nscript=ExtResource(1)\n'
 for owner in m['owners']:
  for key in ['content','new_parent']:owner[key+'_relative']=posixpath.relpath(owner[key],owner['path'])if owner[key]else''
 config=dict(name=m['name'],owners=m['owners'],families=[{k:v[k]for k in ['id','source']}for v in m['families']],parents=m['parents'],cases=cases);files['input.json']=json.dumps(config)
 proof.update(generated_sha256={k:hashlib.sha256(v.encode()).hexdigest()for k,v in files.items()},copied_sha256={k:hashlib.sha256(v).hexdigest()for k,v in copied.items()},scope='Actual original Breakable/Flaggable scripts and two prefab AnimationPlayers. Explicit Node2D contents, pause/rumble, factory and voice-command providers;ordinary idle timers/tweens. No contents AI,coin physics,economy,whole-room,3DS/GPU/hardware admission.');return files,copied,proof

def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--engine',type=Path,required=True);ap.add_argument('--out',type=Path,required=True);a=ap.parse_args();out=a.out.resolve();b.require(out.is_relative_to(ROOT/'build')and not out.exists(),'Fresh Breakable reference under build required');files,copied,proof=source();out.mkdir(parents=True);project=out/'project';project.mkdir();(project/'.owned-breakable-reference').write_bytes(b'breakable-reference-v1')
 for name,raw in copied.items():path=project/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(raw)
 def write():
  for name,value in files.items():path=project/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(value.encode())
 write();(out/'inputs.json').write_text(json.dumps(proof,indent=2)+'\n');invoke(a.engine,project,['--video-driver','GLES2','--debug','--editor','--quit'],out/'editor.log');write();invoke(a.engine,project,['--video-driver','GLES2','--debug','--fixed-fps','60'],out/'godot.log');b.require(all((project/name).read_bytes()==value.encode()for name,value in files.items())and all((project/name).read_bytes()==value for name,value in copied.items()),'Breakable executed source/observer changed');raw=(out/'reference.json').read_bytes();doc=json.loads(raw);b.require(len(doc['rows'])==6*47,'Breakable observation count');proof.update(producer_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),engine_sha256=hashlib.sha256(a.engine.read_bytes()).hexdigest(),result_sha256=hashlib.sha256(raw).hexdigest());(out/'receipt.json').write_text(json.dumps(proof,indent=2)+'\n');print('Original Breakable scripts:',len(doc['rows']),'observations with explicit prepared providers')

if __name__=='__main__':main()
