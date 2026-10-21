extends Node
func is_in_battle():return false
func info_plates_show(a,b):global.events.append(["show",a,b])
func info_plates_update(a):global.events.append(["update",a])
func create_flying_num(text, target_pos: Vector2):
	var FlyingNumTscn := load("res://Nodes/Ui/Battle/FlyingNumber.tscn")
	var flying_num: Label = FlyingNumTscn.instance()
	flying_num.text = str(text)
	global.currentScene.add_child(flying_num)
	flying_num.rect_position = target_pos - Vector2(16, 0) 
	flying_num.run()
	global.events.append(["number",int(text),[flying_num.rect_position.x,flying_num.rect_position.y],flying_num.get_meta("observer_distance")])


func observer_words(value):
	var b=StreamPeerBuffer.new()
	b.put_double(value)
	b.seek(0)
	return [b.get_u32(),b.get_u32()]
