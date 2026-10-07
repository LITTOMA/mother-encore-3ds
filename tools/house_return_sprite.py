#!/usr/bin/env python3
"""Full actual House CharacterSprite/Fetcher source resource; no scene Ready."""
from __future__ import annotations
import argparse,hashlib,struct,sys,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import PIN,read,write,sha,require
from tools import field_sprite_bridge as sprite,house_node_tree as tree,house_geometry as house,house_return_npc as npc
IR=ROOT/'content/native-house-return-sprite.json'
REVIEW=ROOT/'reports/house-return-sprite/source-review.json'
PACK=ROOT/'romfs/data/house-return-sprites.encsprite'
ASSET_RECEIPT=ROOT/'content/asset-receipts/graphics/house-return-sprites.json'
SCENE=sprite.SpriteScene(house.SCENE,tree.node_id('.'),497,6,10,tree.node_id,'House')

def derive():
 t=tree.load();n=npc.load();bindings={r['node']:r['script']for r in t['records']if r['script']}
 records,nodes,resources,proof=sprite.source_instances(house.NATIVE,house.SOURCE,ROOT/'upstream/MOTHER-Encore',SCENE,bindings)
 d=sprite.validate(sprite.build(records,nodes,resources,proof,n,SCENE),SCENE);rows={r['id']:r for r in t['records']}
 for r in d['records']:
  actual=rows.get(r['id']);expected=sprite.CHARACTER if r['kind']==1 else sprite.FETCHER
  require(actual is not None and actual['node']==r['node'] and actual['parent']==r['parent_id'] and actual['ready']==r['ready_ordinal'] and actual['script']==expected and actual['script_sha']==d['sources'][expected],'House sprite actual source node binding differs')
  if r['kind']==1:
   parent=rows[r['parent_id']];require(parent['script']==npc.npc.SCRIPT and any(x['id']==parent['id']for x in n['npcs']),'House CharacterSprite actual NPC parent differs')
  else:require(r['target_id']in rows and t['classes'][rows[r['target_id']]['class_index']]in('Sprite','AnimatedSprite'),'House Fetcher actual native Sprite target differs')
 d['sources']=dict(sorted({**t['sources'],**n['sources'],**d['sources']}.items()))
 d.update(house_tree_ir_sha256=sha(tree.IR),house_npc_ir_sha256=sha(npc.IR),producer_sha256=sha(Path(__file__)),shared_producer_sha256=sha(ROOT/'tools/field_sprite_bridge.py'))
 return d

def review(d):
 return dict(schema=1,commit=PIN,ir_sha256=sha(IR),scene=d['scene'],scene_id=d['scene_id'],character_count=6,fetcher_count=10,texture_count=len(d['textures']),animation_count=len(d['animations']),tree_ir_sha256=d['house_tree_ir_sha256'],npc_ir_sha256=d['house_npc_ir_sha256'],native_export_sha256=d['provenance']['native_export_sha256'],source_receipt_sha256=d['provenance']['source_receipt_sha256'],producer_sha256=d['producer_sha256'],shared_producer_sha256=d['shared_producer_sha256'],sources=d['sources'],semantics=d['semantics'],unsupported=d['unsupported'],admission_ready=False,proofs=['All 497 native nodes and 124 original script bindings determine the exact six CharacterSprite and ten Fetcher scope','Serialized child preload texture/grid/frame/offset and initial animation are preserved before actual parent NPC setup','All original YAML directional tags/keyframes and parent setup texture/animation/connections are retained; no VM or synthetic factory','Four actual Present Fetchers bind their own native Sprite source IDs; six NPC Fetchers bind their actual CharacterSprite child','Complete House export proves root FloorReflector absent; reflective scenes still require concrete checked lifecycle endpoints'])

