#!/usr/bin/env python3
"""Original Podunk Canvas commands; no script/Ready approval or generic shader VM."""
from __future__ import annotations
import argparse, json, math, re, struct, subprocess, sys, zlib
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import PIN,SCENE,read,write,decode,stable,sha,require
from tools.field_node_tree import load as tree_load
IR=ROOT/'content/podunk-canvas-art.json';REVIEW=ROOT/'compatibility/reviews/podunk-canvas-art-v0410.json';PACK=ROOT/'romfs/data/podunk.enccanvasart';RECEIPT=ROOT/'content/asset-receipts/graphics/sprites/canvas-art/source.json'
OWNERS=['Native','Grass','Dandelion','Butterfly','Character','Emotes','Prompt','Enemy','Present','DeadBush','OpenableDoor','Jump','Phone','Dropped','Vending','Melody','Birds','Npc','Landmark']
SCRIPTS={'Scripts/misc/grass spawner.gd':1,'Scripts/misc/dandelion spawner.gd':2,'Scripts/misc/butterfly.gd':3,'Scripts/Main/character_sprite.gd':4,'Nodes/Ui/emotes.tscn::6':5,'Scripts/UI/Button Prompt.gd':6,'Scripts/Main/Enemy Spawner.gd':7,'Scripts/Main/Present.gd':8,'Scripts/Main/Dead Bush.gd':9,'Scripts/Main/Openable Door.gd':10,'Scripts/Main/Jump Area.gd':11,'Maps/Testing/phone.gd':12,'Scripts/Main/DroppedItem.gd':13,'Scripts/Main/VendingMachine.gd':14,'Nodes/Ui/effects/melodyBG.gd':15,'Scripts/misc/birds.gd':16,'Scripts/Main/npc.gd':17,'Scripts/Main/Flag Landmarks.gd':18}
ENGINE={'servers/visual/visual_server_canvas.cpp':'f55e0a8f33127b49f5cbad4a47efd3aaf6ea91da6b9a74485e81108e769a50b2','scene/2d/sprite.cpp':'ce678f79ba902084ad04398b3f36485543da9318835776de4480655618d9e4dd','scene/gui/texture_rect.cpp':'17498ec0735199c1cc7c3e9b509e328554e7aa1247076ef17cee97c4372ce149','scene/2d/canvas_item.cpp':'513b9efe70b2003b1ff3e451ac1b07527c3054b1eb81a0faab2ffe3eec0b6286','core/math/math_funcs.h':'32e8a5a998235947996119bed349cbd57233a9080b556a6b57d438ac40821e13','core/math/math_defs.h':'e5a6d4d80da1503950b05a468598c8a6fa1e5a3e15e05c0b024c0206b76b6a98'}
ENGINE.update({'servers/visual/visual_server_canvas.h':'12e7beb6bbae020b027b721b3e1b19c433f814a85c425e8b1c21b0fd69e7a14c','scene/gui/texture_rect.h':'761667c6b0c16da93ace79507156d009ee135ec398fa44946fdd030c2a6bed9d'})
ENGINE_V2={**ENGINE,'scene/2d/animated_sprite.cpp':'8dc64e924ca6f560f0acaffd43cb86e47a500c8586a03205f026a4445fff0c2e','scene/2d/animated_sprite.h':'806a6385c16a2f0df64eaf30e96234b8552dbbdd39fffe8f8b0c85a66e930016','scene/resources/texture.cpp':'ca3eb1dff44a4d5ea1da1683761694f7c63a803aa0fc58f350396f151a614f76','scene/gui/color_rect.cpp':'8dca7455ad21a361812d429b29d214abc406f6ef9a2cc462a59243db07187a6a'}
ENGINE_V2['scene/2d/tile_map.cpp']='c59d5a0a1dfb440e74938929035ebf224f3628452cfe1a3ebd30b9bee8b4c9ad'
PROGRAM='platform/ctr/shaders/field_canvas_art.v.pica'
def extract(native):
 d=read(native);g=read(ROOT/'content/podunk-scene.json');tree=tree_load();inv=read(ROOT/'compatibility/upstream-inventory.json')['files'];require(sha(native)==g['export_sha256']==tree['native_sha256'] and d['source']=='res://'+SCENE,'Canvas full native identity differs');ns={n['path']:n for n in d['nodes']};rs={r['id']:r for r in d['resources']};tr={r['id']:r for r in tree['records']};sources=dict(tree['sources']);textures={};records=[]
 def source(p):require(p in inv and sha(ROOT/'upstream/MOTHER-Encore'/p)==inv[p]['sha256'],'Canvas changed source '+p);sources[p]=inv[p]['sha256']
 def material(row):
  p=decode(ns[row['node']]['properties'])
  if p.get('use_parent_material') and row['canvas_parent']:return material(tr[row['canvas_parent']])
  return p.get('material')
 for row in tree['records']:
  if row['flags']&1:
   props=decode(ns[row['node']]['properties']);require(not props.get('rect_clip_content',False) and props.get('light_mask')==1,'Canvas clipping/light ownership requires additional source capability');require(not(row['flags']&128)or row['z']==0,'Nonzero nested YSort z requires a new source ordering capability')
  n=ns[row['node']]
  if n['class']not in ['Sprite','TextureRect']:continue
  p=decode(n['properties']);require(p.get('normal_map')is None,'Canvas normal-map capability missing');t=p['texture'];tid=0
  if t:
   r=rs[t['id']];require(r['class']=='StreamTexture'and r['path'].startswith('res://'),'Canvas unsupported texture resource');src=r['path'][6:];source(src);source(src+'.import');imp=(ROOT/'upstream/MOTHER-Encore'/(src+'.import')).read_text();require('flags/filter=false'in imp and 'flags/mipmaps=false'in imp and 'flags/repeat=0'in imp and 'process/premult_alpha=false'in imp and 'process/invert_color=false'in imp,'Canvas source texture sampling/process differs');raw=(ROOT/'upstream/MOTHER-Encore'/src).read_bytes();size=list(struct.unpack('>II',raw[16:24]));require(decode(r['size'])==size and all(0<v<=1024 for v in size),'Canvas actual PNG/native extent differs');tid=t['id'];textures[tid]=dict(id=tid,source=src,path='graphics/sprites/canvas-art/'+str(tid)+'.t3x',size=size,nearest=True,repeat=False)
  owner=0;owner_id=0;owner_script='';q=row
  while q:
   if q['script']in SCRIPTS:owner=SCRIPTS[q['script']];owner_id=q['id'];owner_script=q['script'];break
   if q['script']and q['script']not in ['Scripts/Main/Room.gd','Scripts/Main/RoomTypes/AreaRoom.gd']:raise ValueError('Canvas unreviewed appearance ancestor '+q['script']+' at '+row['node'])
   q=tr.get(q['parent'])
  m=material(row);shader=0;shader_source=''
  if m:
   mr=rs[m['id']];require(mr['class']=='ShaderMaterial','Canvas unknown effective material');mp=decode(mr['properties']);sh=rs[mp['shader']['id']];shader_source=sh['path'][6:]
   if '::'in shader_source:
    file,sub=shader_source.split('::');source(file);body=re.search(r'\[sub_resource type="Shader" id='+re.escape(sub)+r'\]\n(.*?)(?=\n\[|\Z)',(ROOT/'upstream/MOTHER-Encore'/file).read_text(),re.S);require(body,'Canvas embedded shader missing');code=json.JSONDecoder(strict=False).raw_decode(body[1].split('code = ',1)[1])[0];reference=(ROOT/'upstream/MOTHER-Encore/Shaders/Flash.tres').read_text();flash=json.JSONDecoder(strict=False).raw_decode(reference.split('code = ',1)[1])[0];require(code==flash and owner!=0,'Canvas unknown embedded shader');source('Shaders/Flash.tres');shader=3
   else:
    require(shader_source in ['Shaders/Outline.shader','Shaders/Distortionator.shader'],'Canvas unknown source shader '+shader_source+' '+row['node']);source(shader_source);shader=1 if shader_source=='Shaders/Outline.shader'else 2;require((shader==1 and owner==11)or(shader==2 and owner==15),'Canvas shader owner mismatch')
  if n['class']=='Sprite':
   require(not p['region_enabled'] and not p['region_filter_clip'],'Canvas Sprite region unsupported in this original source');h,v,frame=p['hframes'],p['vframes'],p['frame'];require(h>0 and v>0 and 0<=frame<h*v,'Canvas Sprite source frame invalid');offset=p['offset'];centered=p['centered'];stretch=0;size=[0,0]
  else:
   h=v=1;frame=0;offset=[0,0];centered=False;stretch=p['stretch_mode'];require(stretch in [2,3] and(stretch!=2 or owner==15),'Canvas source TextureRect stretch requires owned effect');size=[p['margin_right']-p['margin_left'],p['margin_bottom']-p['margin_top']];require(not p['rect_clip_content'],'Canvas source TextureRect clip unsupported')
  records.append(dict(id=row['id'],node=row['node'],kind=0 if n['class']=='Sprite'else 1,flags=row['flags'],texture=tid,hframes=h,vframes=v,frame=frame,offset=offset,centered=centered,flip_h=p['flip_h'],flip_v=p['flip_v'],stretch=stretch,size=size,owner=owner,owner_id=owner_id,owner_script=owner_script,owner_sha=tr[owner_id]['script_sha']if owner_id else '0'*64,shader=shader,shader_source=shader_source))
 require(len(records)==1873 and len(textures)==21,'Canvas full original1873/21 scope lost');source('project.godot');source('LICENSE');require('2d/snapping/use_gpu_pixel_snap=true'in(ROOT/'upstream/MOTHER-Encore/project.godot').read_text(),'Canvas source pixel snap differs')
 # Source export-variable setters assign these textures before the original
 # Openable Ready body. The native quarantine deliberately did not run scripts,
 # so its Sprite.texture null value alone does not close the GPU asset graph.
 from tools.field_openable_door import load as openable_load
 doors=openable_load()
 for image in doors['textures']:
  src=image['source'];source(src);source(src+'.import')
  candidates=[r for r in rs.values()if r['class']=='StreamTexture'and r.get('path')=='res://'+src]
  require(len(candidates)==1,'Canvas Openable source StreamTexture closure differs '+src)
  resource=candidates[0];raw=(ROOT/'upstream/MOTHER-Encore'/src).read_bytes();size=list(struct.unpack('>II',raw[16:24]))
  require(decode(resource['size'])==size and image['region']==[0,0,*size],'Canvas Openable region requires an actual cropped native texture consumer')
  imp=(ROOT/'upstream/MOTHER-Encore'/(src+'.import')).read_text()
  require(all(v in imp for v in ['flags/filter=false','flags/mipmaps=false','flags/repeat=0','process/premult_alpha=false','process/invert_color=false']),'Canvas Openable source sampling/process differs')
  tid=resource['id'];texture=dict(id=tid,source=src,path='graphics/sprites/canvas-art/'+str(tid)+'.t3x',size=size,nearest=True,repeat=False)
  require(tid not in textures or textures[tid]==texture,'Canvas Openable source resource ID collision')
  textures[tid]=texture
 require(len(textures)==26,'Canvas native21 plus Openable source7 texture closure differs')
 out=dict(schema=1,kind='encore.field-canvas-art.source-ir',commit=PIN,scene=SCENE,scene_id=tree['scene_id'],scene_sha256=tree['source_sha256'],native_sha256=sha(native),tree_ir_sha256=sha(ROOT/'content/podunk-node-tree.json'),source_receipt_sha256=tree['source_receipt_sha256'],sources=dict(sorted(sources.items())),engine_tag='3.6.2-stable',engine_sources=ENGINE,owners=OWNERS,records=records,textures=list(sorted(textures.values(),key=lambda t:t['id'])),program=dict(source=PROGRAM,path='shaders/field-canvas-art.shbin',source_sha256=sha(ROOT/PROGRAM)),pixel_snap=True,y_epsilon=0.00001,alpha_prune=0.007,scene_admitted=False,semantics=['Native Sprite frame rect, centered offset floors locally under GPU pixel snap; flips only UV after original VS normalization','TextureRect KEEP enum3 uses actual texture extent and origin zero; current TILE source belongs to Melody consumer','Actual ObjectID live affine transform, inherited modulate versus own self_modulate, visible/queued source ownership','Continuous visible nestedYSort collection relative origin; approximateY comparison with collection index, behind-parent order and ascending source z buckets','Typed appearance owner required for dynamic normal sprites; Character/Grass/Prompt/Melody/Outline delegate actual existing consumer, never default-shader substitution','Structural CanvasArt resource admits no Ready callback and no complete Podunk scene','All visible actual Canvas slots including independent TileMap/Label/AnimatedSprite are exported for host z/order stream merging','Openable source exported Sprite setters close all seven actual StreamTexture assets before Ready; no Node layout or native execution approval changes'],unverified=['Manual tests not executed','Emulator and hardware not verified','Source SceneHost/Main integration belongs to parent'])
 write(IR,out);write(REVIEW,dict(schema=1,commit=PIN,ir_sha256=sha(IR),tree_ir_sha256=out['tree_ir_sha256'],engine_tag=out['engine_tag'],engine_sources=ENGINE,source_semantics=out['semantics'],sources=out['sources']));return out
