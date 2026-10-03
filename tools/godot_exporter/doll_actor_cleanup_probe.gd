extends SceneTree
class Driver:
    extends Node
    var runner
    func _process(_delta): runner.idle_tick()
var driver
var body
var replacement
var output
var result
var case_index=0
var frame=0
var frames=[]
var events=[]
var state
var cases=[{"name":"different_idle","dir":[0,1],"timer":false},{"name":"same_idle","dir":[0,-1],"timer":false},{"name":"different_idle_timer","dir":[0,1],"timer":true},{"name":"same_idle_timer","dir":[0,-1],"timer":true}]
func _init():
    for arg in OS.get_cmdline_args():
        if arg.begins_with("--encore-out="): output=arg.trim_prefix("--encore-out=")
    var file=File.new()
    assert(file.open("res://cleanup_metadata.json",File.READ)==OK)
    result=JSON.parse(file.get_as_text()).result
    result["godot"]=Engine.get_version_info()
    result["cases"]=[]
    driver=Driver.new()
    driver.runner=self
    get_root().add_child(driver)
    driver.set_process(false)
    call_deferred("start_case")
func start_case():
    replacement=load("res://replacement.gd").new()
    get_root().add_child(replacement)
    var d=cases[case_index].dir
    replacement.set_direction(Vector2(d[0],d[1]))
    replacement.character_sprite.animationTree.advance(0)
    replacement.hide()
    body=load("res://actor.gd").new()
    body.animation_name="4dir"
    get_root().add_child(body)
    body.set_physics_process(false)
    body._replaced=replacement
    body.position=Vector2(128,160)
    body._direction=Vector2(0,-1)
    frame=0
    frames=[]
    events=[]
    replacement.character_sprite.connect("frame_changed",self,"frame_changed")
    body.connect("tree_exited",self,"actor_exited")
    driver.set_process(true)
func frame_changed(): events.append({"frame":frame,"event":"replacement_frame_changed","sprite":replacement.character_sprite.frame})
func actor_exited(): events.append({"frame":frame,"event":"actor_tree_exited"})
func begin_cleanup():
    events.append({"frame":frame,"event":"call_update_npcs"})
    state=body.update_npcs()
    if state is GDScriptFunctionState: state.connect("completed",self,"completed")
func completed(): events.append({"frame":frame,"event":"update_npcs_completed"})
func idle_tick():
    if frame==1:
        if cases[case_index].timer:
            var timer=Timer.new()
            timer.one_shot=true
            timer.wait_time=.001
            replacement.add_child(timer)
            timer.connect("timeout",self,"begin_cleanup")
            timer.start()
        else: begin_cleanup()
    create_timer(0).connect("timeout",self,"capture")
func capture():
    frames.append({"frame":frame,"replacement_visible":replacement.visible,"sprite":replacement.character_sprite.frame,"position":[replacement.position.x,replacement.position.y],"direction":[replacement.direction.x,replacement.direction.y],"proxy_exists":is_instance_valid(body)})
    frame+=1
    if frame==10:
        driver.set_process(false)
        result.cases.append({"definition":cases[case_index],"events":events,"frames":frames})
        if is_instance_valid(body):
            body.disconnect("tree_exited",self,"actor_exited")
            body.queue_free()
        replacement.queue_free()
        case_index+=1
        if case_index==cases.size():call_deferred("finish")
        else:call_deferred("start_case")
func finish():
    var file=File.new()
    assert(file.open(output,File.WRITE)==OK)
    file.store_string(JSON.print(result,"  ")+"\n")
    file.close()
    print("ENCORE_DOLL_CLEANUP_REFERENCE_COMPLETE")
    quit()
