extends SceneTree

# Scene scripts and signals are quarantined by scene_reference.py. The
# animation resources and property tracks are the original engine data.
func _init():
    var output = ""
    for arg in OS.get_cmdline_args():
        if arg.begins_with("--encore-out="):
            output = arg.substr(13)
    if output.empty():
        printerr("Missing animation reference output")
        quit(1)
        return
    var packed = load("res://Nodes/Reusables/Player.tscn")
    var actor = packed.instance()
    var player = actor.get_node("AnimationPlayer")
    var sprite = actor.get_node("Position/main")
    var special = actor.get_node("SpecialAnimations")
    var samples = []
    var timelines = []
    var sequences = []
    var times = [0.0, 0.0001, 0.083333, 0.1, 0.166667, 0.2, 0.25,
        0.333333, 0.4, 0.416667, 0.5, 0.6, 0.799999, 0.8, 1.0, 1.5,
        2.4, 8.0, 12.9]
    for state in ["Idle", "Walk", "Run", "Crouch"]:
        for direction in ["Down", "Left", "Right", "Up", "DownLeft", "DownRight", "UpLeft", "UpRight"]:
            var name = state + " " + direction
            var anim = player.get_animation(name)
            var keys = []
            # Fail closed before advancing any unreviewed method/property.
            for track in range(anim.get_track_count()):
                var path = str(anim.track_get_path(track))
                if anim.track_get_type(track) != Animation.TYPE_VALUE or not path in ["Position/main:frame", "Position/main:visible", "SpecialAnimations:visible"]:
                    printerr("Unknown animation track: ", name, " ", path)
                    actor.free()
                    quit(1)
                    return
                if path == "Position/main:frame":
                    for key in range(anim.track_get_key_count(track)):
                        keys.append(["%.17f" % anim.track_get_key_time(track, key), anim.track_get_key_value(track, key)])
            timelines.append({"name": name, "length": "%.17f" % anim.length, "loop": anim.loop, "keys": keys})
            for elapsed in times:
                for visible in [false, true]:
                    player.stop(true)
                    sprite.frame = 199
                    sprite.visible = visible
                    special.visible = visible
                    player.play(name)
                    player.advance(0.0)
                    player.advance(elapsed)
                    samples.append({"name": name, "time": elapsed, "initial_visible": visible,
                        "frame": sprite.frame, "main_visible": sprite.visible,
                        "special_visible": special.visible})
            player.stop(true)
            sprite.frame = 199
            sprite.visible = true
            special.visible = true
            player.play(name)
            player.advance(0.0)
            var frames = []
            for tick in range(75):
                var delta = 1.0 / 60.0 if tick < 70 else [0.0, 0.25, 0.8, 0.0, 1.2][tick-70]
                player.advance(delta)
                frames.append({"frame": sprite.frame, "main_visible": sprite.visible,
                    "special_visible": special.visible})
            sequences.append({"name": name, "frames": frames})
    actor.free()
    var file = File.new()
    if file.open(output, File.WRITE) != OK:
        printerr("Cannot write animation reference")
        quit(1)
        return
    file.store_string(JSON.print({"schema": 1, "godot": Engine.get_version_info(),
        "source": "res://Nodes/Reusables/Player.tscn", "samples": samples, "timelines": timelines,
        "sequences": sequences}, "  "))
    file.close()
    print("Original AnimationPlayer: ", samples.size(), " samples")
    quit(0)
