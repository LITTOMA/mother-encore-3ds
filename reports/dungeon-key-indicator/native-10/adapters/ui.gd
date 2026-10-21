extends Node
var _key
func get_key_count() -> int: # Returns the amount of keys collected in a scene. If you can't collect keys in a scene, it returns -1 
	if global.currentScene.has_method("get_region_name"):
		return globaldata.keys.get(global.currentScene.get_region_name(), 0)
	return 0

func update_key_indicator():
	if get_key_count() <= 0:
		_key.close()
	else:
		_key.open()

func close_key_indicator():
	_key.close()
