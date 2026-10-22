extends Reference
var values={"flash_modifier":0.0,"flash_color":Color.white,"glow_modifier":0.0,"glow_color":Color.white}
func _get(property):
	assert(property.begins_with("shader_param/") and values.has(property.substr(13)))
	return values[property.substr(13)]
func _set(property,value):
	assert(property.begins_with("shader_param/") and values.has(property.substr(13)))
	values[property.substr(13)]=value
	return true
func get_shader_param(name):return values[name]
