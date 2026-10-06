#!/usr/bin/env python3
"""Source PCK projection and actual DirAccessPack rules; never a loose-directory trace."""
from __future__ import annotations
import argparse,hashlib,json,re,struct,sys,zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import PIN,read,write,sha,require
from tools.global_yaml_caches import load as caches_load,IR as CACHES_IR,OWNER,ENGINE
from tools.extract_battle_entry import Extractor
IR=ROOT/'content/native-global-packed-directory.json'
REVIEW=ROOT/'reports/global-directory/source-review.json'
RECEIPT=ROOT/'reports/global-directory/packed-source.json'
OUT=ROOT/'romfs/data/global.encpackeddir'
FAMILY=0x454e0051
ENGINE_FILES=['core/io/file_access_pack.cpp','core/io/file_access_pack.h','core/bind/core_bind.cpp','core/bind/core_bind.h','core/os/dir_access.cpp','core/reference.cpp','core/reference.h','core/map.h','core/set.h','core/ustring.cpp','core/ustring.h','editor/editor_export.cpp','editor/editor_file_system.cpp','core/project_settings.cpp','core/io/pck_packer.cpp']
def prepare(directory):
 ex=Extractor(ROOT);c=caches_load();prefixes=[x['directory']for x in c['policies']];inv=read(ROOT/'compatibility/upstream-inventory.json')['files'];rows=[]
 require(not directory.exists()or not any(directory.iterdir()),'Source conversion directory must be empty')
 directory.mkdir(parents=True,exist_ok=True)
 for path in inv:
  if not any(path.startswith(x)for x in prefixes):continue
  raw=ex.data(path);target=directory/path;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(raw);rows.append(dict(source=path,sha256=ex.sources[path],size=len(raw)))
 write(directory/'manifest.json',rows)
 (directory/'project.godot').write_text('[application]\nconfig/name="Directory source conversion"\n',encoding='utf-8')
 (directory/'export.gd').write_bytes((ROOT/'tools/godot_exporter/global_packed_directory.gd').read_bytes())

def package_paths(raw):
 require(len(raw)>=88 and raw[:4]==b'GDPC','Actual PCK conversion required')
 version,major,minor,patch=struct.unpack_from('<4I',raw,4)
 require((version,major,minor,patch)==(1,3,6,2),'Unknown PCK version')
 require(raw[20:84]==bytes(64),'Unknown PCK reserved flags')
 count=struct.unpack_from('<I',raw,84)[0];at=88;files=[]
 for _ in range(count):
  size=struct.unpack_from('<I',raw,at)[0];at+=4;name=raw[at:at+size].rstrip(b'\0').decode();at+=size
  offset,length=struct.unpack_from('<QQ',raw,at);at+=16;md5=raw[at:at+16];at+=16
  require(name.startswith('res://')and offset+length<=len(raw),'PCK path/range rejected');data=raw[offset:offset+length]
  require(md5==bytes(16),'Unknown source PCKPacker MD5 policy')
  files.append(dict(source=name[6:],size=length,sha256=hashlib.sha256(data).hexdigest()))
 require(len({x['source']for x in files})==count,'Duplicate PCK entries');return files

