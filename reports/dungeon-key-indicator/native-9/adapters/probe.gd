extends Node
var config
var hud
var rows=[]
var stepped={}
var append_attempts=[]
func capture(ci,pi):
	var panel=hud.get_node("HBoxContainer")
	rows.append({"case":ci,"phase":pi,"x":panel.rect_position.x,"y":panel.rect_position.y,"visible":panel.visible,"open":hud._is_open,"text":panel.get_node("Money").text,"running":hud._tween.is_running() if hud._tween else false})
func _ready():call_deferred("run")
func run():
	var f=File.new()
	if f.open("res://input.json",File.READ)!=OK:
		get_tree().quit(2)
		return
	config=JSON.parse(f.get_as_text()).result
	f.close()
	for ci in config.cases.size():
		hud=load("res://source.gd").new()
		var panel=HBoxContainer.new()
		panel.name="HBoxContainer"
		panel.rect_position=Vector2(config.parameters[0],config.parameters[1])
		var label=Label.new()
		label.name="Money"
		label.text=config.initial_text
		panel.add_child(label)
		hud.add_child(panel)
		get_tree().root.add_child(hud)
		uiManager._key=hud
		globaldata.keys={}
		stepped={}
		capture(ci,-1)
		for pi in config.cases[ci].size():
			var operation=config.cases[ci][pi]
			print("KEY_HUD_OPERATION_BEGIN ",ci," ",pi)
			if operation.op=="update":
				if operation.count>0 and hud.get_node("HBoxContainer").rect_position.x==config.parameters[2] and hud._tween and hud._tween.is_running() and stepped.has(hud._tween.get_instance_id()):
					append_attempts.append({"case":ci,"phase":pi})
				globaldata.keys[config.region]=operation.count
				uiManager.update_key_indicator()
			elif operation.op=="close":uiManager.close_key_indicator()
			elif operation.op=="step":
				if hud._tween:
					if operation.delta>0:stepped[hud._tween.get_instance_id()]=true
					hud._tween.custom_step(operation.delta)
			else:
				push_error("Unknown key HUD reference action")
				get_tree().quit(3)
				return
			capture(ci,pi)
			print("KEY_HUD_OPERATION_END ",ci," ",pi)
		if hud._tween:hud._tween.kill()
		hud.free()
	f.open("res://../indicator.json",File.WRITE)
	f.store_string(JSON.print({"engine":Engine.get_version_info(),"rows":rows,"append_attempts":append_attempts},"  "))
	f.close()
	print("KEY_INDICATOR_NATIVE_COMPLETE rows=",rows.size())
	get_tree().quit(17)
