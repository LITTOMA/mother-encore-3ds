extends Node
var config
var born=[]
var case_results=[]
var base_idle=0
var base_physics=0
var active_scene
func bits(value):
 var stream=StreamPeerBuffer.new()
 stream.put_float(value)
 stream.seek(0)
 return stream.get_u32()
func real64(value):
 var stream=StreamPeerBuffer.new()
 stream.put_double(value)
 return stream.data_array.hex_encode()
func vector(value):return [bits(value.x),bits(value.y)]
func _ready():call_deferred("run")
func on_node(node):
 if node.get_script()==preload("res://Scripts/Main/Coin.gd"):node.connect("ready",self,"coin_ready",[node])
func coin_ready(node):
 var identity=0
 for binding in config.bindings:
  if node.global_position==Vector2(binding.position[0],binding.position[1]):
   if identity!=0:
	get_tree().quit(11)
	return
   identity=binding.id
 if identity==0 or not node._drop_sfx is AudioStreamPlayer2D:
  get_tree().quit(12)
  return
 var drop=preload("res://drop.gd").new()
 drop.base=base_idle
 node.add_child(drop)
 node._drop_sfx=drop
 born.append({"ordinal":born.size(),"factory":identity,"physics":Engine.get_physics_frames()-base_physics,"idle":Engine.get_idle_frames()-base_idle,"position":vector(node.position),"speed":real64(node._speed),"direction":vector(node._direction),"air_time":real64(node._air_time),"sparkle":node.get_node("Sprite/Sparkles").frame})
func snapshot(frame,objects):
 var factories=[]
 for i in objects.size():
  var object=objects[i]
  var factory=object.coinSpawner
  factories.append({"id":config.bindings[i].id,"timer":real64(factory._timer),"remaining":factory._coins_spawned,"spawning":factory._spawning,"position":vector(factory.global_position),"parent":str(active_scene.get_path_to(object.get_parent())),"flag":object._get_flag_status(),"body_disabled":object.get_node("StaticBody2D/CollisionShape2D").disabled})
 return {"frame":frame,"physics":Engine.get_physics_frames()-base_physics,"idle":Engine.get_idle_frames()-base_idle,"factories":factories,"born":born.size()}
func run():
 var file=File.new()
 if file.open("res://input.json",File.READ)!=OK:
  get_tree().quit(2)
  return
 config=JSON.parse(file.get_as_text()).result
 file.close()
 for mode in ["forward","reverse","flagged"]:
  var scene=preload("res://scene.gd").new()
  scene.name=config.name
  active_scene=scene
  global.currentScene=scene
  global.rumble=[]
  global.paused=false
  globaldata.flags={}
  globaldata.object_flags={}
  globaldata.cash=43
  audioManager.calls=[]
  uiManager._cash.opens=0
  for name in config.parents:
   var parent=Node2D.new()
   parent.name=name
   scene.add_child(parent)
  if mode=="flagged":globaldata.object_flags[config.owners[0].flag]=true
  var objects=[]
  for owner in config.owners:
   var object=load("res://"+config.barrel).instance()
   for key in owner.properties:
	var value=owner.properties[key]
	if typeof(value)==TYPE_DICTIONARY:
	 if value.kind=="vector":value=Vector2(value.value[0],value.value[1])
	 elif value.kind=="path":value=NodePath(value.value)
	 else:
	  get_tree().quit(13)
	  return
	object.set(key,value)
   object.name=owner.name
   var original=object.get_node("AudioStreamPlayer")
   if not original is AudioStreamPlayer2D:
	get_tree().quit(14)
	return
   object.remove_child(original)
   original.free()
   var voice=preload("res://voice.gd").new()
   voice.name="AudioStreamPlayer"
   object.add_child(voice)
   scene.get_node(owner.parent).add_child(object)
   objects.append(object)
  get_tree().connect("node_added",self,"on_node")
  add_child(scene)
  yield(get_tree(),"idle_frame")
  seed(int(config.seed))
  base_idle=Engine.get_idle_frames()
  base_physics=Engine.get_physics_frames()
  global.base=base_idle
  born=[]
  var rows=[]
  rows.append(snapshot(-1,objects))
  var hit_order=objects.duplicate()
  if mode=="reverse":hit_order.invert()
  var emitter=Node2D.new()
  scene.add_child(emitter)
  for object in hit_order:
   object.get_node("AudioStreamPlayer").base=base_idle
   emitter.global_position=object.global_position
   object._on_Hurtbox_area_entered(emitter)
  rows.append(snapshot(0,objects))
  for frame in range(1,41):
   yield(get_tree(),"idle_frame")
   rows.append(snapshot(frame,objects))
  case_results.append({"mode":mode,"rows":rows,"births":born.duplicate(true),"rng_tail":randi(),"cash":globaldata.cash,"cash_opens":uiManager._cash.opens,"collect_audio":audioManager.calls.duplicate(true)})
  get_tree().disconnect("node_added",self,"on_node")
  scene.free()
  global.currentScene=null
  active_scene=null
  yield(get_tree(),"idle_frame")
 if file.open("res://../reference.json",File.WRITE)!=OK:
  get_tree().quit(3)
  return
 file.store_string(JSON.print({"schema":1,"godot":Engine.get_version_info(),"cases":case_results}))
 file.close()
 get_tree().quit()
