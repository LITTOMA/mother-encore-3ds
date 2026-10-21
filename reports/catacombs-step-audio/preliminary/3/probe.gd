extends SceneTree
var rows=[]
func _init():call_deferred("run")
func capture(index,phase,actor,manager,original):
    var node=manager.get_sfx("run")
    var stream=node.stream
    rows.append({"index":index,"phase":phase,"running":actor._running,"playing":node.playing,"same_node":node==original,"children":manager.get_node("Sfx").get_child_count(),"name":node.name,"source":stream.resource_path,"bus":node.bus,"gain_db":node.volume_db,"pitch":node.pitch_scale,"autoplay":node.autoplay,"stream_paused":node.stream_paused,"loop":stream.loop,"loop_offset":stream.loop_offset,"length":stream.get_length()})
func settle(node,expected):
    # AudioServer mixes independently of idle frames. Observe a bounded stable
    # result instead of assuming two arbitrarily short idle frames drain stop.
    var stable=0
    for _i in 200:
        yield(create_timer(0.01),"timeout")
        stable=stable+1 if node.playing==expected else 0
        if stable==3:return
    assert(false)
func run():
    var file=File.new()
    assert(file.open("res://input.json",File.READ)==OK)
    var config=JSON.parse(file.get_as_text()).result
    file.close()
    var manager=load("res://manager.gd").new()
    var container=Node.new()
    container.name="Sfx"
    manager.add_child(container)
    get_root().add_child(manager)
    var actor=load("res://actor.gd").new()
    actor.audioManager=manager
    get_root().add_child(actor)
    var original
    for i in config.sounds.size():
        var stream=load(config.sounds[i].source)
        assert(stream is AudioStreamMP3)
        actor.run_sound=config.sounds[i].name
        actor._running=false
        var node=manager.add_sfx(stream,"run")
        if original==null:original=node
        yield(settle(node,false),"completed")
        capture(i,"initial",actor,manager,original)
        actor._set_running(true)
        yield(settle(node,true),"completed")
        capture(i,"play",actor,manager,original)
        actor._set_running(true)
        yield(settle(node,true),"completed")
        capture(i,"repeat",actor,manager,original)
        actor._set_running(false)
        yield(settle(node,false),"completed")
        capture(i,"stop",actor,manager,original)
    var buses=[]
    for i in AudioServer.bus_count:buses.append({"name":AudioServer.get_bus_name(i),"gain_db":AudioServer.get_bus_volume_db(i),"mute":AudioServer.is_bus_mute(i)})
    assert(file.open("res://../reference.json",File.WRITE)==OK)
    file.store_string(JSON.print({"schema":1,"godot":Engine.get_version_info(),"rows":rows,"buses":buses},"  "))
    file.close()
    actor.queue_free()
    manager.queue_free()
    yield(self,"idle_frame")
    yield(self,"idle_frame")
    quit()
