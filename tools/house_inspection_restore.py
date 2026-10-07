#!/usr/bin/env python3
"""Original Restore semantics, independently bound to the inspection Room."""
from __future__ import annotations
import argparse,copy,hashlib,json,struct,sys,zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import PIN,read,write,sha,require
from tools import native_restore as restore,house_return_inspection_programme as inspection
from tools import house_inspection_reentry as reentry,house_node_tree as tree,native_content as room
IR=ROOT/'content/native-house-inspection-restore.json'
REVIEW=ROOT/'reports/house-inspection-restore/source-review.json'
PACK=ROOT/'romfs/data/house-return.encrestore'
FAMILY=0x454e0082

def semantic_bytes(d):
 return json.dumps({k:v for k,v in d.items()if k not in ('dependencies','fingerprints')},sort_keys=True,separators=(',',':'),ensure_ascii=False).encode()

def inner_binary(d):
 # The original Writer is reused after source/prefix validation in derive.
 # No old binary is patched and no frozen producer global is replaced.
 w=restore.Writer()
 for name in ('room','house'):
  f=d['fingerprints'][name];w.put('I32s',f['bytes'],bytes.fromhex(f['sha256']))
 w.put('I',d['scene_id']);w.text(d['scene_path']);w.put('I',len(d['npc_event_positions']))
 for n in d['npc_event_positions']:
  w.put('5I',*[n[k]for k in ('npc_index','npc_id','actor_index','actor_id','body_id')]);w.text(n['source_path']);w.condition(n['condition']);w.put('2f',*n['position'])
 w.put('I',len(d['music_areas']))
 for m in d['music_areas']:
  w.put('4I',m['id'],m['room_resource_index'],m['room_resource_id'],m['supported']);w.text(m['source_path']);w.text(m['resource_path']);w.put('32s4f3d',bytes.fromhex(m['resource_sha256']),*m['center'],*m['extents'],m['volume_db'],m['fadein_seconds'],m['fadeout_seconds']);w.put('I',len(m['conditions']))
  for c in m['conditions']:w.condition(c)
 w.put('2I',d['uid_policy'],len(d['inventory_load_order']))
 for row in d['inventory_load_order']:
  w.put('3I',row['order_id'],row['kind'],row['rebuilds_inventory']);w.text(row['character_id']);w.put('I',len(row['projected_items']))
  for item in row['projected_items']:w.text(item['item_id']);w.put('2I',item['equipped'],item['doses'])
 return struct.pack('<8s4I',b'ENCREST1',1,len(w.raw)+24,zlib.crc32(w.raw),1)+w.raw

def derive():
 base=restore.extract();require(read(restore.IR)==base,'Original Restore source recipe is stale')
 original=read(ROOT/inspection.BASE);extended=read(ROOT/inspection.ROOM_IR)
 inspection.verify_extension(extended);room.verify_provenance(extended)
 require(extended['rules']==8 and extended['capabilities']==10,'Inspection Room schema differs')
 require(extended['strings'][:len(original['strings'])]==original['strings'],'Inspection Room string prefix differs')
 for name,values in original['sections'].items():
  actual=extended['sections'][name]
  require((actual[:len(values)]==values if name in ('Program','Command')else actual==values),'Inspection Room altered original section '+name)
 # Validate the ORIGINAL source recipe against the extended Room's unchanged
 # flag/actor/body/music inventory. Its old raw fingerprint is checked too.
 restore.validate(base,extended,read(ROOT/inspection.HOUSE))
 expected,_=room.compile_ir(extended);target_room=ROOT/inspection.PACK
 require(target_room.read_bytes()==expected,'Inspection Room compiled bytes differ')
 door=reentry.load();tree_data=tree.load()
 require((ROOT/'romfs/data/house-return.encreentry').read_bytes()==reentry.binary(door),'Inspection Reentry pack differs')
 require(door['target_scene']==tree_data['scene'] and door['source_sha256']==tree_data['source_sha256'],'Inspection Reentry/tree target differs')
 inner=copy.deepcopy(base)
 inner['fingerprints']['room']=dict(bytes=len(expected),sha256=hashlib.sha256(expected).hexdigest())
 inner['dependencies'].pop('content/native-opening.json');inner['dependencies'].pop('romfs/data/opening.encroom')
 inner['dependencies'][inspection.ROOM_IR]=sha(ROOT/inspection.ROOM_IR)
 inner['dependencies'][inspection.PACK]=sha(target_room)
 require(semantic_bytes(base)==semantic_bytes(inner),'Restore source/UID/events/music/inventory semantics changed')
 require(inner_binary(base)==restore.encode(base),'Explicit shared-Writer serialization differs from original mechanism')
 require(inner_binary(inner)[96:]==restore.encode(base)[96:],'Restore payload changed beyond raw Room fingerprint')
 sources=dict(base['sources'])
 for closure in (door['sources'],tree_data['sources']):
  for p,h in closure.items():require(p not in sources or sources[p]==h,'Restore source alias '+p);sources[p]=h
 def binding(ir,pack):
  return dict(ir=ir,pack=pack,ir_sha256=sha(ROOT/ir),pack_sha256=sha(ROOT/'romfs'/pack),bytes=(ROOT/'romfs'/pack).stat().st_size)
 bindings=dict(room=binding(inspection.ROOM_IR,'data/house-return-room.encroom'),house=binding('content/native-house.json','data/opening.enchouse'),reentry=binding('content/native-house-inspection-reentry.json','data/house-return.encreentry'))
 dependencies={p:sha(ROOT/p)for p in ['tools/native_restore.py','tools/house_inspection_restore.py','tools/native_content.py','tools/house_return_inspection_programme.py','tools/house_inspection_reentry.py','tools/house_reentry.py','content/native-restore.json','content/native-house-node-tree.json']}
 for v in bindings.values():dependencies[v['ir']]=v['ir_sha256'];dependencies['romfs/'+v['pack']]=v['pack_sha256']
 return dict(schema=1,format=1,family=FAMILY,capability=1,rules=1,commit=PIN,scene=tree_data['scene'],scene_id=tree_data['scene_id'],source_sha256=tree_data['source_sha256'],tree_ir_sha256=sha(tree.IR),room_scene_id=inner['scene_id'],reentry_scene_id=door['scene_id'],door_id=door['door_id'],bindings=bindings,sources=sources,dependencies=dependencies,base_restore_ir_sha256=sha(restore.IR),restore_semantics_sha256=hashlib.sha256(semantic_bytes(inner)).hexdigest(),restore=inner,admission_ready=False)

