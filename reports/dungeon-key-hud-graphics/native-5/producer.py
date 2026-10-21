"""Measure the unchanged KeyCount scene using its original icon/font resources.

This probe sets label text directly to observe native layout, not gameplay or
counter updates. State/tween behavior is the separately reviewed indicator oracle.
"""
import argparse,hashlib,json,re,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.extract_battle_entry import Extractor,require,PIN
from tools.run_scene_reference import invoke
RECIPE=ROOT/'content/catacombs-key-hud-graphics.json'
PROBE='''extends Node
var hud
var rows=[]
func vec(v):return [v.x,v.y]
func color(v):return [v.r,v.g,v.b,v.a]
func _ready():call_deferred("run")
func run():
    hud=load("res://Nodes/Ui/KeyCount.tscn").instance()
    add_child(hud)
    var panel=hud.get_node("HBoxContainer")
    var icon=panel.get_node("TextureRect")
    var label=panel.get_node("Money")
    var font=label.get_font("font")
    for value in ["x 4","x 0","x 1","x 2","x 3"]:
        label.text=value
        for _i in range(4):yield(get_tree(),"idle_frame")
        rows.append({"text":value,"panel_position":vec(panel.rect_position),"panel_size":vec(panel.rect_size),"icon_position":vec(icon.rect_position),"icon_size":vec(icon.rect_size),"label_position":vec(label.rect_position),"label_size":vec(label.rect_size),"string_size":vec(font.get_string_size(value)),"label_minimum":vec(label.get_minimum_size()),"separation":panel.get_constant("separation"),"align":label.align,"valign":label.valign,"font_height":font.get_height(),"font_ascent":font.get_ascent(),"text_color":color(label.get_color("font_color")),"shadow_color":color(label.get_color("font_color_shadow")),"outline_color":color(font.outline_color),"outline_size":font.outline_size,"char_spacing":font.extra_spacing_char,"space_spacing":font.extra_spacing_space})
    var advances=[]
    for cp in [32,48,49,50,51,52,120]:advances.append([cp,font.get_char_size(cp,32).x])
    var f=File.new()
    if f.open("res://../layout.json",File.WRITE)!=OK:
        get_tree().quit(2)
        return
    f.store_string(JSON.print({"engine":Engine.get_version_info(),"rows":rows,"advances":advances},"  "))
    f.close()
    print("KEY_HUD_LAYOUT_NATIVE_COMPLETE rows=",rows.size())
    get_tree().quit(0)
'''
def run(out,engine):
 out=Path(out).resolve();engine=Path(engine).resolve()
 require(out.is_relative_to(ROOT/'build')and not out.exists()and engine.is_file(),'Fresh native key HUD layout project required')
 from tools import dungeon_key_hud_graphics as graphics
 p,checked_source=graphics.read();require(p['commit']==PIN and hashlib.sha256(engine.read_bytes()).hexdigest()==p['engine']['native_sha256'],'Key HUD layout engine/pin changed')
 ex=Extractor(ROOT);project=out/'project';project.mkdir(parents=True);(project/'.owned-key-hud-layout').write_text('source-key-hud-layout-v1',encoding='ascii')
 (out/'producer.py').write_bytes(Path(__file__).read_bytes())
 (out/'review.json').write_bytes(RECIPE.read_bytes())
 # Import the image before installing a scripted scene or autoloads. The native
 # editor otherwise does not terminate in this environment; gameplay is never
 # executed in the editor and no timeout is treated as a successful import.
 icon=p['paths']['icon'];raw=ex.data(icon);require(hashlib.sha256(raw).hexdigest()==p['sources'][icon],'Key HUD icon changed')
 image=project/icon;image.parent.mkdir(parents=True,exist_ok=True);image.write_bytes(raw)
 (project/'project.godot').write_text('config_version=4\n[application]\nconfig/name="Original key HUD image import"\n',encoding='utf-8')
 (out/'import-project.godot').write_bytes((project/'project.godot').read_bytes())
 invoke(engine,project,['--video-driver','GLES2','--debug','--editor','--quit'],out/'import.log')

 copied={}
 for name,sha in p['sources'].items():
  raw=ex.data(name);require(hashlib.sha256(raw).hexdigest()==sha,'Key HUD layout source changed: '+name)
  if name=='LICENSE':continue
  target=project/name;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(raw);copied[name]={'sha256':sha,'bytes':len(raw)}
 (project/'probe.gd').write_text(PROBE,encoding='utf-8')
 (project/'ui.gd').write_text('extends Node\nfunc get_key_count():return 0\n',encoding='utf-8')
 (project/'probe.tscn').write_text('[gd_scene load_steps=2 format=2]\n[ext_resource path="res://probe.gd" type="Script" id=1]\n[node name="Probe" type="Node"]\nscript=ExtResource( 1 )\n',encoding='utf-8')
 (project/'project.godot').write_text('config_version=4\n[application]\nconfig/name="Original key HUD layout"\nrun/main_scene="res://probe.tscn"\n[autoload]\nuiManager="*res://ui.gd"\n[display]\nwindow/size/width=320\nwindow/size/height=180\n',encoding='utf-8')
 (out/'source.json').write_text(json.dumps({'recipe_sha256':hashlib.sha256(RECIPE.read_bytes()).hexdigest(),'sources':copied,'engine_sha256':p['engine']['native_sha256'],'rendering_backend':'GLES2; native layout/metrics only, no pixel observation','scope':'Unchanged original scene/script/font/icon; label text is set by the observer, original script methods and tweens are not invoked. Native geometry and metrics only, no pixel/gameplay/emulator claim.'},indent=2)+'\n',encoding='utf-8')
 invoke(engine,project,['--video-driver','GLES2'],out/'engine.log')
 result=json.loads((out/'layout.json').read_text('utf-8'));require(result['engine']['hash']==p['engine']['commit']and len(result['rows'])==5 and 'KEY_HUD_LAYOUT_NATIVE_COMPLETE rows=5'in(out/'engine.log').read_text('utf-8'),'Key HUD layout output incomplete')
 for name in['probe.gd','ui.gd','probe.tscn','project.godot']:(out/name).write_bytes((project/name).read_bytes())
 print('Original key HUD:5 text layouts,7 glyph advances; scene/icon/font unchanged; no tween/gameplay claim')
 return result
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',type=Path,required=True);p.add_argument('--godot',type=Path,required=True);a=p.parse_args();run(a.out,a.godot)
