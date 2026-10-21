extends Node
signal finished
var playing=false setget change
var writes=[]
func change(value):
    playing=value
    writes.append(value)
