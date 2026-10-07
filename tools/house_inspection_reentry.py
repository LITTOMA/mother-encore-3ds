#!/usr/bin/env python3
"""Same original Door lifecycle bound to the complete inspection Room resource."""
from pathlib import Path
import argparse,sys
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import house_reentry as reentry
from tools.podunk_scene import PIN,read,write,sha,require
IR=ROOT/'content/native-house-inspection-reentry.json'
REVIEW=ROOT/'reports/house-inspection-reentry/source-review.json'
PACK=ROOT/'romfs/data/house-return.encreentry'
ROOM_IR='content/native-house-return-room-programmes.json'
ROOM_PACK='data/house-return-room.encroom'
def derive():return reentry.derive(ROOM_IR,ROOM_PACK)
def review(d):
 return dict(schema=1,commit=PIN,ir_sha256=sha(IR),producer_sha256=sha(Path(__file__)),
             shared_producer_sha256=sha(Path(reentry.__file__)),sources=d['sources'],
             dependencies=d['dependencies'],admission=d['admission'])
def extract():
 d=derive();write(IR,d);write(REVIEW,review(d));return d
def load():
 d=read(IR);require(d==derive()and read(REVIEW)==review(d),'Inspection House Door source/resource review stale');return d
def binary(d):return reentry.binary(d,sha(IR))
def bundle_context(d):return dict(scene=d['target_scene'],scene_id=d['scene_id'],source_sha256=d['source_sha256'])
def stage_files(root):
 raw=binary(load());p=Path('data/house-return.encreentry');require((Path(root)/p).read_bytes()==raw,'Inspection House Door staged pack differs');return {p:raw}
def main():
 p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile','verify']);a=p.parse_args()
 if a.action=='extract':extract();return
 raw=binary(load())
 if a.action=='compile':PACK.parent.mkdir(parents=True,exist_ok=True);PACK.write_bytes(raw)
 else:require(PACK.read_bytes()==raw,'Inspection House Door pack stale')
 print('Inspection House Door:',len(raw),'bytes; original lifecycle; no Ready grant')
if __name__=='__main__':main()
