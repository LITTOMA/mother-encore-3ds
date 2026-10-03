#!/usr/bin/env python3
"""Run native AnimationPlayer/SceneTreeTween clocks without a visual surface."""
import argparse,json,re,subprocess,sys,os
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from tools.world_effect_assets import Extractor,SCENE,one
SCRIPT='''extends SceneTree
var finished=false
func done():finished=true
func _init():call_deferred("run")
func run():
 var parent=Node2D.new()
 get_root().add_child(parent)
 var bg=Node2D.new()
 bg.name="BG"
 parent.add_child(bg)
 var player=AnimationPlayer.new()
 parent.add_child(player)
 player.add_animation("Melody",load("res://melody.tres"))
 player.play("Melody")
 player.set_active(false)
 var tween=create_tween()
 tween.pause()
 tween.connect("finished",self,"done")
 tween.tween_property(bg,"self_modulate",Color8(255,255,255,255),0.5).from(Color8(255,255,255,0)).set_ease(Tween.EASE_OUT)
 var file=File.new()
 assert(file.open("res://timing.bin",File.WRITE)==OK)
 var alive=true
 var active=true
 for i in range(140):
  if i==100:
   tween=create_tween()
   tween.pause()
   finished=false
   tween.connect("finished",self,"done")
   tween.tween_property(bg,"self_modulate",Color8(255,255,255,0),0.5).set_ease(Tween.EASE_OUT)
   alive=true
  if active:player.advance(1.0/60.0)
  if alive:
   alive=tween.custom_step(1.0/60.0)
   if i>=100 and finished:
    active=false
    player.stop()
  file.store_32(i)
  file.store_double(player.current_animation_position if active else 0)
  file.store_float(bg.modulate.r)
  file.store_float(bg.modulate.g)
  file.store_float(bg.modulate.b)
  file.store_float(bg.modulate.a)
  file.store_float(bg.self_modulate.a)
  file.store_32(int(active))
 file.close()
 print("WORLD_EFFECT_NATIVE_TIMING_COMPLETE")
 quit()
'''
def main():
 p=argparse.ArgumentParser();p.add_argument('--godot',type=Path,required=True);p.add_argument('--out',type=Path,default=ROOT/'build/world-effect-timing-reference');a=p.parse_args();a.out.mkdir(parents=True,exist_ok=True)
 ex=Extractor(ROOT);scene=ex.text(SCENE);body=one(r'^\[sub_resource type="Animation" id=5\]\n(.*?)(?=^\[|\Z)',scene,'Melody animation',re.M|re.S)[1]
 (a.out/'melody.tres').write_text('[gd_resource type="Animation" format=2]\n[resource]\n'+body)
 (a.out/'probe.gd').write_text(SCRIPT);(a.out/'project.godot').write_text('config_version=4\n[application]\nconfig/name="Headless effect timing probe"\n[logging]\nfile_logging/enable_logging=false\n')
 result=subprocess.run([str(a.godot),'--path',str(a.out),'-s','probe.gd'],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=30,env={**os.environ,"XDG_DATA_HOME":str(a.out.resolve()/"userdata")})
 (a.out/'native.log').write_text(result.stdout);print(result.stdout);assert result.returncode==0 and 'WORLD_EFFECT_NATIVE_TIMING_COMPLETE' in result.stdout
if __name__=='__main__':main()
