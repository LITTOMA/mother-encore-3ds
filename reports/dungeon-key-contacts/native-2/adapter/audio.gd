extends Node
signal finished
var playing=false setget change
var writes=[]
func change(value):
    playing=value
    writes.append(value)
func finish():
    playing=false
    emit_signal("finished")
