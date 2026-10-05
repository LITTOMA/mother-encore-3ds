#!/usr/bin/env python3
"""Pinned complete DandelionSpawner Ready/visibility factory source resource.

Native particle emission remains a separate required backend, never an empty
emit callback. The original inverted screen_exited guard is preserved.
"""
from __future__ import annotations
import argparse,hashlib,math,re,struct,subprocess,sys,zlib
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import PIN,SCENE,stable,decode,read,write,sha,require
SCRIPT='Scripts/misc/dandelion spawner.gd';PLANT='Scripts/Main/dandelion.gd';PROTO='Nodes/Overworld/dandelion.tscn'
IR=ROOT/'content/podunk-dandelion.json';ASSETS=ROOT/'content/podunk-dandelion-assets.json';REVIEW=ROOT/'compatibility/reviews/podunk-dandelion-v0410.json';OUT=ROOT/'romfs/data/podunk.encdandelion'
FORMATS={1:'2I',2:'B',3:'6I18f',4:'11I11f',5:'5I64s',6:'2If',7:'5f',8:'I32s',9:'5I',10:'2I64s'}
def flat(t):return [v for p in t for v in p]
def extract(native,prototype):
 d=read(native);p=read(prototype);g=read(ROOT/'content/podunk-scene.json');inv=read(ROOT/'compatibility/upstream-inventory.json')['files'];sources=dict(g['sources'])
 require(d['source']=='res://'+SCENE and p['source']=='res://'+PROTO and g['export_sha256']==sha(native),'Dandelion native identity rejected')
 require([p['godot'].get(k) for k in ('major','minor','patch','status')]==[3,6,2,'stable'],'Dandelion prototype engine rejected')
 for name in [PLANT,PROTO,'Graphics/Objects/Common/Dandelion.png','Graphics/Objects/Common/DandelionSeed.png','LICENSE']:sources[name]=inv[name]['sha256']
 for name,h in sources.items():require(h==inv[name]['sha256']==sha(ROOT/'upstream/MOTHER-Encore'/name),'Changed dandelion source '+name)
 nm={n['path']:n for n in d['nodes']};pn={n['path']:decode(n['properties']) for n in p['nodes']};res={r['id']:r for r in p['resources']}
 require(len(nm)==8686 and set(pn)=={'.','Sprite','CollisionShape2D','CPUParticles2D'},'Dandelion complete source coverage rejected')
 spawners=[]
 for b in g['pending']:
  if b['script']!=SCRIPT:continue
  n=nm[b['node']];parent=b['node'].rsplit('/',1)[0] if '/' in b['node'] else '.';notifier=nm[b['node']+'/VisibilityNotifier2D'];pr=decode(notifier['properties'])
  spawners.append(dict(id=b['stable_id'],ready=b['ready_ordinal'],node=b['node'],parent=stable(parent),sprite=stable(b['node']+'/Sprite'),notifier=stable(notifier['path']),position=decode(n['properties']['position']),parent_transform=decode(nm[parent]['world_transform']),notifier_transform=decode(notifier['world_transform']),rect=pr['rect']))
 require(len(spawners)==135,'Dandelion spawner count differs');spawners.sort(key=lambda n:n['ready'])
 area=pn['.'];sprite=pn['Sprite'];shape=pn['CollisionShape2D'];particle=pn['CPUParticles2D'];ext=decode(res[shape['shape']['id']]['properties']['extents'])
 require(shape['disabled'] is False and shape['one_way_collision'] is False and res[shape['shape']['id']]['class']=='RectangleShape2D','Unsupported plant geometry')
 require(sprite['centered'] and not sprite['region_enabled'] and not sprite['flip_h'] and not sprite['flip_v'],'Unreviewed plant sprite')
 text=(ROOT/'upstream/MOTHER-Encore'/PLANT).read_text(encoding='utf-8');idle=int(re.search(r'Sprite.frame != ([0-9]+)',text)[1]);blown=int(re.search(r'Sprite.frame = ([0-9]+)',text)[1]);mult=float(re.search(r'get_direction\(\) \* ([0-9.]+)',text)[1])
 profile=dict(id=stable(PROTO),source=PROTO,layer=area['collision_layer'],mask=area['collision_mask'],flags=int(area['monitoring'])|(int(area['monitorable'])<<1)|(int(area['visible'])<<2),columns=sprite['hframes'],rows=sprite['vframes'],idle=idle,blown=blown,collision_offset=shape['position'],extents=ext,sprite_offset=sprite['offset'],sprite_position=sprite['position'],particle_position=particle['position'],gravity_multiplier=mult)
 params=[]
 for key,value in particle.items():
  if isinstance(value,(bool,int,float)):params.append(dict(name=key,kind=1 if isinstance(value,bool) else 2 if isinstance(value,int) else 0,value=value))
  elif key in ['direction','gravity']:
   params.extend(dict(name=key+'.'+axis,kind=0,value=v) for axis,v in zip(('x','y'),value))
 gradient=decode(res[particle['color_ramp']['id']]['properties']);colors=gradient['colors']['value'];ramp=[[offset]+[color[c] for c in ('r','g','b','a')] for offset,color in zip(gradient['offsets'],colors)]
 material=decode(res[particle['material']['id']]['properties']);mat=[material['blend_mode'],material['light_mode'],material['particles_anim_h_frames'],material['particles_anim_v_frames'],int(material['particles_animation'])|(int(material['particles_anim_loop'])<<1)]
 write(IR,dict(schema=1,commit=PIN,scene=SCENE,scene_id=stable('.'),source_sha256=sources[SCENE],scene_admitted=False,capabilities=3,native_sha256=sha(native),prototype_sha256=sha(prototype),sources=sources,spawners=spawners,profile=profile,particle_parameters=params,particle_ramp=ramp,material=mat,pending=['CPUParticles2D emission/clock/RNG/render backend required before body_entered'],source_exited_guard='if !_current_dandelion: null queue_free error; an existing plant remains'))
