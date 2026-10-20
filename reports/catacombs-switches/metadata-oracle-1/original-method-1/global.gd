extends Node
signal flags_updated
class CameraService:
    extends Node
    signal release
    var events=[]
    func move_camera(position,duration):
        events.append({"kind":"move","x":position.x,"y":position.y,"duration":duration})
        yield(self,"release")
    func return_camera(duration):
        events.append({"kind":"return","duration":duration})
        yield(self,"release")
    func shake_camera(a,b,c,d,e,f):events.append({"kind":"shake","arguments":[a,b,c.x,c.y,d,e,f]})
class PlayerService:
    extends Node
    var paused=false
    var events=[]
    var camera
    func is_paused():return paused
    func pause():
        paused=true
        events.append("pause")
    func unpause():
        paused=false
        events.append("unpause")
var currentScene
var currentCamera
var player
func _ready():
    player=PlayerService.new()
    add_child(player)
    currentCamera=CameraService.new()
    add_child(currentCamera)
    player.camera=currentCamera
func get_player():return player
