extends KinematicBody2D
class_name PartyObject
const PAUSABLE_FLASH_ANIMS := ["Flash"]
var _party_member: Character
var _attack_damage := 0
var _damage_variance := 0
var _is_continuous_damage := false
onready var _flash_anim=$FlashAnim
func _is_invulnerable() -> bool:
	return _flash_anim.get_current_animation() == "Flash"

func damage(damage: int, damage_variance := 0, hit_direct := Vector2.ZERO, cant_kill := true):
	var _observer_hit=hit_direct
	#if cant_kill and _party_member.get_hp() == 1:
	#	return
	damage = damage + int(rand_range(-damage_variance, damage_variance))
	if cant_kill:
		var hp := int(max(_party_member.get_hp() - damage, 1))
		damage = _party_member.get_hp() - hp
	play_flash_anim("Flash")
	audioManager.play_sfx(load("res://Audio/Sound effects/Hurt 1.mp3"), "damage")
	if !_party_member.is_incapacitated() and _party_member.get_stat(Character.MAXHP) != 0:
		_party_member.set_hp(_party_member.get_hp() - damage)
		uiManager.info_plates_show(false, true)
		uiManager.info_plates_update(true)
		if damage > 0:
			uiManager.create_flying_num(damage, global_position)
		if _party_member.get_hp() <= 0:
			_party_member.add_status(Status.AILMENT_UNCONSCIOUS)
			if global.get_conscious_party() == []:
				global.get_player().game_over()

func start_continuous_damage(damage: int, damage_variance := 0, hit_direct := Vector2.ZERO, status := ""):
	var _observer_hit=hit_direct
	_is_continuous_damage = true
	_attack_damage = damage
	_damage_variance = damage_variance
	if status != "":
		_party_member.add_status(status)

func stop_continuous_damage():
	_is_continuous_damage = false

func play_flash_anim(anim: String):
	_flash_anim.play(anim)

func pause_flash_anim():
	if _flash_anim.get_current_animation() in PAUSABLE_FLASH_ANIMS:
		_flash_anim.stop(false)
	else:
		_flash_anim.play("RESET")

func resume_flash_anim():
	if _flash_anim.get_assigned_animation() in PAUSABLE_FLASH_ANIMS \
	and _flash_anim.get_current_animation_position() > 0 and _flash_anim.get_current_animation_position() < _flash_anim.get_current_animation_length():
		_flash_anim.play(_flash_anim.get_assigned_animation())

