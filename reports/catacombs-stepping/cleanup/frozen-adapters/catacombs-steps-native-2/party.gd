extends KinematicBody2D
func set_shadow(anim: String):
	$Shadow.set_anim(anim)

func get_shadow() -> String:
	return $Shadow.animation
