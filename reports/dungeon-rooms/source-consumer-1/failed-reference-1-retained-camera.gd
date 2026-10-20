extends Camera2D
var camareas=0
var _camarea_offset=Vector2.ZERO
func set_camarea_offset(off: Vector2):
	_camarea_offset = off
	position = _camarea_offset
