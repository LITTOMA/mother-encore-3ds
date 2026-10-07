#!/usr/bin/env python3
"""Complete actual House TileMaps in existing ENCFMAP1; no source Ready."""
from __future__ import annotations
import argparse,struct,subprocess,sys,zlib
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import PIN,read,write,sha,require
from tools import podunk_map as maps,house_geometry as house,house_node_tree as tree
IR=ROOT/'content/native-house-return-map.json'
DETAIL=ROOT/'reports/house-return-map/native-detail.json'
REVIEW=ROOT/'reports/house-return-map/source-review.json'
ASSETS=ROOT/'content/asset-receipts/graphics/house-return-map/source.json'
PACK=ROOT/'romfs/data/house-return.encmap'
SCENE=maps.MapScene(house.SCENE,tree.node_id('.'),3,291,tree.node_id,tree.IR,'native_sha256','records')

def exporter():
 require(maps.EXPORT_SCRIPT.count('Maps/podunk/podunk.tscn')==2 and maps.EXPORT_SCRIPT.count('field_map_detail.json')==1,'Map exporter source changed')
 return maps.EXPORT_SCRIPT.replace('Maps/podunk/podunk.tscn',house.SCENE).replace('field_map_detail.json','house_return_map_detail.json')

def derive():
 t=tree.load();g=house.load();d=maps.extract(house.NATIVE,DETAIL,house.SOURCE,ROOT/'upstream/MOTHER-Encore',SCENE);rows={r['node']:r for r in t['records']}
 require([(m['node'],m['cell_count'])for m in d['maps']]==[('Below',1),('Objects',14),('Above',276)],'House all three source TileMap cells differ')
 require(d['counts']['native_skipped_cells']==1 and d['native_skips']=={'Below':1} and d['counts']['draws']==290 and not d['polygons'] and not d['local_geometries'] and not d['shape_transforms'],'House missing native tile30/no collision certificate differs')
 for m in d['maps']:
  row=rows[m['node']];require(row['id']==m['stable_id'] and t['classes'][row['class_index']]=='TileMap' and row['world'][:2]==[[1,0],[0,1]] and row['world'][2]==m['position'],'House source map/tree identity/transform differs')
 for c in d['canvases']:require(rows[c['node']]['id']==c['stable_id'] and rows[c['node']]['world'][2]==c['position'],'House map canvas source differs')
 require([(p['node'],p['cell_count'],p['native_collision_parts'])for p in g['tile_collision']]==[(m['node'],m['cell_count'],m['poly_count'])for m in d['maps']],'House map/geometry no-native-tile-collision proof differs')
 d['sources']={**t['sources'],**d['sources']}
 inventory=read(ROOT/'compatibility/upstream-inventory.json')['files']
 for p in ['LICENSE',*{v['source']+'.import'for v in d['textures']}]:
  require(sha(ROOT/'upstream/MOTHER-Encore'/p)==inventory[p]['sha256'],'House map source/license/import changed '+p);d['sources'][p]=inventory[p]['sha256']
 d.update(tree_ir_sha256=sha(tree.IR),geometry_ir_sha256=sha(house.IR),producer_sha256=sha(Path(__file__)),shared_producer_sha256=sha(ROOT/'tools/podunk_map.py'),license_review='Pinned upstream LICENSE graphics conditions permit only game-related forks, modifications and translations; this Mother: Encore port retains those conditions and source attribution.',scene_admitted=False)
 return d

def review(d):
 return dict(schema=1,commit=PIN,ir_sha256=sha(IR),scene=house.SCENE,scene_id=d['scene_id'],source_sha256=d['source_sha256'],native_sha256=d['native_sha256'],detail_sha256=d['detail_sha256'],tree_ir_sha256=d['tree_ir_sha256'],geometry_ir_sha256=d['geometry_ir_sha256'],producer_sha256=d['producer_sha256'],shared_producer_sha256=d['shared_producer_sha256'],sources=d['sources'],counts=d['counts'],scene_admitted=False,semantics=['Complete original Below/Objects/Above native cells; retain missing tile30 as an explicit skipped native cell','Original single/atlas texture regions, flip/transpose, signed native draw dimensions and quadrant order','Actual Godot 3.6.2 supplemental TileSet texture/shape/z getters, with no source scene entry or Ready','Native empty placed tile collision parts cross-bound to the independent complete House geometry certificate','Real texture page crop rectangles/PNG fingerprints and tex3ds fingerprints live in the separate resource receipt'])

def extract():
 d=derive();maps.write_ir(IR,d);write(REVIEW,review(d));return d

def load():
 d=read(IR);require(d==derive() and read(REVIEW)==review(d),'House map original source/review stale');return d

