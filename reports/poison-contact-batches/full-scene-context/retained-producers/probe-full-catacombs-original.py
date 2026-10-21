from pathlib import Path
import argparse,hashlib,json,os,subprocess,time
ap=argparse.ArgumentParser();ap.add_argument('--name',required=True);a=ap.parse_args();r=Path.cwd();out=r/'build/full-port/full-original-oracle-1';p=out/'project';source=r/'upstream/MOTHER-Encore';adapter=p/'oracle-full-catacombs';adapter.mkdir(exist_ok=True)
assert (out/'.owned-original-full-oracle').read_bytes()==b'original-full-oracle-v1'
probe='''extends Node
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
 file.store_string(JSON.print({"rows":rows,"scene":global.currentScene.filename,"player_script":player.get_script().resource_path,"rng_tail":randi()})+"\\n")
 file.close()
 print("FULL CATACOMBS complete ", rows.size())
 get_tree().quit()
'''
(adapter/'probe.gd').write_text(probe,encoding='utf8');inputs=(r/'reports/poison-contacts/native1/observer/input.json').read_bytes();(adapter/'input.json').write_bytes(inputs)
config=(source/'project.godot').read_text('utf8').replace('[debug]','FullCatacombsOracle="*res://oracle-full-catacombs/probe.gd"\n\n[debug]');assert config.count('FullCatacombsOracle=')==1;(p/'project.godot').write_text(config,encoding='utf8')
result_dir=out/a.name;assert not result_dir.exists();result_dir.mkdir()
for name in ['probe.gd','input.json']:(result_dir/name).write_bytes((adapter/name).read_bytes())
(result_dir/'project.godot').write_bytes((p/'project.godot').read_bytes());(adapter/'reference.json').unlink(missing_ok=True)
env=dict(os.environ)
for key,child in [('APPDATA','user-roaming'),('LOCALAPPDATA','user-local'),('XDG_DATA_HOME','user-data'),('XDG_CONFIG_HOME','user-config')]:env[key]=str(out/child)
engine=r/'build/godot-reference/Godot_v3.6.2-stable_win64.exe';cmd=[str(engine),'--path',str(p),'--no-window','--audio-driver','Dummy','--video-driver','GLES2','--fixed-fps','60'];start=time.monotonic();code=None;timed_out=False
with(result_dir/'godot.log').open('wb')as log:
 try:result=subprocess.run(cmd,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=75,creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0));code=result.returncode
 except subprocess.TimeoutExpired:timed_out=True
text=(result_dir/'godot.log').read_text('utf8',errors='replace');diagnostics=[line for line in text.splitlines()if 'ERROR:' in line or 'WARNING:' in line or 'SCRIPT ERROR' in line]
result=None
if(adapter/'reference.json').is_file():
 raw=(adapter/'reference.json').read_bytes();(result_dir/'reference.json').write_bytes(raw);result=hashlib.sha256(raw).hexdigest()
proof=dict(command=cmd,return_code=code,timed_out=timed_out,elapsed_seconds=time.monotonic()-start,result_sha256=result,diagnostics=diagnostics,scope='Actual whole original Catacombs scene and full Player via original SceneTransition. Teleport input service only. Existing original bootstrap/exit diagnostics retained;NOT a clean game boot or approved native equivalence result.')
(result_dir/'completion.json').write_text(json.dumps(proof,indent=2)+'\n');print(json.dumps({k:v for k,v in proof.items()if k not in ['diagnostics','command']}),flush=True)
