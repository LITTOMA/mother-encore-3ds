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
func run():
    global.currentScene=self
    var file=File.new()
    assert(file.open("res://input.json",File.READ)==OK)
    var config=JSON.parse(file.get_as_text()).result
    file.close()
    var actor=create_actor(int(config.hp),int(config.maxhp))
    actor.run_sound=config.initial_run
    actor.shadow_mode=config.initial_shadow
    var outsider=Node2D.new()
    add_child(outsider)
    var areas=[]
    for binding in config.areas:
        var area=load("res://Nodes/Reusables/Overlap/Poison.tscn").instance()
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
        add_child(area)
        areas.append(area)
    var geyser=load("res://geyser.gd").new()
    add_child(geyser)
    seed(int(config.seed))
    var rows=[]
    for index in config.actions.size():
        var action=config.actions[index]
        match action[0]:
            "area":
                var body=actor if action[3]=="player" else outsider
                var area=areas[int(action[1])]
                if action[2]:area._on_Area2D_body_entered(body)
                else:area._on_Poison_body_exited(body)
            "geyser":
                var body=actor if action[2]=="player" else outsider
                if action[1]:geyser._on_Poison_Geyser_body_entered(body)
                else:geyser._on_Poison_Geyser_body_exited(body)
            "pause":
                actor._paused=bool(action[1])
                if action[1]:actor.pause_flash_anim()
                else:actor.resume_flash_anim()
            "tick":
                actor.physics_tail()
                if actor._flash_anim.is_playing():actor._flash_anim.advance(float(action[1]))
            _:assert(false)
        var row=capture(actor,str(index))
        row.action=action.duplicate(true)
        if action[0]=="tick":row.action[1]=bits(action[1])
        row.continuous=actor._is_continuous_damage
        row.damage=actor._attack_damage
        row.variance=actor._damage_variance
        row.paused=actor._paused
        row.run_sound=actor.run_sound
        row.shadow=actor.shadow_mode
        rows.append(row)
    rows[-1].rng_next=randi()
    assert(file.open("res://../reference.json",File.WRITE)==OK)
    file.store_string(JSON.print({"schema":1,"godot":Engine.get_version_info(),"rows":rows},"  "))
    file.close()
    retire(actor)
    get_tree().quit()
