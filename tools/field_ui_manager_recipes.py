#!/usr/bin/env python3
"""Complete source UiManager recipes; no constructor or Ready capability granted."""
from __future__ import annotations
import argparse,hashlib,re,struct,sys,zlib,math,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import PIN,read,write,decode,sha,require
from tools.field_node_tree import METHODS
from tools.field_node_recipe import CLASSES as ORIGINAL_CLASSES,scalar,numbers
import tools.field_node_recipe as source_helpers
CLASSES=ORIGINAL_CLASSES+['PanelContainer','ColorRect','CenterContainer','MarginContainer','VBoxContainer','TextureButton','ScrollContainer','HScrollBar','TextureProgress','Panel','Path2D','PathFollow2D']
CONTROLS=[CLASSES[i]for i in [11,12,13,24]+list(range(26,41))]
SCENE=''

def evidence(a,b):
 if isinstance(a,list):return isinstance(b,list)and len(a)==len(b)and all(evidence(x,y)for x,y in zip(a,b))
 return isinstance(a,(int,float))and isinstance(b,(int,float))and abs(a-b)<=0.500001e-6
def stable(path):
 value=int.from_bytes(hashlib.sha256(('recipe:'+SCENE+'#'+path).encode()).digest()[:4],'little');require(value,'Zero UI source ID');return value
def source_scripts(d,proof,sources,inv):
 previous=source_helpers.SCENE
 try:
  source_helpers.SCENE=SCENE;return source_helpers.source_scripts(d,proof,sources,inv)
 finally:source_helpers.SCENE=previous
def method_inventory(script,inv,sources,cache,classes):
 previous=source_helpers.CLASSES
 try:
  source_helpers.CLASSES=CLASSES+['BaseButton','Range','Container'];return source_helpers.method_inventory(script,inv,sources,cache,classes)
 finally:source_helpers.CLASSES=previous

def extract_recipe(scene,native,tree,receipt):
 global SCENE
 SCENE=scene
 d=read(native);a=read(tree);proof=read(receipt);inv=read(ROOT/'compatibility/upstream-inventory.json')['files']
 require(d['source']=='res://'+SCENE and d['native_compatible']is False and a['schema']==1 and a['scene']==SCENE and a['scene_entered']is False and [a['engine'].get(k)for k in ['major','minor','patch','status']]==[3,6,2,'stable']and [d['godot'].get(k)for k in ['major','minor','patch','status']]==[3,6,2,'stable'],'Recipe original engine/scene rejected')
 require(proof['commit']==PIN and proof['scene']==SCENE,'Recipe source receipt pin rejected');sources={}
 for file,r in proof['files'].items():require(sha(ROOT/'upstream/MOTHER-Encore'/file)==r['sha256']==inv[file]['sha256'],'Recipe source changed '+file);sources[file]=r['sha256']
 nm={n['path']:n for n in d['nodes']};rows=a['nodes'];require(len(nm)==len(rows)and bool(rows) and [n['path']for n in rows]==list(nm),'Recipe full original 47 nodes required');rm={n['path']:n for n in rows};script,nulls=source_scripts(d,proof,sources,inv);classes={};cache={}
 for f in(ROOT/'upstream/MOTHER-Encore').rglob('*.gd'):
  c=re.search(r'^class_name\s+(\w+)',f.read_text(encoding='utf-8'),re.M)
  if c:require(c[1]not in classes,'Duplicate recipe class_name');classes[c[1]]=f.relative_to(ROOT/'upstream/MOTHER-Encore').as_posix()
 records=[];children={p:[]for p in nm};controls=[];layers=[]
 for n in rows:
  path=n['path'];require(n['class']==nm[path]['class']and n['class']in CLASSES and n['name']==nm[path]['name'],'Recipe native class/name rejected');parent=''if path=='.'else path.rsplit('/',1)[0]if'/'in path else'.';require(n['parent']==parent and(not n['owner']or n['owner']in rm)and not n['unique'],'Recipe owner/unique path rejected');props=decode(nm[path]['properties']);require(n['pause']==props['pause_mode']and n['priority']==props['process_priority'],'Recipe source process state rejected')
  if parent:children[parent].append(path)
  flags=int(n['canvas']);local=[[1,0],[0,1],[0,0]];world=local;mod=[1,1,1,1];selfmod=mod;cp='';z=light=0
  if n['canvas']:
   local=numbers(n['local']);world=numbers(n['world']);mod=numbers(n['modulate']);selfmod=numbers(n['self_modulate']);cp=n['canvas_parent'];require(cp==(''if n['top_level']or not parent or not rm[parent]['canvas']else parent),'Recipe Canvas ancestry rejected');require('world_transform'not in nm[path]or all(abs(x-y)<=0.500001e-6 for left,right in zip(world,decode(nm[path]['world_transform']))for x,y in zip(left,right)),'Recipe native JSON-six-decimal world evidence rejected');flags|=sum(int(n[k])<<(i+1)for i,k in enumerate(['visible','top_level','behind','use_parent_material','material','z_relative','y_sort','notify_transform','notify_local_transform']));z=n['z'];light=n['light_mask']
  s,h=script.get(path,('','0'*64));mask=method_inventory(s,inv,sources,cache,classes)if s else 0;generated=n['name'].startswith('@');require(not generated or (n['class']=='VScrollBar'and parent and rm[parent]['class']=='RichTextLabel'and not n['owner']and re.fullmatch(r'@@[0-9]+',n['name'])),'Unknown native internal constructor node');records.append(dict(native_generated=generated,id=stable(path),node=path,name=n['name'],class_index=CLASSES.index(n['class']),native_class=n['class'],parent=stable(parent)if parent else 0,owner=stable(n['owner'])if n['owner']else 0,canvas_parent=stable(cp)if cp else 0,index=n['index'],pause=n['pause'],priority=n['priority'],flags=flags,z=z,light_mask=light,local=local,world=world,modulate=mod,self_modulate=selfmod,groups=n['groups'],script=s,script_sha=h,script_methods=mask,ready=0))
  if n['class']in CONTROLS:
   c=n['control'];c={k:numbers(v)if isinstance(v,list)else scalar(v)if k in ['rotation','stretch']else v for k,v in c.items()};require(evidence(c['anchors'],[props['anchor_'+v]for v in ['left','top','right','bottom']])and evidence(c['margins'],[props['margin_'+v]for v in ['left','top','right','bottom']])and evidence(c['scale'],props['rect_scale'])and evidence(c['rotation'],props['rect_rotation'])and evidence(c['min_size'],props['rect_min_size']),'Recipe native Control properties rejected');controls.append(dict(id=stable(path),**c))
  if n['class']=='CanvasLayer':
   c=n['canvas_layer'];require(c['custom_viewport']is False and props.get('custom_viewport')is None,'Recipe source custom Viewport not supported');c={k:numbers(v)if isinstance(v,list)else scalar(v)if k in ['rotation','follow_scale']else v for k,v in c.items()};require(c['transform']==props['transform']and c['layer']==props['layer'],'Recipe CanvasLayer native properties rejected');layers.append(dict(id=stable(path),visible=props['visible'],world_2d_binding=0,**c))
 order=[]
 def visit(p):
  for child in children[p]:visit(child)
  order.append(p)
 visit('.');ready={p:i for i,p in enumerate(order)}
 for r in records:r['ready']=ready[r['node']]
 require(len(order)==len(rows),'Recipe incomplete ready traversal')
 return dict(schema=1,kind='encore.field-node-recipe.source-ir',commit=PIN,scene=SCENE,scene_id=stable('.'),source_sha256=sources[SCENE],sources=sources,native_sha256=sha(native),tree_export_sha256=sha(tree),source_receipt_sha256=sha(receipt),case_aliases=proof.get('case_aliases',{}),exporter_sha256=sha(ROOT/'tools/godot_exporter/field_ui_manager.gd'),classes=CLASSES,script_methods=METHODS,script_null_overrides=nulls,records=records,controls=controls,canvas_layers=layers,scene_admitted=False,pending=['Native CanvasLayer external Viewport/World2D must be supplied by real host at ENTER','Every native class and inherited script requires actual typed owner callbacks','RichTextLabel native VScrollBar children retained; no fake Control Ready/layout/render approval','Source timers/animations/fonts/audio/signals owned by independent checked consumers'])


