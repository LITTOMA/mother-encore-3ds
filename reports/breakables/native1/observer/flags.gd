extends Node
var flags={}
var object_flags={}
func check_appear_disappear_flags(appear_flag: String, disappear_flag: String) -> bool:
	var flag_on := true
	if appear_flag != "":
		flag_on = flags.get(appear_flag, false)
	if disappear_flag != "":
		flag_on = flag_on and !flags.get(disappear_flag, false)
	return flag_on

func set_flag(flag_name: String, value: bool, emit_signal := true):
	if flags.has(flag_name):
		flags[flag_name] = value
		if emit_signal: global.emit_signal("flags_updated")

func set_object_flag(flag_name: String, value: bool, emit_signal := true):
	object_flags[flag_name] = value
	if emit_signal: global.emit_signal("flags_updated")
