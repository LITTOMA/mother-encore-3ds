extends Node
var flags={}
func check_appear_disappear_flags(appear_flag: String, disappear_flag: String) -> bool:
	var flag_on := true
	if appear_flag != "":
		flag_on = flags.get(appear_flag, false)
	if disappear_flag != "":
		flag_on = flag_on and !flags.get(disappear_flag, false)
	return flag_on
