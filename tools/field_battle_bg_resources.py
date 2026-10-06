#!/usr/bin/env python3
"""Complete original imported battle PackedScene graphs as checked typed TLV."""
from __future__ import annotations
import argparse,hashlib,json,struct,sys,zlib,re
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import PIN,read,write,sha,require
IR=ROOT/'content/field-battle-bg-resources.json';REVIEW=ROOT/'compatibility/reviews/field-battle-bg-resources-v0410.json';OUT=ROOT/'romfs/data/global.encbgresources'
def stable(path):return int.from_bytes(hashlib.sha256(('source-packed:'+path).encode()).digest()[:4],'little')
# Typed structural/Variant serialization. This is resource data, never a VM.
def value(v,depth=0):
 require(depth<64,'BG resource recursion depth rejected')
 if v is None:return bytes([0])
 if isinstance(v,bool):return bytes([1,int(v)])
 if isinstance(v,int):require(-(1<<63)<=v<(1<<63),'BG integer overflow');return bytes([2])+struct.pack('<q',v)
 if isinstance(v,float):require(v==v and abs(v)!=float('inf'),'BG nonfinite scalar');return bytes([3])+struct.pack('<d',v)
 if isinstance(v,str):raw=v.encode();return bytes([4])+struct.pack('<I',len(raw))+raw
 if isinstance(v,list):return bytes([5])+struct.pack('<I',len(v))+b''.join(value(x,depth+1)for x in v)
 require(isinstance(v,dict),'BG unsupported source Variant');return bytes([6])+struct.pack('<I',len(v))+b''.join(value(k,depth+1)+value(x,depth+1)for k,x in v.items())
def extract(native,source):
 d=read(native);receipt=read(source);inv=read(ROOT/'compatibility/upstream-inventory.json')['files'];require(receipt['commit']==PIN and d['schema']==1 and d['source_scene_entered']is False and [d['engine'].get(k)for k in ['major','minor','patch','status']]==[3,6,2,'stable'],'BG native source export rejected');files={}
 for p,r in receipt['files'].items():require(r['sha256']==inv[p]['sha256']==sha(ROOT/'upstream/MOTHER-Encore'/p),'BG original source changed '+p);files[p]=r['sha256']
 policy='Scripts/global/uiManager.gd';require(sha(ROOT/'upstream/MOTHER-Encore'/policy)==inv[policy]['sha256'],'BG source UI loader changed');files[policy]=inv[policy]['sha256']
 expected=[]
 for r in d['directory_entries']:
  name=r['name'];require(isinstance(r['directory'],bool)and '/'not in name and'\\'not in name,'BG native directory entry rejected')
  if not r['directory']and name.endswith(('.bbg.import','.dsp.import')):
   file='Graphics/Battle BGS/'+name[:-7];require(file in files and file+'.import'in files,'BG directory remap absent');expected.append((file,'res://'+file,name.replace('.bbg.import','').replace('.dsp.import','')))
 require(len(expected)==len(d['scenes'])==50 and len({x[0]for x in expected})==50,'Complete original fifty BG resources required');scenes=[];texturepaths=set()
 for (file,path,key),scene in zip(expected,d['scenes']):
  require(scene['source']==path and scene['key']==key and len(scene['scene_states'])==1 and scene['scene_states'][0]['source']==path,'BG full PackedScene source order rejected');require(scene['nodes']and all(n['class']in ['PanelContainer','TextureRect','BackBufferCopy']for n in scene['nodes']),'BG native node opcode unknown');require(all(not n.get('properties',{}).get('script')for n in scene['nodes']),'BG unexpected gameplay script');require(not scene['scene_states'][0]['connections'],'BG unexpected original signal connection')
  ids={r['id']for r in scene['resources']};require(ids==set(range(len(ids))),'BG Resource graph ID sequence rejected')
  for r in scene['resources']:
   if r['class']in ['StreamTexture','ImageTexture']:
    p=r['path'];require(p.startswith('res://')and p[6:]in files and p.endswith('.png'),'BG texture real source payload missing');texturepaths.add(p[6:])
  scenes.append(dict(id=stable(file),path=file,key=key,sha256=files[file],graph=scene))
 write(IR,dict(schema=1,kind='encore.field-battle-bg-resources.source-ir',commit=PIN,scene='Graphics/Battle BGS/',scene_id=stable('Graphics/Battle BGS/'),source_sha256=files['addons/distortionator_integration/scene_importer.gd'],sources=files,directory_entries=d['directory_entries'],scenes=scenes,textures=[dict(path=p,sha256=files[p])for p in sorted(texturepaths)],native_sha256=sha(native),receipt_sha256=sha(source),exporter_sha256=sha(ROOT/'tools/godot_exporter/field_battle_bg_resources.gd'),plugin_sha256=sha(ROOT/'tools/godot_exporter/field_battle_bg_import.gd'),scene_admitted=False,pending=['No BackBufferCopy/material/Shader GPU class or game lifecycle approval; instance still requires native typed consumers','Complete packed resource graphs are immutable source resources, not live SceneTree Nodes']))
