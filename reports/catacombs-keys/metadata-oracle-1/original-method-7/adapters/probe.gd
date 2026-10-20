extends Node
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
	yield(get_tree(),"idle_frame")
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
	yield(get_tree(),"idle_frame")
	yield(get_tree(),"idle_frame")
	capture("key_collision_deferred")
	assert(key.get_node("Area2D/CollisionShape2D").disabled)
	key._on_AudioStreamPlayer_finished()
	capture("key_audio_finish_queued")
	yield(get_tree(),"idle_frame")
	yield(get_tree(),"idle_frame")
	capture("key_deleted")
	assert(not is_instance_valid(key))
	key=make_key()
	capture("saved_key_ready_queued")
	assert(key.is_queued_for_deletion() and uiManager.get_key_count()==1)
	yield(get_tree(),"idle_frame")
	yield(get_tree(),"idle_frame")
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
	yield(get_tree(),"idle_frame")
	yield(get_tree(),"idle_frame")
	door=make_lock()
	capture("saved_lock_ready")
	assert(door.opened and door.get_node("StaticBody2D/CollisionShape2D").disabled and not door.get_node("interact/ButtonPrompt").enabled)
	door.queue_free()
	yield(get_tree(),"idle_frame")
	yield(get_tree(),"idle_frame")
	assert(file.open("res://observations.json",File.WRITE)==OK)
	file.store_string(JSON.print({"engine":Engine.get_version_info(),"rows":rows,"connection_return":connection},"  "))
	file.close()
	get_tree().quit(17) # Intentional expected-negative signal fixture, not gameplay exit.
