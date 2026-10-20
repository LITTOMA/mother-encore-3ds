extends SceneTree
var target
var animator
var sprite
var body
var rows=[]
var completed=[]
func _init():call_deferred("run")
func finished(name):completed.append(name)
func capture(profile,index,action):
    rows.append({"file":profile.file,"index":index,"op":action.op,"frame":sprite.frame,"visible":sprite.visible and target.visible,"body":not body.disabled if body!=null else false,"playing":animator.is_playing(),"current":animator.current_animation if animator.is_playing() else "","position":"%.17f" % animator.current_animation_position,"completed":completed.duplicate()})
    completed.clear()
func run():
    yield(self,"idle_frame")
    var file=File.new()
    assert(file.open("res://input.json",File.READ)==OK)
    var config=JSON.parse(file.get_as_text()).result
    file.close()
    for profile in config.profiles:
        target=load("res://"+profile.scene).instance()
        animator=target.get_node("AnimationPlayer")
        animator.playback_process_mode=AnimationPlayer.ANIMATION_PROCESS_MANUAL
        sprite=target.get_node(profile.frame_owner)
        body=target.get_node(profile.body_owner) if profile.body_owner!=null else null
        get_root().add_child(target)
        animator.connect("animation_finished",self,"finished")
        for i in profile.actions.size():
            var action=profile.actions[i]
            if action.op=="play":animator.play(action.name)
            elif action.op=="advance":animator.advance(action.delta)
            else:assert(false)
            capture(profile,i,action)
        target.queue_free()
        yield(self,"idle_frame")
        yield(self,"idle_frame")
    assert(file.open("res://observations.json",File.WRITE)==OK)
    file.store_string(JSON.print({"engine":Engine.get_version_info(),"rows":rows},"  "))
    file.close()
    quit()
