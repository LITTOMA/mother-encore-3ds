extends Node
func vec(v):return ["%.17f"%v.x,"%.17f"%v.y]
func rect(v):return {"position":vec(v.position),"size":vec(v.size)}
func _ready():call_deferred("run")
func run():
	var file=File.new()
	assert(file.open("res://input.json",File.READ)==OK)
	var input=JSON.parse(file.get_as_text()).result
	file.close()
	var scene=Node2D.new()
	scene.name="SourceGeometry"
	scene.position=Vector2(input.root_position[0],input.root_position[1])
	var parents={}
	for row in input.parents:
		var parent=TileMap.new()
		parent.name=row.path
		parent.position=Vector2(row.position[0],row.position[1])
		parent.cell_size=Vector2(row.cell_size[0],row.cell_size[1])
		parent.cell_y_sort=row.sort
		parent.visible=row.visible
		parent.z_index=row.z
		parent.z_as_relative=row.z_relative
		scene.add_child(parent)
		parents[row.path]=parent
	var targets=[]
	for row in input.owners:
		var target=load("res://"+row.scene).instance()
		target.name=row.name
		target.position=Vector2(row.position[0],row.position[1])
		parents[row.parent].add_child(target)
		target.get_node("AnimationPlayer").playback_process_mode=AnimationPlayer.ANIMATION_PROCESS_MANUAL
		if row.child:target.get_node("Lock/AnimationPlayer").playback_process_mode=AnimationPlayer.ANIMATION_PROCESS_MANUAL
		targets.append(target)
	add_child(scene)
	var sprites=[]
	var shapes=[]
	var sounds=[]
	for i in targets.size():
		var target=targets[i]
		var owner=input.owners[i]
		for selection in owner.sprites:
			var sprite=target.get_node(selection.path)
			for frame in sprite.hframes*sprite.vframes:
				sprite.frame=frame
				sprites.append({"owner":owner.id,"profile":selection.profile,"path":selection.path,"frame":frame,"local_position":vec(sprite.position),"global_position":vec(sprite.global_position),"rect":rect(sprite.get_rect()),"tree_visible":sprite.is_visible_in_tree(),"centered":sprite.centered,"offset":vec(sprite.offset),"flip_h":sprite.flip_h,"flip_v":sprite.flip_v,"z":sprite.z_index,"z_relative":sprite.z_as_relative,"parent_type":target.get_parent().get_class(),"parent_sort":target.get_parent().cell_y_sort})
		for selection in owner.shapes:
			var node=target.get_node(selection.path)
			assert(node is CollisionShape2D and node.shape is RectangleShape2D)
			var body=node.get_parent()
			var corners=[]
			for point in [Vector2(-1,-1),Vector2(1,-1),Vector2(1,1),Vector2(-1,1)]:corners.append(vec(node.global_transform*(point*node.shape.extents)))
			shapes.append({"owner":owner.id,"path":selection.path,"role":selection.role,"type":body.get_class(),"center":vec(node.global_transform.origin),"extents":vec(node.shape.extents),"scale":vec(node.global_transform.get_scale()),"corners":corners,"layer":body.collision_layer,"mask":body.collision_mask,"disabled":node.disabled,"one_way":node.one_way_collision,"margin":node.one_way_collision_margin,"monitoring":body.monitoring if body is Area2D else false,"monitorable":body.monitorable if body is Area2D else false})
		var audio=target.get_node(owner.sound_path)
		var stream=audio.stream
		sounds.append({"owner":owner.id,"path":owner.sound_path,"bus":audio.bus,"gain":"%.17f"%audio.volume_db,"pitch":"%.17f"%audio.pitch_scale,"pause":audio.pause_mode,"stream_type":stream.get_class(),"length":"%.17f"%stream.get_length()})
	var reflected=load("res://Scripts/Main/Lock.gd").new()
	var turn=reflected.player_turn.duplicate()
	reflected.free()
	assert(file.open("res://../geometry.json",File.WRITE)==OK)
	file.store_string(JSON.print({"engine":Engine.get_version_info(),"sprites":sprites,"shapes":shapes,"sounds":sounds,"player_turn":turn},"  "))
	file.close()
	scene.queue_free()
	yield(get_tree(),"idle_frame")
	yield(get_tree(),"idle_frame")
	print("KEY_MEDIA_GEOMETRY_COMPLETE")
	get_tree().quit()