def load():
 d=read(IR);r=read(REVIEW);inv=read(ROOT/'compatibility/upstream-inventory.json')['files'];require(d['schema']==1 and d['commit']==PIN and not d['scene_admitted']and len(d['scenes'])==50 and r['commit']==PIN and r['ir_sha256']==sha(IR)and d['exporter_sha256']==sha(ROOT/'tools/godot_exporter/field_battle_bg_resources.gd')and d['plugin_sha256']==sha(ROOT/'tools/godot_exporter/field_battle_bg_import.gd'),'BG source review rejected')
 for p,h in d['sources'].items():require(h==inv[p]['sha256']==sha(ROOT/'upstream/MOTHER-Encore'/p),'BG changed source '+p)
 return d
def encode(d):
 b=bytearray(128)
 def u(*v):b.extend(struct.pack('<'+'I'*len(v),*v))
 def t(s):v=s.encode();u(len(v));b.extend(v)
 t(d['scene']);u(len(d['sources']))
 for p,h in d['sources'].items():t(p);b.extend(bytes.fromhex(h))
 u(len(d['scenes']))
 for s in d['scenes']:u(s['id']);t(s['path']);t(s['key']);b.extend(bytes.fromhex(s['sha256']));raw=value(s['graph']);u(len(raw));b.extend(raw)
 u(len(d['textures']))
 for s in d['textures']:t(s['path']);b.extend(bytes.fromhex(s['sha256']));raw=(ROOT/'upstream/MOTHER-Encore'/s['path']).read_bytes();u(len(raw));b.extend(raw)
 struct.pack_into('<8s8I',b,0,b'ENCFBGR1',1,128,len(b),0,0x454e004a,1,len(d['scenes']),d['scene_id']);b[40:60]=bytes.fromhex(PIN);b[60:92]=bytes.fromhex(d['source_sha256']);b[92:124]=hashlib.sha256(IR.read_bytes()).digest();struct.pack_into('<I',b,20,zlib.crc32(b[128:]));return bytes(b)
def stage_files(source):
 raw=encode(load());require((Path(source)/'data/global.encbgresources').read_bytes()==raw,'Staged BG resources differ');return{Path('data/global.encbgresources'):raw}
def main():
 p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile','verify']);p.add_argument('--native',type=Path);p.add_argument('--source',type=Path);a=p.parse_args()
 if a.action=='extract':require(a.native and a.source,'Explicit complete native/source receipts required');extract(a.native,a.source);return
 raw=encode(load())
 if a.action=='verify':require(OUT.read_bytes()==raw,'BG resources stale')
 else:OUT.parent.mkdir(parents=True,exist_ok=True);OUT.write_bytes(raw)
 print('Original50BG complete PackedScene resources:',len(raw),'bytes; native/script instance remains checked')
if __name__=='__main__':
 try:main()
 except(ValueError,KeyError,TypeError,OSError,struct.error)as e:sys.exit('FIELD BG RESOURCE ERROR: '+str(e))
