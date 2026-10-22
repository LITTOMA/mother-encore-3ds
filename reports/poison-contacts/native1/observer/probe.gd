extends Node
var retained=[]
func _ready():call_deferred("run")
func bits(v):
    var b=StreamPeerBuffer.new()
    b.put_float(v)
    b.seek(0)
    return b.get_u32()
func color(v):return [bits(v.r),bits(v.g),bits(v.b),bits(v.a)]
func stat_changed(stat,value,max_value):global.events.append(["stat",stat,value,max_value])
func status_changed():global.events.append(["status"])
func capture(actor,label):
    for s in actor._party_member.get_status_ailments():
        if !retained.has(s):retained.append(s)
    var p=actor._flash_anim
    var row={"label":label,"hp":actor._party_member.get_hp(),"status":actor._party_member.status_to_array(),"current":p.current_animation,"assigned":p.assigned_animation,"playing":p.is_playing(),"time":bits(p.current_animation_position),"invulnerable":actor._is_invulnerable(),"main_alpha":bits(actor.get_node("Position/main").modulate.a),"special_alpha":bits(actor.get_node("SpecialAnimations").modulate.a),"main_modifier":bits(actor.get_node("Position/main").material.get_shader_param("flash_modifier")),"main_color":color(actor.get_node("Position/main").material.get_shader_param("flash_color")),"events":global.events.duplicate(true),"properties":[[bits(actor.get_node("Position/main").material.get_shader_param("flash_modifier")),0,0,0],color(actor.get_node("Position/main").material.get_shader_param("flash_color")),[bits(actor.get_node("SpecialAnimations").material.get_shader_param("flash_modifier")),0,0,0],color(actor.get_node("SpecialAnimations").material.get_shader_param("flash_color")),color(actor.get_node("Position/main").modulate),color(actor.get_node("SpecialAnimations").modulate),[bits(float(actor.get_node("Position/main").visible)),0,0,0],color(actor.get_node("Position/main").material.get_shader_param("glow_color")),[bits(actor.get_node("Position/main").material.get_shader_param("glow_modifier")),0,0,0]]}
    global.events.clear()
    return row
func create_actor(hp,maxhp):
    var member=load("res://member.gd").new()
    member.init_from_dict({"name":"ninten","level":1,"exp":0,"hp":hp,"maxhp":maxhp,"pp":0,"maxpp":0,"offense":1,"defense":1,"speed":1,"iq":1,"guts":1})
    var actor=load("res://actor.tscn").instance()
    actor._party_member=member
    actor.camera=load("res://camera.gd").new()
    actor.add_child(actor.camera)
    add_child(actor)
    member.connect("stat_changed",self,"stat_changed")
    member.connect("status_changed",self,"status_changed")
    global.party=[member]
    global.player=actor
    actor._flash_anim.play("RESET")
    actor._flash_anim.advance(0)
    actor._flash_anim.advance(0.01)
    global.events.clear()
    return actor
func retire(actor):
    actor._party_member.free()
    actor.free()
    for s in retained:
        if is_instance_valid(s):s.free()
    retained.clear()
var config
var areas=[]
var rows=[]
var contacts=[]
var selected
func notice(body,index,entered):contacts.append([index,entered,body==selected])
func settle():
    for _i in 4:yield(get_tree(),"physics_frame")
    yield(get_tree(),"idle_frame")
func describe(actor,body,ci,phase):
    var row=capture(actor,str(ci)+":"+phase)
    row.case=ci
    row.phase=phase
    row.position=[bits(body.position.x),bits(body.position.y)]
    row.layer=body.collision_layer
    row.mask=body.collision_mask
    row.enabled=!body.get_node("CollisionShape2D").disabled
    row.paused=actor._paused
    row.continuous=actor._is_continuous_damage
    row.damage=actor._attack_damage
    row.variance=actor._damage_variance
    row.run_sound=actor.run_sound
    row.shadow=actor.shadow_mode
    row.contacts=contacts.duplicate(true)
    row.overlaps=[]
    for area in areas:row.overlaps.append(area.overlaps_body(body))
    contacts.clear()
    rows.append(row)
func shape(body):
    var collider=CollisionPolygon2D.new()
    collider.name="CollisionShape2D"
    collider.position=Vector2(config.player.position[0],config.player.position[1])
    var points=PoolVector2Array()
    for point in config.player.points:points.append(Vector2(point[0],point[1]))
    collider.polygon=points
    body.add_child(collider)
    body.collision_layer=config.player.layer
    body.collision_mask=config.player.mask
    body.position=Vector2(20000,20000)
