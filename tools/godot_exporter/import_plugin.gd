tool
extends EditorPlugin
# The runner imposes an external timeout and retains output even on failure.
func _enter_tree():
    call_deferred("finish_import")
func finish_import():
    var filesystem = get_editor_interface().get_resource_filesystem()
    yield(get_tree().create_timer(1.0), "timeout")
    while filesystem.is_scanning():
        yield(get_tree(), "idle_frame")
    print("ENCORE_IMPORT_COMPLETE")
    get_tree().quit()
