extends SceneTree
var target
func _init():
    call_deferred("run")
func run():
    target=load("res://probe_node.gd").new()
    get_root().add_child(target)
