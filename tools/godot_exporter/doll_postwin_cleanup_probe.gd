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
var cases=[
    {"name":"doll_same_idle","animation":"Floater","direction":[0,1],"final_direction":[0,1],"position":[40,40]},
    {"name":"minnie_same_idle","animation":"4dir","direction":[0,1],"final_direction":[0,1],"position":[472,88]},
    {"name":"minnie_changed_idle","animation":"4dir","direction":[0,1],"final_direction":[-1,0],"position":[472,88]},
    {"name":"mimmie_same_idle","animation":"4dir","direction":[0,1],"final_direction":[0,1],"position":[112,88]},
    {"name":"mimmie_changed_idle","animation":"4dir","direction":[-1,0],"final_direction":[0,1],"position":[112,88]}
]
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
    var definition=cases[case_index]
    replacement=load("res://replacement.gd").new()
    replacement.animation_name=definition.animation
    get_root().add_child(replacement)
    replacement.set_direction(Vector2(definition.direction[0],definition.direction[1]))
    replacement.character_sprite.animationTree.advance(0)
    replacement.hide()
    body=load("res://actor.gd").new()
    body.animation_name=definition.animation
    get_root().add_child(body)
    body.set_physics_process(false)
    body._replaced=replacement
    body.position=Vector2(definition.position[0],definition.position[1])
    body._direction=Vector2(definition.final_direction[0],definition.final_direction[1])
    frame=0
    frames=[]
    events=[]
    replacement.character_sprite.connect("frame_changed",self,"frame_changed")
    body.connect("tree_exited",self,"actor_exited")
    driver.set_process(true)
func frame_changed(): events.append({"frame":frame,"event":"replacement_frame_changed","sprite":replacement.character_sprite.frame,"play_position":"%.17f"%replacement.character_sprite.animationState.get_current_play_position()})
func actor_exited(): events.append({"frame":frame,"event":"actor_tree_exited"})
func completed(): events.append({"frame":frame,"event":"update_npcs_completed"})
func idle_tick():
    if frame==1:
        events.append({"frame":frame,"event":"call_update_npcs"})
        state=body.update_npcs()
        if state is GDScriptFunctionState: state.connect("completed",self,"completed")
    create_timer(0).connect("timeout",self,"capture")
func capture():
    frames.append({"frame":frame,"replacement_visible":replacement.visible,"sprite":replacement.character_sprite.frame,"position":[replacement.position.x,replacement.position.y],"direction":[replacement.direction.x,replacement.direction.y],"proxy_exists":is_instance_valid(body),"play_position":"%.17f"%replacement.character_sprite.animationState.get_current_play_position(),"play_length":"%.17f"%replacement.character_sprite.animationState.get_current_length(),"play_node":replacement.character_sprite.animationState.get_current_node()})
    frame+=1
    if frame==120:
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
    print("ENCORE_DOLL_POSTWIN_CLEANUP_REFERENCE_COMPLETE")
    quit()
