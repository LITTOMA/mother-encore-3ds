extends SceneTree
# Native engine-only reference. Game assets and scripts are not imported.
class Driver:
    extends Node
    var runner
    func _physics_process(delta): runner.tick(delta)
var cases
var rows = []
var case_index = 0
var frame_index = 0
var warmup = true
var actor
var bodies = []
var driver
var output = ""
func _init():
    for arg in OS.get_cmdline_args():
        if arg.begins_with("--encore-out="): output = arg.trim_prefix("--encore-out=")
    assert(output != "")
    var file = File.new()
    assert(file.open("res://cases.json", File.READ) == OK)
    var parsed = JSON.parse(file.get_as_text())
    assert(parsed.error == OK)
    cases = parsed.result
    file.close()
    driver = Driver.new()
    driver.runner = self
    get_root().add_child(driver)
    driver.set_physics_process(false)
    call_deferred("start_case")
func shape_node(points, kind="ConvexPolygonShape2D"):
    if kind == "RectangleShape2D":
        var low = Vector2(points[0][0],points[0][1])
        var high = low
        for p in points:
            low.x = min(low.x,p[0]); low.y = min(low.y,p[1])
            high.x = max(high.x,p[0]); high.y = max(high.y,p[1])
        var rect = RectangleShape2D.new()
        rect.extents = (high-low)/2
        var node = CollisionShape2D.new()
        node.shape = rect
        node.position = (high+low)/2
        return node
    assert(kind == "ConvexPolygonShape2D")
    var shape = ConvexPolygonShape2D.new()
    var vertices = PoolVector2Array()
    for p in points: vertices.append(Vector2(p[0],p[1]))
    shape.points = vertices
    var node = CollisionShape2D.new()
    node.shape = shape
    return node
func start_case():
    var entry = cases[case_index]
    for i in range(entry.obstacles.size()):
        var body = StaticBody2D.new()
        var kind = entry.obstacle_kinds[i] if entry.has("obstacle_kinds") else "ConvexPolygonShape2D"
        body.add_child(shape_node(entry.obstacles[i],kind))
        get_root().add_child(body)
        bodies.append(body)
    actor = KinematicBody2D.new()
    actor.add_child(shape_node(entry.actor))
    actor.set_safe_margin(0.08)
    actor.position = Vector2(entry.position[0],entry.position[1])
    get_root().add_child(actor)
    frame_index = 0
    warmup = true
    rows.append({"name": entry.name, "frames": []})
    driver.set_physics_process(true)
func exact(v): return ["%.9f" % v.x,"%.9f" % v.y]
func tick(delta):
    assert(abs(delta-1.0/60.0) < 0.00000001)
    if warmup:
        warmup = false
        return
    var entry = cases[case_index]
    var v = entry.velocities[frame_index]
    var velocity = actor.move_and_slide(Vector2(v[0],v[1]))
    if entry.get("round_and_recover", false):
        actor.move_and_slide(Vector2.ZERO)
        actor.position = actor.position.round()
    rows[case_index].frames.append({"position":exact(actor.position),"velocity":exact(velocity)})
    frame_index += 1
    if frame_index == entry.velocities.size():
        driver.set_physics_process(false)
        actor.queue_free()
        for b in bodies: b.queue_free()
        bodies.clear()
        case_index += 1
        if case_index < cases.size(): call_deferred("start_case")
        else: call_deferred("finish")
func finish():
    var file = File.new()
    assert(file.open(output,File.WRITE) == OK)
    file.store_string(JSON.print({"engine":Engine.get_version_info(),"cases":rows},"  "))
    file.close()
    quit()
