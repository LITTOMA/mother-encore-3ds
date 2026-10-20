extends Node
signal flags_updated
var currentScene
var currentCamera
var player
var flag_signals=0
func _ready():
    player=Node2D.new()
    player.name="PlayerService"
    add_child(player)
    connect("flags_updated",self,"count_flag")
func count_flag():flag_signals+=1
func get_player():return player
