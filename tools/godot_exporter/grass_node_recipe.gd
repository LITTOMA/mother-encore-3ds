# Godot3.6.2 source DATA only: never enter scene, execute source scripts or Ready.
extends SceneTree
var rows = []
var root_node
func vector(p): return ["%.17f" % p.x, "%.17f" % p.y]
func matrix(t): return [vector(t.x), vector(t.y), vector(t.origin)]
func relative(n): return str(root_node.get_path_to(n)) if n != null else ""
func visit(n):
    var owner = n.get_owner()
    var r = {"path": relative(n), "name": str(n.name), "class": n.get_class(), "parent": relative(n.get_parent()), "owner": relative(owner), "index": n.get_index(), "groups": Array(n.get_groups()), "unique": n.is_unique_name_in_owner(), "pause": n.pause_mode, "priority": n.process_priority, "canvas": n is CanvasItem}
    if n is CanvasItem:
        var canvas_parent = n.get_parent() if (not n.is_set_as_toplevel()) and n.get_parent() is CanvasItem else null
        r["canvas_parent"] = relative(canvas_parent)
        r["top_level"] = n.is_set_as_toplevel()
        r["local"] = matrix(n.get_transform())
        r["world"] = matrix(n.get_global_transform())
        r["visible"] = n.visible
        r["modulate"] = ["%.17f" % n.modulate.r, "%.17f" % n.modulate.g, "%.17f" % n.modulate.b, "%.17f" % n.modulate.a]
        r["self_modulate"] = ["%.17f" % n.self_modulate.r, "%.17f" % n.self_modulate.g, "%.17f" % n.self_modulate.b, "%.17f" % n.self_modulate.a]
        r["behind"] = n.show_behind_parent
        r["light_mask"] = n.light_mask
        r["use_parent_material"] = n.use_parent_material
        r["material"] = n.material != null
        r["z"] = n.z_index if n is Node2D else 0
        r["z_relative"] = n.z_as_relative if n is Node2D else true
        r["y_sort"] = n.sort_enabled if n is YSort else false
        r["notify_transform"] = n.is_transform_notification_enabled()
        r["notify_local_transform"] = n.is_local_transform_notification_enabled()
    if n is CanvasLayer:
        r["canvas_layer"] = {"layer": n.layer, "follow_viewport": n.follow_viewport_enable, "follow_scale": "%.17f" % n.follow_viewport_scale, "transform": matrix(n.transform), "offset": vector(n.offset), "rotation": "%.17f" % n.rotation, "scale": vector(n.scale), "custom_viewport": n.custom_viewport != null}
    if n is Control:
        r["control"] = {"anchors": ["%.17f" % n.anchor_left, "%.17f" % n.anchor_top, "%.17f" % n.anchor_right, "%.17f" % n.anchor_bottom], "margins": ["%.17f" % n.margin_left, "%.17f" % n.margin_top, "%.17f" % n.margin_right, "%.17f" % n.margin_bottom], "position": vector(n.rect_position), "size": vector(n.rect_size), "scale": vector(n.rect_scale), "rotation": "%.17f" % n.rect_rotation, "pivot": vector(n.rect_pivot_offset), "min_size": vector(n.rect_min_size), "grow": [n.grow_horizontal, n.grow_vertical], "size_flags": [n.size_flags_horizontal, n.size_flags_vertical], "stretch": "%.17f" % n.size_flags_stretch_ratio, "clip": n.rect_clip_content, "mouse": n.mouse_filter, "focus": n.focus_mode}
    rows.append(r)
    for child in n.get_children(): visit(child)
func _init():
    root_node = load("res://Nodes/Overworld/Grass/grass.tscn").instance()
    if root_node == null:
        quit(1)
        return
    visit(root_node)
    var f = File.new()
    if f.open("res://field_node_recipe.json", File.WRITE) != OK:
        root_node.free()
        quit(2)
        return
    f.store_string(JSON.print({"schema": 1, "scene": "Nodes/Overworld/Grass/grass.tscn", "engine": Engine.get_version_info(), "scene_entered": false, "nodes": rows}, "  ") + "\n")
    f.close()
    print("FIELD NODE TREE DATA: ", rows.size(), " actual nodes; no scene enter/Ready")
    root_node.free()
    quit(0)
