#!/usr/bin/env python3
"""Source-backed original Present Sparkles; real tex3ds asset conversion only."""
from __future__ import annotations
import argparse,hashlib,json,math,re,struct,subprocess,sys,zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.extract_battle_entry import Extractor,PIN,node,properties,require
from tools.drawer_program import canonical,digest,read_json,write_json,fields,safe_path
IR='content/present-sparkles.json';REVIEW='reports/present-sparkles/source-review.json';PACK='romfs/data/house.encsparkles';RECEIPT='content/asset-receipts/graphics/story/present-sparkles/source.json'
HOUSE='Maps/podunk/Nintens House.tscn';PRESENT='Nodes/Overworld/Objects/Present.tscn';SPARKLES='Nodes/Reusables/Effects/Sparkles.tscn';SCRIPT='Scripts/misc/sparkles.gd'
ENGINE=dict(commit='3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8',url='https://github.com/godotengine/godot/blob/3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8/scene/2d/animated_sprite.cpp',sha256='8dc64e924ca6f560f0acaffd43cb86e47a500c8586a03205f026a4445fff0c2e',review='set_frame clamps to frame_count-1, int truncates rand_range; float32 timeout loop advances before consuming the next remaining delta; stop retains frame, play resets timeout only on playing-state change; visibility does not gate INTERNAL_PROCESS',default_speed_scale=1.0,default_centered=True,default_offset=[0,0])

def build(root=ROOT):
 ex=Extractor(root);house=ex.text(HOUSE);present=ex.text(PRESENT);sparkles=ex.text(SPARKLES);script=ex.text(SCRIPT);ps=ex.text('Scripts/Main/Present.gd');project=ex.text('project.godot');require('2d/snapping/use_gpu_pixel_snap=true' in project,'Unreviewed source pixel snapping');ex.text('Scripts/Main/ItemHolder.gd');ex.text('Scripts/Main/FlaggableObject.gd');ex.text('LICENSE')
 require(re.fullmatch(r'tool\s+extends AnimatedSprite\s+func _ready\(\):\s+frame = int\(rand_range\(([-0-9.]+), ([-0-9.]+)\)\)\s*',script),'Unknown Sparkles source behavior')
 limits=re.search(r'rand_range\(([-0-9.]+), ([-0-9.]+)\)',script);rootnode=node(sparkles,'.');p=node(house,'Objects/Present1');child=node(present,'Sparkles')
 require(rootnode['playing'] is True and 'func _ready():'in ps and ps.index('_update_sprite()',ps.index('func _ready'))<ps.index('if _get_flag_status():',ps.index('func _ready')) and '$Sparkles.stop()'in ps and '$Sparkles.hide()'in ps and '$Sparkles.show()'in ps and '$Sparkles.play()'in ps,'Unknown source Sparkles/Present lifecycle')
 require(rootnode['frames']=={'SubResource':14} and rootnode['script']=={'ExtResource':2},'Sparkles source binding changed')
 block=re.search(r'\[sub_resource type="SpriteFrames" id=14\]\n([\s\S]*?)(?=\n\[|\Z)',sparkles);require(block is not None,'Source SpriteFrames missing');a=properties(block[1])['animations'];require(len(a)==1 and set(a[0])=={'frames','loop','name','speed'} and a[0]['name']==rootnode['animation'] and a[0]['loop'] is True,'Unknown source SpriteFrames animation')
 animation=a[0];rects=[]
 for ref in animation['frames']:
  require(set(ref)=={'SubResource'},'Unknown atlas frame reference');b=re.search(r'\[sub_resource type="AtlasTexture" id='+str(ref['SubResource'])+r'\]\n([\s\S]*?)(?=\n\[|\Z)',sparkles);require(b is not None,'Dangling atlas frame');body=b[1];require('atlas = ExtResource( 1 )'in body,'Different source atlas');m=re.search(r'region = Rect2\( ([^)]+) \)',body);require(m is not None,'Unknown atlas frame geometry');rects.append([int(v.strip())for v in m[1].split(',')])
 image=re.search(r'\[ext_resource path="res://([^"]+)" type="Texture" id=1\]',sparkles);require(image is not None,'Source PNG binding missing');source=image[1];size=ex.png_size(source);ex.data(source+'.import')
 identity='Objects/Present1/Sparkles';return dict(schema=1,kind='encore.present-sparkles.source-ir',commit=PIN,sources=dict(sorted(ex.sources.items())),engine_reference=ENGINE,binding=dict(scene=HOUSE,node=identity,parent='Objects/Present1',stable_id=zlib.crc32((HOUSE+':'+identity).encode())&0xffffffff,parent_id=zlib.crc32((HOUSE+':Objects/Present1').encode())&0xffffffff,flag=p['flag'],parent_position=p['position'],position=child['position'],offset=ENGINE['default_offset'],ready_before_parent=True),resource=dict(source=source,source_sha256=ex.sources[source],size=size,output='graphics/story/present-sparkles/sparkles.t3x'),animation=dict(name=animation['name'],speed=animation['speed'],speed_scale=ENGINE['default_speed_scale'],loop=animation['loop'],playing=rootnode['playing'],centered=ENGINE['default_centered'],pixel_snap=True,serialized_frame=rootnode['frame'],random_range=[float(limits[1]),float(limits[2])],frames=rects),licence_review='Pinned upstream LICENSE permits game-related forks/modifications. Original art remains under upstream terms. Engine timing semantics reviewed against Godot MIT source; docs/licenses/Godot-3.6.2-LICENSE.txt retained.')

