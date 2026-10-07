#!/usr/bin/env python3
"""Original eight House InteractDialog instances in the shared ENCFDLG1 codec."""
from __future__ import annotations
import argparse,struct,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import PIN,read,write,sha,require
from tools import field_interact_dialog as dialog,house_node_tree as tree,house_geometry as house
IR=ROOT/'content/native-house-return-interact-dialog.json'
REVIEW=ROOT/'reports/house-return-interact-dialog/source-review.json'
PACK=ROOT/'romfs/data/house-return-interact.encdialog'
SCENE=dialog.InteractScene(house.SCENE,tree.node_id('.'),497,8,tree.node_id)

def derive():
 t=tree.load();bindings={n['node']:n['script']for n in t['records']if n['script']}
 records,nodes,resources,provenance=dialog.source_instances(house.NATIVE,house.SOURCE,ROOT/'upstream/MOTHER-Encore',SCENE,bindings)
 d=dialog.build(records,nodes,resources,provenance,SCENE);rows={n['id']:n for n in t['records']}
 for r in d['records']:
  n=rows.get(r['id']);p=rows.get(r['prompt_id'])
  require(n is not None and n['node']==r['node']and n['ready']==r['ready']and n['script']==dialog.SCRIPT and n['script_sha']==d['sources'][dialog.SCRIPT]and t['classes'][n['class_index']]=='Area2D'and n['script_methods']==1,'House InteractDialog actual full-tree source/Ready binding differs')
  require(p is not None and p['parent']==n['id']and p['node']==r['node']+'/ButtonPrompt'and p['script']=='Scripts/UI/Button Prompt.gd'and p['ready']<n['ready'],'House InteractDialog onready source ButtonPrompt binding differs')
 d['sources']=dict(sorted({**t['sources'],**d['sources']}.items()))
 d['semantics'][0]='Complete eight actual House instances, inherited inner-to-outer overrides and full-tree postorder Ready'
 dialog.validate(d,SCENE);return d

def review(d):
 return dict(schema=1,commit=PIN,ir_sha256=sha(IR),scene=d['scene'],scene_id=d['scene_id'],instance_count=len(d['records']),tree_ir_sha256=sha(tree.IR),producer_sha256=sha(Path(__file__)),shared_producer_sha256=sha(ROOT/'tools/field_interact_dialog.py'),native_export_sha256=d['provenance']['native_export_sha256'],source_receipt_sha256=d['provenance']['source_receipt_sha256'],sources=d['sources'],semantics=d['semantics']+['Original _ready performs _check_flags before the synchronous global.flags_updated connection, including queued-hidden nodes','Only serialized button_offset assignments execute the source setget, before ButtonPrompt Ready; no Ready replay','InteractDialog is a nullable-NPC caller: same HouseRuntime lease, retained native DialogueBox/factory/printer/choices and original OpeningWorld Room VM','Normal flag Dictionary.get(default false), last matching _all_dialog entry wins; unknown programme/native/source receivers reject','Original interact_item compares actual Item.item_name; telepathy has an independent source effect prefix and requires a real thoughts owner','There is no source _exit_tree body; native exit/deletion and ObjectDB signal release retain their original owners'],records=[dict(id=r['id'],node=r['node'],ready=r['ready'],prompt_id=r['prompt_id'],dialogue=r['dialogue'],thoughts=r['thoughts'],appear=r['appear'],disappear=r['disappear'],offset_assigned=r['offset_assigned'])for r in d['records']],admission_ready=False,unverified=['Manual negative cases not run','ARM/native target integration not built','Hardware interaction not observed'])

def extract():
 d=derive();write(IR,d);write(REVIEW,review(d));return d
def load():
 d=read(IR);require(d==derive()and read(REVIEW)==review(d),'House InteractDialog actual source/review stale');return d
def pack(d):return dialog.encode(d,SCENE)
def bundle_context(d):return dict(scene=d['scene'],scene_id=d['scene_id'],source_sha256=d['sources'][d['scene']])
def stage_files(root):
 b=pack(load());p=Path('data/house-return-interact.encdialog');require((Path(root)/p).read_bytes()==b,'House InteractDialog staged binary differs');return{p:b}
def main():
 p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile','verify']);a=p.parse_args();d=extract()if a.action=='extract'else load();b=pack(d)
 if a.action=='verify':require(PACK.read_bytes()==b,'House InteractDialog binary stale')
 else:PACK.parent.mkdir(parents=True,exist_ok=True);PACK.write_bytes(b)
 print('Actual House InteractDialog:',len(d['records']),'objects;',len(b),'bytes; source Ready not granted')
 for r in d['records']:print(r['node'],r['id'],'Ready',r['ready'],'prompt',r['prompt_id'],'offset',r['button_offset'],r['offset_assigned'],'dialogue',r['dialogue'],'thoughts',r['thoughts'])
if __name__=='__main__':
 try:main()
 except(ValueError,KeyError,TypeError,OSError,struct.error)as e:sys.exit('HOUSE INTERACT ERROR: '+str(e))
