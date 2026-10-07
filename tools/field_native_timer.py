#!/usr/bin/env python3
"""Checked native Timer properties; does not approve any attached script."""
from __future__ import annotations
import argparse,json,hashlib,math,struct,sys,zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import PIN,read,write,sha,decode,require
IR=ROOT/'content/native-field-timers.json';REVIEW=ROOT/'compatibility/reviews/native-field-timers-v0410.json';OUT=ROOT/'romfs/data/podunk.enctimers'
FAMILY=0x454e0044
def validate(d,tree_count=2,record_count=314):
 require(set(d)=={'schema','kind','commit','family','engine','engine_sources','sources','trees','native','records','scope'} and d['schema']==1 and d['kind']=='encore.field-native-timer.source-ir' and d['commit']==PIN and d['family']==FAMILY and d['engine']=='3.6.2-stable','Timer source/schema')
 require(len(d['trees'])==tree_count and len(d['records'])==record_count and len(d['native'])==tree_count,'Timer full source scope')
 keys=set()
 for r in d['records']:
  require(set(r)=={'id','scene','scene_sha','mode','wait','flags','script_sha','node'} and type(r['mode'])is int and r['mode'] in (0,1) and type(r['flags'])is int and 0<=r['flags']<=3 and isinstance(r['wait'],float) and math.isfinite(r['wait'])and r['wait']>0,'Timer unknown property')
  require((r['scene'],r['id'])not in keys and r['id']>0 and r['scene']>0,'Timer duplicate/zero identity');keys.add((r['scene'],r['id']))
  for n in ('scene_sha','script_sha'):require(len(r[n])==64 and all(c in '0123456789abcdef'for c in r[n]),'Timer source SHA')
 return d
def extract(podunk,dialogue,engine):
 sources={};trees={};proof={};rows=[]
 for native,path in [(podunk,'content/podunk-node-tree.json'),(dialogue,'content/dialogue-node-recipe.json')]:
  d=read(native);tree=read(ROOT/path);require(sha(native)==tree['native_sha256'] and d['native_compatible']is False and d['source']=='res://'+tree['scene'] and [d['godot'].get(k)for k in ['major','minor','patch','status']]==[3,6,2,'stable'],'Timer native source differs')
  nm={n['path']:n for n in d['nodes']};sources.update(tree['sources']);trees[path]=sha(ROOT/path);proof[tree['scene']]=sha(native)
  for n in tree['records']:
   if n.get('native_class',tree['classes'][n['class_index']])!='Timer':continue
   props=decode(nm[n['node']]['properties']);require(props['process_mode']in(0,1) and type(props['one_shot'])is bool and type(props['autostart'])is bool,'Timer source property enum')
   rows.append(dict(id=n['id'],scene=tree['scene_id'],scene_sha=tree['source_sha256'],mode=props['process_mode'],wait=float(props['wait_time']),flags=int(props['one_shot'])|int(props['autostart'])<<1,script_sha=n['script_sha'],node=n['node']))
 engines={n:sha(engine/Path(n).name)for n in ['scene/main/timer.cpp','scene/main/timer.h']}
 require(engines=={'scene/main/timer.cpp':'79e2ce272fd56465d8ccc2a5077bf6dc39422cbc588e71ddfab482eb7a33d862','scene/main/timer.h':'20a3de2206b528223bd5b1ddf3175dd150e286c305f7ba43857a3dc29e80dd22'},'Timer original engine bytes differ')
 out=validate(dict(schema=1,kind='encore.field-native-timer.source-ir',commit=PIN,family=FAMILY,engine='3.6.2-stable',engine_sources=engines,sources=sources,trees=trees,native=proof,records=rows,scope=['Original native Timer only; attached and ancestor scripts retain independent required Ready consumers','314 actual Timer objects: 310 Podunk and 4 complete original DialogueBox nodes','Actual internal idle/physics Tree priority order; source pause inheritance; synchronous original timeout reentrancy','Native single timeout on strictly negative f32 time; repeat overshoot preserved; one-shot stop precedes source signal','Source initial paused=false is native constructor state; no source resource paused assignment supported']))
 write(IR,out);write(REVIEW,dict(schema=1,commit=PIN,ir_sha256=sha(IR),engine_sources=engines,native=proof,semantics=out['scope']));return out
def load():
 d=validate(read(IR));review=read(REVIEW);require(review['commit']==PIN and review['ir_sha256']==sha(IR) and review['engine_sources']==d['engine_sources'] and review['native']==d['native'],'Timer reviewed source changed')
 inventory=read(ROOT/'compatibility/upstream-inventory.json');require(inventory['commit']==PIN,'Timer upstream inventory pin')
 for p,h in d['sources'].items():require(inventory['files'][p]['sha256']==h==sha(ROOT/'upstream/MOTHER-Encore'/p),'Timer source changed '+p)
 for p,h in d['trees'].items():require(sha(ROOT/p)==h,'Timer source node tree changed '+p)
 return d
def encode(d,tree_count=2,record_count=314):
 validate(d,tree_count,record_count);b=bytes.fromhex(PIN)+struct.pack('<I',len(d['records']))
 for r in d['records']:b+=struct.pack('<4If32s32s',r['id'],r['scene'],r['mode'],r['flags'],r['wait'],bytes.fromhex(r['scene_sha']),bytes.fromhex(r['script_sha']))
 return struct.pack('<8s6I',b'ENCFNT01',1,32+len(b),zlib.crc32(b),FAMILY,1,1)+b
def stage_files(source):
 blob=encode(load());require((Path(source)/'data/podunk.enctimers').read_bytes()==blob,'Timer staged resource differs');return {Path('data/podunk.enctimers'):blob}
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile']);p.add_argument('--podunk',type=Path);p.add_argument('--dialogue',type=Path);p.add_argument('--engine',type=Path);a=p.parse_args()
 try:
  if a.action=='extract':print(len(extract(a.podunk,a.dialogue,a.engine)['records']))
  else:OUT.write_bytes(encode(load()));print(OUT.stat().st_size)
 except(ValueError,KeyError,TypeError,OSError,struct.error)as e:sys.exit('NATIVE TIMER ERROR: '+str(e))