def load():
 d=read(IR);r=read(REVIEW);tree=tree_load();require(set(d)==set('schema kind commit scene scene_id scene_sha256 native_sha256 tree_ir_sha256 source_receipt_sha256 sources engine_tag engine_sources owners records textures program pixel_snap y_epsilon alpha_prune scene_admitted semantics unverified'.split()),'Canvas unknown/missing source IR field');require(d['schema']==1 and d['kind']=='encore.field-canvas-art.source-ir'and d['commit']==PIN and d['scene']==SCENE and not d['scene_admitted']and d['owners']==OWNERS and r['ir_sha256']==sha(IR)and r['engine_sources']==d['engine_sources']==ENGINE and d['tree_ir_sha256']==r['tree_ir_sha256']==sha(ROOT/'content/podunk-node-tree.json') and d['scene_sha256']==tree['source_sha256'] and d['program']['source_sha256']==sha(ROOT/PROGRAM),'Canvas source review/Tree/PICA differs');inv=read(ROOT/'compatibility/upstream-inventory.json')['files']
 for p,h in d['sources'].items():require(sha(ROOT/'upstream/MOTHER-Encore'/p)==h==inv[p]['sha256'],'Canvas changed source '+p)
 return d
def encode(d,receipt,ir_sha256=None):
 if d['schema']==2:return encode_v2(d,receipt,ir_sha256)
 b=bytearray(128)
 def u(*v):b.extend(struct.pack('<'+'I'*len(v),*v))
 def f(*v):b.extend(struct.pack('<'+'f'*len(v),*v))
 def t(v):s=v.encode();u(len(s));b.extend(s)
 def asset(a,p):u(a.get('id',0),*a.get('size',[0,0]),p['bytes'],p['crc']);t(a['source']);t(a['path']);b.extend(bytes.fromhex(p['source_sha256']+p['output_sha256']))
 t(d['scene']);f(d['y_epsilon'],d['alpha_prune']);u(int(d['pixel_snap']));b.extend(bytes.fromhex(d['tree_ir_sha256']));u(len(d['textures']))
 for a,p in zip(d['textures'],receipt['textures']):asset(a,p)
 asset(d['program'],receipt['program']);u(len(d['records']))
 for r in d['records']:
  u(*(r[k]for k in ['id','kind','flags','texture','hframes','vframes','frame']),int(r['centered']),int(r['flip_h']),int(r['flip_v']),r['stretch'],r['owner'],r['owner_id'],r['shader']);f(*r['offset'],*r['size']);t(r['node']);t(r['owner_script']);t(r['shader_source']);b.extend(bytes.fromhex(r['owner_sha']))
 u(len(d['sources']))
 for p,h in d['sources'].items():t(p);b.extend(bytes.fromhex(h))
 struct.pack_into('<8s8I',b,0,b'ENCFCA01',1,128,len(b),0,0x454e0040,1,1,d['scene_id']);b[40:60]=bytes.fromhex(PIN);b[60:92]=bytes.fromhex(d['scene_sha256']);b[92:124]=bytes.fromhex(sha(IR)if ir_sha256 is None else ir_sha256);struct.pack_into('<I',b,20,zlib.crc32(b[128:]));return bytes(b)

