extends Node
var config
var rows=[]
var scene
var switches={}
var objects={}
func _ready():call_deferred("run")
func read_config():
	var file=File.new()
	assert(file.open("res://input.json",File.READ)==OK)
	config=JSON.parse(file.get_as_text()).result
	file.close()
func parent_for(path):
	var parent_path=path.get_base_dir()
	if scene.has_node(parent_path):return scene.get_node(parent_path)
	var parent=Node2D.new()
	parent.name=parent_path
	scene.add_child(parent)
	return parent
func animator(target,names):
	var player=AnimationPlayer.new()
	player.name="AnimationPlayer"
	player.playback_process_mode=AnimationPlayer.ANIMATION_PROCESS_MANUAL
	for name in names:
		var animation=Animation.new()
		animation.length=0.01 # Request service only; not a source timeline.
		player.add_animation(name,animation)
	target.add_child(player)
func make_scene():
	scene=load("res://scene.gd").new()
	scene.name="Catacombs"
	global.currentScene=scene
	var music=Node.new()
	music.name="Music"
	scene.add_child(music)
	var area=load("res://music.gd").new()
	area.name="MusicArea"
	music.add_child(area)
	switches.clear()
	objects.clear()
	# Keep the actual source child order. Registration in _enter_tree runs
	# before controlled objects' _ready, including later siblings.
	for row in config.nodes:
		var value
		var target
		if row.kind=="lever" or row.kind=="plate":
			value=config.switches[row.path]
			target=load("res://"+config.paths.switch_script).new()
			target.name=row.path.get_file()
			target.position=Vector2(value.x,value.y)
			target.flag=value.flag if not (int(value.bits)&1) else ""
			target.is_object_flag=bool(int(value.bits)&1)
			target.emit_flag_updated_signal=bool(int(value.bits)&2)
			target.reset_when_leaving_area=bool(int(value.bits)&4)
			target.reset_when_leaving_region=bool(int(value.bits)&8)
			target._is_one_way=bool(int(value.bits)&16)
			target._pause_when_changed=bool(int(value.bits)&32)
			target._focus_objects_when_changed=bool(int(value.bits)&64)
			target._camera_delay=value.camera
			target._controlled_objects=[]
			for reference in config.references[row.path]:target._controlled_objects.append(NodePath(reference))
			animator(target,["TurnOn","TurnOff"])
			var audio=AudioStreamPlayer.new()
			audio.name="AudioStreamPlayer"
			var sample=AudioStreamSample.new()
			sample.mix_rate=8000
			var pcm=PoolByteArray()
			pcm.resize(80000) # Explicit ten-second service occupancy, no audio equivalence.
			sample.data=pcm
			audio.stream=sample
			target.add_child(audio)
			switches[row.path]=target
		else:
			value=config.objects[row.path]
			target=load("res://"+config.paths["gate_script" if row.kind=="gate" else "wall_script"]).new()
			target.name=row.path.get_file()
			target.position=Vector2(value.x,value.y)
			target._initially_on=bool(int(value.bits)&1)
			target._move_cam_when_changed=bool(int(value.bits)&2)
			target._move_cam_offset=Vector2(value.offset_x,value.offset_y)
			target._update_order=int(value.order)
			animator(target,["On","Off","StayOn","StayOff"] if row.kind=="gate" else ["On","StayOn","StayOff"])
			objects[row.path]=target
		parent_for(row.path).add_child(target)
	switches[config.main].connect("switch_hit",scene,"_on_main_switch_hit")
	objects[config.wall].connect("state_changed",scene,"_on_wall_door_state_changed")
	add_child(scene)
func complete_animations():
	for target in objects.values():target.get_node("AnimationPlayer").advance(0.02)
	for target in switches.values():target.get_node("AnimationPlayer").advance(0.02)
func capture(action):
	var switch_rows=[]
	var object_rows=[]
	for row in config.data.switches:
		var target=switches[row.path]
		switch_rows.append({"path":row.path,"on":target.is_on(),"disabled":target._is_disabled,"ready":target._is_ready_to_interact,"animation":target.get_node("AnimationPlayer").assigned_animation,"audio":target.get_node("AudioStreamPlayer").playing})
	for row in config.data.objects:
		var target=objects[row.path]
		object_rows.append({"path":row.path,"on":target._get_state(),"animation":target.get_node("AnimationPlayer").assigned_animation,"order":target.get_update_order(),"registrations":target._switches.size()})
	rows.append({"action":action,"switches":switch_rows,"objects":object_rows,"paused":global.player.paused,"cutscene":uiManager.cutscene,"player_events":global.player.events.duplicate(),"ui_events":uiManager.events.duplicate(),"audio_events":audioManager.events.duplicate(true),"camera_events":global.currentCamera.events.duplicate(true)})
	global.player.events.clear()
	uiManager.events.clear()
	audioManager.events.clear()
	global.currentCamera.events.clear()
func run():
	read_config()
	globaldata._init_flags()
	make_scene()
	capture("ready_timer_pending")
	switches["Bottom2/switch1"].interact()
	capture("early_interact_ignored")
	complete_animations()
	yield(get_tree().create_timer(0.25),"timeout")
	capture("ready_timer_complete")
	global.player.paused=true
	switches["Bottom2/switch1"].interact()
	capture("paused_interact_ignored")
	global.player.paused=false
	scene.can_switch=false
	switches["Bottom2/switch1"].interact()
	capture("scene_switch_denied")
	scene.can_switch=true
	switches["Bottom2/switch1"].interact()
	capture("four_gates_requested")
	complete_animations()
	capture("four_gates_complete")
	var other=Node.new()
	switches["Bottom2/switch1"]._on_hit(other)
	complete_animations()
	capture("hit_toggles_back")
	switches["Bottom2/Plate1"]._on_player_entered(other)
	other.free()
	capture("nonplayer_plate_ignored")
	switches["Bottom2/Plate1"]._on_player_entered(global.player)
	capture("plate_turns_on_once")
	switches["Bottom2/Plate1"]._on_player_entered(global.player)
	capture("plate_repeat_ignored")
	complete_animations()
	capture("plate_gate_complete")
	switches[config.main].interact()
	capture("main_switch_camera_pending")
	complete_animations()
	capture("wall_animation_complete_camera_pending")
	yield(get_tree().create_timer(0.55),"timeout")
	capture("wall_callback_delay_complete")
	global.currentCamera.emit_signal("release")
	yield(get_tree(),"idle_frame")
	yield(get_tree(),"idle_frame")
	capture("camera_return_pending")
	global.currentCamera.emit_signal("release")
	capture("main_control_complete")
	switches[config.main].interact()
	capture("main_repeat_ignored")
	switches["Bottom2/switch1"].interact()
	complete_animations()
	capture("reset_switch_on_before_leave")
	scene.emit_signal("area_left",false)
	capture("area_reset_flag_only")
	scene.queue_free()
	yield(get_tree(),"idle_frame")
	yield(get_tree(),"idle_frame")
	make_scene()
	capture("saved_flags_ready_timer_pending")
	complete_animations()
	yield(get_tree().create_timer(0.25),"timeout")
	capture("saved_flags_ready_timer_complete")
	scene.queue_free()
	yield(get_tree(),"idle_frame")
	yield(get_tree(),"idle_frame")
	var file=File.new()
	assert(file.open("res://observations.json",File.WRITE)==OK)
	file.store_string(JSON.print({"engine":Engine.get_version_info(),"rows":rows},"  "))
	file.close()
	get_tree().quit()
