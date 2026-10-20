extends Camera2D
var camareas=0
var _camarea_offset=Vector2.ZERO
var offset_calls=[]
func set_camarea_offset(off: Vector2):
	_camarea_offset = off
	position = _camarea_offset
	offset_calls.append(["%.17f"%off.x,"%.17f"%off.y])
