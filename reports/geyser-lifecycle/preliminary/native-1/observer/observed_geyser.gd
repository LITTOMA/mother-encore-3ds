extends "res://Scripts/Main/Poison Geyser.gd"
var observer_ticket=0
var observer_ready=false
func _init():observer_ticket=global.currentScene.register_created(self)
func _ready():
    ._ready()
    observer_ready=true
    global.currentScene.observed_ready(self)
func _on_AnimationPlayer_animation_finished(name):
    ._on_AnimationPlayer_animation_finished(name)
    global.currentScene.observed_finished(self,name)
func _exit_tree():
    if is_instance_valid(global.currentScene):global.currentScene.observed_exit(observer_ticket)
