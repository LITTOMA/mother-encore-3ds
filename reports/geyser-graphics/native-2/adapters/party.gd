extends KinematicBody2D
class_name PartyObject
signal paused
signal unpaused
func set_observed_pause(value):
	if value:emit_signal("paused")
	else:emit_signal("unpaused")
func is_paused():return false
func start_continuous_damage(_a,_b,_c,_d):pass
func stop_continuous_damage():pass
