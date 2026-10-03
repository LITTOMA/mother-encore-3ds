# Godot 3.6.2 native scene DATA exporter. Never enters the scene tree.
# Production input has scripts/signals quarantined by scene_reference.py.
# Original serialized script properties remain in scene_states. This output
# does not approve any gameplay, script defaults, signals or dynamic factories.
extends SceneTree

var failed = false
var resources = []
var resource_ids = {}
var scene_states = []
var seen_scenes = {}
var nodes = []
var root_node

func fail(message):
    printerr("SCENE DATA ERROR: ", message)
    failed = true

func argument(name):
    var args = OS.get_cmdline_args()
    for value in args:
        if value.begins_with(name + "="): return value.substr(name.length() + 1)
    for i in range(args.size() - 1):
        if args[i] == name: return args[i + 1]
    return ""

func tag(value):
    match typeof(value):
        TYPE_NIL, TYPE_BOOL, TYPE_STRING:
            return value
        TYPE_INT:
            # JSON numbers cannot represent all Godot signed 64-bit integers.
            return {"type": "int64", "value": str(value)}
        TYPE_REAL:
            if is_nan(value) or is_inf(value):
                fail("Non-finite real")
                return null
            # Physics adapters must opt in to a round-trippable decimal form:
            # Godot 3 JSON.print otherwise truncates real precision.
            if argument("--encore-exact-reals") == "true":
                return {"type": "real", "value": "%.17f" % value}
            return value
        TYPE_VECTOR2:
            return {"type": "Vector2", "x": tag(value.x), "y": tag(value.y)}
        TYPE_VECTOR3:
            return {"type": "Vector3", "x": tag(value.x), "y": tag(value.y), "z": tag(value.z)}
        TYPE_RECT2:
            return {"type": "Rect2", "position": tag(value.position), "size": tag(value.size)}
        TYPE_TRANSFORM2D:
            return {"type": "Transform2D", "x": tag(value.x), "y": tag(value.y), "origin": tag(value.origin)}
        TYPE_COLOR:
            return {"type": "Color", "r": tag(value.r), "g": tag(value.g), "b": tag(value.b), "a": tag(value.a)}
        TYPE_NODE_PATH:
            return {"type": "NodePath", "value": str(value)}
        TYPE_ARRAY, TYPE_RAW_ARRAY, TYPE_INT_ARRAY, TYPE_REAL_ARRAY, TYPE_STRING_ARRAY, TYPE_VECTOR2_ARRAY, TYPE_VECTOR3_ARRAY, TYPE_COLOR_ARRAY:
            var entries = []
            for entry in value: entries.append(tag(entry))
            var names = {TYPE_ARRAY: "Array", TYPE_RAW_ARRAY: "PoolByteArray", TYPE_INT_ARRAY: "PoolIntArray",
                TYPE_REAL_ARRAY: "PoolRealArray", TYPE_STRING_ARRAY: "PoolStringArray",
                TYPE_VECTOR2_ARRAY: "PoolVector2Array", TYPE_VECTOR3_ARRAY: "PoolVector3Array", TYPE_COLOR_ARRAY: "PoolColorArray"}
            return {"type": names[typeof(value)], "value": entries}
        TYPE_DICTIONARY:
            var pairs = []
            for key in value: pairs.append([tag(key), tag(value[key])])
            return {"type": "Dictionary", "pairs": pairs}
        TYPE_OBJECT:
            if value == null: return null
            if value is Resource: return resource(value)
            if value is Node and root_node != null and (value == root_node or root_node.is_a_parent_of(value)):
                return {"type": "NodeReference", "path": str(root_node.get_path_to(value))}
    fail("Unsupported Variant type " + str(typeof(value)))
    return null

func properties(object):
    var result = {}
    for item in object.get_property_list():
        if item.usage & PROPERTY_USAGE_STORAGE:
            result[item.name] = tag(object.get(item.name))
    return result

func resource(value):
    var identity = value.get_instance_id()
    if resource_ids.has(identity):
        return {"type": "ResourceReference", "id": resource_ids[identity]}
    var index = resources.size()
    resource_ids[identity] = index
    # Reserve before traversing cyclic/shared resource graphs. IDs are local
    # references only; persistent identity must use an audited source registry.
    var result = {"id": index, "class": value.get_class(), "path": value.resource_path}
    resources.append(result)
    if value is Texture or value is AudioStream or value is Font:
        result["payload"] = "external codec; original bytes tracked in source.json"
        if value is Texture:
            result["size"] = tag(value.get_size())
    elif value is PackedScene:
        collect_state(value)
    elif value is Script:
        # Only isolated test fixtures may retain scripts. Do not execute them.
        result["source_code"] = value.source_code
    else:
        result["properties"] = properties(value)
    return {"type": "ResourceReference", "id": index}

