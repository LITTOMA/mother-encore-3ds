extends SceneTree
func _init():call_deferred("run")
func shadow(body):
    var s=body.get_node("Shadow")
    return {"mode":body.get_shadow(),"frame":s.frame,"behind":s.show_behind_parent,"playing":s.playing,"start":s.start_anim}
func run():
    var file=File.new()
    assert(file.open("res://input.json",File.READ)==OK)
    var config=JSON.parse(file.get_as_text()).result
    file.close()
    var actor=load("res://actor.gd").new()
    var other=load("res://party.gd").new()
    var outsider=Node2D.new()
    for body in [actor,other]:
        body.add_child(load("res://"+config.shadow_scene).instance())
        get_root().add_child(body)
        body.set_shadow(config.shadow)
    var world=load("res://world.gd").new()
    world.player=actor
    actor.global=world
    var manager=load("res://manager.gd").new()
    actor.audioManager=manager
    actor.run_sound=config.default
    manager.names=config.names
    var sounds=[]
    for path in config.sounds:
        var stream=load(path)
        assert(stream is AudioStreamMP3 and stream.loop and stream.loop_offset==0)
        sounds.append(stream)
    manager.current.stream=sounds[config.names.find(config.default)]
    var areas=[]
    for row in config.areas:
        var area=load("res://step.gd").new()
        area.global=world
        area.entering_sound=row.enter
        area.exiting_sound=row.exit
        area.enter_shadow_effect=row.enter_shadow
        area.exit_shadow_effect=row.exit_shadow
        area.enabled=row.enabled
        areas.append(area)
    var animations=[]
    var sprite=actor.get_node("Shadow")
    for name in sprite.frames.get_animation_names():
        var frames=[]
        for i in sprite.frames.get_frame_count(name):
            var t=sprite.frames.get_frame(name,i)
            assert(t is AtlasTexture)
            assert(not(t.atlas.flags&Texture.FLAG_FILTER) and not(t.atlas.flags&Texture.FLAG_MIPMAPS) and not(t.atlas.flags&Texture.FLAG_REPEAT) and not(t.atlas.flags&Texture.FLAG_ANISOTROPIC_FILTER))
            frames.append({"atlas":t.atlas.resource_path,"region":[t.region.position.x,t.region.position.y,t.region.size.x,t.region.size.y]})
        animations.append({"name":name,"speed":sprite.frames.get_animation_speed(name),"loop":sprite.frames.get_animation_loop(name),"frames":frames})
    var rows=[]
    for action in config.cases:
        manager.events=[]
        match int(action[0]):
            0:
                actor._paused=bool(action[2])
                actor._state=actor.MOVE if bool(action[3]) else 1
                actor._climbing=bool(action[4])
                actor._set_running(bool(action[1]))
            1:
                var body=outsider if int(action[3])==2 else(actor if int(action[3])==1 else other)
                if bool(action[2]):areas[int(action[1])]._on_Stepping_Sounds_body_entered(body)
                else:areas[int(action[1])]._on_Stepping_Sounds_body_exited(body)
            2:
                if bool(action[2]):areas[int(action[1])].enable()
                else:areas[int(action[1])].disable()
            3,4:
                actor._paused=int(action[0])==3
                world.entering_door=int(action[0])==4
                actor._move_state(1.0/60)
                world.entering_door=false
            5:
                var seeded_body=actor if int(action[1])==1 else other
                seeded_body.set_shadow(action[2])
                seeded_body.get_node("Shadow").frame=int(action[3])
            _:assert(false)
        rows.append({"action":action,"wanted":config.names.find(actor.run_sound),"registered":manager.names.find(manager.current.stream.resource_path.get_file().get_basename()),"playing":manager.current.playing,"running":actor._running,"events":manager.events.duplicate(true),"player":shadow(actor),"party":shadow(other)})
    assert(file.open("res://../reference.json",File.WRITE)==OK)
    file.store_string(JSON.print({"schema":1,"godot":Engine.get_version_info(),"animations":animations,"rows":rows},"  "))
    file.close()
    for area in areas:area.free()
    actor.free()
    other.free()
    outsider.free()
    world.free()
    manager.free()
    quit()
