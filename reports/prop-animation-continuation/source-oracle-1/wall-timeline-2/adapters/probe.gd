extends SceneTree
signal flushed
var target
var animator
var audio
var service
var events=[]
var rows=[]
var results=[]
func _init():call_deferred("run")
func event(value):events.append(value)
func completed(name):event({"kind":"finished","clip":name})
func flush_queue():emit_signal("flushed")
func capture(index,action):
    rows.append({"index":index,"op":action.op,"frame":target.get_node("main").frame,"body":not target.get_node("collision/CollisionShape2D").disabled,"playing_request":audio.requested_playing,"animation_playing":animator.is_playing(),"animation":animator.current_animation if animator.is_playing() else "","position":"%.17f"%animator.current_animation_position,"events":events.duplicate(true)})
    events=[]
func run():
    var file=File.new()
    assert(file.open("res://input.json",File.READ)==OK)
    var config=JSON.parse(file.get_as_text()).result
    file.close()
    assert(file.open("res://audio-policy.json",File.READ)==OK)
    var policy=JSON.parse(file.get_as_text()).result
    file.close()
    service=load("res://service.gd").new()
    service.runner=self
    get_root().add_child(service)
    for test in config:
        events=[]
        rows=[]
        target=load("res://wall.tscn").instance()
        target.global=service
        audio=target.get_node("AudioStreamPlayer")
        # Fresh private imports must not reuse an upstream .mp3str cache path.
        # Apply the reviewed source import options to the native resource.
        audio.stream.loop=policy.loop
        audio.stream.loop_offset=policy.offset
        audio.set_script(load("res://audio.gd"))
        audio.runner=self
        assert(audio.stream is AudioStreamMP3 and not audio.stream.loop)
        animator=target.get_node("AnimationPlayer")
        animator.playback_process_mode=AnimationPlayer.ANIMATION_PROCESS_MANUAL
        assert(animator.method_call_mode==AnimationPlayer.ANIMATION_METHOD_CALL_DEFERRED)
        animator.connect("animation_finished",self,"completed")
        get_root().add_child(target)
        for i in test.actions.size():
            var action=test.actions[i]
            if action.op=="play":animator.play(action.clip)
            elif action.op=="advance":animator.advance(action.delta)
            elif action.op=="flush":
                call_deferred("flush_queue")
                yield(self,"flushed")
            else:assert(false)
            capture(i,action)
        results.append({"name":test.name,"rows":rows})
        target.free()
    assert(file.open("res://observations.json",File.WRITE)==OK)
    file.store_string(JSON.print({"engine":Engine.get_version_info(),"cases":results},"  "))
    file.close()
    print("WALL_TIMELINE_REFERENCE_COMPLETE")
    quit()
