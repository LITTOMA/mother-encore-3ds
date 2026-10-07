#!/usr/bin/env python3
"""The complete original House native Timer closure; source Ready stays separate."""
from pathlib import Path
import argparse,sys
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import PIN,read,write,sha,decode,require
from tools import field_native_timer as timer
TREE=ROOT/'content/native-house-node-tree.json'
IR=ROOT/'content/native-house-return-timers.json'
REVIEW=ROOT/'reports/house-return-timers/source-review.json'
PACK=ROOT/'romfs/data/house-return.enctimers'
NATIVE=ROOT/'reports/cloud-world/house-exact.json'

def validate(d):
 timer.validate(d,1,11);tree=read(TREE)
 require(d['trees']=={'content/native-house-node-tree.json':sha(TREE)} and
         d['sources']==tree['sources'] and d['native']=={tree['scene']:tree['native_sha256']},'House Timer full source ownership')
 actual={r['id']:r for r in tree['records'] if tree['classes'][r['class_index']]=='Timer'}
 require(len(actual)==len(d['records'])==11 and {r['id']for r in d['records']}==set(actual),'House Timer complete native coverage')
 for r in d['records']:
  n=actual[r['id']]
  require(r['node']==n['node'] and r['scene']==tree['scene_id'] and r['scene_sha']==tree['source_sha256'] and r['script_sha']==n['script_sha'],'House Timer actual source ID/path/attachment')
 approved=read(ROOT/'content/native-field-timers.json')['engine_sources']
 require(d['engine_sources']==approved,'House Timer original engine differs')
 return d

def review(d):
 return dict(schema=1,commit=PIN,ir_sha256=sha(IR),producer_sha256=sha(Path(__file__)),shared_producer_sha256=sha(ROOT/'tools/field_native_timer.py'),engine_sources=d['engine_sources'],native=d['native'],trees=d['trees'],sources=d['sources'],record_count=11,scene_admitted=False)

def extract(native,engine):
 tree=read(TREE);d=read(native)
 require(tree['commit']==PIN and len(tree['records'])==497 and sha(native)==tree['native_sha256'] and d['source']=='res://'+tree['scene'] and d['native_compatible']is False,'House Timer actual native snapshot')
 require([d['godot'].get(k)for k in ['major','minor','patch','status']]==[3,6,2,'stable'],'House Timer native engine version')
 engines={}
 for name in ['timer.cpp','timer.h']:
  file=engine/name
  if not file.exists():file=engine/('godot-3.6.2-'+name)
  engines['scene/main/'+name]=sha(file)
 native_nodes={n['path']:n for n in d['nodes']};rows=[]
 for n in tree['records']:
  if tree['classes'][n['class_index']]!='Timer':continue
  props=decode(native_nodes[n['node']]['properties'])
  require(props['script']is None and not n['script'] and props['process_mode']in(0,1) and type(props['one_shot'])is bool and type(props['autostart'])is bool,'House Timer attached script/enum needs actual owner')
  rows.append(dict(id=n['id'],scene=tree['scene_id'],scene_sha=tree['source_sha256'],mode=props['process_mode'],wait=float(props['wait_time']),flags=int(props['one_shot'])|int(props['autostart'])<<1,script_sha=n['script_sha'],node=n['node']))
 result=validate(dict(schema=1,kind='encore.field-native-timer.source-ir',commit=PIN,family=timer.FAMILY,engine='3.6.2-stable',engine_sources=engines,sources=tree['sources'],trees={'content/native-house-node-tree.json':sha(TREE)},native={tree['scene']:sha(native)},records=rows,scope=['All eleven native Timers in the complete original 497-node House','The existing unique native Timer executor and actual SceneTree process order','All attached and ancestor source lifecycle consumers remain independent; no House Ready approval']))
 write(IR,result);write(REVIEW,review(result));return result

def load():
 d=validate(read(IR));require(read(REVIEW)==review(d),'House Timer public source review stale')
 inventory=read(ROOT/'compatibility/upstream-inventory.json');require(inventory['commit']==PIN,'House Timer upstream pin')
 for p,h in d['sources'].items():require(sha(ROOT/'upstream/MOTHER-Encore'/p)==h==inventory['files'][p]['sha256'],'House Timer upstream changed '+p)
 return d

def encode(d):return timer.encode(validate(d),1,11)
def bundle_context(d):
 validate(d);tree=read(TREE)
 return {k:tree[k]for k in ['scene','scene_id','source_sha256']}
def stage_files(root):
 relative=Path('data/house-return.enctimers');raw=encode(load());require((Path(root)/relative).read_bytes()==raw,'House Timer staged binary differs');return {relative:raw}
def main():
 p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile']);p.add_argument('--native',type=Path,default=NATIVE);p.add_argument('--engine',type=Path);a=p.parse_args()
 if a.action=='extract':require(a.engine,'Actual reviewed Timer source directory required');extract(a.native,a.engine)
 else:PACK.parent.mkdir(parents=True,exist_ok=True);PACK.write_bytes(encode(load()));print('House native Timers: 11; source Ready not admitted')
if __name__=='__main__':
 try:main()
 except (ValueError,KeyError,TypeError,OSError)as e:sys.exit('HOUSE TIMER ERROR: '+str(e))
