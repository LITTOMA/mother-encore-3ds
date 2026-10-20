extends AudioStreamPlayer
var runner
var requested_playing=false setget requested
func requested(value):
    requested_playing=value
    runner.event({"kind":"audio","playing":value})
    playing=value