func collect_state(scene):
    var key = scene.get_instance_id()
    if seen_scenes.has(key): return
    seen_scenes[key] = true
    var state = scene.get_state()
    var result = {"source": scene.resource_path, "nodes": [], "connections": []}
    scene_states.append(result)
    for i in range(state.get_node_count()):
        if state.is_node_instance_placeholder(i):
            fail("InstancePlaceholder requires a reviewed runtime loader")
        var props = {}
        for j in range(state.get_node_property_count(i)):
            props[state.get_node_property_name(i, j)] = tag(state.get_node_property_value(i, j))
        var child = state.get_node_instance(i)
        result.nodes.append({"path": str(state.get_node_path(i)), "class": state.get_node_type(i),
            "name": state.get_node_name(i), "owner": str(state.get_node_owner_path(i)),
            "sibling_index": state.get_node_index(i), "groups": tag(state.get_node_groups(i)),
            "instance": tag(child), "properties": props})
    for i in range(state.get_connection_count()):
        result.connections.append({"source": str(state.get_connection_source(i)), "target": str(state.get_connection_target(i)),
            "signal": state.get_connection_signal(i), "method": state.get_connection_method(i),
            "flags": state.get_connection_flags(i), "binds": tag(state.get_connection_binds(i))})

func visit(node):
    var result = {"path": str(root_node.get_path_to(node)), "name": node.name, "class": node.get_class(),
        "properties": properties(node)}
    if node is Node2D: result["world_transform"] = tag(node.global_transform)
    if node is CollisionObject2D:
        var owners = []
        for owner_id in node.get_shape_owners():
            var owner_node = node.shape_owner_get_owner(owner_id)
            if not (owner_node is CollisionShape2D or owner_node is CollisionPolygon2D) or owner_node.get_parent() != node:
                fail("Unreviewed native collision owner structure")
                continue
            var shapes = []
            for i in range(node.shape_owner_get_shape_count(owner_id)):
                shapes.append(tag(node.shape_owner_get_shape(owner_id, i)))
            # Godot updates an owner's transform on entering the tree. Outside
            # it, the cached owner can still contain the base scene's transform
            # after an instance override. Preserve that cache as evidence and
            # export the fully resolved child transform for geometry adapters.
            owners.append({"owner": tag(owner_node), "transform": tag(owner_node.transform),
                "cached_transform_before_enter_tree": tag(node.shape_owner_get_transform(owner_id)),
                "disabled": node.is_shape_owner_disabled(owner_id),
                "one_way": node.is_shape_owner_one_way_collision_enabled(owner_id),
                "one_way_margin": tag(node.get_shape_owner_one_way_collision_margin(owner_id)), "shapes": shapes})
        result["physics_shape_owners"] = owners
    if node is TileMap:
        var cells = []
        for pos in node.get_used_cells():
            cells.append({"position": tag(pos), "tile": node.get_cellv(pos),
                "autotile": tag(node.get_cell_autotile_coord(pos.x, pos.y)),
                "flip_x": node.is_cell_x_flipped(pos.x, pos.y), "flip_y": node.is_cell_y_flipped(pos.x, pos.y),
                "transpose": node.is_cell_transposed(pos.x, pos.y), "local_origin": tag(node.map_to_world(pos))})
        result["cells"] = cells
    nodes.append(result)
    for child in node.get_children(): visit(child)

func _init():
    var source = argument("--encore-scene")
    var output = argument("--encore-out")
    if source == "" or output == "":
        fail("Missing --encore-scene / --encore-out")
        quit(2)
        return
    var scene = ResourceLoader.load(source)
    if not scene is PackedScene:
        fail("Cannot load PackedScene: " + source)
        quit(1)
        return
    collect_state(scene)
    root_node = scene.instance()
    if root_node == null:
        fail("Cannot instantiate scene data")
        quit(1)
        return
    visit(root_node)
    var document = {"schema": 1, "coverage": "native_instantiated_data_with_raw_scene_states",
        "native_compatible": false, "godot": Engine.get_version_info(), "source": source,
        "nodes": nodes, "resources": resources, "scene_states": scene_states,
        "not_executed": ["_ready", "_process", "_physics_process", "animation playback"],
        "blockers": ["script mechanism/default bindings", "runtime factories", "signals", "method tracks"]}
    root_node.free()
    root_node = null
    if failed:
        printerr("Export blocked; no partial output written")
        quit(1)
        return
    var file = File.new()
    if file.open(output, File.WRITE) != OK:
        fail("Cannot write output")
        quit(1)
        return
    file.store_string(JSON.print(document, "  ") + "\n")
    file.close()
    print("Scene DATA: ", nodes.size(), " native nodes, ", resources.size(), " resources, ", scene_states.size(), " scene states. Gameplay remains blocked.")
    quit(0)
