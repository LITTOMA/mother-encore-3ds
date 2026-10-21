extends Node
class Sfx:
    extends Reference
    var stream
    var playing=false
    var owner
    func play():
        playing=true
        owner.events.append([1,owner.names.find(stream.resource_path.get_file().get_basename())])
    func stop():
        playing=false
        owner.events.append([3,owner.names.find(stream.resource_path.get_file().get_basename())])
var current=Sfx.new()
var events=[]
var names=[]
func _init():current.owner=self
func get_sfx(_slot):return current
func play_sfx(stream,_slot):
    current.stream=stream
    current.playing=true
    events.append([2,names.find(stream.resource_path.get_file().get_basename())])
