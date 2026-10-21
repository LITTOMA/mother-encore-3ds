extends Node
var rows={}
func _ready():
	var file=File.new()
	assert(file.open("res://ailments.json",File.READ)==OK)
	rows=JSON.parse(file.get_as_text()).result
	file.close()
func get_ailment_data(name):return rows.get(name,{})
func does_ailment_exist(name):return rows.has(name)
