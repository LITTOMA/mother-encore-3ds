#!/usr/bin/env python3
"""Native Godot proof of source NPC sheet offsets and initial Doll Idle.

Only exact reviewed character_sprite methods run in a fresh build copy. The
upstream checkout remains read-only, and this is not a Doll cutscene adapter.
"""
import argparse,json,os,shutil,subprocess,sys
from pathlib import Path
import yaml
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from tools.extract_battle_entry import node
from tools.reference_movement import extract
from tools.house_assets import RECIPE,validate_source,sha
from tools.upstream import read_json,write_json

def main():
 ap=argparse.ArgumentParser();ap.add_argument('--godot',type=Path,required=True);ap.add_argument('--work',type=Path,required=True);ap.add_argument('--reports',type=Path,required=True);ap.add_argument('--stress-talk',action='store_true');a=ap.parse_args()
 source=ROOT/'upstream/MOTHER-Encore';recipe=read_json(RECIPE);validate_source(source,recipe,read_json(ROOT/'upstream.lock'))
 work=a.work.resolve();reports=a.reports.resolve()
 if work.exists() or reports.exists() or not work.is_relative_to(ROOT/'build') or not reports.is_relative_to(ROOT/'reports'):raise ValueError('Need fresh build and reports directories')
 work.mkdir(parents=True);reports.mkdir(parents=True)
 path='Scripts/Main/character_sprite.gd';methods,hashes=extract((source/path).read_bytes(),recipe['sources'][path],['_create_animations','_create_tree','travel','get_state','blend_position','set_spritesheet'])
 (work/'sprite.gd').write_text('''extends Sprite
signal sprite_changed
var _json_data
var dir = 0
var _direction = Vector2.ZERO
var animationState
var animationTree
var _current_state = ""
var _directional_tags = []
var _all_tags = []
var _auto_offset = true
var _default_offset = Vector2.ZERO
var sprite = ""
onready var _anim_player = $AnimationPlayer
onready var _sprite_frame_path: NodePath = ".:frame"
'''+methods)
 cases=[];house=(source/'Maps/podunk/Nintens House.tscn').read_text();npc=(source/'Nodes/Reusables/npc.tscn').read_text()
 for actor,role in [('Objects/npc','carol'),('Objects/npc2','mimmie'),('Objects/npcdoll','doll')]:
  instance=node(house,actor);resource=next(r for r in recipe['resources'] if r['role']==role)
  shutil.copyfile(source/resource['source'],work/(role+'.png'))
  animation_path=instance.get('yaml','res://Data/Animations/4dir.yaml').removeprefix('res://')
  cases.append(dict(role=role,animation=yaml.safe_load((source/animation_path).read_text()),position=node(npc,'CharacterSprite')['position']))
 write_json(work/'cases.json',cases)
 (work/'project.godot').write_text('config_version=4\n[logging]\nfile_logging/enable_logging=false\n')
 (work/'probe.gd').write_text('''extends SceneTree
func read_json(path):
 var f=File.new()
 assert(f.open(path,File.READ)==OK)
 return JSON.parse(f.get_as_text()).result
func _init():
 call_deferred("run")
func run():
 assert(Engine.get_version_info().hash=="3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8")
 var results=[]
 for c in read_json("res://cases.json"):
  var s=load("res://sprite.gd").new()
  var ap=AnimationPlayer.new()
  ap.name="AnimationPlayer"
  s.add_child(ap)
  get_root().add_child(s)
  s._json_data=c.animation
  s.sprite="res://"+c.role+".png"
  s.position=Vector2(c.position[0],c.position[1])
  s.animationTree=AnimationTree.new()
  s.add_child(s.animationTree)
  s._create_animations([["Talk","Idle",2]])
  s.set_spritesheet()
  s.animationTree.process_mode=AnimationTree.ANIMATION_PROCESS_MANUAL
  s.blend_position(Vector2.DOWN)
  s.travel("Idle")
  s.animationTree.advance(0)
  var result={"role":c.role,"offset":[s.offset.x,s.offset.y],"combined_offset":[s.position.x+s.offset.x,s.position.y+s.offset.y],"frame":s.frame,"state":s.animationState.get_current_node(),"frames":[]}
  for tick in range(240):
   s.blend_position(Vector2(tick%3-1,tick%5-2))
   s.travel("Idle")
   if tick%2 and (c.role=="doll" or OS.get_environment("HOUSE_PROFILE_STRESS")=="1"):s.travel("Talk")
   s.animationTree.advance(1.0/60.0)
   result.frames.append({"tick":tick,"frame":s.frame,"state":s.animationState.get_current_node()})
  results.append(result)
  s.free()
 var f=File.new()
 assert(f.open(OS.get_environment("HOUSE_PROFILE_OUT"),File.WRITE)==OK)
 f.store_string(JSON.print({"engine":Engine.get_version_info(),"profiles":results},"  "))
 f.close()
 quit(0)
''')
 env=dict(os.environ,XDG_DATA_HOME=str(work/'userdata'),HOUSE_PROFILE_OUT=str(reports/'reference.json'),HOUSE_PROFILE_STRESS='1' if a.stress_talk else '0')
 for label,args in [('import',['--editor','--quit']),('reference',['-s','probe.gd'])]:
  result=subprocess.run([str(a.godot.resolve()),'--path',str(work),*args],env=env,capture_output=True,text=True,timeout=30)
  (reports/(label+'.log')).write_text(result.stdout+result.stderr)
  if result.returncode or 'SCRIPT ERROR' in result.stderr:raise ValueError('Native profile '+label+' failed')
 data=read_json(reports/'reference.json');presentation=read_json(ROOT/'content/native-house-presentation.json');profiles={p['role']:p for p in presentation['profiles']}
 for result in data['profiles']:
  p=profiles[result['role']]
  if result['combined_offset']!=p['sprite_offset'] or result['state']!='Idle':raise ValueError('Native profile offset/initial-state mismatch')
  if result['role']=='doll' and (result['frame']!=0 or any(f['frame']!=0 or f['state']!='Idle' for f in result['frames'])):raise ValueError('Source Doll changed from initial Idle')
 if data['profiles'][0]['frames']!=data['profiles'][1]['frames']:raise ValueError('Source shared 4dir profile behavior changed')
 data.update(commit=recipe['commit'],stress_talk=a.stress_talk,methods=hashes,sources={p:recipe['sources'][p] for p in ['Scripts/Main/character_sprite.gd','Data/Animations/4dir.yaml','Data/Animations/Floater.yaml','Nodes/Reusables/npc.tscn','Maps/podunk/Nintens House.tscn']},presentation_sha256=sha(ROOT/'content/native-house-presentation.json'),scope='Exact source character sprite methods in native Godot3.6.2; original textures, automatic offset, four-direction equivalence and 240-frame initial Doll Idle; no cutscene/battle implementation')
 write_json(reports/'reference.json',data)
 with (reports/'frames.tsv').open('w') as stream:
  for profile in data['profiles']:
   for row in profile['frames']:
    tick=row['tick'];talk=tick%2 if profile['role']=='doll' or a.stress_talk else 0;stream.write(f"{profile['role']} {tick} {talk} {tick%3-1} {tick%5-2} {row['frame']} {row['state']}\n")
 print('Verified3 native source profiles, 720 animation samples, original offsets and initial Doll Idle')
if __name__=='__main__':main()
