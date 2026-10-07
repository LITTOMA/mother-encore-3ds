#!/usr/bin/env python3
"""Complete source House structural tree; no native/script lifecycle approval."""
from __future__ import annotations
import argparse,math,re,struct,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import PIN,read,write,decode,stable,sha,require
from tools import field_node_tree as tree,house_geometry as house
SCENE=house.SCENE
IR=ROOT/'content/native-house-node-tree.json'
NATIVE_TREE=ROOT/'reports/house-node-tree/native-tree.json'
REVIEW=ROOT/'reports/house-node-tree/source-review.json'
PACK=ROOT/'romfs/data/house.encnodetree'
# Preserve every existing structural class ordinal and add the two actual
# native House classes missing from the original Podunk structural roster.
CLASSES=tree.CLASSES+['Control','ColorRect']
EXPORTER=ROOT/'tools/godot_exporter/field_node_tree.gd'
def exporter():
 text=EXPORTER.read_text(encoding='utf-8')
 require(text.count('Maps/podunk/podunk.tscn')==2 and text.count('field_node_tree.json')==1,'Structural exporter source changed')
 return text.replace('Maps/podunk/podunk.tscn',SCENE).replace('field_node_tree.json','house_node_tree.json')
def node_id(path):return stable('house-reentry-native:'+SCENE+'#'+path)if path else 0
def scalar(value):
 v=struct.unpack('<f',struct.pack('<f',float(value)))[0]
 require(math.isfinite(v),'House nonfinite native structural scalar');return v
def derive():
 d=read(house.NATIVE);s=read(house.SOURCE);a=read(NATIVE_TREE)
 inv=read(ROOT/'compatibility/upstream-inventory.json');sources={}
 require(d['schema']==s['schema']==a['schema']==1 and d['source']=='res://'+SCENE and s['scene']==a['scene']==SCENE and s['commit']==PIN==inv['commit'] and a['scene_entered']is False,'House structural source identity differs')
 require([a['engine'].get(k)for k in ('major','minor','patch','status')]==[3,6,2,'stable'],'House structural native engine differs')
 for p,v in s['files'].items():
  require(sha(ROOT/'upstream/MOTHER-Encore'/p)==v['sha256']==inv['files'][p]['sha256'],'House structural source changed '+p);sources[p]=v['sha256']
 require(len(sources)==71,'House structural original dependency count differs')
 nm={n['path']:n for n in d['nodes']};rm={n['path']:n for n in a['nodes']};resources={r['id']:r for r in d['resources']}
 require(len(nm)==len(rm)==len(a['nodes'])==497 and list(nm)==list(rm),'House structural full 497 native order differs')
 bindings,nulls,attachments=house.scripts(d,s,sources,resources)
 require(len(bindings)==124 and len(attachments)==25,'House structural original script closure differs')
 classes={};cache={}
 for f in (ROOT/'upstream/MOTHER-Encore').rglob('*.gd'):
  c=re.search(r'^class_name\s+(\w+)',f.read_text(encoding='utf-8'),re.M)
  if c:require(c[1]not in classes,'House duplicate named original script class');classes[c[1]]=f.relative_to(ROOT/'upstream/MOTHER-Encore').as_posix()
 records=[];children={p:[]for p in nm}
 for path,n in rm.items():
  parent=''if path=='.'else house.geometry.parent(path);original=nm[path];props=decode(original['properties'])
  require(n['class']==original['class']and n['class']in CLASSES and n['name']==original['name'],'House native class/name differs '+path)
  require(n['parent']==parent and (not n['owner']or n['owner']in rm) and not n['unique'],'House native owner/parent/unique mechanism differs '+path)
  require(type(n['index'])is int and type(n['priority'])is int and n['pause']in(0,1,2)and n['pause']==props['pause_mode']and n['priority']==props['process_priority'],'House native process/index differs '+path)
  require(isinstance(n['groups'],list)and len(n['groups'])==len(set(n['groups']))and all(isinstance(g,str)and g for g in n['groups']),'House native source groups malformed '+path)
  if parent:
   require(n['index']==len(children[parent]),'House actual native sibling order differs '+path);children[parent].append(path)
  else:require(n['index']==-1,'House actual root native index differs')
  flags=int(n['canvas']);local=[[1,0],[0,1],[0,0]];world=local;mod=[1,1,1,1];selfmod=mod;canvas_parent='';z=0;light=0
  if n['canvas']:
   local=[[scalar(v)for v in row]for row in n['local']];world=[[scalar(v)for v in row]for row in n['world']]
   require(len(local)==len(world)==3 and all(len(row)==2 for row in local+world),'House native matrix shape differs '+path)
   mod=[scalar(v)for v in n['modulate']];selfmod=[scalar(v)for v in n['self_modulate']];require(len(mod)==len(selfmod)==4,'House native modulation shape differs')
   canvas_parent=n['canvas_parent'];require(canvas_parent==(''if n['top_level']or not parent or not rm[parent]['canvas']else parent),'House native immediate Canvas parent differs '+path)
   require('world_transform'not in original or world==decode(original['world_transform']),'House native matrix differs from original export '+path)
   flags|=sum(int(n[k])<<(i+1)for i,k in enumerate(('visible','top_level','behind','use_parent_material','material','z_relative','y_sort','notify_transform','notify_local_transform')))
   z=n['z'];light=n['light_mask']
  script,proof=bindings.get(path,('', '0'*64));methods=tree.script_inventory(script,inv['files'],sources,cache,classes)if script else 0
  records.append(dict(id=node_id(path),node=path,name=n['name'],class_index=CLASSES.index(n['class']),parent=node_id(parent),owner=node_id(n['owner']),canvas_parent=node_id(canvas_parent),index=n['index'],pause=n['pause'],priority=n['priority'],flags=flags,z=z,light_mask=light,local=local,world=world,modulate=mod,self_modulate=selfmod,groups=n['groups'],script=script,script_sha=proof,script_methods=methods,ready=0))
 ready=[]
 def visit(path):
  for c in children[path]:visit(c)
  ready.append(path)
 visit('.');require(len(ready)==497,'House full native Ready traversal differs');order={p:i for i,p in enumerate(ready)}
 for row in records:row['ready']=order[row['node']]
 require(len({r['id']for r in records})==497 and sum(bool(r['script'])for r in records)==124,'House stable ID/script coverage differs')
 for p,h in sources.items():require(h==inv['files'][p]['sha256']==sha(ROOT/'upstream/MOTHER-Encore'/p),'House inherited source closure changed '+p)
 return dict(schema=1,kind='encore.field-node-tree.source-ir',commit=PIN,scene=SCENE,scene_id=node_id('.'),source_sha256=sources[SCENE],sources=sources,native_sha256=sha(house.NATIVE),tree_export_sha256=sha(NATIVE_TREE),exporter_sha256=sha(EXPORTER),source_receipt_sha256=sha(house.SOURCE),producer_sha256=sha(Path(__file__)),geometry_producer_sha256=sha(ROOT/'tools/house_geometry.py'),shared_producer_sha256=sha(ROOT/'tools/field_node_tree.py'),classes=CLASSES,scene_admitted=False,source_attachments=attachments,script_null_overrides=nulls,script_methods=tree.METHODS,records=records,pending=['Structural data grants no native or script constructor, Enter, Ready, process, physics, input or signal capability','Full actual House lifecycle requires concrete checked source owners for all original native/script nodes; no subset skeleton admission','Native CanvasLayer/Viewport and singleton SceneTree are supplied by the actual checked global owner'])
