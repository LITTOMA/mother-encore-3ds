extends Node2D
signal area_left(region_changed)
func leave(region_changed):emit_signal("area_left",region_changed)
