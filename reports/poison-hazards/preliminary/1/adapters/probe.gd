extends Node
var config
var player
var party
var outsider
var holes=[]
var areas=[]
var geyser
var rows=[]
var timeouts=[]
var tick=0
var initial=[]
func bits(value):
	var bytes=StreamPeerBuffer.new()
	bytes.big_endian=false
	bytes.put_float(value)
	bytes.seek(0)
	return bytes.get_u32()
func clocks():
	var result=[]
	for hole in holes:result.append([bits(hole.timer.time_left),bits(hole.timer.wait_time),hole.timer.paused])
	return result
func timeout(index):timeouts.append(index)
func _ready():
	pause_mode=Node.PAUSE_MODE_PROCESS
	var file=File.new()
	assert(file.open("res://input.json",File.READ)==OK)
	config=JSON.parse(file.get_as_text()).result
	file.close()
	player=load("res://party.gd").new()
	party=load("res://party.gd").new()
	outsider=Node2D.new()
	for body in [player,party,outsider]:add_child(body)
	global.player=player
	global.currentScene=self
	var objects=Node2D.new()
	objects.name="Objects"
	add_child(objects)
	var source_node=Node2D.new()
	source_node.name="Poison Geyser"
	var factory=PackedScene.new()
	assert(factory.pack(source_node)==OK)
	source_node.free()
	for index in config.holes.size():
		var binding=config.holes[index]
		var hole=load("res://hole.gd").new()
		hole.pause_mode=Node.PAUSE_MODE_STOP
		hole.interval=binding.interval
		hole.delay=binding.delay
		hole.position=Vector2(binding.position[0],binding.position[1])
		hole.poison=factory
		var timer=Timer.new()
		timer.name="Timer"
		timer.wait_time=8.0
		assert(timer.connect("timeout",hole,"_on_Timer_timeout")==OK)
		hole.add_child(timer)
		add_child(hole)
		assert(timer.connect("timeout",self,"timeout",[index])==OK)
		assert(timer.process_mode==Timer.TIMER_PROCESS_IDLE and !timer.one_shot and !timer.autostart)
		holes.append(hole)
	for binding in config.areas:
		var area=load("res://"+config.area_scene).instance()
		area.collision_layer=int(binding.layer)
		area.collision_mask=int(binding.mask)
		area.monitoring=bool(int(binding.bits)&1)
		area.monitorable=bool(int(binding.bits)&2)
		area.stepping_sound=NodePath("Step")
		var step=load("res://step.gd").new()
		step.name="Step"
		step.entering_sound=config.step.enter
		step.exiting_sound=config.step.exit
		step.enter_shadow_effect=config.step.enter_shadow
		step.exit_shadow_effect=config.step.exit_shadow
		step.enabled=config.step.enabled
		area.add_child(step)
		for role in 2:
			var shape=config.shapes[int(binding.first)+role]
			var polygon=area.get_node("CollisionPolygon2D" if role==0 else "StaticBody2D/CollisionPolygon2D")
			polygon.position=Vector2(shape.position[0],shape.position[1])
			var points=PoolVector2Array()
			for index in range(int(shape.first),int(shape.first+shape.count)):points.append(Vector2(config.points[index][0],config.points[index][1]))
			polygon.polygon=points
			if role==1:
				area.get_node("StaticBody2D").collision_layer=int(shape.layer)
				area.get_node("StaticBody2D").collision_mask=int(shape.mask)
		add_child(area)
		areas.append(area)
	geyser=load("res://geyser.gd").new()
	add_child(geyser)
	initial=clocks()
func _process(_delta):
	var before_pause=get_tree().paused
	var delta_bits=bits(get_process_delta_time())
	for action in config.timeline[tick]:
		match action[0]:
			"player_pause":
				player._paused=action[1]
				player.emit_signal("paused" if action[1] else "unpaused")
			"hole_pause":
				if action[2]:holes[int(action[1])].pause()
				else:holes[int(action[1])].unpause()
			"tree_pause":get_tree().paused=action[1]
			"visible":
				for hole in holes:hole.visible=action[1]
			_:assert(false)
	rows.append({"tick":tick,"actions":config.timeline[tick],"before_tree_pause":before_pause,"delta_bits":delta_bits,"clocks":clocks(),"timeouts":timeouts.duplicate()})
	timeouts.clear()
	tick+=1
	if tick==config.timeline.size():
		set_process(false)
		for hole in holes:hole.free()
		get_tree().paused=false
		call_deferred("finish")
func body_state(body):
	return [body._is_continuous_damage,body._attack_damage,body._damage_variance,body._party_member.statuses.duplicate(),body.shadow_mode]
func finish():
	yield(get_tree(),"idle_frame")
	yield(get_tree(),"idle_frame")
	var spawned=[]
	for child in get_node("Objects").get_children():spawned.append([bits(child.position.x),bits(child.position.y)])
	var contacts=[]
	for action in config.contacts:
		player._paused=action[4]
		var body=[player,party,outsider][int(action[2])]
		var entered=action[3]
		if int(action[0])==0:
			var area=areas[int(action[1])]
			if entered:area._on_Area2D_body_entered(body)
			else:area._on_Poison_body_exited(body)
		else:
			if entered:geyser._on_Poison_Geyser_body_entered(body)
			else:geyser._on_Poison_Geyser_body_exited(body)
		contacts.append({"action":action,"player":body_state(player),"party":body_state(party),"wanted":player.run_sound})
	var geometry=[]
	for area in areas:
		for path in ["CollisionPolygon2D","StaticBody2D/CollisionPolygon2D"]:
			var polygon=area.get_node(path)
			var vertices=[]
			for point in polygon.polygon:
				var world=polygon.to_global(point)
				vertices.append([bits(world.x),bits(world.y)])
			geometry.append(vertices)
	var file=File.new()
	assert(file.open("res://../reference.json",File.WRITE)==OK)
	file.store_string(JSON.print({"schema":1,"godot":Engine.get_version_info(),"initial":initial,"rows":rows,"spawned":spawned,"contacts":contacts,"geometry":geometry}))
	file.close()
	get_tree().quit()
