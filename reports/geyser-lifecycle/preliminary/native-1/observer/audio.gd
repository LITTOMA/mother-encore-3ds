extends AudioStreamPlayer2D
var commands=[]
func bits(value):
    var bytes=StreamPeerBuffer.new()
    bytes.put_float(value)
    bytes.seek(0)
    return bytes.get_u32()
func set_stream(value):
    commands.append(["stream",value.resource_path])
    .set_stream(value)
func play(value=0.0):
    commands.append(["play",bits(value)])
    .play(value)
func stop():
    commands.append(["stop"])
    .stop()
