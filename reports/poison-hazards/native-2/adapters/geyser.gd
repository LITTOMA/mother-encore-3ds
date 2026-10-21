extends Area2D
func _on_Poison_Geyser_body_entered(body):
	if body is PartyObject:
		if global.get_player().is_paused() == false:
			body.start_continuous_damage(5, 2, Vector2.ZERO, Status.AILMENT_POISONED)

func _on_Poison_Geyser_body_exited(body):
	if body is PartyObject:
		body.stop_continuous_damage()
