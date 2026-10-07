#!/usr/bin/env python3
"""Complete House native visibility; consumes the existing World2D executor."""
from pathlib import Path
import argparse,re,sys
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import PIN,read,write,sha,require
from tools import field_visibility as visibility
from tools.house_node_tree import node_id,SCENE
TREE=ROOT/'content/native-house-node-tree.json'
IR=ROOT/'content/native-house-return-visibility.json'
REVIEW=ROOT/'reports/house-return-visibility/source-review.json'
PACK=ROOT/'romfs/data/house-return.encvisibility'
NATIVE=ROOT/'reports/cloud-world/house-exact.json'

def validate(d):
 tree=read(TREE);require(d['schema']==1 and d['kind']=='encore.field-visibility.source-ir' and d['commit']==tree['commit']==PIN and d['scene']==SCENE and d['scene_id']==tree['scene_id'] and d['source_sha256']==tree['source_sha256'] and d['tree_ir_sha256']==sha(TREE) and d['native_sha256']==tree['native_sha256'] and d['flags']==visibility.FLAGS and d['scene_admitted']is False,'House visibility source/tree identity')
 require(d['sources']==tree['sources'],'House visibility node sources differ from complete tree')
 project=ROOT/'upstream/MOTHER-Encore/project.godot'
 setting=re.search(r'^world/2d/cell_size=(\d+)$',project.read_text(encoding='utf8'),re.M)
 proof=d['project_settings'];require(proof==dict(sources={'project.godot':sha(project)},name='world/2d/cell_size',value=d['cell_size'],uses_engine_default=setting is None) and (setting is None or int(setting[1])==d['cell_size']),'House visibility project setting provenance differs')
 approved=read(ROOT/'content/native-field-visibility.json');require(d['engine_sources']==approved['engine_sources'] and d['cell_size']==approved['cell_size'],'House visibility shared native spatial defaults differ')
 actual={r['id']:r for r in tree['records']if tree['classes'][r['class_index']]in ['VisibilityNotifier2D','VisibilityEnabler2D']}
 require(len(actual)==len(d['records'])==12 and {r['id']for r in d['records']}==set(actual),'House visibility complete native coverage')
 for r in d['records']:
  n=actual[r['id']];require(r['node']==n['node'] and r['parent']==n['parent'] and r['ready']==n['ready'] and r['kind']==(1 if tree['classes'][n['class_index']]=='VisibilityNotifier2D'else 2),'House visibility actual class/parent/order')
 require(len(d['connections'])==12 and all(r['adapter']==2 for r in d['connections']),'House visibility actual six NPC callbacks')
 return d
def review(d):
 return dict(schema=1,commit=PIN,ir_sha256=sha(IR),producer_sha256=sha(Path(__file__)),shared_producer_sha256=sha(ROOT/'tools/field_visibility.py'),tree_ir_sha256=sha(TREE),native_sha256=d['native_sha256'],sources=d['sources'],project_settings=d['project_settings'],engine_sources=d['engine_sources'],notifier_count=12,connection_count=12,scene_admitted=False)
def extract(native,engine):
 visibility.extract(native,engine,tree_path=TREE,scene=SCENE,node_count=497,node_id=node_id,ir_path=IR)
 d=read(IR);project=ROOT/'upstream/MOTHER-Encore/project.godot'
 setting=re.search(r'^world/2d/cell_size=(\d+)$',project.read_text(encoding='utf8'),re.M)
 # Project settings are global producer inputs, not Node attachments. Their
 # separate proof is included in the IR hash and the complete bundle closure.
 d['project_settings']=dict(sources={'project.godot':sha(project)},name='world/2d/cell_size',value=d['cell_size'],uses_engine_default=setting is None)
 d['pending']=['Actual native animation owners are required for Enabler state changes','Actual House NPC source consumers and complete House lifecycle remain required']
 validate(d);write(IR,d);write(REVIEW,review(d));return d
def load():
 d=validate(read(IR));require(read(REVIEW)==review(d),'House visibility public source review stale')
 inventory=read(ROOT/'compatibility/upstream-inventory.json');require(inventory['commit']==PIN,'House visibility upstream pin')
 for p,h in d['sources'].items():require(sha(ROOT/'upstream/MOTHER-Encore'/p)==h==inventory['files'][p]['sha256'],'House visibility source changed '+p)
 for p,h in d['project_settings']['sources'].items():require(h==inventory['files'][p]['sha256'],'House visibility project source changed '+p)
 return d
def encode(d):return visibility.encode(validate(d),ir_path=IR)
def stage_files(root):
 relative=Path('data/house-return.encvisibility');raw=encode(load());require((Path(root)/relative).read_bytes()==raw,'House visibility staged binary differs');return {relative:raw}
def main():
 p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile']);p.add_argument('--native',type=Path,default=NATIVE);p.add_argument('--engine',type=Path);a=p.parse_args()
 if a.action=='extract':require(a.engine,'Actual reviewed visibility engine directory required');extract(a.native,a.engine)
 else:PACK.parent.mkdir(parents=True,exist_ok=True);PACK.write_bytes(encode(load()));print('House native visibility: 12 nodes, 12 source callbacks; source Ready not admitted')
if __name__=='__main__':
 try:main()
 except (ValueError,KeyError,TypeError,OSError)as e:sys.exit('HOUSE VISIBILITY ERROR: '+str(e))
