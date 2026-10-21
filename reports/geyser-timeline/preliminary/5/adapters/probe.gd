extends Node
var rows=[]
var finished=[]
func _ready():call_deferred("run")
func bits(value):
    var bytes=StreamPeerBuffer.new()
    bytes.put_float(value)
    bytes.seek(0)
    return bytes.get_u32()
func done(name):finished.append(name)
func capture(actor,case_index,index,phase,delta):
    var anim=actor.get_node("AnimationPlayer")
    var audio=actor.get_node("AudioStreamPlayer2D")
    rows.append({"case":case_index,"index":index,"phase":phase,"delta":bits(delta),"time":bits(anim.current_animation_position),"frame":actor.get_node("Sprite").frame,"monitoring":actor.monitoring,"disabled":actor.get_node("CollisionShape2D").disabled,"active":anim.playback_active,"paused":audio.stream_paused,"gain":bits(audio.volume_db),"commands":audio.commands.duplicate(true),"global_calls":audioManager.commands.duplicate(true),"finished":finished.duplicate(),"queued":actor.is_queued_for_deletion(),"on_screen":actor.get_node("VisibilityNotifier2D").is_on_screen(),"ui":[uiManager.battle,uiManager.cutscene,uiManager.game_over]})
    audio.commands.clear()
    audioManager.commands.clear()
func stable(node,expected):
    var count=0
    for _i in 200:
        yield(get_tree().create_timer(.01),"timeout")
        count=count+1 if node.playing==expected else 0
        if count==3:return
    assert(false)
func run():
    var file=File.new()
    assert(file.open("res://input.json",File.READ)==OK)
    var config=JSON.parse(file.get_as_text()).result
    file.close()
    var player=load("res://party.gd").new()
    add_child(player)
    global.player=player
    var meta={}
    for path in config.sounds:
        var stream=load(path)
        meta[path]={"length":bits(stream.get_length()),"loop":stream.loop,"offset":bits(stream.loop_offset)}
    var geometry={}
    for case_index in config.cases.size():
        var spec=config.cases[case_index]
        for old in audioManager.get_node("Sfx").get_children():old.free()
        audioManager.commands.clear()
        uiManager.battle=false
        uiManager.cutscene=false
        uiManager.game_over=false
        if spec.get("voice",1)!=1:
            var named=audioManager.add_sfx(load(config.sounds[2]),"geyser")
            if spec.voice==3:named.play()
            yield(stable(named,spec.voice==3),"completed")
        finished=[]
        var actor=load("res://geyser.tscn").instance()
        actor.position=Vector2(-1000,-1000) if spec.get("offscreen",false) else Vector2(100,100)
        add_child(actor)
        var anim=actor.get_node("AnimationPlayer")
        anim.playback_process_mode=AnimationPlayer.ANIMATION_PROCESS_MANUAL
        actor.set_process(false)
        assert(anim.connect("animation_finished",self,"done")==OK)
        yield(get_tree(),"idle_frame")
        yield(get_tree(),"idle_frame")
        if case_index==0:
            var sprite=actor.get_node("Sprite")
            var audio=actor.get_node("AudioStreamPlayer2D")
            var shape=actor.get_node("CollisionShape2D")
            var rect=actor.get_node("VisibilityNotifier2D").rect
            geometry={"centered":sprite.centered,"size":[bits(sprite.texture.get_width()/float(sprite.hframes)),bits(sprite.texture.get_height())],"sprite":[bits(sprite.position.x),bits(sprite.position.y),bits(sprite.offset.x),bits(sprite.offset.y)],"shape":[bits(shape.position.x),bits(shape.position.y),bits(shape.rotation),bits(shape.shape.extents.x),bits(shape.shape.extents.y)],"audio":[bits(audio.volume_db),bits(audio.pitch_scale),bits(audio.max_distance),bits(audio.attenuation),bits(audio.panning_strength)],"notify":[bits(rect.position.x),bits(rect.position.y),bits(rect.size.x),bits(rect.size.y)]}
        capture(actor,case_index,-1,"ready",0.0)
        for index in spec.deltas.size():
            var paused=spec.get("pause",{}).get(str(index),null)
            if paused!=null:player.set_observed_pause(paused)
            anim.advance(spec.deltas[index])
            var gate=spec.get("gate",{}).get(str(index),0)
            uiManager.battle=gate==1
            uiManager.cutscene=gate==2
            uiManager.game_over=gate==3
            actor._process(0.0)
            capture(actor,case_index,index,"before_deferred",spec.deltas[index])
            # Even queued_free source methods remain in the MessageQueue until
            # flush;an observer deferred after them runs before delete flush.
            call_deferred("capture_deferred",actor,case_index,index,spec.deltas[index])
            yield(get_tree(),"idle_frame")
            if !is_instance_valid(actor):break
        if is_instance_valid(actor):actor.queue_free()
        yield(get_tree(),"idle_frame")
    assert(file.open("res://../reference.json",File.WRITE)==OK)
    var cases=[]
    for spec in config.cases:cases.append({"name":spec.name,"steps":spec.deltas.size()})
    file.store_string(JSON.print({"schema":1,"godot":Engine.get_version_info(),"meta":meta,"geometry":geometry,"cases":cases,"rows":rows},"  "))
    file.close()
    get_tree().quit()
func capture_deferred(actor,case_index,index,delta):
    capture(actor,case_index,index,"after_deferred",delta)
