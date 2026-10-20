extends Node
var config
var targets=[]
var rows=[]
var entries=[]
var notices=0
var initials=[]
func _ready():call_deferred("run")
func notice():notices+=1
func describe(node):
	if not is_instance_valid(node):return {"alive":false}
	return {"alive":true,"queued":node.is_queued_for_deletion(),"visible":node.visible,"audio":node.get_node("AudioStreamPlayer").playing,"disabled":node.get_node("Area2D/CollisionShape2D").disabled}
func entered(body,owner):entries.append({"owner":owner,"player":body==global.player})
func body_shape(body):
	var shape=CollisionPolygon2D.new()
	shape.name="CollisionShape2D"
	var points=PoolVector2Array()
	for value in config.player.points:points.append(Vector2(value[0],value[1]))
	shape.polygon=points
	shape.position=Vector2(config.player.position[0],config.player.position[1])
	body.add_child(shape)
func place(body,owner):body.global_position=Vector2(owner.center[0]-config.player.center[0],owner.center[1]-config.player.center[1])
func make_keys():
	var parent=TileMap.new()
	parent.name=config.parent
	parent.cell_size=Vector2(config.cell_size[0],config.cell_size[1])
	global.currentScene.add_child(parent)
	targets.clear()
	for definition in config.keys:
		var target=Sprite.new()
		target.name=definition.name
		target.hframes=definition.hframes
		target.position=Vector2(definition.position[0],definition.position[1])
		target.set_script(load("res://"+config.key_script))
		target.emit_flag_updated_signal=definition.emit_flags
		var area=Area2D.new()
		area.name="Area2D"
		area.collision_layer=definition.layer
		area.collision_mask=definition.mask
		area.monitoring=bool(int(definition.bits)&2)
		area.monitorable=bool(int(definition.bits)&4)
		var shape=CollisionShape2D.new()
		shape.name="CollisionShape2D"
		shape.position=Vector2(definition.center[0]-definition.position[0],definition.center[1]-definition.position[1])
		var rectangle=RectangleShape2D.new()
		rectangle.extents=Vector2(definition.extents[0],definition.extents[1])
		shape.shape=rectangle
		shape.disabled=bool(int(definition.bits)&1)
		shape.one_way_collision_margin=definition.margin
		area.add_child(shape)
		target.add_child(area)
		var audio=load("res://audio.gd").new()
		audio.name="AudioStreamPlayer"
		target.add_child(audio)
		assert(area.connect("body_entered",target,"_on_Area2D_body_entered")==OK)
		assert(audio.connect("finished",target,"_on_AudioStreamPlayer_finished")==OK)
		assert(area.connect("body_entered",self,"entered",[definition.owner])==OK)
		parent.add_child(target)
		targets.append(target)
func settle():
	for _i in 4:yield(get_tree(),"physics_frame")
	yield(get_tree(),"idle_frame")
func capture(ci,pi):
	var states=[]
	var flags=[]
	var writes=[]
	for i in config.keys.size():
		states.append(describe(targets[i]))
		flags.append(globaldata.object_flags.get(config.keys[i].flag,false))
		if is_instance_valid(targets[i]):
			var audio=targets[i].get_node("AudioStreamPlayer")
			for value in audio.writes:writes.append({"owner":config.keys[i].owner,"playing":value})
			audio.writes.clear()
	rows.append({"case":ci,"phase":pi,"count":uiManager.get_key_count(),"states":states,"flags":flags,"entries":entries.duplicate(true),"notices":notices,"ui":uiManager.events.duplicate(true),"audio_writes":writes})
	entries.clear()
	notices=0
	uiManager.events.clear()
func run():
	var file=File.new()
	assert(file.open("res://input.json",File.READ)==OK)
	config=JSON.parse(file.get_as_text()).result
	file.close()
	global.connect("flags_updated",self,"notice")
	body_shape(global.player)
	var other=KinematicBody2D.new()
	other.name="OtherBody"
	body_shape(other)
	global.add_child(other)
	for ci in config.cases.size():
		var test=config.cases[ci]
		global.currentScene.queue_free()
		yield(get_tree(),"idle_frame")
		yield(get_tree(),"idle_frame")
		global.currentScene=global.SceneService.new()
		global.currentScene.name="Catacombs"
		global.add_child(global.currentScene)
		globaldata.object_flags.clear()
		globaldata.flags.clear()
		globaldata.keys={"Catacombs":0}
		entries.clear()
		notices=0
		uiManager.events.clear()
		global.player.position=Vector2(10000,10000)
		other.position=Vector2(10000,10000)
		global.player.collision_layer=config.player.layer
		global.player.collision_mask=config.player.mask
		other.collision_layer=config.player.layer
		other.collision_mask=config.player.mask
		global.player.get_node("CollisionShape2D").disabled=false
		global.player.paused=test.initial_paused
		# Flush the old body's physical transform while the new scene is empty.
		# Otherwise a newly registered Area can observe the previous case's pose.
		yield(settle(),"completed")
		if test.saved:globaldata.object_flags[config.keys[test.target].flag]=true
		if test.initial_inside:place(global.player,config.keys[test.target])
		make_keys()
		yield(settle(),"completed")
		initials.append({"case":ci,"count":uiManager.get_key_count()})
		for pi in test.phases.size():
			var phase=test.phases[pi]
			global.player.paused=phase.paused
			global.player.collision_layer=phase.layer
			global.player.collision_mask=phase.mask
			global.player.get_node("CollisionShape2D").disabled=not phase.enabled
			global.player.position=Vector2(10000,10000)
			other.position=Vector2(10000,10000)
			if phase.inside:place(other if phase.other else global.player,config.keys[test.target])
			if phase.finish and is_instance_valid(targets[test.target]):targets[test.target].get_node("AudioStreamPlayer").finish()
			yield(settle(),"completed")
			capture(ci,pi)
	global.currentScene.queue_free()
	other.queue_free()
	yield(get_tree(),"idle_frame")
	yield(get_tree(),"idle_frame")
	assert(file.open("res://../contacts.json",File.WRITE)==OK)
	file.store_string(JSON.print({"engine":Engine.get_version_info(),"rows":rows,"initials":initials},"  "))
	file.close()
	print("KEY_CONTACT_REFERENCE_COMPLETE")
	get_tree().quit()
