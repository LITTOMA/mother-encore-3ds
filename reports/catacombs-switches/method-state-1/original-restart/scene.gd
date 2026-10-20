extends Node2D
signal area_left(region_changed)
signal synced_switches_changed(emitter,value,silent)
var can_switch=true
func get_switches_state():return false
func leave(changed):emit_signal("area_left",changed)
func notify_synced(emitter,value,silent):emit_signal("synced_switches_changed",emitter,value,silent)
func _on_main_switch_hit():
	audioManager.music_fadeout(0)
	global.get_player().pause()
	uiManager.set_cutscene(true)

func _on_wall_door_state_changed(state: bool, silent: bool):
	if state and !silent:
		yield(get_tree().create_timer(0.5), "timeout")
		uiManager.set_cutscene(false)
		global.get_player().unpause()
		$Music/MusicArea.play_music()
