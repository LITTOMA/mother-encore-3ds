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
    var row={"label":label,"hp":actor._party_member.get_hp(),"status":actor._party_member.status_to_array(),"current":p.current_animation,"assigned":p.assigned_animation,"playing":p.is_playing(),"time":bits(p.current_animation_position),"invulnerable":actor._is_invulnerable(),"main_alpha":bits(actor.get_node("Position/main").modulate.a),"special_alpha":bits(actor.get_node("SpecialAnimations").modulate.a),"main_modifier":bits(actor.get_node("Position/main").material.get_shader_param("flash_modifier")),"main_color":color(actor.get_node("Position/main").material.get_shader_param("flash_color")),"events":global.events.duplicate(true)}
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
    assert(file.open("res://cases.json",File.READ)==OK)
    var cases=JSON.parse(file.get_as_text()).result
    file.close()
    var rows=[]
    for index in cases.size():
        var c=cases[index]
        var actor=create_actor(int(c.hp),int(c.maxhp))
        if c.has("status"):actor._party_member.add_status(c.status)
        capture(actor,"initial")
        seed(1)
        actor.damage(int(c.damage),int(c.variance),Vector2.ZERO,!c.kill)
        var row=capture(actor,c.name)
        row.rng_next=randi()
        rows.append(row)
        retire(actor)
    for scenario in ["complete","pause_zero","pause_positive","restart"]:
        var actor=create_actor(100,100)
        actor.play_flash_anim("Flash")
        rows.append(capture(actor,scenario+":play"))
        if scenario=="pause_zero":
            actor.pause_flash_anim()
            rows.append(capture(actor,scenario+":pause"))
            actor.resume_flash_anim()
            rows.append(capture(actor,scenario+":resume"))
        for index in 55:
            actor._flash_anim.advance(1.0/60)
            rows.append(capture(actor,scenario+":"+str(index)))
            if scenario=="pause_positive" and index==5:
                actor.pause_flash_anim()
                rows.append(capture(actor,scenario+":pause"))
                actor._flash_anim.advance(0.1)
                rows.append(capture(actor,scenario+":manual_paused"))
                actor.resume_flash_anim()
                rows.append(capture(actor,scenario+":resume"))
            if scenario=="restart" and index==20:
                actor.play_flash_anim("Flash")
                rows.append(capture(actor,scenario+":replay"))
        retire(actor)
    assert(file.open("res://../reference.json",File.WRITE)==OK)
    file.store_string(JSON.print({"schema":1,"godot":Engine.get_version_info(),"rows":rows},"  "))
    file.close()
    get_tree().quit()