def extract(receipt,pck,engine_dir):
 c=caches_load();ex=Extractor(ROOT);native=read(receipt)
 require(native['schema']==1 and native['source_tree_entered']is False and [native['engine'].get(k)for k in ['major','minor','patch','status']]==[3,6,2,'stable'],'Unknown native conversion')
 files=package_paths(pck.read_bytes());prefixes=[x['directory']for x in c['policies']];inv=read(ROOT/'compatibility/upstream-inventory.json')['files'];all_paths=sorted(p for p in inv if any(p.startswith(x)for x in prefixes))
 require(sorted(x['source']for x in native['files'])==all_paths,'Incomplete source six-directory inventory')
 extensions=native['recognized_extensions'];require('txt'not in extensions,'Documentation became native recognized Resource')
 for row in native['files']:
  ex.data(row['source']);require(row['sha256']==ex.sources[row['source']]and row['size']==len(ex.data(row['source'])),'Changed native raw source')
  require(row['included']==(row['source'].endswith('.yaml')or row['source'].rsplit('.',1)[-1].lower()in extensions),'Unknown export filter')
 require(sorted(x['source']for x in files)==sorted(x['source']for x in native['files']if x['included']),'Actual PCK file tree mismatch')
 for row in files:require(row['sha256']==inv[row['source']]['sha256'],'Changed PCK source bytes')
 ex.data('export_presets.cfg');preset=ex.text('export_presets.cfg').split('[preset.0]',1)[1].split('[preset.0.options]',1)[0]
 require('export_filter="all_resources"'in preset and 'include_filter="*.json, Graphics/Battle BGS/*, *.yaml, *.ecs, *.dat"'in preset and 'exclude_filter=""'in preset,'Unknown original PCK cache inclusion rule')
 engine={p:sha(engine_dir/p.replace('/','-'))for p in ENGINE_FILES}
 pack=(engine_dir/'core-io-file_access_pack.cpp').read_text();header=(engine_dir/'core-io-file_access_pack.h').read_text();binding=(engine_dir/'core-bind-core_bind.cpp').read_text();settings=(engine_dir/'core-project_settings.cpp').read_text()
 for token in ['Map<String, PackedDir *> subdirs;','Set<String> files;']:require(token in header,'Unknown PackedDir native containers')
 for token in ['list_dirs.push_back(E->key());','list_files.push_back(E->get());','if (list_dirs.size()) {','cdir = true;','cdir = false;','return String();']:require(token in pack,'Unknown native enumeration')
 require('DirAccess::make_default<DirAccessPack>(DirAccess::ACCESS_RESOURCES);'in settings,'Unknown PackedData Directory mode')
 require('DEFVAL(false), DEFVAL(false)'in binding,'Changed Directory skip defaults')
 dirs={''}
 for row in files:
  p=Path(row['source']).parent.as_posix()
  while p!='.':dirs.add(p+'/');p=Path(p).parent.as_posix()
 d=dict(schema=1,kind='encore.global-packed-directory.source-ir',commit=PIN,owner=OWNER,source_sha256=c['source_sha256'],scene_id=int.from_bytes(hashlib.sha256(('packed-dir:'+OWNER).encode()).digest()[:4],'little'),class_id=int.from_bytes(hashlib.sha256(('native-reference:'+OWNER+'#Directory').encode()).digest()[:4],'little'),native_class='Directory',yaml_suffix=re.search(r'file_name\.ends_with\("([^"]+)"\)',ex.text(OWNER))[1],mode=1,cache_ir_sha256=sha(CACHES_IR),engine_commit=ENGINE,engine_sources=engine,source_exporter_sha256=sha(ROOT/'tools/godot_exporter/global_packed_directory.gd'),rules=[1,1,0,0],files=files,directories=sorted(dirs),excluded=[x for x in native['files']if not x['included']],sources={OWNER:c['source_sha256'],'export_presets.cfg':ex.sources['export_presets.cfg']},policies=[dict(role=x['role'],directory=x['directory'],closure_sha256=x['closure_sha256'])for x in c['policies']],pck_sha256=sha(pck),empty_directory_entries=[],semantics=['Only explicit original Windows PCK six-cache file-tree projection; no whole game export claim','PackedData adds ancestors of actual file entries; PCK carries no standalone empty directories','DirAccessPack native Map/String and Set/String forward order: directories then files; UTF32 scalar lexical order','Source recursive _load_data executes child directory before requesting the next parent entry','Directory pure Reference ownership; default skip navigation/hidden false; PCK has neither synthetic navigation nor hidden entries','Canonical file manifest is closure only, runtime constructs the source PackedDir map and separate mutable iterator queues','Original get_json_data/SmartFileReader File/Parser Reference callbacks still required; no source Ready admission'],whole_init_admitted=False)
 write(RECEIPT,native);write(IR,d);write(REVIEW,dict(schema=1,commit=PIN,ir_sha256=sha(IR),receipt_sha256=sha(RECEIPT),engine_commit=ENGINE,engine_sources=engine,source_review='Actual native PCK conversion and fixed official source semantic audit; no Directory behavior probe'))

