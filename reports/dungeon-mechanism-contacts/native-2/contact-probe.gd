extends Node
var config
var rows=[]
var scene
var switches={}
var objects={}
func _ready():call_deferred("run")
func read_config():
    var file=File.new()
    assert(file.open("res://input.json",File.READ)==OK)
    config=JSON.parse(file.get_as_text()).result
    file.close()

func parent_for(path):
    var parent_path=path.get_base_dir()
    if scene.has_node(parent_path):return scene.get_node(parent_path)
    var parent=Node2D.new()
    parent.name=parent_path
    scene.add_child(parent)
    return parent

signal flushed
var animators=[]
var timeline_events=[]
func event(value):timeline_events.append(value)
func completed(clip,path):timeline_events.append({"kind":"finished","path":path,"clip":clip})
func barrier():emit_signal("flushed")
func setup(target,path):
    var player=target.get_node("AnimationPlayer")
    player.playback_process_mode=AnimationPlayer.ANIMATION_PROCESS_MANUAL
    assert(player.method_call_mode==AnimationPlayer.ANIMATION_METHOD_CALL_DEFERRED)
    player.connect("animation_finished",self,"completed",[path])
    animators.append(player)
    var audio=target.get_node_or_null("AudioStreamPlayer")
    if audio and audio.stream:
        var policy=config.audio[audio.stream.resource_path]
        if audio.stream is AudioStreamMP3:
            audio.stream.loop=policy.loop
            audio.stream.loop_offset=policy.offset
    if target.name==config.wall.get_file():
        audio.set_script(load("res://logged-audio.gd"))
        audio.runner=self
func advance_frames(count,label):
    for index in count:
        for player in animators:
            if player.is_playing():player.advance(1.0/60)
        call_deferred("barrier")
        yield(self,"flushed")
        capture(label+"/"+str(index))

func make_scene():
    scene=load("res://scene.gd").new()
    scene.name="Catacombs"
    global.currentScene=scene
    var music=Node.new()
    music.name="Music"
    scene.add_child(music)
    var area=load("res://music.gd").new()
    area.name="MusicArea"
    music.add_child(area)
    switches.clear()
    objects.clear()
    # Keep the actual source child order. Registration in _enter_tree runs
    # before controlled objects' _ready, including later siblings.
    for row in config.nodes:
        var value
        var target
        if row.kind=="lever" or row.kind=="plate":
            value=config.switches[row.path]
            target=load("res://"+config.paths[row.kind]).instance()
            target.name=row.path.get_file()
            target.position=Vector2(value.x,value.y)
            target.flag=value.flag if not (int(value.bits)&1) else ""
            target.is_object_flag=bool(int(value.bits)&1)
            target.emit_flag_updated_signal=bool(int(value.bits)&2)
            target.reset_when_leaving_area=bool(int(value.bits)&4)
            target.reset_when_leaving_region=bool(int(value.bits)&8)
            target._is_one_way=bool(int(value.bits)&16)
            target._pause_when_changed=bool(int(value.bits)&32)
            target._focus_objects_when_changed=bool(int(value.bits)&64)
            target._camera_delay=value.camera
            target._controlled_objects=[]
            for reference in config.references[row.path]:target._controlled_objects.append(NodePath(reference))
            setup(target,row.path)
            switches[row.path]=target
        else:
            value=config.objects[row.path]
            target=load("res://"+config.paths[row.kind]).instance()
            target.name=row.path.get_file()
            target.position=Vector2(value.x,value.y)
            target._initially_on=bool(int(value.bits)&1)
            target._move_cam_when_changed=bool(int(value.bits)&2)
            target._move_cam_offset=Vector2(value.offset_x,value.offset_y)
            target._update_order=int(value.order)
            setup(target,row.path)
            objects[row.path]=target
        parent_for(row.path).add_child(target)
    switches[config.main].connect("switch_hit",scene,"_on_main_switch_hit")
    objects[config.wall].connect("state_changed",scene,"_on_wall_door_state_changed")
    add_child(scene)

func capture(action):
    var nodes=[]
    for definition in config.nodes:
        var target=switches[definition.path] if switches.has(definition.path) else objects[definition.path]
        var player=target.get_node("AnimationPlayer")
        var sprite=target.get_node("Sprite" if definition.kind=="plate" else "main")
        var body=target.get_node_or_null("collision/CollisionShape2D") if definition.kind!="plate" else null
        nodes.append({"path":definition.path,"frame":sprite.frame,"body":not body.disabled if body else false,"assigned":player.assigned_animation,"playing":player.is_playing(),"position":"%.17f"%player.current_animation_position,"on":target.is_on() if switches.has(definition.path) else target._get_state()})
    rows.append({"action":action,"nodes":nodes,"paused":global.player.paused,"cutscene":uiManager.cutscene,"player_events":global.player.events.duplicate(),"ui_events":uiManager.events.duplicate(),"audio_events":audioManager.events.duplicate(true),"camera_events":global.currentCamera.events.duplicate(true),"timeline_events":timeline_events.duplicate(true)})
    global.player.events.clear()
    uiManager.events.clear()
    audioManager.events.clear()
    global.currentCamera.events.clear()
    timeline_events.clear()

