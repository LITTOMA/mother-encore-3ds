extends Node
var hud
var rows=[]
func vec(v):return [v.x,v.y]
func color(v):return [v.r,v.g,v.b,v.a]
func _ready():call_deferred("run")
func run():
	hud=load("res://Nodes/Ui/KeyCount.tscn").instance()
	add_child(hud)
	var panel=hud.get_node("HBoxContainer")
	var icon=panel.get_node("TextureRect")
	var label=panel.get_node("Money")
	var font=label.get_font("font")
	for value in ["x 4","x 0","x 1","x 2","x 3"]:
		label.text=value
		for _i in range(4):yield(get_tree(),"idle_frame")
		rows.append({"text":value,"panel_position":vec(panel.rect_position),"panel_size":vec(panel.rect_size),"icon_position":vec(icon.rect_position),"icon_size":vec(icon.rect_size),"label_position":vec(label.rect_position),"label_size":vec(label.rect_size),"string_size":vec(font.get_string_size(value)),"label_minimum":vec(label.get_minimum_size()),"separation":panel.get_constant("separation"),"align":label.align,"valign":label.valign,"font_height":font.get_height(),"font_ascent":font.get_ascent(),"text_color":color(label.get_color("font_color")),"shadow_color":color(label.get_color("font_color_shadow")),"outline_color":color(font.outline_color),"outline_size":font.outline_size,"char_spacing":font.extra_spacing_char,"space_spacing":font.extra_spacing_space})
	var advances=[]
	for cp in [32,48,49,50,51,52,120]:advances.append([cp,font.get_char_size(cp,32).x])
	var f=File.new()
	if f.open("res://../layout.json",File.WRITE)!=OK:
		get_tree().quit(2)
		return
	f.store_string(JSON.print({"engine":Engine.get_version_info(),"rows":rows,"advances":advances},"  "))
	f.close()
	print("KEY_HUD_LAYOUT_NATIVE_COMPLETE rows=",rows.size())
	get_tree().quit(0)
