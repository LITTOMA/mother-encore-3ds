extends Node
var events=[]
func update_key_indicator():events.append(get_key_count())
func get_key_count() -> int: # Returns the amount of keys collected in a scene. If you can't collect keys in a scene, it returns -1 
	if global.currentScene.has_method("get_region_name"):
		return globaldata.keys.get(global.currentScene.get_region_name(), 0)
	return 0

func try_alter_key_count(delta: int) -> bool: # Increases or decreases the amount of keys collected. Returns true if and only if it succeeds.
	var key_count := get_key_count()
	if key_count >= 0 and key_count + delta >= 0:
		globaldata.keys[global.currentScene.get_region_name()] = globaldata.keys.get(global.currentScene.get_region_name(), 0) + delta
		return true
	else:
		return false