def encode_v2(d,receipt,ir_sha256):
 require(d['schema']==d['format']==d['capabilities']==2 and d['rules']==1 and d['commit']==PIN and ir_sha256 is not None and receipt['ir_sha256']==ir_sha256,'Canvas v2 requires explicit reviewed source IR identity')
 b=bytearray(128)
 def u(*v):b.extend(struct.pack('<'+'I'*len(v),*v))
 def i(v):b.extend(struct.pack('<i',v))
 def f(*v):b.extend(struct.pack('<'+'f'*len(v),*v))
 def t(v):s=v.encode();u(len(s));b.extend(s)
 def h(v):require(len(v)==64,'Canvas v2 hash extent');b.extend(bytes.fromhex(v))
 t(d['scene']);f(d['y_epsilon'],d['alpha_prune']);u(int(d['pixel_snap']));h(d['tree_ir_sha256']);u(len(d['textures']))
 for a in d['textures']:
  u(a['id'],*a['size']);t(a['source']);h(a['source_sha256']);u(len(a['pages']))
  pages=[p for p in receipt['pages']if p['texture_id']==a['id']];require(len(pages)==len(a['pages']),'Canvas v2 complete virtual texture pages missing')
  for expected,p in zip(a['pages'],pages):
   require(expected['path']==p['path']and expected['crop']==p['crop']and p['source']==a['source']and p['source_sha256']==a['source_sha256'],'Canvas v2 page/source differs')
   u(stable(p['path']),*p['crop'],p['bytes'],p['crc']);t(p['path']);h(p['output_sha256']);h(p['crop_png_sha256'])
 a=d['program'];p=receipt['program'];u(0,0,0,p['bytes'],p['crc']);t(a['source']);t(a['path']);h(p['source_sha256']);h(p['output_sha256']);u(len(d['records']))
 for r in d['records']:
  u(*(r[k]for k in ['id','kind','flags','texture','hframes','vframes','frame']),int(r['centered']),int(r['flip_h']),int(r['flip_v']),r['stretch'],r['owner'],r['owner_id'],r['shader']);f(*r['offset'],*r['size']);t(r['node']);t(r['owner_script']);t(r['shader_source']);h(r['owner_sha'])
  f(*r.get('color',[0,0,0,0]),r.get('speed_scale',0),r.get('ready_rng',{}).get('minimum',0),r.get('ready_rng',{}).get('maximum',0));u(int(r.get('playing',False)));t(r.get('animation',''));t(r.get('ready_rng',{}).get('method',''));u(len(r.get('animations',[])))
  for a in r.get('animations',[]):
   t(a['name']);f(a['speed']);u(int(a['loop']),len(a['frames']))
   for frame in a['frames']:u(frame['texture']);f(*frame['region'],*frame['margin']);u(int(frame['filter_clip']));t(frame.get('atlas_source',''))
  m=r['material'];u(int(m is not None))
  if m:
   u(int(m['local_to_scene']));i(m['priority']);t(m['source']);h(m['shader_code_sha256']);u(len(m['params']))
   for p in m['params']:
    t(p['name']);kind=['bool','int','float','vec2','vec4','sampler2D'].index(p['type']);u(kind,['serialized_material','shader_default','source_uninitialized'].index(p['provenance']),int(p['value']is not None));values=[0]*4;integer=0
    if p['value']is not None:
     if kind in(0,1):integer=int(p['value'])
     elif kind==2:values[0]=p['value']
     elif kind in(3,4):values[:len(p['value'])]=p['value']
     else:raise ValueError('Canvas original initialized sampler requires concrete source texture binding')
    f(*values);i(integer)
 u(len(d['control_boundaries']))
 for c in d['control_boundaries']:
  p=c['native_properties'];u(c['id'],c['flags'],c['owner_id']);t(c['node']);t(c['class_name']);t(c['owner_script']);h(c['owner_sha']);h(__import__('hashlib').sha256(json.dumps(p,sort_keys=True,separators=(',',':'),ensure_ascii=False).encode()).hexdigest());t(c.get('text',''));t(c.get('font_source',''));h(c.get('font_source_sha256','0'*64));f(p['margin_right']-p['margin_left'],p['margin_bottom']-p['margin_top']);u(p['align']if c['class_name']=='Label'else 0,p['valign']if c['class_name']=='Label'else 0);f(p['percent_visible']if c['class_name']=='Label'else 0);u(int(p['autowrap'])if c['class_name']=='Label'else 0,int(p['clip_text'])if c['class_name']=='Label'else 0)
 u(len(d['sources']))
 for p,hsh in d['sources'].items():t(p);h(hsh)
 struct.pack_into('<8s8I',b,0,b'ENCFCA01',2,128,len(b),0,0x454e0040,2,1,d['scene_id']);b[40:60]=bytes.fromhex(PIN);b[60:92]=bytes.fromhex(d['scene_sha256']);b[92:124]=bytes.fromhex(ir_sha256);struct.pack_into('<I',b,20,zlib.crc32(b[128:]));return bytes(b)