def asset_records(d,tex3ds=None):
 old=read(npc.EXISTING_RECEIPT);current=read(npc.ASSET_RECEIPT);records={}
 retained=read(ASSET_RECEIPT)['outputs']if ASSET_RECEIPT.exists()else{}
 require(old['commit']==current['commit']==PIN and old['recipe_sha256']==sha(ROOT/'content/native-field-npc.json') and current['recipe_sha256']==sha(npc.IR),'House sprite referenced texture receipt stale')
 for t in d['textures']:
  name='graphics/npcs/'+hashlib.sha256(t['path'].encode()).hexdigest()[:16]+'.t3x';file=ROOT/'romfs'/name
  source=read(ROOT/'content/native-field-npc.json')['assets'].get(name)
  prior=current['outputs'].get(name)
  if prior is not None:
   require(prior['source']==t['path'] and prior['source_sha256']==d['sources'][t['path']] and prior['size']==t['size'],'House sprite NPC converted texture source differs');tool=current['tex3ds_sha256'];origin='house-npc'
  elif source is not None:
   require(source['source']==t['path'] and source['source_sha256']==d['sources'][t['path']] and source['size']==t['size'],'House sprite preload source differs')
   prior=old['outputs'][name];tool=old['tex3ds_sha256'];origin='podunk-npc'
  else:
   require(sha(ROOT/'upstream/MOTHER-Encore'/t['path'])==d['sources'][t['path']],'House sprite preload PNG changed')
   if tex3ds is not None:
    file.parent.mkdir(parents=True,exist_ok=True)
    subprocess.run([str(tex3ds),'-f','rgba8','-z','none','-o',str(file),str(ROOT/'upstream/MOTHER-Encore'/t['path'])],check=True)
    prior=dict(bytes=file.stat().st_size,sha256=sha(file));tool=sha(tex3ds)
   else:
    prior=retained.get(name);require(prior is not None and prior['source']==t['path'] and prior['source_sha256']==d['sources'][t['path']] and prior['size']==t['size'] and prior['origin']=='source-conversion','House sprite preload requires actual tex3ds conversion');tool=prior['tex3ds_sha256']
   origin='source-conversion'
  require(prior['bytes']==file.stat().st_size and prior['sha256']==sha(file),'House sprite referenced texture binary differs')
  records[name]=dict(source=t['path'],source_sha256=d['sources'][t['path']],size=t['size'],texture_id=t['id'],bytes=file.stat().st_size,sha256=sha(file),origin=origin,tex3ds_sha256=tool,conversion=['-f','rgba8','-z','none'])
 return dict(schema=1,commit=PIN,recipe_sha256=sha(IR),producer_sha256=sha(Path(__file__)),npc_receipt_sha256=sha(npc.ASSET_RECEIPT),existing_receipt_sha256=sha(npc.EXISTING_RECEIPT),license_sha256=d['sources']['LICENSE'],license_review=npc.load()['license_review'],outputs=records)

def extract(tex3ds=None):
 d=derive();write(IR,d);write(REVIEW,review(d));write(ASSET_RECEIPT,asset_records(d,tex3ds));return d

def load():
 d=read(IR);require(d==derive() and read(REVIEW)==review(d) and read(ASSET_RECEIPT)==asset_records(d),'House sprite source/review/asset receipt stale');return d

def pack(d):return sprite.encode(d,SCENE)

def stage_files(root):
 d=load();b=pack(d);p=Path('data/house-return-sprites.encsprite');require((Path(root)/p).read_bytes()==b,'House sprite staged pack differs');out={p:b}
 for name in read(ASSET_RECEIPT)['outputs']:
  raw=(ROOT/'romfs'/name).read_bytes();require((Path(root)/name).read_bytes()==raw,'House sprite staged texture differs '+name);out[Path(name)]=raw
 return out

def main():
 p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile','verify']);p.add_argument('--tex3ds',type=Path);a=p.parse_args();d=extract(a.tex3ds)if a.action=='extract'else load();b=pack(d)
 if a.action=='verify':require(PACK.read_bytes()==b,'House sprite binary stale')
 else:PACK.parent.mkdir(parents=True,exist_ok=True);PACK.write_bytes(b)
 print('Actual House sprite: 6 CharacterSprite; 10 Fetchers;',len(d['textures']),'source textures;',len(d['animations']),'animation resources;',len(b),'bytes; Ready not granted')

if __name__=='__main__':
 try:main()
 except(ValueError,KeyError,TypeError,OSError,struct.error,subprocess.SubprocessError)as e:sys.exit('HOUSE SPRITE ERROR: '+str(e))
