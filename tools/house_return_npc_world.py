#!/usr/bin/env python3
"""Actual House native body/ray/child/callback closure, no lifecycle admission."""
from __future__ import annotations
import argparse,struct,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import PIN,read,write,sha,require
from tools import field_npc_world as world,house_node_tree as tree,house_geometry as house,house_return_npc as npc
IR=ROOT/'content/native-house-return-npc-world.json'
REVIEW=ROOT/'reports/house-return-npc-world/source-review.json'
PACK=ROOT/'romfs/data/house-return.encnpcworld'
SCENE=world.NpcWorldScene(house.SCENE,tree.IR,tree.IR,npc.IR,IR,REVIEW,6,6,6,tree.node_id,'native_sha256')

def validate(d):
 t=tree.load();n=npc.load();records={r['id']:r for r in t['records']}
 require(d['schema']==d['format']==d['capability']==d['rules']==1 and d['family']==world.FAMILY and d['commit']==PIN and d['scene']==house.SCENE and d['scene_id']==t['scene_id']==n['scene_id'] and d['source_sha256']==t['source_sha256']==n['sources'][house.SCENE],'House NPC world identity/source differs')
 require(len(d['bodies'])==len(d['rays'])==len(d['npcs'])==6 and len(d['callbacks'])==13 and d['admission_ready']is False,'House NPC world complete topology differs')
 for kind,rows in (('KinematicBody2D',d['bodies']),('RayCast2D',d['rays'])):
  for row in rows:
   r=records.get(row['id']);require(r is not None and r['node']==row['path'] and t['classes'][r['class_index']]==kind,'House NPC world actual native node differs')
   if kind=='RayCast2D':require(r['parent']==row['parent'] and not r['script'],'House NPC ray parent/script differs')
   else:require(r['script']==npc.npc.SCRIPT and r['script_sha']==n['sources'][npc.npc.SCRIPT] and all(s['id']in records and records[s['id']]['parent']==row['id']for s in row['shapes']),'House NPC body script/shape owner differs')
 require({r['id']for r in d['npcs']}=={r['id']for r in n['npcs']},'House NPC link binding differs')
 for row in d['npcs']:
  for key,id in row.items():require(id in records,'House NPC child link missing '+key)

def review(d):
 return dict(schema=1,commit=PIN,ir_sha256=sha(IR),scene_id=d['scene_id'],sources=d['sources'],engine=d['engine'],native_sha256=d['native_sha256'],tree_ir_sha256=d['tree_ir_sha256'],npc_ir_sha256=d['npc_ir_sha256'],producer_sha256=sha(Path(__file__)),shared_producer_sha256=sha(ROOT/'tools/field_npc_world.py'),body_count=6,ray_count=6,npc_link_count=6,callback_count=13,admission_ready=False,semantics=['Actual safe Godot 3.6.2 House export supplies every body shape-owner and Ray enabled/exclude-parent/body/area/mask/cast property','Actual engine source proof retains cached Ray query and zero-cast substitution; no synthetic immediate force-update','All six source NPCs have nine actual native child links and thirteen audited source callback names/arities','Same House root stable ID and pinned scene/script closure; geometry schema resource identity remains independently checked by runtime'])

def extract(engine):
 npc.load();d=world.derive(house.NATIVE,engine,SCENE);validate(d);write(IR,d);write(REVIEW,review(d));return d

def load():
 d=world.load(SCENE);validate(d)
 # The previously reviewed exact engine files are shared, while every native
 # House record is re-derived from its own source-safe export and NPC resource.
 engine=read(world.IR)['engine'];require(d['engine']==engine,'House NPC world exact shared engine proof differs')
 require(d==world.derive(house.NATIVE,None,SCENE,engine) and read(REVIEW)==review(d),'House NPC world reviewed source stale');return d

def pack(d):return world.encode(d,sha(IR))

def stage_files(root):
 b=pack(load());p=Path('data/house-return.encnpcworld');require((Path(root)/p).read_bytes()==b,'House NPC world staged binary differs');return {p:b}

def main():
 p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile','verify']);p.add_argument('--engine',type=Path);a=p.parse_args()
 if a.action=='extract':require(a.engine is not None,'House NPC world requires actual reviewed Godot 3.6.2 engine sources');d=extract(a.engine)
 else:d=load()
 b=pack(d)
 if a.action=='verify':require(PACK.read_bytes()==b,'House NPC world binary stale')
 else:PACK.parent.mkdir(parents=True,exist_ok=True);PACK.write_bytes(b)
 print('Actual House NPC world: 6 bodies; 6 rays; 6 NPC links;',len(b),'bytes; Ready not granted')

if __name__=='__main__':
 try:main()
 except(ValueError,KeyError,TypeError,OSError,struct.error)as e:sys.exit('HOUSE NPC WORLD ERROR: '+str(e))