def validate(ir):
 fields(ir,('schema','kind','commit','sources','engine_reference','binding','resource','animation','licence_review'),'Sparkles IR');require(ir['schema']==1 and ir['kind']=='encore.present-sparkles.source-ir'and ir['commit']==PIN and ir['engine_reference']==ENGINE,'Sparkles schema/engine pin')
 b=ir['binding'];fields(b,('scene','node','parent','stable_id','parent_id','flag','parent_position','position','offset','ready_before_parent'),'Sparkles binding');require(b['scene']==HOUSE and all(safe_path(b[k])for k in('node','parent')) and all(type(b[k])is int and 0<b[k]<0xffffffff for k in('stable_id','parent_id')) and b['ready_before_parent'] is True and re.fullmatch('[A-Za-z0-9_]+',b['flag']),'Sparkles source identity')
 for k in('parent_position','position','offset'):require(len(b[k])==2 and all(type(v)in(int,float)and math.isfinite(v)and abs(v)<1e6 for v in b[k]),'Sparkles transform')
 r=ir['resource'];fields(r,('source','source_sha256','size','output'),'Sparkles resource');require(safe_path(r['source'])and r['source'].startswith('Graphics/')and r['source'].endswith('.png')and safe_path(r['output'])and r['output'].startswith('graphics/')and r['output'].endswith('.t3x')and ir['sources'].get(r['source'])==r['source_sha256']and len(r['size'])==2 and all(type(v)is int and 0<v<=1024 for v in r['size']),'Sparkles resource source/extent')
 a=ir['animation'];fields(a,('name','speed','speed_scale','loop','playing','centered','pixel_snap','serialized_frame','random_range','frames'),'Sparkles animation');require(a['name']and a['loop']is True and a['playing']is True and a['centered']is True and a['pixel_snap']is True and type(a['serialized_frame'])is int and 0<=a['serialized_frame']<len(a['frames'])and 0<len(a['frames'])<=1024,'Sparkles animation flags/frame')
 require(all(type(a[k])in(int,float)and math.isfinite(a[k])and 0<a[k]<=1024 for k in('speed','speed_scale'))and len(a['random_range'])==2 and all(type(v)in(int,float)and math.isfinite(v)and 0<=v<65536 for v in a['random_range'])and a['random_range'][0]<=a['random_range'][1],'Sparkles clocks/RNG bounds')
 for rect in a['frames']:require(len(rect)==4 and all(type(v)is int for v in rect)and all(v>=0 for v in rect[:2])and all(v>0 for v in rect[2:])and all(rect[i]+rect[i+2]<=r['size'][i]for i in(0,1)),'Sparkles source frame rectangle')
 require(0<len(ir['sources'])<=128 and all(safe_path(p)and re.fullmatch('[0-9a-f]{64}',h)for p,h in ir['sources'].items()),'Sparkles source receipt')
 return ir

def extract(root=ROOT):
 root=Path(root);ir=validate(build(root));write_json(root/IR,ir);write_json(root/REVIEW,dict(schema=1,commit=PIN,ir_sha256=digest(root/IR),sources=ir['sources'],engine_reference=ENGINE,semantics=['Child Ready consumes shared global rand_range even when parent is already opened','Source random frame is truncated then AnimatedSprite clamps, never modulo','Source ordered atlas frames and float32 idle clock; hidden nodes still process if playing','Parent Present opened state stops/hides; unopened shows/plays; stop keeps frame and resume resets timeout'],unverified=['Manual tests not run','Real tex3ds build pending','Emulator and hardware visual/timing verification pending']));return ir

def load(root=ROOT):
 root=Path(root);ir=validate(read_json(root/IR));require(canonical(ir)==canonical(build(root)),'Unreviewed Sparkles source IR');r=read_json(root/REVIEW);require(r['schema']==1 and r['commit']==PIN and r['sources']==ir['sources']and r['ir_sha256']==digest(root/IR)and r['engine_reference']==ENGINE,'Sparkles source review');return ir

