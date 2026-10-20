extends Node
var config
var target
var root_anim
var child_anim
var rows=[]
var completions=[]
var all_completed=[]
func _ready():call_deferred("run")
func profile_by_id(id):
	for value in config.profiles:
		if value.id==id:return value
	assert(false)
func completed(name,profile):
	var event={"profile":profile,"name":name}
	completions.append(event)
	all_completed.append(event)
func service(parent,name,script):
	var node=load(script).new()
	node.name=name
	parent.add_child(node)
	return node
func animate(parent,profile):
	var node=AnimationPlayer.new()
	node.name="AnimationPlayer"
	node.playback_process_mode=AnimationPlayer.ANIMATION_PROCESS_MANUAL
	for clip in profile.clips:
		node.add_animation(clip.name,load("res://clips/"+str(clip.id)+".tres"))
	parent.add_child(node)
	for connection in profile.connections:
		if connection.from=="AnimationPlayer":assert(node.connect(connection.signal,parent,connection.method)==OK)
	return node
func make(profile):
	var node=Sprite.new()
	node.name=profile.node_name
	node.set_script(load("res://"+profile.script))
	if profile.kind!=3:
		node.is_object_flag=true
		node.emit_flag_updated_signal=true
	var sprite=Sprite.new()
	sprite.name="Sprite"
	sprite.hframes=profile.frames
	sprite.frame=profile.frame
	sprite.position=Vector2(profile.position[0],profile.position[1])
	sprite.visible=bool(int(profile.mask)&1)
	node.add_child(sprite)
	if profile.kind==1:
		var area=Node.new()
		area.name="Area2D"
		node.add_child(area)
		service(area,"CollisionShape2D","res://state.gd")
	elif profile.kind==2:
		var body=Node.new()
		body.name="StaticBody2D"
		node.add_child(body)
		service(body,"CollisionShape2D","res://state.gd")
		var interact=Node.new()
		interact.name="interact"
		node.add_child(interact)
		service(interact,"ButtonPrompt","res://state.gd")
		var child=make(profile_by_id(profile.child))
		child.name="Lock"
		node.add_child(child)
	else:
		node.anim_name="Open"
		node.anim_player=NodePath("AnimationPlayer")
	if profile.kind!=2:
		var audio=service(node,"AudioStreamPlayer","res://audio.gd")
		if profile.kind==1:assert(audio.connect("finished",node,"_on_AudioStreamPlayer_finished")==OK)
	animate(node,profile)
	return node
func pose(node,profile):
	if not is_instance_valid(node):return {"alive":false}
	var sprite=node.get_node("Sprite")
	var anim=node.get_node("AnimationPlayer")
	var p={"alive":true,"queued":node.is_queued_for_deletion(),"frame":sprite.frame,"visible":sprite.visible,"root_visible":node.visible,"position":["%.17f"%sprite.position.x,"%.17f"%sprite.position.y],"playing":anim.is_playing(),"assigned":anim.assigned_animation,"time":"%.17f"%anim.current_animation_position if anim.assigned_animation!="" else "0"}
	if profile.kind!=2:
		var audio=node.get_node("AudioStreamPlayer")
		p.audio=audio.playing
		p.audio_writes=audio.writes.duplicate()
		audio.writes.clear()
	if profile.kind==1:p.disabled=node.get_node("Area2D/CollisionShape2D").disabled
	elif profile.kind==2:
		p.disabled=node.get_node("StaticBody2D/CollisionShape2D").disabled
		p.prompt=node.get_node("interact/ButtonPrompt").enabled
		p.opened=node.opened
		p.child=pose(node.get_node("Lock"),profile_by_id(profile.child))
	return p
func capture(case_index,op_index,op,profile):
	rows.append({"case":case_index,"index":op_index,"op":op,"pose":pose(target,profile),"completed":completions.duplicate(true),"counter":uiManager.get_key_count(),"flags":globaldata.object_flags.duplicate(true),"ui":uiManager.events.duplicate(true)})
	completions.clear()
	uiManager.events.clear()
func run():
	var file=File.new()
	assert(file.open("res://input.json",File.READ)==OK)
	config=JSON.parse(file.get_as_text()).result
	file.close()
	for ci in config.cases.size():
		var test=config.cases[ci]
		var profile=profile_by_id(test.profile)
		globaldata.object_flags={}
		if test.get("saved",false):globaldata.object_flags[test.flag]=true
		globaldata.keys["Catacombs"]=1
		target=make(profile)
		global.currentScene.add_child(target)
		root_anim=target.get_node("AnimationPlayer")
		root_anim.connect("animation_finished",self,"completed",[profile.id])
		child_anim=target.get_node("Lock/AnimationPlayer") if profile.kind==2 else null
		if child_anim!=null:child_anim.connect("animation_finished",self,"completed",[profile.child])
		for i in test.actions.size():
			var action=test.actions[i]
			if action.op=="play":root_anim.play(action.name)
			elif action.op=="interact":target.interact()
			elif action.op=="pickup":target._on_Area2D_body_entered(global.player)
			elif action.op=="finish_audio":target.get_node("AudioStreamPlayer").finish()
			elif action.op=="noop":pass
			elif action.op=="advance":
				if child_anim!=null:child_anim.advance(action.delta)
				root_anim.advance(action.delta)
			elif action.op=="flush":
				yield(get_tree(),"idle_frame")
				yield(get_tree(),"idle_frame")
			else:assert(false)
			capture(ci,i,action.op,profile)
		if is_instance_valid(target):target.queue_free()
		yield(get_tree(),"idle_frame")
		yield(get_tree(),"idle_frame")
	assert(file.open("res://../observations.json",File.WRITE)==OK)
	file.store_string(JSON.print({"engine":Engine.get_version_info(),"rows":rows,"completions":all_completed},"  "))
	file.close()
	print("KEY_CLOCK_SOURCE_REFERENCE_COMPLETE")
	get_tree().quit(17)
