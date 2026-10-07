#!/usr/bin/env python3
"""Actual full House CharacterTint resource, using the shared ENCTINT1 codec."""
from __future__ import annotations
import argparse,struct,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import PIN,read,write,sha,require
from tools import field_tint as tint,house_node_tree as tree,house_geometry as house
IR=ROOT/'content/native-house-return-tint.json'
REVIEW=ROOT/'reports/house-return-tint/source-review.json'
PACK=ROOT/'romfs/data/house-return-tint.enctint'
SCENE=tint.TintScene(house.SCENE,tree.node_id('.'),497,10,tree.node_id,'House',False)

def derive():
 t=tree.load();bindings={r['node']:r['script']for r in t['records']if r['script']}
 records,nodes,resources,proof=tint.source_instances(house.NATIVE,house.SOURCE,ROOT/'upstream/MOTHER-Encore',SCENE,bindings)
 d=tint.validate(tint.build(records,nodes,proof,SCENE),SCENE);rows={r['id']:r for r in t['records']}
 for r in d['records']:
  n=rows.get(r['id'])
  require(n is not None and n['node']==r['node']and n['ready']==r['ready_ordinal']and n['script']==tint.SCRIPT and n['script_sha']==d['sources'][tint.SCRIPT]and t['classes'][n['class_index']]=='Node'and n['script_methods']==1,'House Tint actual full-tree source/Ready binding differs')
  for target in r['targets']:
   actual=rows.get(target['source_id']);require(target['exists']==(actual is not None),'House Tint nullable target differs from complete scene')
   if actual is not None:
    require(actual['node']==target['node']and t['classes'][actual['class_index']]==target['kind']and actual['flags']&1 and actual['self_modulate']==target['initial_self_modulate'],'House Tint target native Canvas identity/color differs')
 d['sources']=dict(sorted({**t['sources'],**d['sources']}.items()))
 d.update(house_tree_ir_sha256=sha(tree.IR),producer_sha256=sha(Path(__file__)),shared_producer_sha256=sha(ROOT/'tools/field_tint.py'))
 d['semantics']+=['Actual House ten scene-only Tint instances; no enemy/actor prototype admission','Targets use complete House source IDs, nullable NodePaths and real native self_modulate; no inferred game colors','Concrete runtime uses the retained actual ObjectDB changed_tint connections and synchronous receiver frames, never a second authoritative edge graph']
 return d

def review(d):
 return dict(schema=1,commit=PIN,ir_sha256=sha(IR),scene=d['scene'],scene_id=d['scene_id'],tint_count=len(d['records']),target_count=sum(len(r['targets'])for r in d['records']),nullable_count=sum(not x['exists']for r in d['records']for x in r['targets']),tree_ir_sha256=d['house_tree_ir_sha256'],producer_sha256=d['producer_sha256'],shared_producer_sha256=d['shared_producer_sha256'],native_export_sha256=d['provenance']['native_export_sha256'],source_receipt_sha256=d['provenance']['source_receipt_sha256'],sources=d['sources'],semantics=d['semantics'],unsupported=d['unsupported'],admission_ready=False,unverified=['Manual negative cases not run','Native House integration and ARM build not run','Hardware source visual parity not observed'])

def extract():
 d=derive();write(IR,d);write(REVIEW,review(d));return d

def load():
 d=read(IR);require(d==derive()and read(REVIEW)==review(d),'House Tint source/review stale');return d

def pack(d):return tint.encode(d,SCENE)

def bundle_context(d):
 return dict(scene=SCENE.scene,scene_id=SCENE.scene_id,source_sha256=d['sources'][SCENE.scene])

def stage_files(root):
 b=pack(load());p=Path('data/house-return-tint.enctint');require((Path(root)/p).read_bytes()==b,'House Tint staged pack differs');return{p:b}

def main():
 p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile','verify']);a=p.parse_args();d=extract()if a.action=='extract'else load();b=pack(d)
 if a.action=='verify':require(PACK.read_bytes()==b,'House Tint stale binary')
 else:PACK.parent.mkdir(parents=True,exist_ok=True);PACK.write_bytes(b)
 print('Actual House Tint:',len(d['records']),'source instances;',sum(len(r['targets'])for r in d['records']),'ordered targets;',len(b),'bytes; Ready not granted')
if __name__=='__main__':
 try:main()
 except(ValueError,KeyError,TypeError,OSError,struct.error)as e:sys.exit('HOUSE TINT ERROR: '+str(e))
