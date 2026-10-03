extends SceneTree
class Driver:
    extends Node
    var runner
    func _physics_process(delta): runner.physics_tick(delta)
    func _process(delta): runner.idle_tick(delta)
var body
var driver
var case_wall
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
    if cases[case_index].kind == "Ninten": body.animation_name = "PartyMember"
    elif cases[case_index].kind == "Mimmie": body.animation_name = "4dir"
    elif cases[case_index].kind == "Doll": body.animation_name = "Floater"
    get_root().add_child(body)
    body.set_physics_process(false)
    body.character_sprite.animationTree.process_mode = AnimationTree.ANIMATION_PROCESS_IDLE
    body.position = Vector2(cases[case_index].start[0],cases[case_index].start[1])
    if cases[case_index].name == "free_through_wall":
        var wall = StaticBody2D.new()
        wall.collision_layer = 1
        wall.collision_mask = 1
        var shape = CollisionShape2D.new()
        var rectangle = RectangleShape2D.new()
        rectangle.extents = Vector2(20,200)
        shape.shape = rectangle
        shape.position = Vector2(45,0)
        wall.add_child(shape)
        get_root().add_child(wall)
        case_wall = wall
    if cases[case_index].kind == "Ninten":
        body.character_sprite.offset = Vector2(0,-13)
        body.is_party_member = true
    if cases[case_index].kind == "Mimmie": body.character_sprite.offset = Vector2(0,-12)
    body.character_sprite.animationTree.advance(0)
    body.emotes = Sprite.new()
    body.character_sprite.add_child(body.emotes)
    var player = AnimationPlayer.new()
    player.name = "AnimationPlayer"
    player.playback_process_mode = AnimationPlayer.ANIMATION_PROCESS_IDLE
    body.emotes.add_child(player)
    body.emotes.hframes = 12
    body.emotes.vframes = 12
    body.emotes.frame = 64
    player.add_animation("surprise",load("res://exclamation.tres"))
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
                body.move_queue(steps,action.animation,action.speed,"step" if action.step else "position",action.moonwalk)
            "teleport": body.global_position = Vector2(action.x,action.y)
            "talk": body.talking = action.enabled
            "move": body.move_queue([body.MoveAction.new(Vector2(action.x,action.y))],"",action.speed,"position")
            "jump": body.jump(action.height,action.length)
            "shake": body.shake(Vector2(action.x,0),action.length)
            "anim": body.play_anim(action.anim)
            "turn": body.set_direction(Vector2(action.x,action.y))
            "turn_to": body.turn_to(Vector2(action.x,action.y),action.length)
            "emote": body.emotes.get_node("AnimationPlayer").play("surprise")
            "timer_turn":
                var timer = Timer.new()
                timer.one_shot = true
                timer.wait_time = .001
                timer.connect("timeout",body,"turn_to",[Vector2(action.x,action.y),action.length])
                body.add_child(timer)
                timer.start()
            "timer_shake":
                var timer = Timer.new()
                timer.one_shot = true
                timer.wait_time = .001
                timer.connect("timeout",body,"shake",[Vector2(action.x,0),action.length])
                body.add_child(timer)
                timer.start()
    body._physics_process(delta)
func idle_tick(delta):
    if !physics_ready: return
    physics_ready = false
    result["idle_delta"] = "%.17f" % delta
    assert(abs(delta - 1.0/60.0) < 0.00000001)
    create_timer(0).connect("timeout",self,"capture_tick")
func capture_tick():
    frames.append({"position":[body.position.x,body.position.y],"direction":[body._direction.x,body._direction.y],"blend":[body.character_sprite._direction.x,body.character_sprite._direction.y],"velocity":[body.velocity.x,body.velocity.y],"frame":body.character_sprite.frame,"emote":body.emotes.frame,"moving":body.state==body.MOVETO or body.state==body.STEP,"rotating":body.state==body.ROTATING,"moonwalk":body._moonwalk,"movements":body.movement_count,"actions":body.action_count})
    frame += 1
    if frame == cases[case_index].ticks:
        driver.set_physics_process(false)
        driver.set_process(false)
        result.cases.append({"definition":cases[case_index],"frames":frames})
        body.queue_free()
        if case_wall:
            case_wall.queue_free()
            case_wall = null
        case_index += 1
        if case_index == cases.size(): call_deferred("finish")
        else: call_deferred("start_case")
func finish():
    var file = File.new()
    assert(file.open(output,File.WRITE)==OK)
    file.store_string(JSON.print(result,"  ")+"\n")
    file.close()
    print("ENCORE_ACTOR_ACTION_REFERENCE_COMPLETE ",result.cases.size())
    quit()
