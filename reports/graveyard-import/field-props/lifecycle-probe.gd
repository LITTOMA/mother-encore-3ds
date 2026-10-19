extends SceneTree
class ViewSignals:
    extends Node2D
    signal screen_entered
    signal screen_exited
    func enter():emit_signal("screen_entered")
    func exit():emit_signal("screen_exited")
class Callback:
    extends Node
    var runner
    func check_start():
        runner.calls.append({"alive":is_instance_valid(runner.target),"queued":runner.target.is_queued_for_deletion() if is_instance_valid(runner.target) else false})
var target
var parent
var rows=[]
var calls=[]
var current_case=""
var services
var flag_store
var dialogue_service
func _init():
    call_deferred("run")
func capture(action):
    var row={"case":current_case,"action":action,"alive":is_instance_valid(target),"calls":calls.duplicate(true),"rumble":services.events.duplicate(true),"dialogue":dialogue_service.events.duplicate(),"clones":[]}
    if is_instance_valid(target):
        var a=target.get_node("AnimationPlayer")
        row.props={"frame":target.get_node("Sprite").frame,"visible":target.get_node("Sprite").visible,"body":not target.get_node("StaticBody2D/CollisionShape2D").disabled,"hitbox":not target.get_node("Hitbox/CollisionShape2D").disabled,"interaction":not target.get_node("interact/CollisionShape2D").disabled,"node_visible":target.visible,"prompt":target.get_node("interact/ButtonPrompt").enabled,"queued":target.is_queued_for_deletion(),"playing":a.is_playing(),"current":a.current_animation if a.is_playing() else ""}
    for child in parent.get_children():
        if child is Sprite:
            row.clones.append({"frame":child.frame,"visible":child.visible,"x":child.position.x,"y":child.position.y})
    rows.append(row)
    calls.clear()
    services.events.clear()
    dialogue_service.events.clear()
func run():
    yield(self,"idle_frame")
    services=get_root().get_node("global")
    flag_store=get_root().get_node("globaldata")
    dialogue_service=get_root().get_node("uiManager")
    var input=File.new()
    assert(input.open("res://lifecycle-input.json",File.READ)==OK)
    var config=JSON.parse(input.get_as_text()).result
    input.close()
    for test in config.cases:
        current_case=test.name
        flag_store.flags=test.flags.duplicate(true)
        parent=Node2D.new()
        get_root().add_child(parent)
        var callback=Callback.new()
        callback.name="Callback"
        callback.runner=self
        parent.add_child(callback)
        target=load("res://prop-probe.tscn").instance()
        target.position=Vector2(32,64)
        target.New_parent=NodePath("..")
        target.called_object=NodePath("../Callback" if test.callback else "../Missing")
        target.call_object_function="check_start" if test.function else ""
        target.object_function_disappear_flag=test.callback_flag
        target.flag=test.flag
        var original_view=target.get_node("VisibilityNotifier2D")
        target.remove_child(original_view)
        original_view.free()
        var view=ViewSignals.new()
        view.name="VisibilityNotifier2D"
        target.add_child(view)
        target.get_node("AnimationPlayer").playback_process_mode=AnimationPlayer.ANIMATION_PROCESS_MANUAL
        parent.add_child(target)
        capture({"op":"ready"})
        for action in test.actions:
            if action.op=="advance":target.get_node("AnimationPlayer").advance(action.delta)
            elif action.op=="grow":target.grow()
            elif action.op=="hit":target._on_Hitbox_area_entered(null)
            elif action.op=="enter":view.enter()
            elif action.op=="exit":view.exit()
            elif action.op=="interact":target.interact()
            elif action.op=="flag":flag_store.flags[action.id]=action.value
            elif action.op=="flush":
                yield(self,"idle_frame")
                yield(self,"idle_frame")
            else:assert(false)
            capture(action)
        parent.free()
    var output=File.new()
    assert(output.open("res://../lifecycle-native.json",File.WRITE)==OK)
    output.store_string(JSON.print({"engine":Engine.get_version_info(),"rows":rows},"  "))
    output.close()
    yield(self,"idle_frame")
    quit()
