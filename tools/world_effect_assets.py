#!/usr/bin/env python3
"""Bounded pinned melodyBG adapter and checked standalone ENCWFX01 compiler.

The image, timing, keys and shader coefficients are data. Only the reviewed
vertical oscillation/scroll path is accepted; unreviewed source changes fail.
"""
import argparse, hashlib, io, json, math, re, struct, sys, zlib
from pathlib import Path
from PIL import Image
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from tools.extract_battle_entry import Extractor, PIN, animation, node, one, properties
SCENE='Nodes/Ui/effects/melodyBG.tscn'
SCRIPT='Nodes/Ui/effects/melodyBG.gd'
SHADER='Shaders/Distortionator.shader'
IMAGE='Graphics/UI/melody bg.png'
IR=ROOT/'content/native-world-effect.json'
PACK=ROOT/'romfs/data/melody.encfx'
FORMATS=['<4I19f','<6f','<I',None]
STRIDES=[92,24,4,1]
HEADER=128
SETTINGS=['source_width','source_height','appear_duration','disappear_duration','cycle_duration','opacity','move_x','move_y','amplitude_y','frequency_y','speed_y','translation_ping_pong_y','amplitude_ping_pong_y','move_divisor','pixel_snap_uv_epsilon','initial_r','initial_g','initial_b','initial_a']

def require(v,m):
 if not v:raise ValueError(m)