def review(d):
 return dict(schema=1,commit=PIN,ir_sha256=sha(IR),producer_sha256=sha(Path(__file__)),shared_writer_sha256=sha(Path(restore.__file__)),sources=d['sources'],dependencies=d['dependencies'],bindings=d['bindings'],restore_semantics_sha256=d['restore_semantics_sha256'],semantics=['Original source Restore, UID allocation, NPC event ordering, music and initial inventory retained byte-for-byte after the embedded raw fingerprints','Inspection Room preserves the complete original Room prefix; only raw Room dependency is rebound','Actual inspection Reentry and full House tree cross-bind target pin/source/root identity; data admission allocates no nodes and consumes no RNG'],tests_run=False,build_run=False,admission_ready=False)

def extract():
 d=derive();write(IR,d);write(REVIEW,review(d));return d
def load():
 d=read(IR);require(d==derive()and read(REVIEW)==review(d),'Inspection Restore source review stale');return d
def binary(d):
 b=bytearray(128)
 def u(*v):b.extend(struct.pack('<'+'I'*len(v),*v))
 def h(s):b.extend(bytes.fromhex(s))
 def t(s):v=s.encode();u(len(v));b.extend(v)
 t(d['scene']);u(d['room_scene_id'],d['reentry_scene_id'],d['door_id']);h(d['tree_ir_sha256'])
 for name in ('room','house','reentry'):
  v=d['bindings'][name];u(v['bytes']);h(v['pack_sha256']);h(v['ir_sha256'])
 h(d['base_restore_ir_sha256']);h(d['restore_semantics_sha256']);u(len(d['sources']))
 for p,s in sorted(d['sources'].items()):t(p);h(s)
 inner=inner_binary(d['restore']);u(len(inner));b.extend(inner)
 struct.pack_into('<8s8I',b,0,b'ENCHRST1',1,128,len(b),zlib.crc32(b[128:]),FAMILY,1,1,d['scene_id']);b[40:60]=bytes.fromhex(PIN);b[60:92]=bytes.fromhex(d['source_sha256']);b[92:124]=bytes.fromhex(sha(IR));return bytes(b)
def bundle_context(d):return dict(scene=d['scene'],scene_id=d['scene_id'],source_sha256=d['source_sha256'])
def stage_files(root):
 raw=binary(load());p=Path('data/house-return.encrestore');require((Path(root)/p).read_bytes()==raw,'Inspection Restore staged pack differs');return {p:raw}
def main():
 p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile']);a=p.parse_args()
 if a.action=='extract':extract();return
 raw=binary(load());PACK.parent.mkdir(parents=True,exist_ok=True);PACK.write_bytes(raw);print('Inspection Restore:',len(raw),'bytes; original source semantics; no Ready/RNG')
if __name__=='__main__':main()
