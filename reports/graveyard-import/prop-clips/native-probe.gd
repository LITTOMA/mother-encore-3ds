extends SceneTree
var target
var player
var rows=[]
var completed=[]
func _init():
    call_deferred("run")
func finished(name):
    completed.append(name)
func capture(action):
    rows.append({"index":rows.size(),"action":action,"position":"%.17f" % player.current_animation_position,"frame":target.get_node("Sprite").frame,"visible":target.get_node("Sprite").visible,"body":not target.get_node("StaticBody2D/CollisionShape2D").disabled,"hitbox":not target.get_node("Hitbox/CollisionShape2D").disabled,"interaction":not target.get_node("interact/CollisionShape2D").disabled,"playing":player.is_playing(),"current":player.current_animation if player.is_playing() else "","completed":completed.duplicate()})
    completed.clear()
func run():
    yield(self,"idle_frame")
    var file=File.new()
    assert(file.open("res://input.json",File.READ)==OK)
    var config=JSON.parse(file.get_as_text()).result
    file.close()
    target=load("res://"+config.scene).instance()
    target.New_parent=NodePath("..")
    player=target.get_node("AnimationPlayer")
    player.playback_process_mode=AnimationPlayer.ANIMATION_PROCESS_MANUAL
    get_root().add_child(target)
    player.connect("animation_finished",self,"finished")
    for action in config.actions:
        if action.op=="play":player.play(action.name)
        elif action.op=="advance":player.advance(action.delta)
        else:assert(false)
        capture(action)
    var output=File.new()
    assert(output.open("res://../native.json",File.WRITE)==OK)
    output.store_string(JSON.print({"engine":Engine.get_version_info(),"rows":rows},"  "))
    output.close()
    target.free()
    yield(self,"idle_frame")
    yield(self,"idle_frame")
    quit()
