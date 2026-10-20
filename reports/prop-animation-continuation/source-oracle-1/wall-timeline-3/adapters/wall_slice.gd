extends Sprite
var global
var _move_cam_when_changed=false
func _shake(duration: float):
	global.currentCamera.shake_camera(5, duration, Vector2.ZERO, 0.02, 1, false)
