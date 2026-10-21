extends Node
func vec(v):return [v.x,v.y]
func rect(v):return [v.position.x,v.position.y,v.size.x,v.size.y]
func xf(v):return [v.x.x,v.x.y,v.y.x,v.y.y,v.origin.x,v.origin.y]
func points(p):
    var out=[]
    for v in p:out.append(vec(v))
    return out
func _ready():call_deferred("run")
func run():
    var terrain=load("res://terrain-source.tscn").instance()
    add_child(terrain)
    for _i in range(4):yield(get_tree(),"physics_frame")
    var ts=load("res://Tilesets/Catacombs.tres")
    var script=ts.get_script()
    var script_info={"present":script!=null}
    if script:
        script_info={"present":true,"class":script.get_class(),"source_code":script.source_code,"has_source":script.has_source_code(),"can_instance":script.can_instance(),"base":script.get_instance_base_type(),"methods":script.get_script_method_list(),"properties":script.get_script_property_list(),"signals":script.get_script_signal_list()}
    var ids=ts.get_tiles_ids()
    ids.sort()
    var tiles=[]
    for id in ids:
        var navigation=ts.tile_get_navigation_polygon(id)
        var nav=null
        if navigation:
            var polys=[]
            for j in navigation.get_polygon_count():polys.append(Array(navigation.get_polygon(j)))
            nav={"vertices":points(navigation.get_vertices()),"polygons":polys,"offset":vec(ts.tile_get_navigation_polygon_offset(id))}
        var shapes=[]
        for row in ts.tile_get_shapes(id):
            var shape=row.shape
            var geometry={"class":shape.get_class()}
            if shape is RectangleShape2D:geometry["extents"]=vec(shape.extents)
            elif shape is ConvexPolygonShape2D:geometry["points"]=points(shape.points)
            elif shape is ConcavePolygonShape2D:geometry["segments"]=points(shape.segments)
            else:
                push_error("Unreviewed native tile shape "+shape.get_class())
                get_tree().quit(4)
                return
            shapes.append({"autotile_coord":vec(row.autotile_coord),"one_way":row.one_way,"margin":row.one_way_margin,"transform":xf(row.shape_transform),"shape":geometry})
        var texture=ts.tile_get_texture(id)
        var info={"path":texture.resource_path,"class":texture.get_class(),"size":[texture.get_width(),texture.get_height()]}
        if texture is AnimatedTexture:
            var frames=[]
            for j in texture.frames:frames.append({"path":texture.get_frame_texture(j).resource_path,"delay":texture.get_frame_delay(j)})
            info["animation"]={"fps":texture.fps,"pause":texture.pause,"oneshot":texture.oneshot,"frames":frames}
        tiles.append({"id":id,"name":ts.tile_get_name(id),"region":rect(ts.tile_get_region(id)),"offset":vec(ts.tile_get_texture_offset(id)),"z":ts.tile_get_z_index(id),"mode":ts.tile_get_tile_mode(id),"shapes":shapes,"navigation":nav,"texture":info})
    var layers=[]
    for map in terrain.get_children():
        var cells=[]
        for p in map.get_used_cells():
            var id=map.get_cell(int(p.x),int(p.y))
            cells.append([int(p.x),int(p.y),id,map.is_cell_x_flipped(int(p.x),int(p.y)),map.is_cell_y_flipped(int(p.x),int(p.y)),map.is_cell_transposed(int(p.x),int(p.y)),int(map.get_cell_autotile_coord(int(p.x),int(p.y)).x),int(map.get_cell_autotile_coord(int(p.x),int(p.y)).y),ids.has(id)])
        cells.sort_custom(self,"cell_before")
        layers.append({"name":map.name,"used_rect":rect(map.get_used_rect()),"position":vec(map.position),"cell_size":vec(map.cell_size),"origin":map.cell_tile_origin,"ysort":map.cell_y_sort,"layer":map.collision_layer,"mask":map.collision_mask,"cells":cells})
    var f=File.new()
    if f.open("res://../terrain.json",File.WRITE)!=OK:
        get_tree().quit(2)
        return
    f.store_string(JSON.print({"engine":Engine.get_version_info(),"script":script_info,"tiles":tiles,"layers":layers},"  "))
    f.close()
    print("TERRAIN_DIAGNOSTIC_NATIVE_COMPLETE layers=",layers.size()," tiles=",tiles.size())
    get_tree().quit(0)
func cell_before(a,b):return a[1]<b[1] or (a[1]==b[1] and a[0]<b[0])
