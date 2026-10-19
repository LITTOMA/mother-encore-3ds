extends Sprite
var ready_flag=false
func _get_flag_status():
    return ready_flag
func _update_sprite():
    pass
func _on_player_paused():
    pass
func _on_player_unpaused():
    pass
func _ready():
	hide()
	if is_inside_tree():
		_update_sprite()
	if !Engine.is_editor_hint():
		if _get_flag_status():
			queue_free()
			return
		global.get_player().connect("paused", self, "_on_player_paused")
		global.get_player().connect("unpaused", self, "_on_player_unpaused")
	yield(get_tree(), "idle_frame")
	show()
	
