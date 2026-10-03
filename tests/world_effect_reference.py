#!/usr/bin/env python3
"""Prepare a numeric offscreen Godot3 source probe; never capture/display images.

The pinned shader text is unchanged except replacing TIME with an injected
uniform for reproducible arithmetic samples. Raw GPU data is compared by tests,
not encoded as pictures or exposed as screenshots.
"""
import argparse,hashlib,json,re,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from tools.world_effect_assets import SCENE,SHADER,IMAGE,Extractor,one,properties
PROBE='''extends SceneTree
var config
var v
var rect
var material
func _init():call_deferred("run")
func run():
 assert(Engine.get_version_info().hash=="3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8")
 var f=File.new()
 assert(f.open("res://config.json",File.READ)==OK)
 config=JSON.parse(f.get_as_text()).result
 f.close()
 var im=Image.new()
 assert(im.load("res://source.png")==OK)
 var texture=ImageTexture.new()
 texture.create_from_image(im,0)
 v=Viewport.new()
 v.usage=Viewport.USAGE_2D
 v.render_target_update_mode=Viewport.UPDATE_ALWAYS
 v.transparent_bg=true
 get_root().add_child(v)
 rect=TextureRect.new()
 rect.texture=texture
 rect.expand=true
 rect.stretch_mode=TextureRect.STRETCH_TILE
 material=ShaderMaterial.new()
 var shader=Shader.new()
 assert(f.open("res://source.shader",File.READ)==OK)
 shader.code=f.get_as_text()
 f.close()
 material.shader=shader
 for key in config.parameters:
  var value=config.parameters[key]
  if value is Array:value=Vector2(value[0],value[1])
  material.set_shader_param(key,value)
 rect.material=material
 v.add_child(rect)
 var records=[]
 for i in range(config.samples.size()):
  var sample=config.samples[i]
  v.size=Vector2(sample.width,sample.height)
  var offset=Vector2((sample.width-320)/2.0,(sample.height-180)/2.0)
  # Geometry extends around source origin; texture coordinates remain original.
  # For QA expanded samples, express negative UV origin in copied shader only.
  # This is the explicit display adapter, separate from 320x180 source evidence.
  material.set_shader_param("probe_origin",-offset/Vector2(16,10))
  rect.rect_size=v.size
  material.set_shader_param("probe_time",sample.time)
  rect.modulate=Color(sample.color[0],sample.color[1],sample.color[2],sample.color[3])
  rect.self_modulate=Color(1,1,1,sample.alpha)
  for _j in range(3):yield(self,"idle_frame")
  yield(VisualServer,"frame_post_draw")
  var result=v.get_texture().get_data()
  result.flip_y()
  result.convert(Image.FORMAT_RGBA8)
  var raw=result.get_data()
  var path="res://result/sample-%02d.rgba"%i
  assert(f.open(path,File.WRITE)==OK)
  f.store_buffer(raw)
  f.close()
  records.append({"sample":i,"bytes":raw.size(),"native_pixel_0":[raw[0],raw[1],raw[2],raw[3]],"rect_size":[rect.rect_size.x,rect.rect_size.y],"texture_flags":texture.flags})
 assert(f.open("res://result/native.json",File.WRITE)==OK)
 f.store_string(JSON.print({"engine":Engine.get_version_info(),"driver":OS.get_video_driver_name(OS.get_current_video_driver()),"samples":records,"scope":"Numeric offscreen source-render readback only. Original320x180 source and separately marked centered400x240 extension. Deterministic TIME uniform substitution."},"  "))
 f.close()
 print("WORLD_EFFECT_NUMERIC_REFERENCE_COMPLETE")
 quit()
'''
def prepare(out):
 ex=Extractor(ROOT);scene=ex.text(SCENE);shader=ex.text(SHADER)
 p=properties(one(r'^\[sub_resource type="ShaderMaterial" id=3\]\n(.*?)(?=^\[|\Z)',scene,'material',re.M|re.S)[1])
 parameters={k.removeprefix('shader_param/'):v for k,v in p.items()if k.startswith('shader_param/')}
 out.mkdir(parents=True,exist_ok=False);(out/'result').mkdir()
 (out/'source.png').write_bytes(ex.data(IMAGE))
 shader=re.sub(r'\bTIME\b','probe_time',shader)
 shader=shader.replace('shader_type canvas_item;','shader_type canvas_item;\nuniform float probe_time;\nuniform vec2 probe_origin;')
 shader=shader.replace('vec2 newuv = UV;','vec2 newuv = UV + probe_origin;').replace('newuv = UV;','newuv = UV + probe_origin;')
 (out/'source.shader').write_text(shader)
 samples=[]
 for width,height in [(320,180),(400,240)]:
  for time in [0,1/60,.25,.5,1,3.125,17.25,63.5]:samples.append(dict(width=width,height=height,time=time,color=[1,1,1,1],alpha=1))
 for color,alpha in [([.423529,.568627,.941176,1],1),([.423529,.568627,.941176,1],.5),([.423529,.568627,.941176,1],0),([.5392155,.5392155,.952941,1],.25)]:samples.append(dict(width=320,height=180,time=.25,color=color,alpha=alpha))
 (out/'config.json').write_text(json.dumps(dict(parameters=parameters,samples=samples),indent=2)+'\n')
 (out/'source.json').write_text(json.dumps(dict(commit=ex.lock['commit'],sources=ex.sources),indent=2)+'\n')
 (out/'probe.gd').write_text(PROBE)
 (out/'project.godot').write_text('config_version=4\n[application]\nconfig/name="Numeric world effect source probe"\n[display]\nwindow/size/width=320\nwindow/size/height=180\n[rendering]\nquality/driver/driver_name="GLES2"\n2d/snapping/use_gpu_pixel_snap=true\n[logging]\nfile_logging/enable_logging=false\n')
 return out
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--out',type=Path,required=True);a=p.parse_args();print(prepare(a.out))
