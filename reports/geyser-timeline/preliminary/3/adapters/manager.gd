extends Node
func _ready():
	var node=Node.new()
	node.name="Sfx"
	add_child(node)
func add_sfx(stream: AudioStream, node_name: String) -> AudioStreamPlayer:
	var sfx_node: AudioStreamPlayer = get_sfx(node_name)
	if !sfx_node:
		sfx_node = AudioStreamPlayer.new()
		sfx_node.bus = "SFX"
		sfx_node.name = node_name
		$Sfx.add_child(sfx_node)
	sfx_node.stream = stream
	return sfx_node

func play_sfx(stream: AudioStream, node_name: String) -> AudioStreamPlayer:
	var sfx_node: AudioStreamPlayer = add_sfx(stream, node_name)
	sfx_node.play()
	return sfx_node

func get_sfx(node_name: String) -> AudioStreamPlayer:
	return $Sfx.get_node_or_null(node_name) as AudioStreamPlayer
