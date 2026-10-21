extends Node
var frames := 0
var started := false
var events := []
var rows := []
var config: Dictionary
var player
func _ready():
 var file := File.new()
 assert(file.open("res://oracle-full-catacombs/input.json", File.READ) == OK)
 config = parse_json(file.get_as_text())
 file.close()
func _process(_delta):
 frames += 1
 if !started and frames >= 300 and Cache.cache_.size() == 2 and Cache.materials.empty():
  started = true
  call_deferred("run")
func contact(body, index: int, entered: bool):
 if body == player:
  events.append([index, entered, player.is_paused(), player._is_continuous_damage, player._attack_damage])
func settle():
 for _i in range(4):
  yield(get_tree(), "physics_frame")
 yield(get_tree(), "idle_frame")
func snapshot(label: String):
 var state := {"label":label,"position":[player.global_position.x,player.global_position.y],"paused":player.is_paused(),"continuous":player._is_continuous_damage,"damage":player._attack_damage,"mask":player.collision_mask,"layer":player.collision_layer,"events":events.duplicate(true)}
 rows.append(state)
 events.clear()
 print("FULL CATACOMBS state ", label, " ", state.continuous, " ", state.events)
func run():
 print("FULL CATACOMBS begin original transition")
 global.scene_transition.goto_scene("res://Maps/podunk/Catacombs.tscn", Vector2(0, 32), Vector2.DOWN)
 yield(global, "scene_changed")
 player = global.get_player()
 assert(global.currentScene is DungeonAreaRoom)
 assert(global.currentScene.name == "Catacombs")
 for index in config.areas.size():
  var area = global.currentScene.get_node(config.areas[index].path)
  assert(area is Area2D)
  assert(area.connect("body_entered", self, "contact", [index, true]) == OK)
  assert(area.connect("body_exited", self, "contact", [index, false]) == OK)
 player.show()
 player.camera.current = true
 player.unpause()
 seed(1)
 yield(settle(), "completed")
 snapshot("entry")
 for n in range(8):
  var index: int = n % 2
  var point = config.cases[index].point
  player.global_position = Vector2(point[0] - config.player.center[0], point[1] - config.player.center[1])
  yield(settle(), "completed")
  snapshot("jump_" + str(n) + "_area_" + str(index))
 player.global_position = Vector2(0,32)
 yield(settle(), "completed")
 snapshot("outside")
 player.pause()
 var file := File.new()
 assert(file.open("res://oracle-full-catacombs/reference.json",File.WRITE)==OK)
 file.store_string(JSON.print({"rows":rows,"scene":global.currentScene.filename,"player_script":player.get_script().resource_path,"rng_tail":randi()})+"\n")
 file.close()
 print("FULL CATACOMBS complete ", rows.size())
 get_tree().quit()
