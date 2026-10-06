#!/usr/bin/env python3
"""Real source File/SmartFileReader lifecycle and compiled original YAML values."""
import argparse,hashlib,json,struct,sys,zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import PIN,read,write,sha,require,stable
from tools.global_yaml_caches import load as caches_load,value,ENGINE
from tools.extract_battle_entry import Extractor
PARSER='Scripts/global/yaml_parser.gd';OWNER='Scripts/global/globalData.gd'
FAMILY=0x454e0052
IR=ROOT/'content/native-global-yaml-file.json';REVIEW=ROOT/'reports/global-yaml-file/source-review.json';RECEIPT=ROOT/'reports/global-yaml-file/native-source.json';PACK=ROOT/'romfs/data/global.encyamlfile'
ENGINE_PATHS=['core/bind/core_bind.h','core/bind/core_bind.cpp','core/os/file_access.cpp','core/object.cpp','core/reference.cpp','modules/gdscript/gdscript.cpp','core/io/file_access_pack.cpp']

def inputs():
 ex=Extractor(ROOT);c=caches_load();parser=ex.text(PARSER);owner=ex.text(OWNER)
 require('var _file := File.new()'in parser and 'class SmartFileReader:'in parser and 'var reader := SmartFileReader.new()'in parser,'Changed reader construction')
 require('var file := File.new()\n\tif file.file_exists(file_path):\n\t\tfile.open(file_path, File.READ)\n\t\tvar file_content := file.get_as_text()\n\t\tvar res = YAMLParser.parse_file(file_path)'in owner,'Changed get_json_data File sequence')
 require('reader.close()\n\t\n\treturn result'in parser and '\t\t_file.close()'in parser,'Changed explicit inner close')
 rows=[]
 for r in c['records']:
  raw=ex.data(r['source']);require(b'\0'not in raw,'Unknown YAML NUL source');raw.decode('utf-8');rows.append(dict(role=r['role'],source=r['source'],source_sha256=r['sha256'],source_hex=raw.hex(),value=r.get('value')))
 bindings=[dict(id=stable(OWNER+'#get_json_data.File'),role=5,name='get_json_data.File',native_class='File',source=OWNER),dict(id=stable(PARSER+'#SmartFileReader'),role=5,name='SmartFileReader',native_class='Reference',source=PARSER),dict(id=stable(PARSER+'#SmartFileReader._file'),role=5,name='SmartFileReader._file',native_class='File',source=PARSER)]
 return c,rows,bindings

def prepare(directory):
 c,rows,bindings=inputs();ex=Extractor(ROOT);items=[r['source']for r in rows if r['role']==4]
 for worker in range(4):
  d=directory/str(worker);d.mkdir(parents=True,exist_ok=True)
  (d/'project.godot').write_bytes(b'config_version=4\n[application]\nconfig/name="Original YAML source compilation"\n[logging]\nfile_logging/enable_logging=false\n')
  (d/'yaml_parser.gd').write_bytes(ex.data(PARSER));(d/'export.gd').write_bytes((ROOT/'tools/godot_exporter/global_yaml_file.gd').read_bytes())
  batch=items[worker::4];write(d/'manifest.json',batch)
  for source in batch:p=d/source;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(ex.data(source))
 print('Prepared',len(items),'actual Items sources in four isolated data-only workers')

def extract(native,engine):
 c,rows,bindings=inputs();n=read(native)
 require(set(n)=={'schema','engine','source_scene_entered','documents'}and n['schema']==1 and n['source_scene_entered']is False and n['engine']['hash']==ENGINE and [n['engine'][k]for k in('major','minor','patch','status','build')]==[3,6,2,'stable','official'],'Unknown official source compiler')
 docs={x['source']:value(x['value'])for x in n['documents']};require(len(docs)==len(n['documents'])and set(docs)=={x['source']for x in rows if x['role']==4},'Incomplete original Items parsed values')
 for r in rows:
  if r['role']==4:r['value']=docs[r['source']]
  require(r['value']['kind']==(5 if r['role']==3 else 6),'Unknown source root kind')
 files={p:sha(engine/p.replace('/','-'))for p in ENGINE_PATHS}
 require('class _File : public Reference'in(engine/'core-bind-core_bind.h').read_text(encoding='utf-8'),'Unknown File native base')
 require('owner = memnew(Reference); //by default, no base means use reference'in(engine/'modules-gdscript-gdscript.cpp').read_text(encoding='utf-8'),'Unknown implicit Reference constructor')
 write(RECEIPT,dict(n,commit=PIN,exporter_sha256=sha(ROOT/'tools/godot_exporter/global_yaml_file.gd')))
 d=dict(schema=1,kind='encore.global-yaml-file.source-ir',family=FAMILY,commit=PIN,scene_id=stable(PARSER+'#parse_file'),parser=PARSER,owner=OWNER,sources={p:c['sources'][p]for p in(PARSER,OWNER)},bindings=bindings,records=rows,dependencies={'content/native-global-yaml-caches.json':sha(ROOT/'content/native-global-yaml-caches.json'),'reports/global-yaml-caches/native-source.json':sha(ROOT/'reports/global-yaml-caches/native-source.json')},native_receipt_sha256=sha(RECEIPT),engine=dict(commit=ENGINE,sources=files),scope='Only exact checked six-cache YAML sources; actual File and implicit SmartFileReader references; no whole globaldata Ready',policy=dict(read_mode=1,outer_get_as_text_skip_cr=True,outer_read_preserves_position=True,outer_explicit_close=False,inner_explicit_close_on_success=True,reader_member_before_object_retirement=True,compiled_values_pristine=True))
 write(IR,d);write(REVIEW,dict(schema=1,commit=PIN,ir_sha256=sha(IR),native_receipt_sha256=sha(RECEIPT),engine=d['engine'],dependencies=d['dependencies'],sources=d['sources'],scope=d['scope']))

