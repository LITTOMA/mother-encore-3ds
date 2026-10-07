# Original audioManager native DATA and source AudioStreamPlayer.new defaults.
# Project scripts/signals are quarantined. Never add scene to SceneTree.
extends "res://scene_data.gd"
func _init():
    # SceneTree derived _init runs after the reusable exporter parent _init.
    # Start this independent data document with fresh collection identities.
    nodes.clear()
    resources.clear()
    resource_ids.clear()
    scene_states.clear()
    seen_scenes.clear()
    var source = argument("--encore-scene")
    var output = argument("--encore-out")
    var packed = load(source)
    if not packed is PackedScene:
        quit(1)
        return
    collect_state(packed)
    root_node = packed.instance()
    visit(root_node)
    var voice = AudioStreamPlayer.new()
    var defaults = properties(voice)
    voice.free()
    var paths_file = File.new()
    if paths_file.open("res://sound_paths.json",File.READ) != OK:
        quit(2)
        return
    var paths = JSON.parse(paths_file.get_as_text()).result
    paths_file.close()
    var streams = []
    for path in paths:
        var stream = load("res://" + path)
        if stream == null:
            failed = true
        else:
            var metadata = properties(stream)
            metadata.erase("data")
            var filename = "stream-" + str(streams.size()) + ".bin"
            var payload = stream.get_data()
            var bytes_file = File.new()
            if bytes_file.open("res://" + filename,File.WRITE) != OK:
                failed = true
            else:
                bytes_file.store_buffer(payload)
                bytes_file.close()
            streams.append({"source": path,"resource": tag(stream),"native": stream.get_class(),"metadata": metadata,"payload": filename,"payload_bytes": payload.size()})
    var document = {"schema":1,"coverage":"native_instantiated_data_with_raw_scene_states","native_compatible":false,"godot":Engine.get_version_info(),"source":source,"nodes":nodes,"resources":resources,"scene_states":scene_states,"voice_defaults":defaults,"streams":streams,"not_executed":["source constructors","_ready","_process","playback"]}
    root_node.free()
    root_node = null
    if failed:
        quit(3)
        return
    var f = File.new()
    if f.open(output,File.WRITE) != OK:
        quit(4)
        return
    f.store_string(JSON.print(document,"  ")+"\n")
    f.close()
    quit(0)
