extends Node
var flags={}
var object_flags={}
var keys={"Catacombs":0}
func set_flag(flag_name: String, value: bool, emit_signal := true):
	if flags.has(flag_name):
		flags[flag_name] = value
		if emit_signal: global.emit_signal("flags_updated")

func set_object_flag(flag_name: String, value: bool, emit_signal := true):
	object_flags[flag_name] = value
	if emit_signal: global.emit_signal("flags_updated")

########################### DATA GETTERS ###########################

# AILMENTS
