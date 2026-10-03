#!/usr/bin/env python3
"""Run original MusicChanger and audioManager in isolated Godot3.6.2.
No player saves, firmware, user screenshots, or upstream modifications.
"""
import hashlib,json,os,re,shutil,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
PROJECT=ROOT if (ROOT/'upstream.lock').is_file() else ROOT.parents[1]/'encore-native';UP=PROJECT/'upstream/MOTHER-Encore';OUT=ROOT/'reports/source-reference';P=OUT/'project'
GODOT=Path(os.environ.get('GODOT',str(PROJECT.parent/'toolchain/godot-3.6.2-headless/Godot_v3.6.2-stable_linux_headless.64')))
P.mkdir(parents=True,exist_ok=True)
r=json.loads((ROOT/'content/podunk-music.json').read_text())
# Remove only unrelated startup SFX dictionary; all manager methods retained.
audio=(UP/'Scripts/global/audioManager.gd').read_text();start=audio.index('var _sound_effects := {');end=audio.index('\n}',start)+2;audio=audio[:start]+'var _sound_effects := {}'+audio[end:];audio=audio.replace('audioManager.','self.');(P/'audioManager.gd').write_text(audio)
changer=(UP/'Nodes/Overworld/MusicChanger.tscn').read_text();at=changer.index('[connection signal=');changer=changer[:at]+'[node name="CollisionShape2D" type="CollisionShape2D" parent="."]\n\n'+changer[at:];(P/'MusicChanger.tscn').write_text(changer)
for a in r['tracks']:
 src=UP/a['source_path'][6:];dst=P/a['source_path'][6:];dst.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(src,dst);shutil.copyfile(str(src)+'.import',str(dst)+'.import')