def assets(tex3ds,picasso):
 d=load();require(tex3ds and picasso and Path(tex3ds).is_file() and Path(picasso).is_file(),'Canvas actual SDK compilers required')
 def one(a):
  p=ROOT/'romfs'/a['path'];p.parent.mkdir(parents=True,exist_ok=True);subprocess.run([str(tex3ds),'-f','rgba8','-z','none','-o',str(p),str(ROOT/'upstream/MOTHER-Encore'/a['source'])],check=True);return dict(id=a['id'],bytes=p.stat().st_size,crc=zlib.crc32(p.read_bytes()),source_sha256=d['sources'][a['source']],output_sha256=sha(p))
 with ThreadPoolExecutor(max_workers=4)as pool:proof=list(pool.map(one,d['textures']))
 p=ROOT/'romfs'/d['program']['path'];p.parent.mkdir(parents=True,exist_ok=True);subprocess.run([str(picasso),'-o',str(p),str(ROOT/PROGRAM)],check=True);receipt=dict(schema=1,commit=PIN,ir_sha256=sha(IR),producer_sha256=sha(Path(__file__)),tex3ds_sha256=sha(tex3ds),picasso_sha256=sha(picasso),workers=4,textures=proof,program=dict(bytes=p.stat().st_size,crc=zlib.crc32(p.read_bytes()),source_sha256=sha(ROOT/PROGRAM),output_sha256=sha(p)));write(RECEIPT,receipt);PACK.parent.mkdir(parents=True,exist_ok=True);PACK.write_bytes(encode(d,receipt));return receipt
