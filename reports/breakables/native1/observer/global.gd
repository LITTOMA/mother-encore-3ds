extends Node
signal flags_updated
var currentScene
var paused=false
var rumble=[]
var base=0
func get_player():return self
func pause():paused=true
func start_joy_vibration(device,weak,strong,duration):rumble.append([Engine.get_idle_frames()-base,device,weak,strong,duration])
func notify_flags():emit_signal("flags_updated")
