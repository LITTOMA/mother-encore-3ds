extends SceneTree
class Driver:
    extends Node
    var runner
    func _physics_process(delta): runner.physics_tick(delta)
    func _process(delta): runner.idle_tick(delta)
var driver
var world
var viewport
var player
var lamp
var player_camera
var mimmie
var mimmie_camera
var lamp_camera
var frame = -3
var case_index = 0
var frames = []
var cases
var result
var output = ""
var previous_idle = false
var physics_global
var physics_center
func read_json(path):
    var f=File.new()
    assert(f.open(path,File.READ)==OK)
    var parsed=JSON.parse(f.get_as_text())
    f.close()
    assert(parsed.error==OK)
    return parsed.result
func _init():
    for arg in OS.get_cmdline_args():
        if arg.begins_with("--encore-out="): output=arg.trim_prefix("--encore-out=")
    assert(output!="")
    cases=read_json("res://cases.json")
    result=read_json("res://metadata.json")
    result["godot"]=Engine.get_version_info()
    result["cases"]=[]
    result["viewports"]=[[400,240],[320,180]]
    driver=Driver.new()
    driver.runner=self
    get_root().add_child(driver)
    driver.set_physics_process(false)
    driver.set_process(false)
    call_deferred("start_case")
func new_camera(parent):
    var cam=load("res://camera.gd").new()
    cam.name="Camera2D"
    cam.pause_mode=Node.PAUSE_MODE_PROCESS
    cam.rotating=true
    cam.process_mode=Camera2D.CAMERA2D_PROCESS_PHYSICS
    cam.smoothing_speed=64
    var arrows=load("res://arrows.gd").new()
    arrows.name="ScopeArrows"
    arrows.visible=false
    cam.add_child(arrows)
    parent.add_child(cam)
    return cam
func start_case():
    viewport=Viewport.new()
    viewport.size=Vector2(cases[case_index].viewport[0],cases[case_index].viewport[1])
    viewport.disable_3d=true
    get_root().add_child(viewport)
    world=Node2D.new()
    viewport.add_child(world)
    player=load("res://player.gd").new()
    var def=cases[case_index]
    player.position=Vector2(def.player[0],def.player[1])
    get_root().get_node("global").player=player
    world.add_child(player)
    player_camera=new_camera(player)
    get_root().get_node("global").currentCamera=player_camera
    player_camera.make_current()
    lamp=Node2D.new()
    lamp.position=Vector2(def.lamp[0],def.lamp[1])
    world.add_child(lamp)
    lamp_camera=new_camera(lamp)
    mimmie=Node2D.new()
    mimmie.position=Vector2(def.mimmie[0],def.mimmie[1])
    world.add_child(mimmie)
    mimmie_camera=new_camera(mimmie)
    var area=load("res://camarea.gd").new()
    area.position=Vector2(def.area.center[0],def.area.center[1])
    var shape=CollisionShape2D.new()
    shape.name="CollisionShape2D"
    shape.position=Vector2.ZERO
    shape.shape=RectangleShape2D.new()
    shape.shape.extents=Vector2(def.area.extents[0],def.area.extents[1])
    area.add_child(shape)
    world.add_child(area)
    area._on_enter()
    frame=-3
    frames=[]
    previous_idle=false
    driver.set_physics_process(true)
    driver.set_process(true)
func physics_tick(delta):
    assert(abs(delta-1.0/60.0)<.00000001)
    result["physics_delta"]="%.17f" % delta
    if previous_idle and frame>=0:
        capture_tick()
        if frame==cases[case_index].ticks:
            driver.set_physics_process(false)
            driver.set_process(false)
            result.cases.append({"definition":cases[case_index],"frames":frames})
            viewport.queue_free()
            case_index+=1
            if case_index==cases.size():call_deferred("finish")
            else:call_deferred("start_case")
            return
    elif previous_idle:frame+=1
    previous_idle=false
    if frame<0:return
    for a in cases[case_index].actions:
        if a.tick==frame and a.op=="lamp":lamp.position=Vector2(a.x,a.y)
        if a.tick==frame and a.op=="mimmie":mimmie.position=Vector2(a.x,a.y)
func idle_tick(delta):
    assert(abs(delta-1.0/60.0)<.00000001)
    result["idle_delta"]="%.17f" % delta
    if frame>=0:
        var cam=get_root().get_node("global").currentCamera
        physics_global=cam.global_position
        physics_center=cam.get_camera_screen_center()
        apply_actions()
    previous_idle=true
func apply_actions():
    for a in cases[case_index].actions:
        if a.tick!=frame:continue
        var cam=get_root().get_node("global").currentCamera
        match a.op:
            "change":lamp_camera.set_current()
            "change_mimmie":mimmie_camera.set_current()
            "return":cam.return_camera(a.length)
            "move":cam.move_camera(Vector2(a.x,a.y),a.length)
            "shake":cam.shake_camera(4,a.length,Vector2.RIGHT)
            "restore":
                player_camera.set_current()
                player_camera.return_camera(.5)
            "pause":
                if cam.tween:cam.tween.pause()
func vec(v):return [v.x,v.y]
func capture_tick():
    var cam=get_root().get_node("global").currentCamera
    frames.append({"global":vec(cam.global_position),"center":vec(cam.get_camera_screen_center()),"offset":vec(cam.offset),"shake":vec(cam._shake_offset),"local":vec(cam.position),"physics_global":vec(physics_global),"physics_center":vec(physics_center),"camera":1 if cam==lamp_camera else (2 if cam==mimmie_camera else 0),"limits":[cam.limit_left,cam.limit_top,cam.limit_right,cam.limit_bottom]})
    frame+=1
func finish():
    var f=File.new()
    assert(f.open(output,File.WRITE)==OK)
    f.store_string(JSON.print(result,"  ")+"\n")
    f.close()
    print("ENCORE_CUTSCENE_CAMERA_REFERENCE_COMPLETE ",result.cases.size())
    quit()