func run():
    read_config()
    make_player_shapes()
    var bat=global.player.get_node("ProbePivot/Bat")
    var bat_shape=bat.get_node("CollisionShape2D")
    var other=global.get_node("ProbeOtherBody")
    for test in config.contact_cases:
        globaldata._init_flags()
        globaldata.object_flags.clear()
        global.player.paused=false
        global.player.position=Vector2(10000,10000)
        other.position=Vector2(10000,10000)
        global.player.collision_layer=config.contact_player.layer
        global.player.collision_mask=config.contact_player.mask
        bat.collision_layer=2
        bat.collision_mask=2
        bat.monitorable=true
        bat_shape.disabled=true
        make_scene()
        scene.can_switch=test.can_switch
        for definition in config.nodes:
            if not switches.has(definition.path):continue
            var target=switches[definition.path]
            target.connect("switch_hit",self,"hit",[definition.path])
            var area=target.get_node("interact" if definition.kind=="lever" else "Area2D")
            if definition.kind=="lever":area.connect("area_entered",self,"area_entered",[definition.path])
            else:area.connect("body_entered",self,"body_entered",[definition.path])
        if test.wait_ready:yield(get_tree().create_timer(0.25),"timeout")
        contact_events.clear()
        audioManager.events.clear()
        var target=scene.get_node(test.target)
        var monitor=target.get_node("interact/CollisionShape2D2" if test.kind=="bat" else "Area2D/CollisionShape2D")
        for phase in test.phases:
            global.player.paused=phase.paused
            global.player.position=Vector2(10000,10000)
            other.position=Vector2(10000,10000)
            bat_shape.disabled=not phase.enabled
            bat.collision_layer=phase.layer
            bat.collision_mask=phase.mask
            bat.monitorable=phase.monitorable
            global.player.collision_layer=phase.body_layer
            global.player.collision_mask=phase.body_mask
            if phase.inside:
                if test.kind=="bat":global.player.global_position+=monitor.global_position-bat_shape.global_position
                elif phase.other:other.global_position=monitor.global_position-Vector2(-0.5,6)
                else:global.player.global_position=monitor.global_position-Vector2(-0.5,6)
            if phase.delay>0:yield(get_tree().create_timer(phase.delay),"timeout")
            yield(settle(),"completed")
            capture_contact(test.name,phase.name)
        scene.queue_free()
        yield(get_tree(),"idle_frame")
        yield(get_tree(),"idle_frame")
    var file=File.new()
    assert(file.open("res://contacts.json",File.WRITE)==OK)
    file.store_string(JSON.print({"engine":Engine.get_version_info(),"rows":rows},"  "))
    file.close()
    print("DUNGEON_CONTACT_REFERENCE_COMPLETE")
    get_tree().quit()

var contact_events=[]
func observed(value,path):
    contact_events.append({"kind":value,"path":path})
func area_entered(other,path):observed("area_entered",path)
func body_entered(other,path):observed("player_entered" if other==global.player else "other_entered",path)
func hit(path):observed("switch_hit",path)
func body_shape(body):
    var shape=CollisionPolygon2D.new()
    var points=PoolVector2Array()
    for point in config.contact_player.points:points.append(Vector2(point[0],point[1]))
    shape.polygon=points
    shape.position=Vector2(config.contact_player.position[0],config.contact_player.position[1])
    body.add_child(shape)
func make_player_shapes():
    global.player.collision_layer=config.contact_player.layer
    global.player.collision_mask=config.contact_player.mask
    body_shape(global.player)
    var p=config.contact_swing
    var pivot=Node2D.new()
    pivot.name="ProbePivot"
    pivot.position=Vector2(p[4],p[5])
    pivot.rotation=p[6]
    global.player.add_child(pivot)
    var area=load("res://"+config.contact_hitbox).instance()
    area.name="Bat"
    area.position=Vector2(p[7],p[8])
    area.rotation=p[9]
    pivot.add_child(area)
    var shape=area.get_node("CollisionShape2D")
    var capsule=CapsuleShape2D.new()
    capsule.radius=p[13]
    capsule.height=p[14]
    shape.shape=capsule
    shape.position=Vector2(p[10],p[11])
    shape.rotation=p[12]
    shape.disabled=true
    var other=KinematicBody2D.new()
    other.name="ProbeOtherBody"
    other.collision_layer=config.contact_player.layer
    other.collision_mask=config.contact_player.mask
    body_shape(other)
    global.add_child(other)
func capture_contact(case_name,phase):
    var states=[]
    for definition in config.nodes:
        if switches.has(definition.path):
            var target=switches[definition.path]
            states.append({"path":definition.path,"on":target.is_on(),"ready":target._is_ready_to_interact,"disabled":target._is_disabled})
    rows.append({"case":case_name,"phase":phase,"events":contact_events.duplicate(true),"states":states,"paused":global.player.paused,"denied":audioManager.events.duplicate(true)})
    contact_events.clear()
    audioManager.events.clear()
func settle():
    for _i in 4:yield(get_tree(),"physics_frame")
    yield(get_tree(),"idle_frame")