def checked_assets(d,root):
 a=read(RECEIPT);require(a['commit']==PIN and a['producer_sha256']==sha(Path(__file__))and a['ir_sha256']==sha(IR)and a['workers']>=4 and len(a['textures'])==len(d['textures']),'Canvas genuine resource receipt differs')
 for resource,proof in [*zip(d['textures'],a['textures']),(d['program'],a['program'])]:
  p=Path(root)/resource['path'];require(p.stat().st_size==proof['bytes']and sha(p)==proof['output_sha256']and zlib.crc32(p.read_bytes())==proof['crc'],'Canvas converted resource changed '+resource['path'])
 return a
def stage_files(source):
 d=load();a=checked_assets(d,source);raw=encode(d,a);require((Path(source)/'data/podunk.enccanvasart').read_bytes()==raw,'Canvas staged pack differs');return{Path('data/podunk.enccanvasart'):raw,**{Path(r['path']):(Path(source)/r['path']).read_bytes()for r in [*d['textures'],d['program']]}}
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','assets','compile']);p.add_argument('--native',type=Path);p.add_argument('--tex3ds',type=Path);p.add_argument('--picasso',type=Path);a=p.parse_args()
 try:
  if a.action=='extract':print(len(extract(a.native)['records']))
  elif a.action=='assets':assets(a.tex3ds,a.picasso);print('Canvas genuine textures/PICA workers4')
  else:d=load();PACK.write_bytes(encode(d,checked_assets(d,ROOT/'romfs')));print(PACK.stat().st_size)
 except(ValueError,KeyError,TypeError,OSError,struct.error,subprocess.CalledProcessError)as e:sys.exit('CANVAS ART ERROR: '+str(e))
