extends Control

enum { MUSIC, FILTER, MORE_FILTER }

const BUSES = ["Music", "Filter", "More Filter"]
const SILENT_SOUND_THRESHOLD = -80

onready var _tween := $Tween

var audioManager
var musicChangers := []
func _ready():
	_add_at_zero()

func _get_audio_player_count() -> int:
	return $AudioPlayers.get_child_count()

func add_audio_player(name = "") -> AudioStreamPlayer:
	var music_player = AudioStreamPlayer.new()
	music_player.bus = BUSES[MUSIC]
	$AudioPlayers.add_child(music_player)
	if name != "":
		music_player.name = name
	return music_player

func get_audio_player(index: int) -> AudioStreamPlayer:
	if index >= $AudioPlayers.get_child_count():
		return null 
	return $AudioPlayers.get_child(index) as AudioStreamPlayer

func get_audio_player_from_song(song: String, excluded_changer = null) -> AudioStreamPlayer:
	if song != "":
		if excluded_changer != null:
			for musicChanger in musicChangers:
				var audioPlayer = musicChanger.attached_player
				if is_instance_valid(audioPlayer) and (audioPlayer.stream == load("res://Audio/Music/" + song)) and musicChanger != excluded_changer:
					return audioPlayer
		else:
			for audioPlayer in audioManager.get_audio_player_list():
				if is_instance_valid(audioPlayer) and (audioPlayer.stream == load("res://Audio/Music/" + song)):
					return audioPlayer
	return null

func get_audio_player_index(node) -> int:
	return get_audio_player_list().find(node)

func get_audio_player_list() -> Array:
	return $AudioPlayers.get_children()

func _remove_audio_player_obj(music_player: AudioStreamPlayer):
	if music_player != null:
		music_player.queue_free()
		yield(music_player, "tree_exited")
		_add_at_zero()

func _add_at_zero():
	if _get_audio_player_count() == 0:
		add_audio_player()

func get_latest_audio_player_index() -> int:
	return _get_audio_player_count() - 1

func play_music(track: String, loop := "", index := 0, start := 0.0) -> AudioStreamPlayer:
	var music_player = get_audio_player(index)
	if music_player != null:
		var path = ""
		if track == "":
			path = "res://Audio/Music/" + loop
		elif track == "AoOni.mp3":
			path = "res://Nodes/Overworld/Enemies/AoOni.mp3"
		else:
			path = "res://Audio/Music/" + track
		var musicFile = load(path)
		music_player.stream = musicFile
		music_player.play(start)
		
		if music_player.is_connected("finished", self, "play_music"):
			music_player.disconnect("finished", self, "play_music")
		if loop != "" and !music_player.is_connected("finished", self, "play_music"):
			music_player.connect("finished", self, "play_music", ["", loop, index, 0.0], CONNECT_DEFERRED)
		else:
			music_player.connect("finished", self, "_remove_audio_player_obj", [music_player], CONNECT_ONESHOT)
	return music_player

func music_fadeout(index: int, duration := 1.0):
	var music_player = get_audio_player(index)
	music_fadeout_obj(music_player, duration)

func music_fadeout_obj(music_player: AudioStreamPlayer, duration := 1.0):
	if music_player != null:
		if music_player.playing:
			_tween.remove(music_player)
			_tween.interpolate_property(music_player, "volume_db", 
				music_player.volume_db, SILENT_SOUND_THRESHOLD, duration,
				_tween.TRANS_QUART, _tween.EASE_IN)
			_tween.start()
			yield(_tween, "tween_all_completed")
			_remove_all_unplaying()

func music_fadein(index, volume, duration = 1):
	var music_player = get_audio_player(index)
	_music_fadein_obj(music_player, volume, duration)

func _music_fadein_obj(music_player: AudioStreamPlayer, volume, duration = 1):
	if music_player != null:
		_tween.remove(music_player)
		_tween.interpolate_property(music_player, "volume_db", 
		SILENT_SOUND_THRESHOLD, volume, duration,
		_tween.TRANS_QUART,_tween.EASE_OUT)
		_tween.start()

func music_fadeto(index, volume = 0, duration = 1):
	var music_player = get_audio_player(index)
	if music_player != null:
		if music_player.volume_db != volume:
			_tween.remove(music_player)
			_tween.interpolate_property(music_player,"volume_db", 
				music_player.volume_db, volume, duration,
				_tween.TRANS_LINEAR,_tween.EASE_IN_OUT)
			_tween.start()

func set_audio_player_volume(index, volume):
	var music_player = get_audio_player(index)
	_set_audio_player_volume_obj(music_player, volume)

func _set_audio_player_volume_obj(music_player: AudioStreamPlayer, volume):
	if music_player != null:
		music_player.volume_db = volume

func stop_audio_player(index: int):
	var music_player = get_audio_player(index)
	if music_player != null:
		_stop_audio_player_obj(music_player)
	else:
		push_warning("Audio player %s was not found" % index)

func _stop_audio_player_obj(music_player: AudioStreamPlayer):
	if music_player != null:
		if music_player.is_connected("finished", self, "play_music"):
			music_player.disconnect("finished", self, "play_music")
		music_player.playing = false

func _remove_all_unplaying(wait_for_tween := false):
	for music_player in get_audio_player_list():
		if music_player and (!music_player.playing or music_player.volume_db <= SILENT_SOUND_THRESHOLD):
			_remove_player_after_tween(music_player, wait_for_tween)

func _remove_player_after_tween(music_player: AudioStreamPlayer, wait_for_tween := false):
	if wait_for_tween and _tween.is_active():
		yield(_tween, "tween_all_completed")
	_remove_audio_player_obj(music_player)
