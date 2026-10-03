extends Node2D
export var speed = 64
export var from_script = "default from script"
var ready_executed = false
func _ready():
    ready_executed = true
    assert(false) # Export must never enter the SceneTree.
func signal_target(_value):
    pass
