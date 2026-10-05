#!/usr/bin/env python3
"""Pack exact frozen rules6/rules7 validation resources for lossless LOAD.

The frozen files are the unchanged generated resources from the reviewed v6
checkpoint, not player saves. Their checked-in SHA-256 manifest and source IR
preserve provenance. They are intentionally NOT regenerated from today's rules.
"""
from __future__ import annotations
import argparse, hashlib, json, struct, sys, zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from tools.native_session import encode as encode_session
FROZEN=ROOT/'content/legacy-rules6'
PACK=ROOT/'romfs/data/rules6-to7.encmigration'
ORDER=['opening.encsession','opening.encroom','opening.enchouse','opening.encround','opening.encitems']
def require(value,message):
    if not value:raise ValueError(message)
def read(path):return json.loads(Path(path).read_text())
def identity(value):return tuple(value[k]for k in ['content_family','content_revision','rules_revision'])
def checked_bundle(root, revision, current):
    frozen=root/('content/legacy-rules'+str(revision));manifest=read(frozen/'manifest.json')
    require(manifest['schema']==1 and manifest['kind']=='encore.session-migration.legacy-validation','Unknown migration manifest')
    require(manifest['source_commit']==read(root/'upstream.lock')['commit'],'Migration source commit mismatch')
    before,declared=identity(manifest['from']),identity(manifest['to']);after=identity(current['compatibility'])
    require(before[:2]==after[:2] and before[2]==revision and declared[:2]==before[:2] and declared[2]==revision+1,'Frozen migration endpoint mismatch')
    require(set(manifest['files'])==set(ORDER),'Unexpected legacy resource coverage')
    for name,expected in {**manifest['files'],**manifest['recipes']}.items():
        require(Path(name).name==name and hashlib.sha256((frozen/name).read_bytes()).hexdigest()==expected,'Frozen legacy fingerprint mismatch: '+name)
    legacy=read(frozen/'native-session.json')
    require(identity(legacy['compatibility'])==before,'Frozen session endpoint mismatch')
    require((frozen/ORDER[0]).read_bytes()==encode_session(legacy),'Frozen session pack/recipe mismatch')
    # Source-equivalent baseline only. New optional keys/skills must NOT be
    # synthesized from flags; snapshots must pass BOTH complete domains.
    for field in ['defaults','levels','saved_flag','earned_cash_flag']:
        require(legacy[field]==current[field],'Lossless migration field changed: '+field)
    for field in ['mutable_flags','camera_area_ids']:
        require(set(legacy[field])<=set(current[field]),'Legacy scope removed: '+field)
    payload=bytearray(struct.pack('<3I',*before))
    for name in ORDER:
        raw=(frozen/name).read_bytes();payload+=struct.pack('<I',len(raw))+raw
    return before,payload

def encode(root=ROOT):
    root=Path(root);current=read(root/'content/native-session.json');after=identity(current['compatibility'])
    require(after[2] in (7,8),'Unreviewed migration target rules')
    first,legacy6=checked_bundle(root,6,current)
    if after[2]==7:
        # Original schema1 and unchanged frozen6 remain byte-compatible.
        payload=struct.pack('<6I',*first,*after)+legacy6[12:]
        return struct.pack('<8s4I',b'ENCMIG01',1,24+len(payload),zlib.crc32(payload),1)+payload
    second,legacy7=checked_bundle(root,7,current)
    require(first[:2]==second[:2]==after[:2] and (first[2],second[2])==(6,7),'Migration bundle identities differ')
    payload=struct.pack('<4I',2,*after)+legacy6+legacy7
    return struct.pack('<8s4I',b'ENCMIG01',2,24+len(payload),zlib.crc32(payload),2)+payload

def stage_files(source_root):
    relative=Path('data/rules6-to7.encmigration');data=(Path(source_root)/relative).read_bytes()
    require(data==encode(),'Stale staged migration pack');return {relative:data}
def main():
    ap=argparse.ArgumentParser();ap.add_argument('action',choices=['compile','verify'],nargs='?',default='compile');args=ap.parse_args();data=encode()
    if args.action=='verify':require(PACK.read_bytes()==data,'Stale migration pack')
    else:PACK.parent.mkdir(parents=True,exist_ok=True);PACK.write_bytes(data)
    print('Session migration:',len(data),'bytes; exact historical validation; unchanged snapshot payload')
if __name__=='__main__':
    try:main()
    except (ValueError,KeyError,OSError,TypeError)as e:print('SESSION MIGRATION ERROR:',e,file=sys.stderr);raise SystemExit(1)
