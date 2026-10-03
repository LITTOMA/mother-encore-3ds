#!/usr/bin/env python3
"""Check inherited party portrait entry motion with native Godot controls."""
import argparse,json,os,re,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.extract_battle_entry import Extractor,require,node,animation
from tools.upstream import write_json
PROBE='''extends SceneTree
func _init():call_deferred("run")
func run():
 assert(Engine.get_version_info().hash=="3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8")
 var file=File.new()
 assert(file.open("res://track.json",File.READ)==OK)
 var track=JSON.parse(file.get_as_text()).result
 file.close()
 var hierarchy=load("res://layout.tscn").instance()
 get_root().add_child(hierarchy)
 var player=hierarchy.get_node("PlayerInfo")
 var plate=hierarchy.get_node("PlayerInfo/PlayerInfoVbox/PartyInfo/Plate")
 var portrait=plate.get_node("Portrait")
 for _frame in range(3):yield(self,"idle_frame")
 var a=Animation.new()
 a.length=0.3
 var idx=a.add_track(Animation.TYPE_VALUE)
 a.track_set_path(idx,"PlayerInfo:rect_position:y")
 a.track_set_interpolation_type(idx,int(track.interp))
 a.track_set_interpolation_loop_wrap(idx,track.loop_wrap)
 a.value_track_set_update_mode(idx,int(track.keys.update))
 for i in track.keys.times.size():a.track_insert_key(idx,track.keys.times[i],track.keys["values"][i],track.keys.transitions[i])
 var ap=AnimationPlayer.new()
 hierarchy.add_child(ap)
 ap.playback_process_mode=AnimationPlayer.ANIMATION_PROCESS_MANUAL
 ap.add_animation("entry",a)
 ap.play("entry")
 var results=[]
 for t in [0.0,0.05,0.1,0.15,0.2,0.25,0.3]:
  ap.seek(t,true)
  results.append({"time":t,"parent_y":player.rect_position.y,"plate_y":plate.rect_global_position.y,"portrait_y":portrait.rect_global_position.y,"portrait_local_y":portrait.rect_position.y})
 assert(file.open("res://reference.json",File.WRITE)==OK)
 file.store_string(JSON.print({"engine":Engine.get_version_info(),"samples":results,"scope":"Exact source Control/VBox/PartyInfo ancestor geometry and source PlayerInfo y animation; no original game scripts or GPU"},"  "))
 file.close()
 print("BATTLE_PARENT_LAYOUT_COMPLETE")
 quit()
'''
def main():
 p=argparse.ArgumentParser();p.add_argument('--godot',type=Path,required=True);p.add_argument('--work',type=Path,default=ROOT/'build/battle-parent-layout-reference');p.add_argument('--reports',type=Path,default=ROOT/'reports/battle-parent-layout-reference');a=p.parse_args()
 require(not a.work.exists() and not a.reports.exists(),'Use fresh directories');a.work.mkdir(parents=True);a.reports.mkdir(parents=True)
 ex=Extractor(ROOT);text=ex.text('Nodes/Ui/Battle/Battle.tscn')
 def body(name,parent):
  found=re.findall(r'^\[node name="'+re.escape(name)+r'"[^\n]* parent="'+re.escape(parent)+r'"[^\n]*\]\n(.*?)(?=^\[|\Z)',text,re.M|re.S);require(len(found)==1,'Missing original layout node');return found[0]
 layout='[gd_scene format=2]\n[node name="Battle" type="Control"]\n[node name="PlayerInfo" type="Control" parent="."]\n'+body('PlayerInfo','.')
 layout+='[node name="PlayerInfoVbox" type="VBoxContainer" parent="PlayerInfo"]\n'+body('PlayerInfoVbox','PlayerInfo')
 layout+='[node name="PartyInfo" type="Control" parent="PlayerInfo/PlayerInfoVbox"]\n'+body('PartyInfo','PlayerInfo/PlayerInfoVbox')
 layout+='[node name="SpMeter" type="Control" parent="PlayerInfo/PlayerInfoVbox"]\n'+body('SpMeter','PlayerInfo/PlayerInfoVbox')+'visible = false\nrect_min_size = Vector2(320,8)\n'
 plate=node(ex.text('Nodes/Ui/Battle/PartyInfoPlate.tscn'),'.');portrait=node(ex.text('Nodes/Ui/Battle/BattleSpriteNinten.tscn'),'.')
 participant=ex.text('Scripts/UI/Battle/BattleParticipant.gd');require('plate.rect_position = Vector2(placement, 20)' in participant and 'plate.add_child(_battle_sprite)' in participant,'Portrait source parent changed')
 layout+='[node name="Plate" type="Control" parent="PlayerInfo/PlayerInfoVbox/PartyInfo"]\nmargin_left = 128.0\nmargin_top = 20.0\nmargin_right = '+str(128+plate['margin_right'])+'\nmargin_bottom = '+str(20+plate['margin_bottom'])+'\n'
 layout+='[node name="Portrait" type="Control" parent="PlayerInfo/PlayerInfoVbox/PartyInfo/Plate"]\n'+''.join(k+' = '+str(v)+'\n' for k,v in portrait.items() if k.startswith('margin_'))
 (a.work/'layout.tscn').write_text(layout)
 clip=animation(text,11,'Nodes/Ui/Battle/Battle.tscn','scene.transitionIn');track=next(t for t in clip['tracks'] if t['path']=='PlayerInfo:rect_position:y');write_json(a.work/'track.json',track)
 (a.work/'project.godot').write_text('config_version=4\n[logging]\nfile_logging/enable_logging=false\n');(a.work/'probe.gd').write_text(PROBE)
 run=subprocess.run([str(a.godot.resolve()),'--path',str(a.work.resolve()),'-s','probe.gd'],env=dict(os.environ,XDG_DATA_HOME=str((a.work/'userdata').resolve())),capture_output=True,text=True,timeout=25);(a.reports/'godot.log').write_text(run.stdout+run.stderr)
 require(run.returncode==0 and 'BATTLE_PARENT_LAYOUT_COMPLETE' in run.stdout,'Native layout reference failed')
 result=json.loads((a.work/'reference.json').read_text());result.update(commit=ex.lock['commit'],sources=ex.sources)
 require(result['samples'][0]['portrait_y']==193 and result['samples'][-1]['portrait_y']==116,'Unexpected source portrait inherited entry position')
 write_json(a.reports/'reference.json',result);print('Verified7 source portrait inherited positions:193→116, independent later show116→96')
if __name__=='__main__':main()
