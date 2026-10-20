extends Node
var config
var areas=[]
var rows=[]
var geometry=[]
var viewport_index=0
func _ready():call_deferred("run")
func vec(v):return ["%.17f"%v.x,"%.17f"%v.y]
func capture(action):
    var camera=global.currentCamera
    var current=global.currentScene._current_room
    var active=[]
    var flags=[]
    for i in areas.size():
        active.append(areas[i].inside)
        flags.append(globaldata.object_flags.get(config.model.rooms[i].flag,false))
    rows.append({"viewport":viewport_index,"action":action,"inside":active,"flags":flags,"flag_signals":global.flag_signals,"camareas":camera.camareas,"limits":[camera.limit_left,camera.limit_top,camera.limit_right,camera.limit_bottom],"camera_position":vec(camera.position),"offset_calls":camera.offset_calls.duplicate(true),"room":global.currentScene.get_current_room_id() if current else -1,"relative":vec(global.currentScene.get_player_position_relative_to_current_room()) if current else null})
    global.flag_signals=0
    camera.offset_calls.clear()
func make_scene():
    var scene=load("res://dungeon.gd").new()
    scene.name=config.model.name
    global.currentScene=scene
    var container=Node2D.new()
    container.name="Rooms"
    container.visible=config.proof.container_visible
    scene.add_child(container)
    areas=[]
    for i in config.model.rooms.size():
        var row=config.model.rooms[i]
        var instance=config.proof.native_instances[i]
        var area=load("res://"+config.paths.area).instance()
        area.set_script(load("res://"+config.paths.room_script))
        area.name=row.path.get_file()
        area.position=Vector2(instance.position[0],instance.position[1])
        area.collision_layer=row.layer
        area.collision_mask=row.mask
        area.monitoring=bool(int(row.bits)&1)
        area.monitorable=bool(int(row.bits)&2)
        var shape=area.get_node("CollisionShape2D")
        var rectangle=RectangleShape2D.new()
        rectangle.extents=Vector2(row.extents[0],row.extents[1])
        shape.shape=rectangle
        shape.position=Vector2(instance.shape_position[0],instance.shape_position[1])
        area.camera_offset=Vector2(row.offset[0],row.offset[1])
        container.add_child(area)
        areas.append(area)
    add_child(scene)
    var camera=load("res://camera.gd").new()
    camera.name="CameraService"
    add_child(camera)
    global.currentCamera=camera
    for area in areas:geometry.append({"viewport":viewport_index,"index":area.get_index(),"flag":area._flag,"center":vec(area.get_area_global_position()),"size":vec(area.get_size()),"layer":area.collision_layer,"mask":area.collision_mask,"monitoring":area.monitoring,"monitorable":area.monitorable,"visible":area.is_visible_in_tree()})
func enter(i):areas[i]._on_camarea_body_entered(global.player)
func leave(i):areas[i]._on_camarea_body_exited(global.player)
func run():
    var file=File.new()
    assert(file.open("res://input.json",File.READ)==OK)
    config=JSON.parse(file.get_as_text()).result
    file.close()
    for view in [Vector2(320,180),Vector2(400,240)]:
        get_tree().root.set_size_override(true,view)
        assert(get_viewport().get_visible_rect().size==view)
        global.player.position=Vector2(7,11)
        globaldata.object_flags={config.model.rooms[0].flag:true}
        global.flag_signals=0
        make_scene()
        capture("ready")
        areas[0]._on_camarea_body_entered(self)
        areas[0]._on_camarea_body_exited(self)
        capture("other_body_ignored")
        for i in areas.size():
            enter(i)
            capture("enter/"+str(i))
            yield(get_tree(),"idle_frame")
            capture("idle/"+str(i))
            leave(i)
            capture("exit/"+str(i))
        enter(0)
        capture("overlap_enter0")
        enter(1)
        capture("overlap_enter1")
        yield(get_tree(),"idle_frame")
        capture("overlap_idle")
        leave(1)
        capture("overlap_exit1_keeps_last_limits")
        leave(0)
        capture("overlap_exit0_resets_limits")
        enter(2)
        capture("cancel_enter")
        leave(2)
        capture("cancel_exit")
        yield(get_tree(),"idle_frame")
        capture("cancel_idle")
        enter(0)
        capture("reenter_visited")
        yield(get_tree(),"idle_frame")
        capture("reenter_idle")
        leave(0)
        capture("reenter_exit")
        enter(0)
        leave(0)
        enter(0)
        capture("same_frame_reenter")
        yield(get_tree(),"idle_frame")
        capture("same_frame_reenter_idle_twice")
        leave(0)
        capture("same_frame_reenter_exit")
        global.currentScene.queue_free()
        global.currentCamera.queue_free()
        yield(get_tree(),"idle_frame")
        yield(get_tree(),"idle_frame")
        viewport_index+=1
    assert(file.open("res://observations.json",File.WRITE)==OK)
    file.store_string(JSON.print({"engine":Engine.get_version_info(),"geometry":geometry,"rows":rows},"  "))
    file.close()
    print("DUNGEON_ROOMS_REFERENCE_COMPLETE")
    get_tree().quit()
