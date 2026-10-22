class_name CoinSpawner
extends Node2D
var base=0
var calls=[]
func spawn_coins(count):calls.append([Engine.get_idle_frames()-base,count])
