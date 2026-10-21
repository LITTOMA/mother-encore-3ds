extends Node
var config
var body
var sprite
var rows=[]
var tick=0
var initial
var warmed=false
func bits(value):
    var bytes=StreamPeerBuffer.new()
    bytes.big_endian=false
    bytes.put_float(value)
    bytes.seek(0)
    return bytes.get_u32()
func operations(index):
    for action in config.timeline[index]:
        match action[0]:
            "mode":body.set_shadow(action[1])
            "frame":sprite.frame=int(action[1])
            "visible":sprite.visible=action[1]
            "player_pause":body._paused=action[1]
            "tree_pause":get_tree().paused=action[1]
            "position":body.position=Vector2(action[1][0],action[1][1])
            _:assert(false)
func _ready():
    pause_mode=Node.PAUSE_MODE_PROCESS
    var file=File.new()
    assert(file.open("res://input.json",File.READ)==OK)
    config=JSON.parse(file.get_as_text()).result
    file.close()
    body=load("res://source.tscn").instance()
    body.pause_mode=Node.PAUSE_MODE_STOP
    body.position=Vector2(128.25,-40.5)
    add_child(body)
    sprite=body.get_node("Shadow")
    initial={"position":[sprite.position.x,sprite.position.y],"offset":[sprite.offset.x,sprite.offset.y],"centered":sprite.centered,"flip_h":sprite.flip_h,"flip_v":sprite.flip_v,"visible":sprite.visible,"behind":sprite.show_behind_parent,"playing":sprite.playing,"speed_scale":sprite.speed_scale,"frame":sprite.frame,"animation":sprite.animation,"gpu_snap":ProjectSettings.get_setting("rendering/2d/snapping/use_gpu_pixel_snap"),"modulate":[sprite.modulate.r,sprite.modulate.g,sprite.modulate.b,sprite.modulate.a],"self_modulate":[sprite.self_modulate.r,sprite.self_modulate.g,sprite.self_modulate.b,sprite.self_modulate.a],"physics_fps":Engine.iterations_per_second,"jitter_fix":Engine.physics_jitter_fix}
func _process(_delta):
    # Start the input sequence after one complete native processing pass. Its
    # mode change resets the timer before the first recorded clock step.
    if not warmed:
        operations(0)
        warmed=true
        return
    var texture=sprite.frames.get_frame(sprite.animation,sprite.frame)
    assert(texture is AtlasTexture)
    rows.append({"tick":tick,"actions":config.timeline[tick],"delta_bits":bits(sprite.get_process_delta_time()),"mode":body.get_shadow(),"frame":sprite.frame,"playing":sprite.playing,"behind":sprite.show_behind_parent,"visible":sprite.is_visible_in_tree(),"process_enabled":sprite.can_process(),"render_pending":not config.no_draw,"player_pause":body._paused,"tree_pause":get_tree().paused,"body_position":[body.position.x,body.position.y],"shadow_global":[sprite.global_position.x,sprite.global_position.y],"texture":texture.atlas.resource_path,"region":[texture.region.position.x,texture.region.position.y,texture.region.size.x,texture.region.size.y],"drawn_frames":Engine.get_frames_drawn()})
    tick+=1
    if tick==config.timeline.size():
        var file=File.new()
        assert(file.open("res://../reference.json",File.WRITE)==OK)
        file.store_string(JSON.print({"schema":1,"godot":Engine.get_version_info(),"fps":config.fps,"no_draw":config.no_draw,"initial":initial,"rows":rows},"  "))
        file.close()
        get_tree().paused=false
        get_tree().quit()
        return
    operations(tick)
