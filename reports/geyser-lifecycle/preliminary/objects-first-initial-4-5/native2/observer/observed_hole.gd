extends "res://hole.gd"
var observer_hole=0
func _on_Timer_timeout():
    ._on_Timer_timeout()
    global.currentScene.timeout(observer_hole)
