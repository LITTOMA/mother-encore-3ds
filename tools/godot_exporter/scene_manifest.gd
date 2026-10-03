# Godot 3.6 diagnostic exporter, NOT a complete scene-to-native converter.
# It reads PackedScene SceneState without instantiating the scene into the tree.
# Inheritance, runtime factories, defaults and signal semantics remain M1 work.
extends SceneTree
var failed = false

func tagged(value):
    match typeof(value):
        TYPE_NIL, TYPE_BOOL, TYPE_INT, TYPE_REAL, TYPE_STRING:
            return value
        TYPE_VECTOR2:
            return {"type": "Vector2", "x": value.x, "y": value.y}
        TYPE_RECT2:
            return {"type": "Rect2", "position": tagged(value.position), "size": tagged(value.size)}
        TYPE_COLOR:
            return {"type": "Color", "r": value.r, "g": value.g, "b": value.b, "a": value.a}
        TYPE_NODE_PATH:
            return {"type": "NodePath", "value": str(value)}
        TYPE_ARRAY:
            var result = []
            for item in value:
                result.append(tagged(item))
            return {"type": "Array", "value": result}
        TYPE_DICTIONARY:
            var pairs = []
            for key in value:
                pairs.append([tagged(key), tagged(value[key])])
            return {"type": "Dictionary", "pairs": pairs}
        TYPE_OBJECT:
            if value is Resource:
                return {"type": "ResourceReference", "path": value.resource_path, "class": value.get_class()}
    push_error("Unsupported Variant type: " + str(typeof(value)))
    failed = true
    return null

func argument(name):
    var args = OS.get_cmdline_args()
    for i in range(args.size() - 1):
        if args[i] == name:
            return args[i + 1]
    return ""

func _init():
    var scene_path = argument("--encore-scene")
    var output = argument("--encore-out")
    if scene_path == "" or output == "":
        printerr("Use --encore-scene res://scene.tscn --encore-out output.json")
        quit(2)
        return
    var scene = ResourceLoader.load(scene_path)
    if not scene is PackedScene:
        printerr("Cannot load PackedScene: " + scene_path)
        quit(1)
        return
    var state = scene.get_state()
    var nodes = []
    for i in range(state.get_node_count()):
        var properties = []
        for j in range(state.get_node_property_count(i)):
            properties.append({"name": state.get_node_property_name(i, j), "value": tagged(state.get_node_property_value(i, j))})
        var instance = state.get_node_instance(i)
        nodes.append({"path": str(state.get_node_path(i)), "name": state.get_node_name(i), "type": state.get_node_type(i),
            "instance": tagged(instance), "properties": properties})
    if failed:
        printerr("Export blocked: unsupported property types; no partial output written")
        quit(1)
        return
    var document = {"schema": 1, "coverage": "raw_scene_state_only", "native_compatible": false,
        "godot": Engine.get_version_info(), "source": scene_path, "nodes": nodes}
    var file = File.new()
    var err = file.open(output, File.WRITE)
    if err != OK:
        printerr("Cannot write output: " + str(err))
        quit(1)
        return
    file.store_string(JSON.print(document, "  ") + "\n")
    file.close()
    print("Diagnostic scene manifest written. This is NOT a playable native content pack.")
    quit(0)
