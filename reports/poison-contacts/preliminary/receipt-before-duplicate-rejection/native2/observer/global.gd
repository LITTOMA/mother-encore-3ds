extends Node
var events=[]
var party=[]
var player
var currentScene
func start_joy_vibration(device,weak,strong,duration):events.append(["vibration",device,weak,strong,duration])
func get_player():return player
func get_conscious_party() -> Array:
	var arr := []
	for mem in party:
		if !mem.is_incapacitated():
			arr.append(mem)
	return arr

