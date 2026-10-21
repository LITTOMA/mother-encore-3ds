extends SceneTree
var rows=[]
var meta={}
var finished=[]
func _init():call_deferred("run")
func bits(value):
    var bytes=StreamPeerBuffer.new()
    bytes.put_float(value)
    bytes.seek(0)
    return bytes.get_u32()
func done(name):finished.append(name)
func capture(actor,index,phase):
    var anim=actor.get_node("AnimationPlayer")
    var audio=actor.get_node("AudioStreamPlayer2D")
    var global_sound=audioManager.get_sfx("geyser")
    rows.append({"index":index,"phase":phase,"time":bits(anim.current_animation_position),"frame":actor.get_node("Sprite").frame,"monitoring":actor.monitoring,"disabled":actor.get_node("CollisionShape2D").disabled,"active":anim.playback_active,"paused":audio.stream_paused,"gain":bits(audio.volume_db),"commands":audio.commands.duplicate(true),"finished":finished.duplicate(),"queued":actor.is_queued_for_deletion(),"on_screen":actor.get_node("VisibilityNotifier2D").is_on_screen(),"rumble":global_sound!=null})
    audio.commands.clear()
func run():
    var player=load("res://party.gd").new()
    get_root().add_child(player)
    global.player=player
    for path in ["res://Audio/Sound effects/shreksplash.mp3","res://Audio/Sound effects/shrekblast.mp3","res://Audio/Sound effects/shrekrumble.mp3"]:
        var stream=load(path)
        meta[path]={"length":bits(stream.get_length()),"loop":stream.loop,"offset":bits(stream.loop_offset)}
    var actor=load("res://geyser.tscn").instance()
    actor.position=Vector2(100,100)
    get_root().add_child(actor)
    var anim=actor.get_node("AnimationPlayer")
    assert(anim.connect("animation_finished",self,"done")==OK)
    anim.playback_active=false
    actor.set_process(false)
    yield(self,"idle_frame")
    yield(self,"idle_frame")
    capture(actor,-1,"ready")
    var deltas=[0.0,0.8,0.066667,0.0,0.2,0.000003,0.06666,0.93334,0.4,0.5,0.1]
    for index in deltas.size():
        anim.advance(deltas[index])
        actor._process(0.0)
        capture(actor,index,"before_deferred")
        if actor.is_queued_for_deletion():break
        yield(self,"idle_frame")
        capture(actor,index,"after_deferred")
    var file=File.new()
    assert(file.open("res://../reference.json",File.WRITE)==OK)
    file.store_string(JSON.print({"godot":Engine.get_version_info(),"meta":meta,"rows":rows},"  "))
    file.close()
    if is_instance_valid(actor):actor.queue_free()
    yield(self,"idle_frame")
    quit()