def fields(o,n):require(isinstance(o,dict) and set(o)==set(n),'Unknown/missing fields')
def digest(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def finite(v):return isinstance(v,(float,int)) and not isinstance(v,bool) and math.isfinite(v)

def extract():
 ex=Extractor(ROOT);scene=ex.text(SCENE);script=ex.text(SCRIPT);shader=ex.text(SHADER)
 imp=ex.text(IMAGE+'.import');project=ex.text('project.godot')
 require('flags/filter=false' in imp and 'flags/mipmaps=false' in imp and 'flags/repeat=0' in imp,'Changed texture sampler')
 require('2d/snapping/use_gpu_pixel_snap=true' in project,'Changed pixel snap setting')
 p=properties(one(r'^\[sub_resource type="ShaderMaterial" id=3\]\n(.*?)(?=^\[|\Z)',scene,'material',re.M|re.S)[1])
 b=node(scene,'BG');a=animation(scene,5,SCENE,'Melody');track=a['tracks'][0]
 require(len(a['tracks'])==1 and a['loop'] and track['path']=='BG:modulate' and track['interp']==1 and track['loop_wrap'] and track['keys']['update']==0,'Unreviewed color animation')
 require(b['stretch_mode']==2 and b['expand'] and b['margin_left']==-b['margin_right'] and b['margin_top']==-b['margin_bottom'],'Unreviewed effect geometry')
 for key in ['ping_pong_speed','osc_amp_ping_pong','compression_amplitude','compression_frequency','compression_speed','comp_amp_ping_pong','comp_trans_ping_pong','interlaced_amplitude','interlaced_frequency','interlaced_speed','inter_amp_ping_pong','inter_trans_ping_pong']:
  require(p['shader_param/'+key]==[0,0],'Unsupported shader parameter '+key)
 for key in ['oscillation_amplitude','oscillation_frequency','oscillation_speed','osc_trans_ping_pong']:require(p['shader_param/'+key][0]==0,'Unsupported horizontal distortion')
 require(not p['shader_param/palette_shifting'] and not p['shader_param/barrel'],'Unsupported shader mode')
 durations=re.findall(r'tween_property\(\$BG, "self_modulate", Color8\(255, 255, 255, (?:255|0)\), ([0-9.]+)\)',script)
 require(len(durations)==2 and script.count('.set_ease(Tween.EASE_OUT)')==2 and '.set_trans(' not in script,'Unreviewed fade behavior')
 require('vec4 modulate = COLOR;' in shader and 'COLOR.a *= opacity;' in shader and shader.count('modulate')==2,'Unreviewed shader modulation')
 require('TIME * move.y/0.5' in shader and 'osc_time.y = cos(osc_trans_ping_pong.y * TIME)' in shader,'Changed shader arithmetic')
 im=Image.open(io.BytesIO(ex.data(IMAGE))).convert('RGBA');palette=[];pixels=[]
 for c in list(im.getdata()):
  if list(c) not in palette:palette.append(list(c))
  pixels.append(palette.index(list(c)))
 s=[b['margin_right']-b['margin_left'],b['margin_bottom']-b['margin_top'],*map(float,durations),a['length'],p['shader_param/opacity'],*p['shader_param/move'],*[p['shader_param/'+n][1]for n in ['oscillation_amplitude','oscillation_frequency','oscillation_speed','osc_trans_ping_pong','osc_amp_ping_pong']],0.5,0.00001,*b['modulate']]
 result={'schema':1,'kind':'encore.world-effect.source-ir','commit':PIN,'sources':ex.sources,
 'scope':'Pinned melodyBG only; linear SceneTreeTween fades and looped linear color track; vertical oscillation and scroll with Godot3 canvas final modulation',
 'display_adapter':'1:1 centered extension: pixel at expanded (40,30) equals source (0,0); source UV origin and shader screen_size stay fixed; no art stretching',
 'engine_reference':{'version':'3.6.2-stable','canvas_source':'https://raw.githubusercontent.com/godotengine/godot/3.6.2-stable/drivers/gles3/shaders/canvas.glsl','semantics':'USE_PIXEL_SNAP adds 1e-5 to UV; final_modulate is multiplied after user fragment unless MODULATE is explicitly used'},
 'texture':{'width':im.width,'height':im.height,'palette':palette,'indices':pixels},'settings':dict(zip(SETTINGS,s)),
 'keys':[{'time':t,'ease':e,'color':v}for t,e,v in zip(track['keys']['times'],track['keys']['transitions'],track['keys']['values'])]}
 validate_ir(result);return result

def validate_ir(ir):
 fields(ir,['schema','kind','commit','sources','scope','display_adapter','engine_reference','texture','settings','keys'])
 require(ir['schema']==1 and ir['kind']=='encore.world-effect.source-ir' and ir['commit']==PIN,'Unreviewed effect identity')
 fields(ir['texture'],['width','height','palette','indices']);fields(ir['settings'],SETTINGS)
 t=ir['texture'];s=list(ir['settings'].values())
 require(all(type(t[k])==int and 0<t[k]<=256 for k in ['width','height']),'Texture dimensions')
 require(0<len(t['palette'])<=256 and len(t['indices'])==t['width']*t['height'],'Texture storage')
 require(all(type(c)==list and len(c)==4 and all(type(v)==int and 0<=v<=255 for v in c)for c in t['palette']),'Palette channels')
 require(all(type(v)==int and 0<=v<len(t['palette'])for v in t['indices']),'Texture indices')
 require(all(finite(v)for v in s),'Nonfinite effect setting')
 v=ir['settings'];require(all(1<=v[k]<=1024 for k in SETTINGS[:2]) and all(.000001<=v[k]<=120 for k in SETTINGS[2:5]),'Effect bounds')
 require(0<=v['opacity']<=1 and all(abs(v[k])<=1000 for k in SETTINGS[6:13]) and .000001<=v['move_divisor']<=100 and 0<=v['pixel_snap_uv_epsilon']<=.001 and all(0<=v[k]<=1 for k in SETTINGS[15:]),'Shader bounds')
 require(1<=len(ir['keys'])<=256,'Color key count');prev=-1
 for k in ir['keys']:
  fields(k,['time','ease','color']);require(finite(k['time']) and prev<k['time']<v['cycle_duration'] and k['ease']==1 and type(k['color'])==list and len(k['color'])==4 and all(finite(c) and 0<=c<=1 for c in k['color']),'Color key');prev=k['time']
 require(ir['keys'][0]['time']==0,'First key time')
 times=[struct.unpack('<f',struct.pack('<f',k['time']))[0]for k in ir['keys']]
 duration=struct.unpack('<f',struct.pack('<f',v['cycle_duration']))[0]
 require(all(t<duration and (i==0 or times[i-1]<t)for i,t in enumerate(times)),'Float32 key order')

def encode(ir):
 validate_ir(ir);t=ir['texture'];s=ir['settings'];blocks=[struct.pack(FORMATS[0],t['width'],t['height'],3,0,*[s[k]for k in SETTINGS]),b''.join(struct.pack(FORMATS[1],k['time'],k['ease'],*k['color'])for k in ir['keys']),bytes(v for c in t['palette'] for v in c),bytes(t['indices'])]
 b=bytearray(HEADER)
 for i,block in enumerate(blocks):
  b.extend(b'\0'*((-len(b))%4));off=len(b);b.extend(block);struct.pack_into('<HHIII',b,64+i*16,i+1,STRIDES[i],off,len(block)//STRIDES[i],len(block))
 struct.pack_into('<8s6I20s',b,0,b'ENCWFX01',1,len(b),0,4,1,1,bytes.fromhex(PIN));struct.pack_into('<I',b,16,zlib.crc32(b));return bytes(b)

def verify_sources(ir):
 inventory=json.loads((ROOT/'compatibility/upstream-inventory.json').read_text())
 require(ir['commit']==inventory['commit']==PIN,'Effect source pin')
 require(set(ir['sources'])=={SCENE,SCRIPT,SHADER,IMAGE,IMAGE+'.import','project.godot'},'Effect source coverage')
 for path,sha in ir['sources'].items():require(inventory['files'][path]['sha256']==sha==digest(ROOT/'upstream/MOTHER-Encore'/path),'Changed source '+path)

def main():
 p=argparse.ArgumentParser();p.add_argument('command',choices=['extract','compile','verify']);p.add_argument('--out',type=Path);a=p.parse_args()
 if a.command=='extract':
  ir=extract();out=a.out or IR;out.parent.mkdir(parents=True,exist_ok=True);out.write_text(json.dumps(ir,indent=2)+'\n');print(out);return
 ir=json.loads(IR.read_text());verify_sources(ir);data=encode(ir);out=a.out or PACK
 if a.command=='compile':out.parent.mkdir(parents=True,exist_ok=True);out.write_bytes(data)
 else:require(out.read_bytes()==data,'Stale effect binary')
 print(f'{out}: {len(data)} checked bytes')
if __name__=='__main__':main()
