#!/usr/bin/env python3
"""Actual six House npc.gd instances; data admission never grants Ready."""
from __future__ import annotations
import argparse,struct,sys,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import PIN,read,write,sha,require
from tools import field_npc as npc,house_node_tree as tree,house_geometry as house
IR=ROOT/'content/native-house-return-npc.json'
REVIEW=ROOT/'reports/house-return-npc/source-review.json'
PACK=ROOT/'romfs/data/house-return-npcs.encnpc'
ASSET_RECEIPT=ROOT/'content/asset-receipts/graphics/house-return-npcs.json'
EXISTING_RECEIPT=ROOT/'content/asset-receipts/graphics/npcs/source.json'
ROOTS=('Objects/npc','Objects/npc2','Objects/npc3','Objects/pillow','Objects/lamp','Objects/npcdoll')
SCENE=npc.NpcScene(house.SCENE,tree.node_id('.'),497,6,tree.node_id,ROOTS,True)

def derive():
 t=tree.load();bindings={n['node']:n['script']for n in t['records']if n['script']}
 d=npc.extract(house.NATIVE,house.SOURCE,SCENE,bindings);rows={n['node']:n for n in t['records']}
 for n in d['npcs']:
  r=rows[n['node']]
  require(r['class_index']==t['classes'].index('KinematicBody2D') and r['script']==npc.SCRIPT and r['script_sha']==d['sources'][npc.SCRIPT],'House NPC actual source script/class differs')
  require(r['id']==n['id'] and r['ready']==n['ready_ordinal'] and r['world'][:2]==[[1,0],[0,1]] and r['world'][2]==n['position'],'House NPC full structural identity/Ready/matrix differs')
 d['sources']={**t['sources'],**d['sources']}
 d['house_tree_ir_sha256']=sha(tree.IR)
 d['producer_sha256']=sha(Path(__file__))
 d['shared_producer_sha256']=sha(ROOT/'tools/field_npc.py')
 npc.validate(d,SCENE)
 return d

def review(d):
 existing=read(ROOT/'content/native-field-npc.json')['assets'];references={}
 for path,a in d['assets'].items():
  old=existing.get(path)
  reused=old is not None and old==a
  references[path]=dict(source=a['source'],source_sha256=a['source_sha256'],size=a['size'],grid=a['grid'],existing_podunk_resource=reused)
 return dict(schema=1,commit=PIN,ir_sha256=sha(IR),scene=d['scene'],scene_id=d['scene_id'],npc_count=6,tree_ir_sha256=d['house_tree_ir_sha256'],native_export_sha256=d['provenance']['native_export_sha256'],source_receipt_sha256=d['provenance']['source_receipt_sha256'],producer_sha256=d['producer_sha256'],shared_producer_sha256=d['shared_producer_sha256'],sources=d['sources'],programmes=d['programs'],assets=references,source_guarded_absent_transitions=d['source_guarded_absent_transitions'],license_review=d['license_review'],assumptions=['All 497 source-safe native nodes and all 124 reviewed original script bindings are cross-bound to the full House tree','The six npc.gd roots preserve original inner-to-outer serialized overrides and actual native postorder Ready positions','Native root and Objects matrices are checked identity bases; rotated/scaled NPC roots are rejected','Original npc.gd exports, animation YAML, PNG dimensions, dialogue selections and child collision geometry use the shared strict source compiler','Original character_sprite.gd has_node guard prevents missing Talk states from creating transitions; the rejected pillow/doll declarations remain reviewed in IR',
 'Original source texture references require actual tex3ds conversion and sprite owner admission; the resource alone does not grant rendering or scene Ready'],admission_ready=False)

def extract():
 d=derive();write(IR,d);write(REVIEW,review(d));return d

def load():
 d=read(IR);require(d==derive() and read(REVIEW)==review(d),'House NPC original source/review stale');return d

def pack(d):return npc.pack(d,SCENE)

