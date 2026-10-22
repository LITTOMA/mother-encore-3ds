extends Node
var rows=[]
var config
var current_case=0
func _ready():call_deferred("run")
func vector(value):return [value.x,value.y]
func path(node):return str(global.currentScene.get_path_to(node)) if is_instance_valid(node) and node.is_inside_tree() else ""
func capture(frame,object,item):
 var animation=object.get_node("AnimationPlayer")
 var row={"case":current_case,"frame":frame,"root_visible":object.visible,"parent":path(object.get_parent()),"position":vector(object.position),"frame_index":object.get_node("Sprite").frame,"sprite_visible":object.get_node("Sprite").visible,"assigned":animation.assigned_animation,"playing":animation.is_playing(),"time":animation.current_animation_position if animation.assigned_animation!="" else null,"body_disabled":object.get_node("StaticBody2D/CollisionShape2D").disabled,"body_layer":object.get_node("StaticBody2D").collision_layer,"hit_layer":object.get_node("Hurtbox").collision_layer,"hit_mask":object.get_node("Hurtbox").collision_mask,"flag":object._get_flag_status(),"paused":global.paused,"rumble":global.rumble.duplicate(true),"audio":object.get_node("AudioStreamPlayer").requests.duplicate(true),"coins":object.coinSpawner.calls.duplicate(true) if object.coinSpawner else [],"has_content":is_instance_valid(item)}
 if is_instance_valid(item):
  row.content={"parent":path(item.get_parent()),"position":vector(item.position),"visible":item.visible,"physics":item.is_physics_processing(),"changing":item.changingParents,"start":vector(item.start_pos),"interactions":item.interactions,"shapes":[item.get_node("CollisionShape2D").disabled,item.get_node("Area2D/CollisionShape2D").disabled]}
 rows.append(row)
func run():
 var file=File.new()
 if file.open("res://input.json",File.READ)!=OK:get_tree().quit(2);return
 config=JSON.parse(file.get_as_text()).result
 file.close()
 for ci in config.cases.size():
  current_case=ci
  var test=config.cases[ci]
  var owner=config.owners[test.owner]
  var scene=Node2D.new()
  scene.name=config.name
  scene.set_script(load("res://scene.gd"))
  add_child(scene)
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
  parent.add_child(object)
  # Source overrides are installed before ready using a detached parent below.
  # Reconfigure paths here then call source setters; initial scene references
  # are otherwise the prefab defaults. Ready is observed separately by tests.
  object.contents=object.get_path_to(item) if item!=null else NodePath("")
  object.Drop_where=object.get_path_to(scene.get_node(owner.drop_parent))
  if owner.drop_literal=="":object.Drop_where=NodePath("")
  object.New_parent=object.get_path_to(scene.get_node(owner.new_parent)) if owner.new_parent!="" else NodePath("")
  object.dropOff=scene.get_node(owner.drop_parent)
  object.newParent=scene.get_node(owner.new_parent) if owner.new_parent!="" else null
  object.set_item()
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
 if file.open("res://../reference.json",File.WRITE)!=OK:get_tree().quit(3);return
 file.store_string(JSON.print({"schema":1,"godot":Engine.get_version_info(),"rows":rows}))
 file.close()
 get_tree().quit()