def review(d):
 return dict(schema=1,commit=PIN,ir_sha256=sha(IR),native_sha256=d['native_sha256'],tree_export_sha256=d['tree_export_sha256'],source_receipt_sha256=d['source_receipt_sha256'],exporter_sha256=d['exporter_sha256'],producer_sha256=d['producer_sha256'],shared_producer_sha256=d['shared_producer_sha256'],sources=d['sources'],node_count=497,script_binding_count=124,original_attachment_count=25,classes=CLASSES,scene_admitted=False,semantics=['Actual Godot 3.6.2 source-safe full House instance; no scene entry, original scripts or Ready executed','Native owner/groups/sibling ordering/Canvas flags/notifications/matrices exported directly, never guessed from serialized declarations','Original script and explicit-null overlays restored and inherited callback inventory derived from pinned source','Existing ENCFNTR1 format1/family0x454e003c/capability3; actual House ID namespace preserves retained Root/Objects identity'])
def extract():
 d=derive();write(IR,d);write(REVIEW,review(d));return d
def load():
 d=read(IR);require(d==derive() and read(REVIEW)==review(d),'House node-tree original source/review stale');return d
def pack(d):return tree.encode(d,sha(IR))
def stage_files(root):
 raw=pack(load());relative=Path('data/house.encnodetree');require((Path(root)/relative).read_bytes()==raw,'House native tree staged pack differs');return {relative:raw}
def main():
 p=argparse.ArgumentParser();p.add_argument('action',choices=['exporter','extract','compile','verify']);p.add_argument('--out',type=Path);a=p.parse_args()
 if a.action=='exporter':require(a.out is not None,'House structural exporter explicit output required');a.out.write_text(exporter(),encoding='utf-8');return
 d=extract()if a.action=='extract'else load();raw=pack(d)
 if a.action=='verify':require(PACK.read_bytes()==raw,'House node-tree stale binary')
 else:PACK.parent.mkdir(parents=True,exist_ok=True);PACK.write_bytes(raw)
 print('House actual native structural tree:',len(d['records']),'nodes;',sum(bool(r['script'])for r in d['records']),'source script bindings;',len(raw),'bytes; no scene lifecycle admitted')
if __name__=='__main__':
 try:main()
 except(ValueError,KeyError,TypeError,OSError,struct.error)as e:sys.exit('HOUSE NODE TREE ERROR: '+str(e))
