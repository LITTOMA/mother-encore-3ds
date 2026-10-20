extends Node
var events=[]
func music_fadeout(value):events.append({"kind":"fade","value":value})
func play_sfx(stream,channel):events.append({"kind":"sfx","source":stream.resource_path,"channel":channel})