def load():
 c,rows,bindings=inputs();d=read(IR);n=read(RECEIPT);review=read(REVIEW)
 require(d['schema']==1 and d['kind']=='encore.global-yaml-file.source-ir'and d['family']==FAMILY and d['commit']==PIN and d['bindings']==bindings and d['scene_id']==stable(PARSER+'#parse_file'),'Unknown YAML File IR')
 require(review['ir_sha256']==sha(IR)and d['native_receipt_sha256']==review['native_receipt_sha256']==sha(RECEIPT)and n['commit']==PIN and n['exporter_sha256']==sha(ROOT/'tools/godot_exporter/global_yaml_file.gd'),'Stale original parser provenance')
 require(d['sources']==review['sources']=={p:c['sources'][p]for p in(PARSER,OWNER)}and d['dependencies']==review['dependencies']and d['engine']==review['engine'],'Changed source bindings')
 for p,h in d['dependencies'].items():require(sha(ROOT/p)==h,'Changed cache source prerequisite')
 docs={x['source']:value(x['value'])for x in n['documents']}
 require(len(rows)==len(d['records']),'Incomplete YAML File closure')
 for old,row in zip(rows,d['records']):
  if old['role']==4:old['value']=docs[old['source']]
  require(row==old,'Changed original YAML content/result '+old['source']);value(row['value'])
 return d

def encode(d):
 b=bytearray(128)
 def u(*v):b.extend(struct.pack('<'+'I'*len(v),*v))
 def text(v):raw=v.encode();u(len(raw));b.extend(raw)
 def val(v):
  k=v['kind'];u(k)
  if k==1:u(v['value'])
  elif k==2:b.extend(struct.pack('<q',int(v['value'])))
  elif k==3:b.extend(bytes.fromhex(v['f64_le']))
  elif k==4:text(v['value'])
  elif k==5:
   u(len(v['values']))
   for x in v['values']:val(x)
  elif k==6:
   u(len(v['entries']))
   for key,x in v['entries']:text(key);val(x)
 text(d['parser']);text(d['owner']);u(len(d['sources']))
 for p,h in d['sources'].items():text(p);b.extend(bytes.fromhex(h))
 u(len(d['bindings']))
 for x in d['bindings']:u(x['id'],x['role']);text(x['name']);text(x['native_class']);text(x['source'])
 u(len(d['records']))
 for x in d['records']:
  u(x['role']);text(x['source']);b.extend(bytes.fromhex(x['source_sha256']));raw=bytes.fromhex(x['source_hex']);u(len(raw));b.extend(raw);val(x['value'])
 struct.pack_into('<8s8I',b,0,b'ENCYFIL1',1,128,len(b),zlib.crc32(b[128:]),FAMILY,1,1,d['scene_id']);b[40:60]=bytes.fromhex(PIN);b[60:92]=bytes.fromhex(d['sources'][PARSER]);b[92:124]=bytes.fromhex(sha(IR));return bytes(b)

def main():
 p=argparse.ArgumentParser();p.add_argument('action',choices=['prepare','extract','compile','verify']);p.add_argument('--directory',type=Path);p.add_argument('--native',type=Path);p.add_argument('--engine',type=Path);a=p.parse_args()
 if a.action=='prepare':require(a.directory,'Explicit private source generation directory required');prepare(a.directory);return
 if a.action=='extract':require(a.native and a.engine,'Explicit source compiler/engine proof required');extract(a.native,a.engine);return
 d=load();b=encode(d)
 if a.action=='compile':PACK.parent.mkdir(parents=True,exist_ok=True);PACK.write_bytes(b)
 else:require(PACK.read_bytes()==b,'Stale YAML File pack')
 print('Actual source YAML File:',len(d['records']),'documents;',len(b),'bytes; no Root Ready admission')
if __name__=='__main__':
 try:main()
 except(ValueError,KeyError,TypeError,OSError,struct.error)as e:sys.exit('YAML FILE ERROR: '+str(e))
