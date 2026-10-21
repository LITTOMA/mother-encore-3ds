extends Node
var config
var bodies=[]
var areas=[]
var source
var rows=[]
var events=[]
func _ready():call_deferred("run")
func shadow(body):
    var s=body.get_node("Shadow")
    return {"mode":body.get_shadow(),"behind":s.show_behind_parent,"playing":s.playing}
func notice(body,index,entered):
    events.append({"area":index,"entered":entered,"body":bodies.find(body),"wanted":global.player.run_sound,"player":shadow(bodies[1]),"party":shadow(bodies[0])})
func settle():
    for _i in 4:yield(get_tree(),"physics_frame")
    yield(get_tree(),"idle_frame")
func capture(ci,phase):
    var overlaps=[]
    for area in areas:
        var active=[]
        for body in bodies:active.append(area.overlaps_body(body))
        overlaps.append(active)
    var positions=[]
    for body in bodies:positions.append({"position":[body.position.x,body.position.y],"layer":body.collision_layer,"mask":body.collision_mask})
    rows.append({"case":ci,"phase":phase,"wanted":global.player.run_sound,"player":shadow(bodies[1]),"party":shadow(bodies[0]),"bodies":positions,"overlaps":overlaps,"events":events.duplicate(true)})
    events.clear()
func geometry():
    var result=[]
    for i in areas.size():
        var area=areas[i]
        var shapes=[]
        for child in area.get_children():
            var points=[]
            if child is CollisionPolygon2D:
                for point in child.polygon:
                    var p=child.global_transform.xform(point)
                    points.append([p.x,p.y])
            else:
                assert(child is CollisionShape2D and child.shape is RectangleShape2D)
                var e=child.shape.extents
                for point in [Vector2(-e.x,-e.y),Vector2(e.x,-e.y),Vector2(e.x,e.y),Vector2(-e.x,e.y)]:
                    var p=child.global_transform.xform(point)
                    points.append([p.x,p.y])
            var parts=[]
            for owner in area.get_shape_owners():
                if area.shape_owner_get_owner(owner)!=child:continue
                for j in area.shape_owner_get_shape_count(owner):
                    var shape=area.shape_owner_get_shape(owner,j)
                    var native=[]
                    if shape is ConvexPolygonShape2D:
                        for point in shape.points:
                            var p=child.global_transform.xform(point)
                            native.append([p.x,p.y])
                    else:
                        assert(shape is RectangleShape2D)
                        native=points.duplicate(true)
                    parts.append(native)
            shapes.append({"path":config.areas[i].path+"/"+child.name,"disabled":child.disabled,"points":points,"parts":parts})
        result.append({"path":config.areas[i].path,"layer":area.collision_layer,"mask":area.collision_mask,"monitoring":area.monitoring,"monitorable":area.monitorable,"enter":area.entering_sound,"exit":area.exiting_sound,"enter_shadow":area.enter_shadow_effect,"exit_shadow":area.exit_shadow_effect,"shapes":shapes})
    return result
func run():
    var file=File.new()
    assert(file.open("res://input.json",File.READ)==OK)
    config=JSON.parse(file.get_as_text()).result
    file.close()
    for i in 3:
        var body=load("res://actor.gd").new() if i==1 else(load("res://party.gd").new() if i==0 else KinematicBody2D.new())
        body.name="Body"+str(i)
        body.collision_layer=config.player.layer
        body.collision_mask=config.player.mask
        var collider=CollisionPolygon2D.new()
        collider.name="CollisionShape2D"
        collider.position=Vector2(config.player.position[0],config.player.position[1])
        var points=PoolVector2Array()
        for point in config.player.points:points.append(Vector2(point[0],point[1]))
        collider.polygon=points
        body.add_child(collider)
        if i!=2:body.add_child(load("res://"+config.shadow_scene).instance())
        body.position=Vector2(20000,20000)
        add_child(body)
        bodies.append(body)
    global.player=bodies[1]
    var original=[]
    for ci in config.cases.size():
        var test=config.cases[ci]
        for body in bodies:
            body.position=Vector2(20000,20000)
            body.collision_layer=config.player.layer
            body.collision_mask=config.player.mask
        bodies[1].run_sound=config.default
        bodies[1].set_shadow(config.shadow)
        bodies[0].set_shadow(config.shadow)
        var body=bodies[test.body]
        body.collision_layer=test.layer
        body.collision_mask=test.mask
        var inside=Vector2(test.point[0]-config.player.center[0],test.point[1]-config.player.center[1])
        if test.initial:body.position=inside
        source=load("res://source.tscn").instance()
        add_child(source)
        areas=[]
        for i in config.areas.size():
            var area=source.get_node(config.areas[i].path)
            area.enabled=test.enabled
            assert(area.connect("body_entered",self,"notice",[i,true])==OK)
            assert(area.connect("body_exited",self,"notice",[i,false])==OK)
            areas.append(area)
        if original.empty():original=geometry()
        events.clear()
        yield(settle(),"completed")
        capture(ci,"initial")
        body.position=inside
        yield(settle(),"completed")
        capture(ci,"inside")
        yield(settle(),"completed")
        capture(ci,"stay")
        for area in areas:area.disable()
        body.position=Vector2(20000,20000)
        yield(settle(),"completed")
        capture(ci,"disabled_exit")
        source.queue_free()
        yield(settle(),"completed")
        areas=[]
        events.clear()
    assert(file.open("res://../contacts.json",File.WRITE)==OK)
    file.store_string(JSON.print({"schema":1,"godot":Engine.get_version_info(),"geometry":original,"rows":rows},"  "))
    file.close()
    for body in bodies:body.queue_free()
    yield(get_tree(),"idle_frame")
    yield(get_tree(),"idle_frame")
    get_tree().quit()
