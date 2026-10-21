extends "res://party.gd"
const MOVE=0
var _state=MOVE
var _running=false
var _paused=false
var _climbing=false
var run_sound=""
var audioManager
var global
func _check_event_collider():pass
func _controls():pass
func _movement(_delta):assert(false)
func _set_running(enabled: bool):
	_running = enabled
	if !_paused and _state == MOVE:
		if enabled and !_climbing:
			var sfx = load("res://Audio/Sound effects/Footsteps/" + run_sound + ".mp3")
			if audioManager.get_sfx("run") != null and audioManager.get_sfx("run").stream == sfx:
				if !audioManager.get_sfx("run").playing :
					audioManager.get_sfx("run").play()
			else:
				audioManager.play_sfx(sfx, "run")
	if audioManager.get_sfx("run") != null and audioManager.get_sfx("run").playing and !enabled:
		audioManager.get_sfx("run").stop()

func _move_state(delta: float):
	if !_paused and !global.entering_door:
		
		_check_event_collider()
		
		_controls()
		_movement(delta)
	else:
		if audioManager.get_sfx("run") != null and audioManager.get_sfx("run").playing:
			audioManager.get_sfx("run").stop()
