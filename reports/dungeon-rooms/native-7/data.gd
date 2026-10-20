extends Node
var object_flags={}
func set_object_flag(flag_name: String, value: bool, emit_signal := true):
	object_flags[flag_name] = value
	if emit_signal: global.emit_signal("flags_updated")

########################### DATA GETTERS ###########################

# AILMENTS