(P/'project.godot').write_text('''config_version=4
[application]
config/name="Isolated original Podunk music callback reference"
run/main_scene="res://run.tscn"
[autoload]
global="*res://global.gd"
globaldata="*res://globaldata.gd"
uiManager="*res://uiManager.gd"
audioManager="*res://audioManager.tscn"
''')
(P/'audioManager.tscn').write_text('''[gd_scene load_steps=2 format=2]
[ext_resource path="res://audioManager.gd" type="Script" id=1]
[node name="audioManager" type="Control"]
script = ExtResource( 1 )
[node name="Tween" type="Tween" parent="."]
[node name="AudioPlayers" type="Node" parent="."]
[node name="Sfx" type="Node" parent="."]
''')
(P/'global.gd').write_text('''extends Node
var player = null
func get_player():
 return player
''')
(P/'player.gd').write_text('''extends Node2D
var collisions = true
func has_collisions():
 return collisions
''')
flags=(UP/'Scripts/global/globalData.gd').read_text();method=re.search(r'(func check_appear_disappear_flags\(.*?\n\treturn flag_on\n)',flags,re.S)[1];(P/'globaldata.gd').write_text('extends Node\nvar flags = {}\n'+method)
(P/'uiManager.gd').write_text('''extends Node
var cutscene = false
var battle = false
func is_in_cutscene():
 return cutscene
func is_in_battle():
 return battle
''')
(P/'bindings.json').write_text(json.dumps(r))
(P/'run.tscn').write_text('[gd_scene load_steps=2 format=2]\n[ext_resource path="res://run.gd" type="Script" id=1]\n[node name="Reference" type="Node"]\nscript = ExtResource( 1 )\n')
(P/'run.gd').write_text('''extends Node
var areas = {}
var snapshots = []
func _ready():
 call_deferred("run")
func snapshot(label):
 var result = {"label":label,"voices":[],"areas":[]}
 for p in audioManager.get_audio_player_list():
  result.voices.append({"track":p.stream.resource_path if p.stream else "", "playing":p.playing, "volume":p.volume_db})
 for a in audioManager.musicChangers:
  result.areas.append(a.name)
 snapshots.append(result)
func enter(name):
 areas[name]._on_Area2D_body_entered(global.player)
func leave(name):
 areas[name]._on_MusicArea_body_exited(global.player)
func run():
 var player = Node2D.new()
 player.set_script(load("res://player.gd"))
 get_tree().root.add_child(player)
 global.player = player
 var f=File.new()
 f.open("res://bindings.json",File.READ)
 var data=JSON.parse(f.get_as_text()).result
 f.close()
 var packed=load("res://MusicChanger.tscn")
 for r in data.regions:
  var a=packed.instance()
  a.name=r.source_path.substr(6)
  for t in data.tracks:
   if t.stable_id==r.track_id:a.loop=t.source_path.get_file()
  a.volume_db=r.volume_db
  a.fadein_length=r.fadein_seconds
  a.fadeout_length=r.fadeout_seconds
  a.appear_flag=r.appear_flag
  a.disappear_flag=r.disappear_flag
  a.monitoring=false
  get_tree().root.add_child(a)
  areas[a.name]=a
 audioManager.get_node("Tween").set_active(false)
 enter("MusicArea")
 snapshot("first_immediate")
 enter("MusicArea3")
 snapshot("same_song_reuse")
 leave("MusicArea3")
 enter("MusicArea3")
 yield(get_tree(),"idle_frame")
 snapshot("same_idle_reentry")
 leave("MusicArea")
 yield(get_tree(),"idle_frame")
 snapshot("shared_exit")
 uiManager.cutscene=true
 leave("MusicArea3")
 yield(get_tree(),"idle_frame")
 snapshot("cutscene_exit_ignored")
 uiManager.cutscene=false
 uiManager.battle=true
 enter("MusicArea6")
 snapshot("battle_enter_ignored")
 uiManager.battle=false
 enter("MusicArea2")
 snapshot("false_flag_ignored")
 enter("MusicArea6")
 snapshot("different_song_crossfade")
 leave("MusicArea3")
 leave("MusicArea6")
 yield(get_tree(),"idle_frame")
 enter("MusicArea")
 snapshot("rapid_duplicate_A")
 var output=File.new()
 output.open("res://../reference.json",File.WRITE)
 output.store_string(JSON.print(snapshots,"  "))
 output.close()
 print("ORIGINAL MUSIC CALLBACK REFERENCE COMPLETE")
 get_tree().quit()
''')
for label,command in [('import',[str(GODOT),'--path',str(P),'--editor','--quit']),('reference',[str(GODOT),'--path',str(P)])]:
 env=os.environ.copy();env['XDG_DATA_HOME']=str(OUT/'xdg-data');env['XDG_CONFIG_HOME']=str(OUT/'xdg-config');env['XDG_CACHE_HOME']=str(OUT/'xdg-cache');proc=subprocess.run(command,capture_output=True,text=True,timeout=120,env=env);(OUT/(label+'.log')).write_text(proc.stdout+proc.stderr)
 if proc.returncode:raise RuntimeError(label+' failed '+str(proc.returncode))
ref=json.loads((OUT/'reference.json').read_text());assert [len(x['voices']) for x in ref]==[1,1,1,1,1,1,1,2,3]
assert ref[0]['voices'][0]['volume']==0
assert ref[-1]['voices'][0]['track']==ref[-1]['voices'][2]['track'] and ref[-1]['voices'][1]['track']!=ref[-1]['voices'][0]['track']
receipt={'source_commit':r['upstream_commit'],'source_hashes':r['sources'],'godot_sha256':hashlib.sha256(GODOT.read_bytes()).hexdigest(),'scope':'Actual embedded MusicChanger and audioManager methods, original compressed source audio copied into isolated import directory; unrelated startup SFX dictionary replaced by empty and manager self-singleton qualification replaced by self to avoid isolated autoload parser cycle; game/UI/flags/player stubs provide callback context. Source callbacks invoked directly, not physics overlap or audio audibility validation.','cases':len(ref),'passed':True}
(OUT/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n');print(json.dumps(receipt))
