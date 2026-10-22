extends Node
func play_sfx(stream,slot):global.events.append(["audio",stream.resource_path,slot])
