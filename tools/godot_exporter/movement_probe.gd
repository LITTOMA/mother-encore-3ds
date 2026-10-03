extends SceneTree
class PhysicsDriver:
    extends Node
    var runner
    func _physics_process(delta): runner.physics_tick(delta)
var cases
var case_index = 0
var frame_index = 0
var body
var result
var output = ""
var driver
var input_warmup = false
func read_json(path):
    var file = File.new()
    assert(file.open(path, File.READ) == OK)
    var parsed = JSON.parse(file.get_as_text())
    file.close()
    assert(parsed.error == OK)
    return parsed.result
func _init():
    for arg in OS.get_cmdline_args():
        if arg.begins_with("--encore-out="): output = arg.trim_prefix("--encore-out=")
    assert(output != "")
    cases = read_json("res://cases.json")
    result = read_json("res://metadata.json")
    result["godot"] = Engine.get_version_info()
    result["sequences"] = []
    driver = PhysicsDriver.new()
    driver.runner = self
    driver.set_physics_process(false)
    get_root().add_child(driver)
    call_deferred("start_case")
func start_case():
    Input.action_release("ui_toggle")
    body = load("res://movement.gd").new()
    get_root().add_child(body)
    body.position = Vector2(cases[case_index].position[0], cases[case_index].position[1])
    frame_index = 0
    input_warmup = true
    result.sequences.append({"name": cases[case_index].name, "initial": cases[case_index].position, "frames": []})
    # physics_frame is emitted before Node physics processing. Use the Node
    # callback itself so Input's just-pressed queries use the physics domain.
    driver.set_physics_process(true)
func physics_tick(delta):
    if case_index >= cases.size() or body == null: return
    assert(abs(delta - 1.0/60.0) < 0.00000001)
    var input = cases[case_index].steps[frame_index]
    if input_warmup:
        # Godot action_press/release registers a physics edge for the NEXT
        # physics frame. Queue input a frame ahead, matching OS input dispatch.
        apply_input(input)
        input_warmup = false
        return
    body._input_vector = Vector2(input.x, input.y)
    body._paused = input.paused
    if !input.paused and !input.entering_door: body._movement(delta)
    var state = {"x": body.position.x, "y": body.position.y, "dx": body._direction.x, "dy": body._direction.y,
        "vx": body._velocity.x, "vy": body._velocity.y, "speed": body._speed,
        "crouch": body._crouch, "tap_run": body._tap_run, "running": body._running,
        "substantial": body._substantial_movement, "walking": body._walk, "toggle": input.toggle,
        "animation": body.animation, "moved": body.moved_count}
    result.sequences[case_index].frames.append({"input": input, "state": state})
    frame_index += 1
    if frame_index == cases[case_index].steps.size():
        driver.set_physics_process(false)
        body.queue_free()
        case_index += 1
        if case_index < cases.size(): call_deferred("start_case")
        else: call_deferred("finish")
    else:
        apply_input(cases[case_index].steps[frame_index])
func apply_input(input):
    if input.toggle and !Input.is_action_pressed("ui_toggle"): Input.action_press("ui_toggle")
    if !input.toggle and Input.is_action_pressed("ui_toggle"): Input.action_release("ui_toggle")
func finish():
    var controls = load("res://controls.gd").new()
    for method in controls.get_method_list():
        if method.name == "_get_vector_sign":
            result["controls_threshold_type"] = method.args[1].type
    assert(result.get("controls_threshold_type") == TYPE_INT)
    result["controls"] = []
    for x in [-4.0,-1.0,-0.71,-0.5,-0.49,0.0,0.49,0.5,0.71,1.0,4.0]:
        for y in [-4.0,-1.0,-0.71,-0.5,-0.49,0.0,0.49,0.5,0.71,1.0,4.0]:
            for threshold in [0.0,0.25,0.5,0.75,1.0]:
                var vector = controls._get_vector_sign(Vector2(x,y), threshold)
                result.controls.append([x,y,threshold,vector.x,vector.y])
    var file = File.new()
    assert(file.open(output, File.WRITE) == OK)
    file.store_string(JSON.print(result,"  ")+"\n")
    file.close()
    print("Original movement kernel: ", result.sequences.size(), " sequences and ", result.controls.size(), " control vectors.")
    quit()
