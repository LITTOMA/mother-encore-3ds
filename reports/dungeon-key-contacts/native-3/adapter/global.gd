extends Node
signal flags_updated
class SceneService:
    extends Node2D
    signal area_left(region_changed)
    func get_region_name():return name
    func leave(region_changed):emit_signal("area_left",region_changed)
class PlayerService:
    extends KinematicBody2D
    var paused=false
    func is_paused():return paused
var currentScene
var player
func _ready():
    currentScene=SceneService.new()
    currentScene.name="Catacombs"
    add_child(currentScene)
    player=PlayerService.new()
    add_child(player)
func get_player():return player
func broadcast_flags_updated():emit_signal("flags_updated")
