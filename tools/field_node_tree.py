#!/usr/bin/env python3
"""Complete original scene Node tree typed structure, never script approval."""
from __future__ import annotations
import argparse,hashlib,re,struct,sys,zlib,math
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import PIN,SCENE,read,write,decode,stable,sha,require
IR=ROOT/'content/podunk-node-tree.json';REVIEW=ROOT/'compatibility/reviews/podunk-node-tree-v0410.json';OUT=ROOT/'romfs/data/podunk.encnodetree'
# Native structural class schema. This approves no subclass Ready or rendering.
CLASSES=['Node','Node2D','Sprite','VisibilityNotifier2D','Position2D','CollisionShape2D','AnimationPlayer','Area2D','Timer','KinematicBody2D','VisibilityEnabler2D','TextureRect','HBoxContainer','Label','AudioStreamPlayer','RayCast2D','AnimatedSprite','StaticBody2D','CollisionPolygon2D','TileMap','YSort','Camera2D','AudioStreamPlayer2D','Tween','ReferenceRect']

# Original inherited method inventory is metadata, not a script interpreter.
METHODS=['_ready','_enter_tree','_exit_tree','_input','_unhandled_input','_unhandled_key_input','_process','_physics_process']
def script_inventory(script, inv, sources, cache, classes):
 if script in cache:return cache[script]
 file,_,sub=script.partition('::');path=ROOT/'upstream/MOTHER-Encore'/file
 require(file in inv and sha(path)==inv[file]['sha256'],'Script inheritance source differs '+file);sources[file]=inv[file]['sha256'];text=path.read_text(encoding='utf-8')
 if sub:
  import json
  block=re.search(r'\[sub_resource type="GDScript" id='+re.escape(sub)+r'\]\n(.*?)(?=\n\[|\Z)',text,re.S)
  require(block,'Embedded script missing '+script);q=re.search(r'script/source = ',block[1]);require(q,'Embedded source missing '+script);text=json.JSONDecoder(strict=False).raw_decode(block[1][q.end():])[0]
 ext=re.search(r'^extends\s+(?:"([^"\n]+)"|([A-Za-z_][A-Za-z_0-9]*))',text,re.M);require(ext,'Script superclass missing '+script)
 base=ext[1]or ext[2];inherited=0
 if ext[1]:
  if base.startswith('res://'):base=base[6:]
  else:base=str((Path(file).parent/base).as_posix())
  inherited=script_inventory(base,inv,sources,cache,classes)
 elif base in classes:inherited=script_inventory(classes[base],inv,sources,cache,classes)
 else:require(base in CLASSES+['Control','CanvasItem','Object','Reference','Resource','CollisionObject2D'],'Unknown original superclass '+base)
 mask=inherited
 for i,name in enumerate(METHODS):
  if re.search(r'^func\s+'+name+r'\s*\(',text,re.M):mask|=1<<i
 cache[script]=mask;return mask


from tools.field_script_bindings import actual_scripts

