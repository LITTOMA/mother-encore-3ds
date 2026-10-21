extends Node
func _ready():call_deferred("run")
func bits(value):
    var bytes=StreamPeerBuffer.new()
    bytes.put_float(value)
    bytes.seek(0)
    return bytes.get_u32()
func vec(value):return [bits(value.x),bits(value.y)]
func rect(value):return vec(value.position)+vec(value.size)
func run():
    var file=File.new()
    assert(file.open("res://input.json",File.READ)==OK)
    var config=JSON.parse(file.get_as_text()).result
    file.close()
    var player=load("res://party.gd").new()
    add_child(player)
    global.player=player
    var actor=load("res://geyser.tscn").instance()
    add_child(actor)
    actor.pause()
    var sprite=actor.get_node("Sprite")
    var defaults={"root":vec(actor.position),"sprite":vec(sprite.position),"offset":vec(sprite.offset),"scale":vec(sprite.scale),"rotation":bits(sprite.rotation),"centered":sprite.centered,"flip_h":sprite.flip_h,"flip_v":sprite.flip_v,"region":sprite.region_enabled,"visible":sprite.visible,"behind":sprite.show_behind_parent,"z":sprite.z_index,"color":[bits(sprite.modulate.r),bits(sprite.modulate.g),bits(sprite.modulate.b),bits(sprite.modulate.a)],"hframes":sprite.hframes,"vframes":sprite.vframes,"size":vec(sprite.texture.get_size()),"pixel_snap":ProjectSettings.get_setting("rendering/2d/snapping/use_gpu_pixel_snap")}
    var frames=[]
    for frame in sprite.hframes*sprite.vframes:
        sprite.frame=frame
        frames.append({"frame":sprite.frame,"coords":vec(sprite.frame_coords),"rect":rect(sprite.get_rect())})
    var positions=[]
    for point in config.positions:
        actor.position=Vector2(point[0],point[1])
        var local=sprite.get_rect()
        positions.append({"root":vec(actor.position),"global":vec(sprite.global_position),"top_left":vec(sprite.to_global(local.position)),"basis":vec(sprite.global_transform.x)+vec(sprite.global_transform.y)})
    assert(file.open("res://../reference.json",File.WRITE)==OK)
    file.store_string(JSON.print({"schema":1,"godot":Engine.get_version_info(),"defaults":defaults,"frames":frames,"positions":positions},"  "))
    file.close()
    actor.queue_free()
    get_tree().quit()
