extends Node
var characters={}
var fixture={}
func _ready():
    var file=File.new()
    assert(file.open("res://input.json",File.READ)==OK)
    fixture=JSON.parse(file.get_as_text()).result
    file.close()
func get_item_data(name):return fixture.items[name]
func get_ailment_data(name):return fixture.statuses.get(name,{})
func does_ailment_exist(name):return fixture.statuses.has(name)
