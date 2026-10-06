tool
extends EditorPlugin
var source_importer
func _enter_tree():
    source_importer = load("res://addons/distortionator_integration/scene_importer.gd").new()
    add_scene_import_plugin(source_importer)
    call_deferred("finish_source_import")
func _exit_tree():
    remove_scene_import_plugin(source_importer)
func finish_source_import():
    var filesystem = get_editor_interface().get_resource_filesystem()
    filesystem.scan()
    yield(get_tree().create_timer(1.0), "timeout")
    while filesystem.is_scanning():
        yield(get_tree(), "idle_frame")
    var source_directory = Directory.new()
    if source_directory.open("res://Graphics/Battle BGS/") != OK:
        get_tree().quit(1)
        return
    source_directory.list_dir_begin()
    var files = PoolStringArray()
    var name = source_directory.get_next()
    while name != "":
        if not source_directory.current_is_dir() and (name.ends_with(".bbg.import") or name.ends_with(".dsp.import")):
            files.append("res://Graphics/Battle BGS/" + name.replace(".import", ""))
        name = source_directory.get_next()
    var pending = true
    var check = File.new()
    while pending:
        pending = filesystem.is_scanning()
        for source in files:
            var metadata = ConfigFile.new()
            if metadata.load(source + ".import") != OK:
                get_tree().quit(1)
                return
            var imported_path = metadata.get_value("remap", "path", "")
            if imported_path == "" or not check.file_exists(imported_path):
                pending = true
        if pending:
            yield(get_tree(), "idle_frame")
    var exporter = load("res://field_battle_bg_resources.gd").new()
    exporter.export_data()
    var code = exporter.exit_code
    exporter = null
    get_tree().quit(code)
