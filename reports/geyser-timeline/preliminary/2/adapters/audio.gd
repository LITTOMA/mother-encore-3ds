extends AudioStreamPlayer2D
var commands=[]
func set_stream(value):
    commands.append(["stream",value.resource_path])
    .set_stream(value)
func play(value=0.0):
    var bytes=StreamPeerBuffer.new()
    bytes.put_float(value)
    bytes.seek(0)
    commands.append(["play",bytes.get_u32()])
    .play(value)
func stop():
    commands.append(["stop"])
    .stop()
