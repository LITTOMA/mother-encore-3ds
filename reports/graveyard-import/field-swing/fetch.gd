class_name SpriteDataFetcher
extends Node
onready var _sprite_node=get_node("../SpecialAnimations")
func get_frames() -> int:
	return _sprite_node.frame

