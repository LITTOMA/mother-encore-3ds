#!/usr/bin/env python3
"""Native AnimationPlayer callback and original Shaker oracle, no rendering."""
import argparse,json,os,re,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'tools'))
from extract_battle_entry import Extractor,properties,one,node,require
from run_doll_growth_reference import method

def main():
 ap=argparse.ArgumentParser();ap.add_argument('--godot',type=Path,required=True);ap.add_argument('--work',type=Path,default=ROOT/'build/doll-boss-reference');ap.add_argument('--reports',type=Path,default=ROOT/'reports/doll-round/boss-reference');a=ap.parse_args();a.work.mkdir(parents=True,exist_ok=True);a.reports.mkdir(parents=True,exist_ok=True);ex=Extractor(ROOT)
 for path,name,out,track in [('Nodes/Ui/Battle/EnemySprite.tscn','bossDefeat','boss.tres',5),('Nodes/Ui/Battle/BossDefeatFlash.tscn','DefeatFlash','flash.tres',3)]:
  source=ex.text(path);rid=node(source,'AnimationPlayer')['anims/'+name]['SubResource'];body=one(r'^\[sub_resource type="Animation" id='+str(rid)+r'\]\n(.*?)(?=^\[|\Z)',source,name,re.M|re.S)[1]
  props=properties(body);start=body.index('tracks/'+str(track)+'/type');end=body.find('tracks/'+str(track+1)+'/type',start);selected=body[start:end if end>=0 else len(body)].replace('tracks/'+str(track)+'/', 'tracks/0/')
  (a.work/out).write_text('[gd_resource type="Animation" format=2]\n[resource]\nlength = '+str(props['length'])+'\n'+selected)
 shaker=ex.text('Scripts/misc/Shaker.gd').replace('class_name Shaker','').replace(' -> Shaker:', ':');(a.work/'shaker.gd').write_text(shaker)
 enemy=ex.text('Scripts/UI/Battle/EnemySprite.gd');owner='''extends Node2D
const Shaker=preload("res://shaker.gd")
var frame=0
var events=[]
var won=false
var flash_started=false
var boss
var flash
func setup():
 var sprite=Sprite.new()
 sprite.name="Sprite"
 add_child(sprite)
 boss=AnimationPlayer.new()
 boss.name="Boss"
 boss.method_call_mode=AnimationPlayer.ANIMATION_METHOD_CALL_IMMEDIATE
 add_child(boss)
 boss.add_animation("boss",load("res://boss.tres"))
 boss.connect("animation_finished",self,"boss_done")
 flash=AnimationPlayer.new()
 flash.name="Flash"
 flash.method_call_mode=AnimationPlayer.ANIMATION_METHOD_CALL_IMMEDIATE
 add_child(flash)
 flash.add_animation("flash",load("res://flash.tres"))
 flash.connect("animation_finished",self,"flash_done")
 boss.play("boss")
func boss_done(_name):events.append({"event":"enemy_hidden","frame":frame})
func start_boss_defeat_flash():
 events.append({"event":"flash_started","frame":frame})
 flash_started=true
 flash.play("flash")
func defeat_enemies():events.append({"event":"defeat_enemies","frame":frame})
func flash_done(_name):
 events.append({"event":"victory_requested","frame":frame})
 won=true
func tick(delta):
 frame+=1
 for child in $Sprite.get_children():
  if child._shaked_object!=null:child._physics_process(delta)
 boss.advance(delta)
 if flash_started:flash.advance(delta)
 if frame in [388,389,390]:events.append({"event":"flash_position","frame":frame,"value":"%.17f"%flash.current_animation_position})
'''+method(enemy,'shake')
 (a.work/'owner.gd').write_text(owner)
 probe='''extends SceneTree
func _init():
 call_deferred("run")
func run():
 var owner=load("res://owner.gd").new()
 get_root().add_child(owner)
 owner.setup()
 seed(725)
 var stream=StreamPeerBuffer.new()
 stream.put_float(1.0/60.0)
 stream.seek(0)
 var delta=stream.get_float()
 for n in range(600):
  owner.tick(delta)
  if owner.won:break
 var next=str(randi())
 var f=File.new()
 var output=""
 for arg in OS.get_cmdline_args():
  if arg.begins_with("--out="):output=arg.substr(6)
 assert(f.open(output,File.WRITE)==OK)
 f.store_string(JSON.print({"engine":Engine.get_version_info(),"events":owner.events,"frames":owner.frame,"next_randi":next},"  "))
 f.close()
 print("DOLL_BOSS_REFERENCE_COMPLETE")
 quit()
'''
 (a.work/'probe.gd').write_text(probe);(a.work/'project.godot').write_text('config_version=4\n[application]\nconfig/name="Source boss callback oracle"\n[logging]\nfile_logging/enable_logging=false\n')
 env=dict(os.environ)
 for key in['XDG_DATA_HOME','XDG_CONFIG_HOME','XDG_CACHE_HOME']:
  d=a.work/key.lower();d.mkdir(exist_ok=True);env[key]=str(d.resolve())
 cmd=[str(a.godot.resolve()),'--path',str(a.work.resolve()),'--script',str((a.work/'probe.gd').resolve()),'--out='+str((a.reports/'reference.json').resolve())]
 with(a.reports/'godot.txt').open('wb')as log:run=subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT,timeout=25,env=env)
 log=(a.reports/'godot.txt').read_text();require(run.returncode==0 and 'SCRIPT ERROR'not in log and 'ERROR:'not in log and 'DOLL_BOSS_REFERENCE_COMPLETE'in log,'Boss oracle failed: '+log[-1300:]);r=json.loads((a.reports/'reference.json').read_text());require(r['engine']['string']=='3.6.2-stable (official)','Unexpected engine')
 (a.reports/'receipt.json').write_text(json.dumps({'sources':ex.sources,'commit':ex.lock['commit'],'adapters':['native AnimationPlayer method tracks extracted without audio/visual tracks','method calls set immediate inside manually advanced idle pass; source Shaker physics ticks precede that pass','audio/video not tested; scalar frame/RNG callback ordering tested']},indent=2)+'\n');print(json.dumps(r))
if __name__=='__main__':main()
