extends Node2D
const MOVE=0
var _state=1
var _tap_run=false
func get_current_skill_action():
    return "swing"
func _attack_animation_finished():
	$HitboxPivot/BatHitbox/CollisionShape2D.disabled = true
	_state = MOVE
	_tap_run = false