def extract(native,tree,receipt):
 d=read(native);a=read(tree);g=read(ROOT/'content/podunk-scene.json');inv=read(ROOT/'compatibility/upstream-inventory.json')['files'];require(d['source']=='res://'+SCENE and sha(native)==g['export_sha256']and a['schema']==1 and a['scene']==SCENE and a['scene_entered']is False and [a['engine'].get(k)for k in ['major','minor','patch','status']]==[3,6,2,'stable'],'NodeTree original source engine/identity differs');nm={n['path']:n for n in d['nodes']};rows=a['nodes'];require(len(nm)==len(rows)==8686 and [n['path']for n in rows]==list(nm),'NodeTree full8686 native node order differs');rm={n['path']:n for n in rows};script,nulls,actual_sources=actual_scripts(native,receipt);sources={**g['sources'],**actual_sources};classes={};cache={}
 for f in (ROOT/'upstream/MOTHER-Encore').rglob('*.gd'):
  c=re.search(r'^class_name\s+(\w+)',f.read_text(encoding='utf-8'),re.M)
  if c:require(c[1]not in classes,'Duplicate source class');classes[c[1]]=f.relative_to(ROOT/'upstream/MOTHER-Encore').as_posix()
 records=[];children={p:[]for p in nm}
 for n in rows:
  path=n['path'];require(n['class']==nm[path]['class']and n['class']in CLASSES and n['name']==nm[path]['name'],'NodeTree native class/name differs');parent=''if path=='.'else path.rsplit('/',1)[0]if'/'in path else'.';require(n['parent']==parent and (not n['owner']or n['owner']in rm),'NodeTree owner/parent missing');require(not n['unique'],'New unique-owner source not reviewed');require(n['pause']in[0,1,2]and type(n['priority'])is int,'NodeTree pause/priority differs')
  if parent:children[parent].append(path)
  props=decode(nm[path]['properties']);require(n['pause']==props['pause_mode']and n['priority']==props['process_priority'],'NodeTree native process state differs');flags=int(n['canvas']);local=[[1,0],[0,1],[0,0]];world=local;mod=[1,1,1,1];selfmod=mod;canvas_parent='';z=0;light=0
  if n['canvas']:
   scalar=lambda v:struct.unpack('<f',struct.pack('<f',float(v)))[0];local=[[scalar(v)for v in row]for row in n['local']];world=[[scalar(v)for v in row]for row in n['world']];mod=[scalar(v)for v in n['modulate']];selfmod=[scalar(v)for v in n['self_modulate']];canvas_parent=n['canvas_parent'];require(canvas_parent==(''if n['top_level']or not parent or not rm[parent]['canvas']else parent),'NodeTree native Canvas immediate parent differs');require('world_transform'not in nm[path]or world==decode(nm[path]['world_transform']),'NodeTree original native Canvas matrix differs');flags|=sum(int(n[k])<<(i+1)for i,k in enumerate(['visible','top_level','behind','use_parent_material','material','z_relative','y_sort','notify_transform','notify_local_transform']));z=n['z'];light=n['light_mask']
  for row in local+world:require(all(math.isfinite(v)for v in row),'Nonfinite NodeTree transform')
  s,h=script.get(path,('', '0'*64));method_mask=script_inventory(s,inv,sources,cache,classes)if s else 0;records.append(dict(id=stable(path),node=path,name=n['name'],class_index=CLASSES.index(n['class']),parent=stable(parent)if parent else 0,owner=stable(n['owner'])if n['owner']else 0,canvas_parent=stable(canvas_parent)if canvas_parent else 0,index=n['index'],pause=n['pause'],priority=n['priority'],flags=flags,z=z,light_mask=light,local=local,world=world,modulate=mod,self_modulate=selfmod,groups=n['groups'],script=s,script_sha=h,script_methods=method_mask,ready=0))
 order=[]
 def visit(p):
  for child in children[p]:visit(child)
  order.append(p)
 visit('.');ready={p:i for i,p in enumerate(order)}
 for row in records:row['ready']=ready[row['node']]
 require(len(order)==8686 and sum(bool(r['script'])for r in records)==len(script),'NodeTree full script/Ready roster differs')
 for p,h in sources.items():require(h==inv[p]['sha256']==sha(ROOT/'upstream/MOTHER-Encore'/p),'NodeTree changed source '+p)
 write(IR,dict(schema=1,kind='encore.field-node-tree.source-ir',commit=PIN,scene=SCENE,scene_id=stable('.'),source_sha256=sources[SCENE],sources=sources,native_sha256=sha(native),tree_export_sha256=sha(tree),exporter_sha256=sha(ROOT/'tools/godot_exporter/field_node_tree.gd'),classes=CLASSES,scene_admitted=False,script_null_overrides=nulls,source_receipt_sha256=sha(receipt),script_methods=METHODS,records=records,pending=['No native class or script Ready/enter/process capability is admitted by this structural resource','Actual typed adapters and source signal/message registry, dynamic factory provenance required','Native CanvasLayer/Viewport external root is supplied by live checked host; no synthetic singleton tree','SourceGroup native78 does not erase quarantined script _input/_process/_physics methods']))