def assets(d,tex3ds):
 old=read(EXISTING_RECEIPT);existing=read(ROOT/'content/native-field-npc.json')['assets'];outputs={}
 require(old['commit']==PIN and old['recipe_sha256']==sha(ROOT/'content/native-field-npc.json'),'House NPC existing sprite receipt stale')
 for name,a in d['assets'].items():
  require(sha(ROOT/'upstream/MOTHER-Encore'/a['source'])==a['source_sha256'],'House NPC source PNG changed')
  target=ROOT/'romfs'/name;reused=existing.get(name)==a
  if reused:
   require(old['outputs'][name]==dict(bytes=target.stat().st_size,sha256=sha(target)),'House NPC retained sprite binary differs')
  else:
   target.parent.mkdir(parents=True,exist_ok=True)
   subprocess.run([str(tex3ds),'-f','rgba8','-z','none','-o',str(target),str(ROOT/'upstream/MOTHER-Encore'/a['source'])],check=True)
  outputs[name]=dict(**a,bytes=target.stat().st_size,sha256=sha(target),retained_existing=reused)
 write(ASSET_RECEIPT,dict(schema=1,commit=PIN,recipe_sha256=sha(IR),producer_sha256=sha(Path(__file__)),existing_receipt_sha256=sha(EXISTING_RECEIPT),tex3ds_sha256=sha(tex3ds),license_sha256=d['sources']['LICENSE'],license_review=d['license_review'],conversion=['-f','rgba8','-z','none'],outputs=outputs))
 checked_assets(d)

def checked_assets(d):
 r=read(ASSET_RECEIPT);old=read(EXISTING_RECEIPT);existing=read(ROOT/'content/native-field-npc.json')['assets'];outputs={}
 require(r['schema']==1 and r['commit']==PIN and r['recipe_sha256']==sha(IR) and r['producer_sha256']==sha(Path(__file__)) and r['existing_receipt_sha256']==sha(EXISTING_RECEIPT) and r['license_sha256']==d['sources']['LICENSE'] and r['license_review']==d['license_review'] and r['conversion']==['-f','rgba8','-z','none'] and set(r['outputs'])==set(d['assets']),'House NPC sprite provenance stale')
 require(old['commit']==PIN and old['recipe_sha256']==sha(ROOT/'content/native-field-npc.json'),'House NPC retained sprite receipt stale')
 for name,a in d['assets'].items():
  raw=(ROOT/'romfs'/name).read_bytes();record=dict(**a,bytes=len(raw),sha256=sha(ROOT/'romfs'/name),retained_existing=existing.get(name)==a)
  require(r['outputs'][name]==record,'House NPC converted sprite bytes/source differs '+name)
  if record['retained_existing']:require(old['outputs'][name]==dict(bytes=len(raw),sha256=record['sha256']),'House NPC retained sprite differs '+name)
  outputs[Path(name)]=raw
 return outputs

def stage_files(root):
 d=load();b=pack(d);p=Path('data/house-return-npcs.encnpc');require((Path(root)/p).read_bytes()==b,'House NPC staged binary differs');out={p:b}
 for asset,raw in checked_assets(d).items():require((Path(root)/asset).read_bytes()==raw,'House NPC staged sprite differs '+str(asset));out[asset]=raw
 return out

def main():
 p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile','verify','assets']);p.add_argument('--tex3ds',type=Path);a=p.parse_args();d=extract()if a.action=='extract'else load()
 if a.action=='assets':require(a.tex3ds is not None,'House NPC sprite conversion needs actual tex3ds');assets(d,a.tex3ds);print('House NPC sprites: all 7 checked references; 3 actual source PNG conversions');return
 b=pack(d)
 if a.action=='verify':require(PACK.read_bytes()==b,'House NPC binary stale')
 else:PACK.parent.mkdir(parents=True,exist_ok=True);PACK.write_bytes(b)
 print('Actual House NPC:',len(d['npcs']),'roots;',len(d['programs']),'source programme paths;',len(b),'bytes; Ready not granted')

if __name__=='__main__':
 try:main()
 except(ValueError,KeyError,TypeError,OSError,struct.error,subprocess.SubprocessError)as e:sys.exit('HOUSE NPC ERROR: '+str(e))
