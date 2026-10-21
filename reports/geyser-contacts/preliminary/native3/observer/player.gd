extends "res://base.gd"
var camera
var _paused := false
var _hit_direct := Vector2.ZERO
var _knockback := Vector2.ZERO
func start_continuous_damage(damage: int, damage_variance := 0, hit_direct := Vector2.ZERO, status = null):
	.start_continuous_damage(damage, damage_variance, hit_direct, status)
	_hit_direct = hit_direct

func damage(damage: int, damage_variance := 0, hit_direct := Vector2.ZERO, cant_kill := true):
	camera.shake_camera(1, 0.2, Vector2(1, 0))
	global.start_joy_vibration(0, 0.6, 0.8, 0.1)
	.damage(damage, damage_variance, hit_direct, cant_kill)

func physics_tail():
	if _is_continuous_damage and !_paused:
		_knockback = _hit_direct * 50
		if !_is_invulnerable():
			damage(_attack_damage, _damage_variance, _hit_direct)
	_knockback = _knockback.move_toward(Vector2.ZERO, 200)

func game_over():
	global.events.append(["game_over"])
	_paused=true

var run_sound=""
var shadow_mode=""
func is_paused():return _paused
func set_shadow(value):shadow_mode=value
