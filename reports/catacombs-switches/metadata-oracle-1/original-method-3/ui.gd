extends Node
var events=[]
var cutscene=false
func set_cutscene(value):
    cutscene=value
    events.append(value)