def assets(tex3ds=None):
 names=['Graphics/Objects/Common/Dandelion.png','Graphics/Objects/Common/DandelionSeed.png'];paths=[ROOT/'romfs/graphics/world/podunk/dandelion.t3x',ROOT/'romfs/graphics/world/podunk/dandelion-seed.t3x']
 if tex3ds:
  def convert(pair):
   source,target=pair;target.parent.mkdir(parents=True,exist_ok=True);subprocess.run([str(tex3ds),'-f','rgba8','-z','none','-o',str(target),str(ROOT/'upstream/MOTHER-Encore'/source)],check=True)
  with ThreadPoolExecutor(max_workers=4) as pool:list(pool.map(convert,zip(names,paths)))
 result=[]
 for source,target in zip(names,paths):
  raw=(ROOT/'upstream/MOTHER-Encore'/source).read_bytes();require(raw[:8]==b'\x89PNG\r\n\x1a\n','Dandelion PNG source rejected');w,h=struct.unpack('>II',raw[16:24]);result.append(dict(id=stable(source),source=source,path=target.relative_to(ROOT/'romfs').as_posix(),width=w,height=h,source_sha256=sha(ROOT/'upstream/MOTHER-Encore'/source),output_sha256=sha(target)))
 return result
def pack(ir,textures):
 strings=[];lookup={};blob=bytearray();rows={k:[] for k in FORMATS}
 def string(v):
  if v not in lookup:
   lookup[v]=len(strings);raw=v.encode();strings.append((len(blob),len(raw)));blob.extend(raw+b'\0')
  return lookup[v]
 scene=string(ir['scene'])
 for n in ir['spawners']:rows[3].append((n['id'],n['ready'],string(n['node']),n['parent'],n['sprite'],n['notifier'],*flat(n['parent_transform']),*n['position'],*flat(n['notifier_transform']),*flat(n['rect'])))
 p=ir['profile'];rows[4]=[(p['id'],p['layer'],p['mask'],p['flags'],0,1,p['columns'],p['rows'],p['idle'],p['blown'],string(p['source']),*p['collision_offset'],*p['extents'],*p['sprite_offset'],*p['sprite_position'],*p['particle_position'],p['gravity_multiplier'])]
 rows[5]=[(t['id'],string(t['source']),string(t['path']),t['width'],t['height'],bytes.fromhex(t['source_sha256']+t['output_sha256'])) for t in textures]
 rows[6]=[(string(n['name']),n['kind'],float(n['value'])) for n in ir['particle_parameters']];rows[7]=ir['particle_ramp'];rows[8]=[(string(p),bytes.fromhex(h)) for p,h in ir['sources'].items()];rows[9]=[ir['material']];rows[10]=[(string(SCRIPT),string(PLANT),bytes.fromhex(ir['sources'][SCRIPT]+ir['sources'][PLANT]))];rows[1]=strings;rows[2]=[(v,) for v in blob]
 begin=128+24*len(FORMATS);result=bytearray(begin);directory=[]
 for k,fmt in FORMATS.items():
  payload=b''.join(struct.pack('<'+fmt,*r) for r in rows[k]);directory.append((k,len(rows[k]),struct.calcsize('<'+fmt),len(result),len(payload),0));result.extend(payload)
 struct.pack_into('<8s8I',result,0,b'ENCFDAN1',1,128,len(result),len(FORMATS),zlib.crc32(result[begin:]),0x454e001d,3,ir['scene_id']);result[40:60]=bytes.fromhex(PIN);result[60:92]=bytes.fromhex(ir['source_sha256']);result[92:124]=hashlib.sha256(IR.read_bytes()).digest();struct.pack_into('<I',result,124,scene)
 for i,row in enumerate(directory):struct.pack_into('<6I',result,128+24*i,*row)
 return bytes(result)
def main():
 parser=argparse.ArgumentParser();parser.add_argument('action',choices=['extract','compile','verify']);parser.add_argument('--native',type=Path);parser.add_argument('--prototype',type=Path);parser.add_argument('--tex3ds',type=Path);a=parser.parse_args()
 if a.action=='extract':require(a.native and a.prototype,'Explicit complete native and prototype required');extract(a.native,a.prototype);return
 ir=read(IR);review=read(REVIEW);inv=read(ROOT/'compatibility/upstream-inventory.json');require(ir['commit']==PIN and ir['scene']==SCENE and ir['schema']==1 and ir['scene_admitted'] is False and ir['capabilities']==3,'Dandelion IR identity rejected');require(review['ir_sha256']==sha(IR) and review['commit']==PIN and review['scene_admitted'] is False,'Dandelion source review missing/stale')
 for p,h in ir['sources'].items():require(h==inv['files'][p]['sha256']==sha(ROOT/'upstream/MOTHER-Encore'/p),'Dandelion source changed '+p)
 textures=assets(a.tex3ds);data=pack(ir,textures)
 if a.action=='verify':require(read(ASSETS)==textures and OUT.read_bytes()==data,'Dandelion resource differs')
 else:write(ASSETS,textures);OUT.parent.mkdir(parents=True,exist_ok=True);OUT.write_bytes(data)
 print('Dandelion source Ready/factory:',len(ir['spawners']),'bindings;',len(data),'bytes; particle emission pending, scene admitted=False')
if __name__=='__main__':
 try:main()
 except (ValueError,KeyError,OSError,subprocess.SubprocessError) as e:sys.exit('FIELD DANDELION ERROR: '+str(e))