func geometry():
    var result=[]
    for i in areas.size():
        var shape=areas[i].get_node("CollisionPolygon2D")
        var parts=[]
        for owner in areas[i].get_shape_owners():
            if areas[i].shape_owner_get_owner(owner)!=shape:continue
            for j in areas[i].shape_owner_get_shape_count(owner):
                var polygon=areas[i].shape_owner_get_shape(owner,j)
                assert(polygon is ConvexPolygonShape2D)
                var points=[]
                for point in polygon.points:
                    var p=shape.global_transform.xform(point)
                    points.append([p.x,p.y])
                parts.append(points)
        result.append({"parts":parts,"layer":areas[i].collision_layer,"mask":areas[i].collision_mask,"monitoring":areas[i].monitoring,"monitorable":areas[i].monitorable})
    return result
func run():
    global.currentScene=self
    var file=File.new()
    assert(file.open("res://input.json",File.READ)==OK)
    config=JSON.parse(file.get_as_text()).result
    file.close()
    for binding in config.areas:
        var area=load("res://Nodes/Reusables/Overlap/Poison.tscn").instance()
        area.collision_layer=int(binding.layer)
        area.collision_mask=int(binding.mask)
        area.monitoring=bool(int(binding.bits)&1)
        area.monitorable=bool(int(binding.bits)&2)
        area.stepping_sound=NodePath("Step")
        var step=load("res://step.gd").new()
        step.name="Step"
        for key in ["entering_sound","exiting_sound","enter_shadow_effect","exit_shadow_effect","enabled"]:
            step.set(key,config.step[key])
        area.add_child(step)
        for role in 2:
            var description=config.shapes[int(binding.first)+role]
            var polygon=area.get_node("CollisionPolygon2D" if role==0 else "StaticBody2D/CollisionPolygon2D")
            polygon.position=Vector2(description.position[0],description.position[1])
            var points=PoolVector2Array()
            for index in range(int(description.first),int(description.first+description.count)):points.append(Vector2(config.points[index][0],config.points[index][1]))
            polygon.polygon=points
        area.connect("body_entered",self,"notice",[areas.size(),true])
        area.connect("body_exited",self,"notice",[areas.size(),false])
        add_child(area)
        areas.append(area)
    yield(settle(),"completed")
    var original_geometry=geometry()
    for ci in config.cases.size():
        var test=config.cases[ci]
        var actor=create_actor(int(config.hp),int(config.maxhp))
        actor.run_sound=config.initial_run
        actor.shadow_mode=config.initial_shadow
        shape(actor)
        var body=actor
        if test.other:
            body=KinematicBody2D.new()
            add_child(body)
            shape(body)
        selected=actor
        contacts.clear()
        seed(int(config.seed))
        yield(settle(),"completed")
        describe(actor,body,ci,"initial")
        body.collision_layer=int(test.layer)
        body.collision_mask=int(test.mask)
        body.get_node("CollisionShape2D").disabled=!test.enabled
        actor._paused=test.paused
        body.position=Vector2(test.point[0]-config.player.center[0],test.point[1]-config.player.center[1])
        yield(settle(),"completed")
        describe(actor,body,ci,"enter")
        actor._paused=false
        body.get_node("CollisionShape2D").disabled=false
        yield(settle(),"completed")
        describe(actor,body,ci,"stay")
        actor.physics_tail()
        if actor._flash_anim.is_playing():actor._flash_anim.advance(1.0/60.0)
        describe(actor,body,ci,"hit")
        body.position=Vector2(20000,20000)
        yield(settle(),"completed")
        describe(actor,body,ci,"leave")
        body.collision_layer=config.player.layer
        body.collision_mask=config.player.mask
        body.position=Vector2(test.point[0]-config.player.center[0],test.point[1]-config.player.center[1])
        yield(settle(),"completed")
        describe(actor,body,ci,"reenter")
        body.position=Vector2(20000,20000)
        yield(settle(),"completed")
        describe(actor,body,ci,"exit")
        rows[-1].rng_next=randi()
        if body!=actor:body.free()
        retire(actor)
        selected=null
    assert(file.open("res://../reference.json",File.WRITE)==OK)
    file.store_string(JSON.print({"schema":1,"godot":Engine.get_version_info(),"geometry":original_geometry,"rows":rows}))
    file.close()
    get_tree().quit()
