extends Node
var base=0
var requests=[]
var playing=false setget set_playing
func set_playing(value):
 playing=value
 requests.append([Engine.get_idle_frames()-base,value])
