extends Node
func play_sfx(stream,slot):global.events.append(["hurt",stream.resource_path,slot])