def encode(ir):
 validate(ir);b=ir['binding'];r=ir['resource'];a=ir['animation'];out=bytearray(64)
 def text(s):raw=s.encode();out.extend(struct.pack('<I',len(raw)));out.extend(raw)
 out.extend(struct.pack('<8I10f',b['stable_id'],b['parent_id'],*r['size'],a['serialized_frame'],15,len(a['frames']),1,*a['random_range'],a['speed'],a['speed_scale'],*b['parent_position'],*b['position'],*b['offset']))
 for s in(b['node'],b['parent'],b['flag'],a['name'],r['output'],r['source']):text(s)
 out.extend(bytes.fromhex(r['source_sha256']));out.extend(bytes.fromhex(ENGINE['sha256']))
 for rect in a['frames']:out.extend(struct.pack('<4I',*rect))
 out.extend(struct.pack('<I',len(ir['sources'])))
 for p,h in ir['sources'].items():text(p);out.extend(bytes.fromhex(h))
 struct.pack_into('<8s6I20s12x',out,0,b'ENCSPL01',1,len(out),0,1,1,len(a['frames']),bytes.fromhex(PIN));struct.pack_into('<I',out,16,zlib.crc32(out)&0xffffffff);return bytes(out)

def compile_pack(root=ROOT):
 root=Path(root);blob=encode(load(root));target=root/PACK;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(blob);return blob

def assets(root,tex3ds):
 root=Path(root);ir=load(root);require(Path(tex3ds).is_file(),'Genuine tex3ds required');r=ir['resource'];target=root/'romfs'/r['output'];target.parent.mkdir(parents=True,exist_ok=True);subprocess.run([str(tex3ds),'-f','rgba8','-z','none','-o',str(target),str(root/'upstream/MOTHER-Encore'/r['source'])],check=True)
 receipt=dict(schema=1,commit=PIN,ir_sha256=digest(root/IR),producer_sha256=digest(root/'tools/present_sparkles.py'),tex3ds_sha256=digest(tex3ds),outputs={r['output']:dict(bytes=target.stat().st_size,sha256=digest(target),size=r['size'])});write_json(root/RECEIPT,receipt);compile_pack(root);return receipt

def verify_receipt(root=ROOT):
 root=Path(root);ir=load(root);r=read_json(root/RECEIPT);fields(r,('schema','commit','ir_sha256','producer_sha256','tex3ds_sha256','outputs'),'Sparkles asset receipt');path=ir['resource']['output'];require(r['schema']==1 and r['commit']==PIN and r['ir_sha256']==digest(root/IR)and r['producer_sha256']==digest(root/'tools/present_sparkles.py')and re.fullmatch('[0-9a-f]{64}',r['tex3ds_sha256'])and set(r['outputs'])=={path},'Sparkles conversion identity');o=r['outputs'][path];fields(o,('bytes','sha256','size'),'Sparkles output');p=root/'romfs'/path;require(o['bytes']>0 and p.stat().st_size==o['bytes']and digest(p)==o['sha256']and o['size']==ir['resource']['size'],'Sparkles texture receipt');return r

def stage_files(source):
 receipt=verify_receipt();ir=load();blob=encode(ir);source=Path(source)
 require((source/'data/house.encsparkles').read_bytes()==blob,'Stale Sparkles binary')
 path=Path(ir['resource']['output']);texture=(source/path).read_bytes();expected=receipt['outputs'][path.as_posix()]
 require(len(texture)==expected['bytes'] and hashlib.sha256(texture).hexdigest()==expected['sha256'],'Staged Sparkles texture differs from genuine conversion')
 return {Path('data/house.encsparkles'):blob,path:texture}

def main():
 p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile','assets','verify']);p.add_argument('--tex3ds');a=p.parse_args()
 try:
  if a.action=='extract':ir=extract();print('Original Sparkles source:',len(ir['animation']['frames']),'frames; shared RNG clamp and source lifecycle');return 0
  if a.action=='assets':require(a.tex3ds,'Genuine tex3ds argument required');assets(ROOT,a.tex3ds);print('Genuine Sparkles atlas conversion');return 0
  if a.action=='compile':blob=compile_pack();print('Sparkles checked metadata:',len(blob),'bytes; real texture conversion separate');return 0
  verify_receipt();require((ROOT/PACK).read_bytes()==encode(load()),'Stale Sparkles metadata');print('Sparkles source, metadata and genuine texture receipts verified')
 except(ValueError,KeyError,OSError,TypeError,struct.error,subprocess.CalledProcessError)as e:print('Sparkles rejected:',e,file=sys.stderr);return 1
 return 0
if __name__=='__main__':raise SystemExit(main())
