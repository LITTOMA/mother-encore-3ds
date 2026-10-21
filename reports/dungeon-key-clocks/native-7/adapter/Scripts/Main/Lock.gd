extends FlaggableObject

var opened = false 
export var player_turn := { 
	"y": true, #Make "x" true if you want the player to turn left/right to face npc
	"x": false #Make "y" true if you want the player to turn up/down to face npc
}

func _ready():
	if !_get_flag_status():
		return
	opened = true
	$AnimationPlayer.play("Opened")
	open()

func interact(): # Opens the door if you have a key. Otherwise it opens a dialog box that says you can't open it.
	if opened:
		return
	if !uiManager.try_alter_key_count(-1):
		uiManager.open_dialogue_box("Reusable/locklocked")
		return
	$AnimationPlayer.play("Open")
	opened = true
	_set_flag_status()
	uiManager.update_key_indicator()
		

func open():
	$StaticBody2D/CollisionShape2D.disabled = true
	$interact/ButtonPrompt.enabled = false