def assets(d,tex3ds,work):
 require(Path(tex3ds).is_file(),'House map requires actual tex3ds')
 maps.prepare_textures(d,ROOT/'upstream/MOTHER-Encore',work,'graphics/world/house-return-map',ASSETS);receipt=read(ASSETS)
 def convert(a):
  target=ROOT/'romfs'/a['output'];target.parent.mkdir(parents=True,exist_ok=True)
  subprocess.run([str(tex3ds),'-f','rgba8','-z','none','-o',str(target),str(work/a['png'])],check=True)
  a.update(output_sha256=sha(target),bytes=target.stat().st_size,crc=zlib.crc32(target.read_bytes()),crop_png_sha256=sha(work/a['png']))
 with ThreadPoolExecutor(max_workers=4)as pool:list(pool.map(convert,receipt['textures']))
 receipt.update(commit=PIN,ir_sha256=sha(IR),producer_sha256=sha(Path(__file__)),tex3ds_sha256=sha(tex3ds),workers=4,license_sha256=d['sources']['LICENSE'],license_review=d['license_review']);write(ASSETS,receipt)

def checked_assets(d,root):
 a=read(ASSETS);require(a['commit']==PIN and a['ir_sha256']==sha(IR) and a['producer_sha256']==sha(Path(__file__)) and a['workers']==4 and a['license_sha256']==d['sources']['LICENSE'],'House map actual texture receipt stale')
 expected=[];groups={}
 for i,t in enumerate(d['textures']):
  groups.setdefault(t['group'],len(expected))
  for y in range(0,t['height'],1024):
   for x in range(0,t['width'],1024):
    w,h=min(1024,t['width']-x),min(1024,t['height']-y);name='map-'+str(i)+'-'+str(x)+'-'+str(y)
    expected.append(dict(source_index=i,source=t['source'],source_sha256=t['source_sha256'],crop=[x,y,w,h],width=w,height=h,group=groups[t['group']],frame=t['frame'],frames=t['frames'],fps=t['fps'],delay=t['delay'],png=name+'.png',output='graphics/world/house-return-map/'+name+'.t3x'))
 require(len(a['textures'])==len(expected),'House map complete source page coverage differs')
 from PIL import Image
 from io import BytesIO
 for p,original in zip(a['textures'],expected):
  require({k:p[k]for k in original}==original,'House map exact original source page metadata differs')
  target=Path(root)/p['output'];source=d['textures'][p['source_index']]
  require(p['source']==source['source'] and p['source_sha256']==source['source_sha256'] and target.stat().st_size==p['bytes'] and sha(target)==p['output_sha256'] and zlib.crc32(target.read_bytes())==p['crc'],'House map texture source/output differs')
  x,y,w,h=p['crop'];image=Image.open(ROOT/'upstream/MOTHER-Encore'/source['source']).convert('RGBA');blob=BytesIO();image.crop((x,y,x+w,y+h)).save(blob,format='PNG');require(__import__('hashlib').sha256(blob.getvalue()).hexdigest()==p['crop_png_sha256'],'House map source crop fingerprint differs')
 return a['textures']

def pack(d):return maps.pack(d,checked_assets(d,ROOT/'romfs'),sha(IR))

def stage_files(root):
 d=load();a=checked_assets(d,root);b=maps.pack(d,a,sha(IR));p=Path('data/house-return.encmap');require((Path(root)/p).read_bytes()==b,'House map staged binary differs');return {p:b,**{Path(t['output']):(Path(root)/t['output']).read_bytes()for t in a}}

def main():
 p=argparse.ArgumentParser();p.add_argument('action',choices=['export-script','extract','assets','compile','verify']);p.add_argument('--out',type=Path);p.add_argument('--tex3ds',type=Path);p.add_argument('--work',type=Path,default=ROOT/'build/house-return-map-textures');a=p.parse_args()
 if a.action=='export-script':require(a.out is not None,'House map native exporter requires explicit private output');a.out.write_text(exporter(),encoding='utf8');return
 d=extract()if a.action=='extract'else load()
 if a.action=='extract':print('House complete TileMap IR:',d['counts']);return
 if a.action=='assets':require(a.tex3ds is not None,'House map source textures require real tex3ds');assets(d,a.tex3ds,a.work);return
 b=pack(d)
 if a.action=='verify':require(PACK.read_bytes()==b,'House map binary stale')
 else:PACK.parent.mkdir(parents=True,exist_ok=True);PACK.write_bytes(b)
 print('House complete ENCFMAP1:',len(b),'bytes;',d['counts'],'; Ready not granted')

if __name__=='__main__':
 try:main()
 except(ValueError,KeyError,TypeError,OSError,struct.error,subprocess.SubprocessError)as e:sys.exit('HOUSE MAP ERROR: '+str(e))
