extends SceneTree
var target
var anim
var rows=[]
func _init():
    call_deferred("run")
func capture(action):
    var special=target.get_node("SpecialAnimations")
    var bat=special.get_node("NintenBat")
    bat._process(0)
    rows.append({"index":rows.size(),"action":action,"position":"%.17f" % anim.current_animation_position,"frame":special.frame,"main":target.get_node("Position/main").visible,"special":special.visible,"hitbox":not target.get_node("HitboxPivot/BatHitbox/CollisionShape2D").disabled,"behind":bat.show_behind_parent,"bat_frame":bat.frame,"pivot":"%.17f" % target.get_node("HitboxPivot").rotation_degrees,"playing":anim.is_playing(),"attack":target._state==1,"tap_run":target._tap_run})
func run():
    yield(self,"idle_frame")
    var file=File.new()
    assert(file.open("res://input.json",File.READ)==OK)
    var config=JSON.parse(file.get_as_text()).result
    file.close()
    target=load("res://probe.tscn").instance()
    anim=target.get_node("AnimationPlayer")
    anim.playback_process_mode=AnimationPlayer.ANIMATION_PROCESS_MANUAL
    get_root().add_child(target)
    for action in config.actions:
        if action.op=="play":
            target._state=1
            target._tap_run=false
            anim.play(action.name)
        elif action.op=="advance":anim.advance(action.delta)
        elif action.op=="flush":yield(self,"idle_frame")
        else:assert(false)
        capture(action)
    var output=File.new()
    assert(output.open("res://../native.json",File.WRITE)==OK)
    output.store_string(JSON.print({"engine":Engine.get_version_info(),"rows":rows},"  "))
    output.close()
    target.free()
    yield(self,"idle_frame")
    quit()
