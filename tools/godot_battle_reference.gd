extends SceneTree
class Driver:
	extends Node
	var runner
	func _process(delta): runner.tick(delta)
var driver
var battle
var cursor
var player_sprite
var plate
var anim
var action_anim
var frame=-3
var events=[]
var snapshots=[]
var output=""
var result
var menu_active=false
func read_json(path):
	var f=File.new()
	assert(f.open(path,File.READ)==OK)
	var p=JSON.parse(f.get_as_text());f.close();assert(p.error==OK)
	return p.result
func _init():
	for arg in OS.get_cmdline_args():
		if arg.begins_with("--encore-out="):output=arg.trim_prefix("--encore-out=")
	assert(output!="")
	result=read_json("res://metadata.json")
	result.godot=Engine.get_version_info()
	result.observer_phase="Driver._process before child AnimationPlayer/process/tween/timer work; frame0 starts play; captures state at beginning of each subsequent fixed60Hz idle"
	driver=Driver.new();driver.runner=self
	get_root().add_child(driver)
	driver.set_process(false)
	call_deferred("setup")
func add_node(parent,node,name):
	node.name=name;parent.add_child(node);return node
func texture(w,h):
	var im=Image.new();im.create(w,h,false,Image.FORMAT_RGBA8)
	var tex=ImageTexture.new();tex.create_from_image(im,0);return tex
func setup():
	battle=load("res://battle.gd").new();battle.runner=self;add_node(get_root(),battle,"Battle")
	add_node(battle,Control.new(),"top");add_node(battle,Control.new(),"bottom")
	var info=add_node(battle,Control.new(),"PlayerInfo")
	var party_info=add_node(info,Control.new(),"PartyInfo")
	battle._party_info=party_info
	plate=load("res://plate.gd").new();plate.runner=self;plate.rect_position=Vector2(128,20);plate.rect_size=Vector2(65,49);add_node(party_info,plate,"Plate")
	var players=add_node(battle,Control.new(),"PlayerTransitions")
	player_sprite=Sprite.new();player_sprite.texture=texture(310,580);player_sprite.hframes=10;player_sprite.vframes=20
	player_sprite.position=Vector2(126,94);add_node(players,player_sprite,"Player")
	add_node(battle,Control.new(),"NpcTransitions")
	var enemies=add_node(battle,Node2D.new(),"Enemies")
	var enemy=load("res://enemy.gd").new();enemy.runner=self;enemy.rect_size=Vector2(33,49);enemy.rect_position=Vector2(143.5,49);enemy.hide();add_node(enemies,enemy,"Lamp")
	var enemy_transitions=add_node(battle,Control.new(),"EnemyTransitions")
	var lamp=Sprite.new();lamp.position=Vector2(142,78);add_node(enemy_transitions,lamp,"Lamp")
	var menu=add_node(battle,Control.new(),"ActionMenuBox")
	add_node(menu,Node2D.new(),"Arrow")
	var icons=add_node(menu,Control.new(),"ActionIcons")
	for name in ["BashIcon","SkillsIcon","PSIIcon","ItemsIcon","DefendIcon","RunIcon"]:add_node(icons,Control.new(),name)
	add_node(battle,Control.new(),"TargetNameBox")
	anim=add_node(battle,AnimationPlayer.new(),"AnimScene")
	action_anim=add_node(battle,AnimationPlayer.new(),"AnimAction")
	var clips=read_json("res://clips.json")
	for c in clips:
		if c.name=="scene.transitionIn":anim.add_animation("transitionIn",make_animation(c))
		elif c.name=="actions.transitionIn":action_anim.add_animation("transitionIn",make_animation(c))
	anim.connect("animation_finished",battle,"_battle_start",[],CONNECT_ONESHOT)
	cursor=load("res://cursor.gd").new();cursor.runner=self;add_node(battle,cursor,"CursorProbe")
	var cm=add_node(battle,HBoxContainer.new(),"CursorMenu")
	cm.rect_position=Vector2(9,1);cm.add_constant_override("separation",16)
	for i in 3:
		var icon=TextureRect.new();icon.texture=texture(16,16);cm.add_child(icon)
	cursor.menu_parent=cm
	var timer=add_node(cursor,Timer.new(),"Timer");timer.wait_time=0.05;timer.one_shot=true
	driver.set_process(true)
func make_animation(clip):
	var a=Animation.new();a.length=clip.length;a.loop=clip.loop;a.step=clip.step
	for t in clip.tracks:
		var idx=a.add_track(Animation.TYPE_VALUE if t.type=="value" else Animation.TYPE_METHOD)
		a.track_set_path(idx,NodePath(t.path));a.track_set_interpolation_type(idx,int(t.interp));a.track_set_interpolation_loop_wrap(idx,t.loop_wrap)
		if t.type=="value":a.value_track_set_update_mode(idx,int(t.keys.update))
		for i in t.keys.times.size():
			var v=t.keys["values"][i]
			if v is Array:
				if v.size()==2:v=Vector2(v[0],v[1])
				elif v.size()==4:v=Color(v[0],v[1],v[2],v[3])
			a.track_insert_key(idx,t.keys.times[i],v,t.keys.transitions[i])
	return a
func event(name):events.append({"event":name,"driver_frame":frame,"nominal_seconds":frame/60.0,"animation_position":anim.current_animation_position if anim!=null and anim.assigned_animation!="" else 0})
func v(vec):return ["%.17f"%vec.x,"%.17f"%vec.y]
func tick(delta):
	assert(abs(delta-1.0/60.0)<0.00000001)
	if frame<0:
		frame+=1;return
	if frame==0:
		event("play_requested")
		anim.play("transitionIn")
		cursor.set_cursor_from_index(0,false)
	if frame==10:
		event("cursor_move_requested")
		cursor.set_cursor_from_index(1,true)
	snapshots.append({"frame":frame,"player_pos":v(player_sprite.position),"player_scale":v(player_sprite.scale),"player_visible":player_sprite.visible,"plate_pos":v(plate.rect_position),"cursor_pos":v(cursor.position),"cursor_scale":v(cursor.scale),"scene_position":"%.17f"%anim.current_animation_position,"menu_active":menu_active})
	frame+=1
	if frame==150:
		driver.set_process(false)
		call_deferred("finish")
func finish():
	result.events=events;result.frames=snapshots;result.menu_active=menu_active
	var f=File.new();assert(f.open(output,File.WRITE)==OK);f.store_string(JSON.print(result,"  ")+"\n");f.close()
	print("BATTLE_ENTRY_REFERENCE_COMPLETE ",snapshots.size())
	quit()
