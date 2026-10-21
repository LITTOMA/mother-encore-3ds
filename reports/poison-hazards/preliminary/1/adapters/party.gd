extends KinematicBody2D
class_name PartyObject
signal paused
signal unpaused
var _paused=false
var _is_continuous_damage=false
var _attack_damage=0
var _damage_variance=0
var shadow_mode="shadow"
var run_sound="wood"
class Member:
    var statuses=[]
    func add_status(status):statuses.append(status)
var _party_member=Member.new()
func is_paused():return _paused
func set_shadow(mode):shadow_mode=mode
func start_continuous_damage(damage: int, damage_variance := 0, hit_direct := Vector2.ZERO, status := ""):
	_is_continuous_damage = true
	_attack_damage = damage
	_damage_variance = damage_variance
	if status != "":
		_party_member.add_status(status)

func stop_continuous_damage():
	_is_continuous_damage = false
