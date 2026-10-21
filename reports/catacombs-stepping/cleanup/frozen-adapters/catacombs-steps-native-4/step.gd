extends Area2D
const PartyObject=preload("res://party.gd")
var global
var entering_sound=""
var exiting_sound=""
var enter_shadow_effect=""
var exit_shadow_effect=""
var enabled=true
func _on_Stepping_Sounds_body_entered(body):
	if body == global.get_player() and enabled:
		set_player_run_sound(entering_sound)
	if body is PartyObject and enter_shadow_effect != "":
		body.set_shadow(enter_shadow_effect)
		

func _on_Stepping_Sounds_body_exited(body):
	if body == global.get_player() and exiting_sound != "" and enabled:
		set_player_run_sound(exiting_sound)
	if body is PartyObject and exit_shadow_effect != "":
		body.set_shadow(exit_shadow_effect)

func set_player_run_sound(sound):
	global.get_player().run_sound = sound

func enable():
	enabled = true

func disable():
	enabled = false
