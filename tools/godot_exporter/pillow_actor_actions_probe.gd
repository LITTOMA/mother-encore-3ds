extends SceneTree
class Driver:
    extends Node
    var runner
    func _physics_process(delta): runner.physics_tick(delta)
    func _process(delta): runner.idle_tick(delta)
var body
var driver
var physics_ready = false
var frames = []
var frame = -1
var output = ""
var cases
var case_index = 0
var result
func read_json(path):
    var file = File.new()
    assert(file.open(path,File.READ)==OK)
    var parsed = JSON.parse(file.get_as_text())
    assert(parsed.error == OK)
    file.close()
    return parsed.result
func _init():
    for arg in OS.get_cmdline_args():
        if arg.begins_with("--encore-out="): output = arg.trim_prefix("--encore-out=")
    assert(output != "")
    cases = read_json("res://cases.json")
    result = read_json("res://metadata.json")
    result["godot"] = Engine.get_version_info()
    result["cases"] = []
    driver = Driver.new()
    driver.runner = self
    get_root().add_child(driver)
    driver.set_physics_process(false)
    driver.set_process(false)
    call_deferred("start_case")
func start_case():
    body = load("res://actor.gd").new()
    body.animation_name = "4dir" if cases[case_index].kind == "Minnie" else "Floater"
    get_root().add_child(body)
    body.set_physics_process(false)
    body.character_sprite.animationTree.process_mode = AnimationTree.ANIMATION_PROCESS_IDLE
    body.position = Vector2(cases[case_index].start[0],cases[case_index].start[1])
    body.character_sprite.offset = Vector2(0,-12 if cases[case_index].kind == "Minnie" else -11)
    body.character_sprite.animationTree.advance(0)
    frames = []
    frame = 0
    driver.set_physics_process(true)
    driver.set_process(true)
func physics_tick(delta):
    result["physics_delta"] = "%.17f" % delta
    physics_ready = true
    assert(abs(delta - 1.0/60.0) < 0.00000001)
    for action in cases[case_index].actions:
        if action.tick != frame: continue
        match action.op:
            "path":
                var steps = []
                for entry in action.entries:
                    if entry.kind == 0: steps.append(body.MoveAction.new(Vector2(entry.x,entry.y)))
                    else: steps.append(body.WaitAction.new(entry.duration))
                body.move_queue(steps,action.animation,action.speed,"position",false,action.loop,action.queue)
            "teleport": body.global_position = Vector2(action.x,action.y)
            "jump": body.jump(action.height,action.length,int(action.times))
            "shake": body.shake(Vector2(action.x,0),action.length)
            "anim": body.play_anim(action.animation)
            "turn": body.set_direction(Vector2(action.x,action.y))
            "turn_to": body.turn_to(Vector2(action.x,action.y),action.length)
            "stop": body.stop_loop()
    body._physics_process(delta)
func idle_tick(delta):
    if !physics_ready: return
    physics_ready = false
    result["idle_delta"] = "%.17f" % delta
    assert(abs(delta - 1.0/60.0) < 0.00000001)
    create_timer(0).connect("timeout",self,"capture_tick")
func capture_tick():
    frames.append({"position":[body.position.x,body.position.y],"direction":[body._direction.x,body._direction.y],"blend":[body.character_sprite._direction.x,body.character_sprite._direction.y],"velocity":[body.velocity.x,body.velocity.y],"sprite_position":[body.character_sprite.position.x,body.character_sprite.position.y],"sprite_offset":[body.character_sprite.offset.x,body.character_sprite.offset.y],"frame":body.character_sprite.frame,"moving":body.state==body.MOVETO or body.state==body.STEP,"rotating":body.state==body.ROTATING,"looping":body._looping,"movements":body.movement_count,"actions":body.action_count})
    frame += 1
    if frame == cases[case_index].ticks:
        driver.set_physics_process(false)
        driver.set_process(false)
        result.cases.append({"definition":cases[case_index],"frames":frames})
        body.queue_free()
        case_index += 1
        if case_index == cases.size(): call_deferred("finish")
        else: call_deferred("start_case")
func finish():
    var file = File.new()
    assert(file.open(output,File.WRITE)==OK)
    file.store_string(JSON.print(result,"  ")+"\n")
    file.close()
    print("ENCORE_PILLOW_ACTOR_ACTION_REFERENCE_COMPLETE ",result.cases.size())
    quit()