def load():
 d=read(IR);r=read(REVIEW);c=caches_load();ex=Extractor(ROOT)
 require(d['schema']==1 and d['commit']==PIN and d['engine_commit']==ENGINE and d['yaml_suffix']=='.yaml'and d['mode']==1 and d['rules']==[1,1,0,0]and d['whole_init_admitted']is False and d['empty_directory_entries']==[] and r['ir_sha256']==sha(IR)and r['receipt_sha256']==sha(RECEIPT)and d['cache_ir_sha256']==sha(CACHES_IR)and d['source_exporter_sha256']==sha(ROOT/'tools/godot_exporter/global_packed_directory.gd'),'Packed Directory source review stale')
 for p,h in d['sources'].items():ex.data(p);require(ex.sources[p]==h,'Packed Directory source changed')
 actual={x['source']:x['sha256']for x in d['files']};expected={x['source']:x['sha256']for x in c['records']};require(actual==expected,'Incomplete six-cache PCK closure')
 for p,h in actual.items():ex.data(p);require(ex.sources[p]==h,'Changed actual PCK member')
 return d

def encode(d):
 b=bytearray(128)
 def u(*v):b.extend(struct.pack('<'+'I'*len(v),*v))
 def t(s):v=s.encode();u(len(v));b.extend(v)
 t(d['owner']);t(d['native_class']);t(d['yaml_suffix']);u(d['class_id'],d['mode'],*d['rules']);b.extend(bytes.fromhex(d['cache_ir_sha256']));t(d['engine_commit']);u(len(d['engine_sources']))
 for p,h in d['engine_sources'].items():t(p);b.extend(bytes.fromhex(h))
 u(len(d['policies']))
 for x in d['policies']:u(x['role']);t(x['directory']);b.extend(bytes.fromhex(x['closure_sha256']))
 u(len(d['directories']));[t(x)for x in d['directories']];u(len(d['files']))
 for x in d['files']:t(x['source']);b.extend(bytes.fromhex(x['sha256']));b.extend(struct.pack('<Q',x['size']))
 struct.pack_into('<8s8I',b,0,b'ENCPDIR1',1,128,len(b),0,FAMILY,1,1,d['scene_id']);b[40:60]=bytes.fromhex(PIN);b[60:92]=bytes.fromhex(d['source_sha256']);b[92:124]=bytes.fromhex(sha(IR));struct.pack_into('<I',b,20,zlib.crc32(b[128:]));return bytes(b)
def main():
 p=argparse.ArgumentParser();p.add_argument('action',choices=['prepare','extract','compile','verify']);p.add_argument('--directory',type=Path);p.add_argument('--receipt',type=Path);p.add_argument('--pck',type=Path);p.add_argument('--engine-directory',type=Path);a=p.parse_args()
 if a.action=='prepare':require(a.directory,'Explicit isolated source directory required');prepare(a.directory);return
 if a.action=='extract':require(a.receipt and a.pck and a.engine_directory,'Explicit actual PCK/source conversion/engine required');extract(a.receipt,a.pck,a.engine_directory);return
 d=load();b=encode(d)
 if a.action=='compile':OUT.parent.mkdir(exist_ok=True,parents=True);OUT.write_bytes(b)
 else:require(OUT.read_bytes()==b,'Stale packed Directory binary')
 print('Actual PCK cache projection:',len(d['files']),'files;',len(d['directories']),'PackedDirs;',len(b),'bytes')
if __name__=='__main__':
 try:main()
 except(ValueError,KeyError,TypeError,OSError,struct.error)as e:sys.exit('PACKED DIRECTORY ERROR: '+str(e))
