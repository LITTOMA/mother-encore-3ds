#!/usr/bin/env python3
"""Prepare a bounded native GLES2 probe of Baby's omitted palette divisor.

It copies pinned resources to a fresh isolated project and compares the original
shader against each fixed palette row at the same native TIME. No original game
script is executed and no shader fix is applied to production content.
"""
import argparse, hashlib, json, re, shutil, sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from tools.extract_battle_entry import Extractor, properties

PROBE='''extends SceneTree
var views=[]
var settings=[]
var outputs=[]
var ticks=0
func _init():
 call_deferred("run")
func run():
 var version=Engine.get_version_info()
 assert(version.hash=="3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8")
 var file=File.new()
 assert(file.open("res://layers.json",File.READ)==OK)
 settings=JSON.parse(file.get_as_text()).result
 file.close()
 var texture=load_texture("res://baby.png")
 var palette=load_texture("res://baby_pal.png")
 var original=load("res://default_shader.tres")
 for layer in range(2):
  var group=[]
  for row in range(-1,4):
   var viewport=Viewport.new()
   viewport.size=Vector2(320,180)
   viewport.usage=Viewport.USAGE_2D
   viewport.render_target_update_mode=Viewport.UPDATE_ALWAYS
   viewport.transparent_bg=false
   get_root().add_child(viewport)
   var rect=TextureRect.new()
   rect.texture=texture
   rect.rect_size=Vector2(320,180)
   rect.stretch_mode=TextureRect.STRETCH_TILE
   var material=ShaderMaterial.new()
   var shader=original.duplicate()
   if row>=0:
    shader.code=shader.code.replace("mod(-TIME * palette_shifting_speed * 1.0 / float(palette_anim_frame_count), 1.0)",str((row+0.5)/4.0))
   material.shader=shader
   for key in settings[layer]:
    var value=settings[layer][key]
    if key in ["shader","texture","texture_stretch"]:continue
    if key=="palette":value=palette
    elif value is Array:value=Vector2(value[0],value[1])
    material.set_shader_param(key,value)
   rect.material=material
   viewport.add_child(rect)
   group.append(viewport)
  views.append(group)
 for sample in range(4):
  for _i in range(30):yield(self,"idle_frame")
  yield(VisualServer,"frame_post_draw")
  var record={"sample":sample,"layers":[]}
  for layer in range(2):
   var actual=views[layer][0].get_texture().get_data()
   actual.flip_y()
   actual.save_png("res://result/source-layer%d-sample%d.png"%[layer,sample])
   var data=actual.get_data()
   var matches=[]
   for row in range(4):
    var candidate=views[layer][row+1].get_texture().get_data()
    candidate.flip_y()
    if data==candidate.get_data():matches.append(row)
   var rect=views[layer][0].get_child(0)
   record.layers.append({"layer":layer,"matching_fixed_palette_rows":matches,"uniform_value":rect.material.get_shader_param("palette_anim_frame_count"),"control_size":[rect.rect_size.x,rect.rect_size.y],"texture_size":[texture.get_width(),texture.get_height()]})
  outputs.append(record)
 assert(file.open("res://result/reference.json",File.WRITE)==OK)
 file.store_string(JSON.print({"engine":version,"driver":OS.get_video_driver_name(OS.get_current_video_driver()),"samples":outputs,"scope":"Original pinned Baby shader vs fixed palette row counterfactuals at identical native TIME, GLES2 GPU. No claim of portable undefined GLSL division semantics."},"  "))
 file.close()
 print("DOLL_BACKGROUND_REFERENCE_COMPLETE")
 quit()
func load_texture(path):
 var img=Image.new()
 assert(img.load(path)==OK)
 var tex=ImageTexture.new()
 tex.create_from_image(img,0)
 return tex
'''

def prepare(out):
 ex=Extractor(ROOT)
 if out.exists():raise ValueError('Use fresh output project')
 out.mkdir(parents=True);(out/'result').mkdir()
 for src,dest in [('Graphics/Battle BGS/baby.png','baby.png'),('Graphics/Battle BGS/baby_pal.png','baby_pal.png'),('addons/distortionator_integration/default_shader.tres','default_shader.tres')]:
  (out/dest).write_bytes(ex.data(src))
 text=ex.text('Graphics/Battle BGS/baby.bbg')
 layers=[properties(m[2]) for m in re.finditer(r'^\[Layer (\d+)\]\s*\n(.*?)(?=^\[|\Z)',text,re.M|re.S)]
 if len(layers)!=2 or any('palette_anim_frame_count' in p for p in layers):raise ValueError('Probe omission scope changed')
 (out/'layers.json').write_text(json.dumps(layers,indent=2)+'\n')
 (out/'probe.gd').write_text(PROBE)
 (out/'project.godot').write_text('config_version=4\n[application]\nconfig/name="Doll source palette-divisor probe"\n[display]\nwindow/size/width=320\nwindow/size/height=180\n[rendering]\nquality/driver/driver_name="GLES2"\n[logging]\nfile_logging/enable_logging=false\n')
 (out/'README.txt').write_text('Run official Godot3.6.2 GUI binary (not headless):\n  Godot_v3.6.2-stable_x11.64 --path '+str(out.resolve())+' --video-driver GLES2 --fixed-fps 60 -s probe.gd\nCaptures original source shader at four native frames, compares pixel bytes to each fixed palette row, and writes result/reference.json and eight original PNGs. Source texture sampling nearest; STRETCH_TILE native Godot semantics. This is an isolated resource probe, not the game or Azahar. Keep complete stdout/stderr. Original source has no palette_anim_frame_count assignment; no guessed4 divisor is used.\n')
 (out/'source.json').write_text(json.dumps({'commit':ex.lock['commit'],'sources':ex.sources,'generator_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest()},indent=2)+'\n')
 return out
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--out',type=Path,default=ROOT/'build/doll-background-reference/project');a=p.parse_args();print(prepare(a.out))