def load():
 d=read(IR);r=read(REVIEW);inv=read(ROOT/'compatibility/upstream-inventory.json')['files'];require(d['schema']==1 and d['commit']==PIN and d['scene']==SCENE and not d['scene_admitted']and r['commit']==PIN and r['ir_sha256']==sha(IR)and d['exporter_sha256']==sha(ROOT/'tools/godot_exporter/field_node_tree.gd')and d['classes']==CLASSES,'NodeTree review/exporter/schema differs')
 for p,h in d['sources'].items():require(h==inv[p]['sha256']==sha(ROOT/'upstream/MOTHER-Encore'/p),'Changed NodeTree source '+p)
 return d

def encode(d,ir_sha256=None):
 b=bytearray(128)
 def u(*v):b.extend(struct.pack('<'+'I'*len(v),*v))
 def i(*v):b.extend(struct.pack('<'+'i'*len(v),*v))
 def f(*v):b.extend(struct.pack('<'+'f'*len(v),*v))
 def t(s):v=s.encode();u(len(v));b.extend(v)
 t(d['scene']);u(len(d['classes']));[t(v)for v in d['classes']]
 for r in d['records']:
  u(*[r[k]for k in ['id','parent','owner','canvas_parent','class_index','ready','pause','flags','light_mask','script_methods']]);i(r['index'],r['priority'],r['z']);f(*[v for row in r['local']for v in row],*[v for row in r['world']for v in row],*r['modulate'],*r['self_modulate']);t(r['node']);t(r['name']);t(r['script']);b.extend(bytes.fromhex(r['script_sha']));u(len(r['groups']));[t(v)for v in r['groups']]
 u(len(d['sources']))
 for p,h in d['sources'].items():t(p);b.extend(bytes.fromhex(h))
 require(ir_sha256 is None or re.fullmatch('[0-9a-f]{64}',ir_sha256),'NodeTree explicit IR hash rejected')
 struct.pack_into('<8s8I',b,0,b'ENCFNTR1',1,128,len(b),0,0x454e003c,3,len(d['records']),d['scene_id']);b[40:60]=bytes.fromhex(PIN);b[60:92]=bytes.fromhex(d['source_sha256']);b[92:124]=bytes.fromhex(ir_sha256)if ir_sha256 is not None else hashlib.sha256(IR.read_bytes()).digest();struct.pack_into('<I',b,20,zlib.crc32(b[128:]));return bytes(b)

def stage_files(source):
 raw=encode(load());require((Path(source)/'data/podunk.encnodetree').read_bytes()==raw,'StagedNodeTree differs');return {Path('data/podunk.encnodetree'):raw}

def main():
 p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile','verify']);p.add_argument('--native',type=Path);p.add_argument('--tree',type=Path);p.add_argument('--source',type=Path);a=p.parse_args()
 if a.action=='extract':require(a.native and a.tree and a.source,'Explicit complete native/tree/source receipt required');extract(a.native,a.tree,a.source);return
 d=load();raw=encode(d)
 if a.action=='verify':require(OUT.read_bytes()==raw,'NodeTree stale output')
 else:OUT.parent.mkdir(parents=True,exist_ok=True);OUT.write_bytes(raw)
 print('NodeTree source',len(d['records']),'actual nodes;',len(raw),'bytes; scene_admitted=False')
if __name__=='__main__':
 try:main()
 except(ValueError,KeyError,TypeError,OSError,struct.error)as e:sys.exit('FIELD NODE TREE ERROR: '+str(e))
