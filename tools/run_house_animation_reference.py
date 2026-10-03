#!/usr/bin/env python3
"""Run unchanged character_sprite animation construction in native Godot3.6.2."""
import argparse,hashlib,json,subprocess,sys
from pathlib import Path
import yaml
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from tools.reference_movement import extract
from tools.house_assets import validate_source,RECIPE
from tools.upstream import read_json,write_json

def main():
 ap=argparse.ArgumentParser();ap.add_argument('--godot',type=Path,required=True);ap.add_argument('--work',type=Path,required=True);ap.add_argument('--reports',type=Path,required=True);a=ap.parse_args()
 root=ROOT/'upstream/MOTHER-Encore';recipe=read_json(RECIPE);validate_source(root,recipe,read_json(ROOT/'upstream.lock'))
 work=a.work.resolve();reports=a.reports.resolve()
 if work.exists() or reports.exists() or not work.is_relative_to(ROOT/'build') or not reports.is_relative_to(ROOT/'reports'):raise ValueError('Need fresh build/reports subdirectories')
 work.mkdir(parents=True);reports.mkdir(parents=True)
 path='Scripts/Main/character_sprite.gd';methods,hashes=extract((root/path).read_bytes(),recipe['sources'][path],['_create_animations','_create_tree','travel','get_state','blend_position'])
 (work/'sprite.gd').write_text('''extends Sprite
var _json_data
var dir = 0
var _direction = Vector2.ZERO
var animationState
var animationTree
var _current_state = ""
var _directional_tags = []
var _all_tags = []
onready var _anim_player = $AnimationPlayer
onready var _sprite_frame_path: NodePath = ".:frame"
'''+methods)
 write_json(work/'animation.json',yaml.safe_load((root/'Data/Animations/4dir.yaml').read_text()))
 (work/'project.godot').write_text('config_version=4\n[logging]\nfile_logging/enable_logging=false\n')
 (work/'probe.gd').write_text('''extends SceneTree
var results=[]
func read_json(path):
 var f=File.new()
 assert(f.open(path,File.READ)==OK)
 return JSON.parse(f.get_as_text()).result
func _init():
 call_deferred("run")
func run():
 assert(Engine.get_version_info().hash=="3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8")
 for c in ["talk_to_idle","direction_change","direction_ties"]:
  var s=load("res://sprite.gd").new()
  var ap=AnimationPlayer.new()
  ap.name="AnimationPlayer"
  s.add_child(ap)
  get_root().add_child(s)
  s._json_data=read_json("res://animation.json")
  s.animationTree=AnimationTree.new()
  s.add_child(s.animationTree)
  s._create_animations([["Talk","Idle",2]])
  s.animationTree.process_mode=AnimationTree.ANIMATION_PROCESS_MANUAL
  s.blend_position(Vector2.DOWN)
  s.animationTree.advance(0)
  for tick in range(48):
   var direction=Vector2.DOWN
   if c=="direction_change" and tick>=8:direction=Vector2.LEFT
   if c=="direction_ties":direction=[Vector2(1,1),Vector2(-1,1),Vector2(1,-1),Vector2(-1,-1)][tick/12]
   s.blend_position(direction)
   s.travel("Idle")
   var talk=tick<15
   if talk:s.travel("Talk")
   s.animationTree.advance(1.0/60.0)
   results.append({"case":c,"tick":tick,"talk":talk,"frame":s.frame,"state":s.animationState.get_current_node(),"position":"%.17f"%s.animationState.get_current_play_position()})
  s.free()
 var f=File.new()
 assert(f.open(OS.get_environment("HOUSE_ANIMATION_OUT"),File.WRITE)==OK)
 f.store_string(JSON.print({"engine":Engine.get_version_info(),"frames":results},"  "))
 f.close()
 quit(0)
''')
 import os
 run=subprocess.run([str(a.godot.resolve()),'--path',str(work),'-s','probe.gd'],env=dict(os.environ,HOUSE_ANIMATION_OUT=str(reports/'reference.json'),XDG_DATA_HOME=str(work/'userdata')),capture_output=True,text=True,timeout=30)
 (reports/'reference.log').write_text(run.stdout+run.stderr)
 if run.returncode or 'SCRIPT ERROR' in run.stderr:raise ValueError('Native animation reference failed')
 result=read_json(reports/'reference.json');result['commit']=recipe['commit'];result['sources']={path:recipe['sources'][path],'Data/Animations/4dir.yaml':recipe['sources']['Data/Animations/4dir.yaml']};result['methods']=hashes;result['scope']='Exact source animation creation/tree/travel/blend; manually stepped native AnimationTree; no NPC AI or rendering claim';write_json(reports/'reference.json',result)
 with (reports/'frames.tsv').open('w') as stream:
  for row in result['frames']:
   case=row['case'];tick=row['tick'];direction=[0,1]
   if case=='direction_change' and tick>=8:direction=[-1,0]
   if case=='direction_ties':direction=[[1,1],[-1,1],[1,-1],[-1,-1]][tick//12]
   stream.write(f"{case} {tick} {int(row['talk'])} {direction[0]} {direction[1]} {row['frame']} {row['state']}\n")
 print('Wrote',len(result['frames']),'native animation samples')
if __name__=='__main__':main()
