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
    globaldata._init_flags()
    make_scene()
    capture("ready")
    yield(advance_frames(2,"initial"),"completed")
    yield(get_tree().create_timer(0.25),"timeout")
    capture("ready_timers_complete")
    Engine.time_scale=0
    switches[config.main].interact()
    capture("main_interact")
    yield(advance_frames(10,"focus_pending"),"completed")
    global.currentCamera.complete()
    capture("focus_complete")
    yield(advance_frames(145,"opening"),"completed")
    capture("wall_timer_pending")
    Engine.time_scale=1
    yield(get_tree().create_timer(0.6),"timeout")
    capture("wall_timer_complete_before_camera_return")
    global.currentCamera.complete()
    capture("camera_return_complete")
    switches[config.main].interact()
    capture("one_way_repeat")
    switches["Bottom2/switch1"].interact()
    capture("other_switch_on")
    Engine.time_scale=0
    yield(advance_frames(65,"other_switch_animations"),"completed")
    scene.leave(false)
    capture("leave_resets_flag_only")
    Engine.time_scale=1
    scene.queue_free()
    yield(get_tree(),"idle_frame")
    yield(get_tree(),"idle_frame")
    var file=File.new()
    assert(file.open("res://observations.json",File.WRITE)==OK)
    file.store_string(JSON.print({"engine":Engine.get_version_info(),"rows":rows},"  "))
    file.close()
    print("DUNGEON_MECHANISM_TIMELINE_REFERENCE_COMPLETE")
    get_tree().quit()