def encode_recipe(d):
 b=bytearray(128)
 def u(*v):b.extend(struct.pack('<'+'I'*len(v),*v))
 def i(*v):b.extend(struct.pack('<'+'i'*len(v),*v))
 def f(*v):b.extend(struct.pack('<'+'f'*len(v),*v))
 def t(s):v=s.encode();u(len(v));b.extend(v)
 t(d['scene']);u(len(d['classes']));[t(v)for v in d['classes']]
 for r in d['records']:
  u(*[r[k]for k in ['id','parent','owner','canvas_parent','class_index','ready','pause','flags','light_mask','script_methods']]);i(r['index'],r['priority'],r['z']);f(*[v for row in r['local']for v in row],*[v for row in r['world']for v in row],*r['modulate'],*r['self_modulate']);t(r['node']);t(r['name']);t(r['script']);b.extend(bytes.fromhex(r['script_sha']));u(len(r['groups']));[t(v)for v in r['groups']];u(int(r['native_generated']))
 u(len(d['sources']))
 for p,h in d['sources'].items():t(p);b.extend(bytes.fromhex(h))
 u(len(d['canvas_layers']))
 for c in d['canvas_layers']:
  u(c['id']);i(c['layer']);u(int(c['follow_viewport']),int(c['custom_viewport']),c['world_2d_binding'],int(c['visible']));f(c['follow_scale'],*[v for row in c['transform']for v in row],*c['offset'],c['rotation'],*c['scale'])
 u(len(d['controls']))
 for c in d['controls']:
  u(c['id'],int(c['clip']),c['mouse'],c['focus']);i(*[int(v)for v in c['grow']]);u(*[int(v)for v in c['size_flags']]);f(*c['anchors'],*c['margins'],*c['position'],*c['size'],*c['scale'],c['rotation'],*c['pivot'],*c['min_size'],c['stretch'])
 struct.pack_into('<8s8I',b,0,b'ENCFNRC1',1,128,len(b),0,0x454e003d,4,len(d['records']),d['scene_id']);b[40:60]=bytes.fromhex(PIN);b[60:92]=bytes.fromhex(d['source_sha256']);b[92:124]=hashlib.sha256(json.dumps(d,ensure_ascii=False,sort_keys=True,separators=(',',':')).encode()).digest();struct.pack_into('<I',b,20,zlib.crc32(b[128:]));return bytes(b)
