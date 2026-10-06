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
    rows.append(r)
    for child in n.get_children(): visit(child)
func _init():
    root_node = load("res://Maps/podunk/podunk.tscn").instance()
    if root_node == null:
        quit(1)
        return
    visit(root_node)
    var f = File.new()
    if f.open("res://field_node_tree.json", File.WRITE) != OK:
        root_node.free()
        quit(2)
        return
    f.store_string(JSON.print({"schema": 1, "scene": "Maps/podunk/podunk.tscn", "engine": Engine.get_version_info(), "scene_entered": false, "nodes": rows}, "  ") + "\n")
    f.close()
    print("FIELD NODE TREE DATA: ", rows.size(), " actual nodes; no scene enter/Ready")
    root_node.free()
    quit(0)
