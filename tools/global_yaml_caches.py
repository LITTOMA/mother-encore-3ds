#!/usr/bin/env python3
"""Complete fixed six-directory closure; five original parsed YAML caches.

Native source-parser export is data-only. This pack does not implement skill,
ailment or shop gameplay, nor admit globaldata Ready. Runtime insertion order
comes from the actual caller Directory cursor, never this canonical manifest.
"""
from __future__ import annotations
import argparse,hashlib,json,re,struct,sys,zlib,math
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from tools.podunk_scene import PIN,read,write,sha,require
from tools.extract_battle_entry import Extractor
IR=ROOT/'content/native-global-yaml-caches.json'
RECEIPT=ROOT/'reports/global-yaml-caches/native-source.json'
REVIEW=ROOT/'reports/global-yaml-caches/source-review.json'
PACK=ROOT/'romfs/data/global.encyamlcaches'
OWNER='Scripts/global/globalData.gd'
PARSER='Scripts/global/yaml_parser.gd'
ENGINE='3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8'
FAMILY=0x454e004f

def source_inputs():
 ex=Extractor(ROOT);body=ex.text(OWNER);ex.data(PARSER)
 init=body.split('func _init():',1)[1].split('func _ready():',1)[0]
 require(init.lstrip().startswith('_init_flags()'),'Unknown flags/cache source order')
 roles=re.findall(r'"([A-Za-z]+)": (_[a-z_]+)',init)
 require(len(roles)==6 and roles[4][0]=='Items','Unknown six-cache initialization source')
 rows=[];sources={OWNER:ex.sources[OWNER],PARSER:ex.sources[PARSER]}
 inventory=read(ROOT/'compatibility/upstream-inventory.json')['files']
 for role,(name,member)in enumerate(roles):
  prefix='Data/'+name+'/'
  paths=sorted(p for p in inventory if p.startswith(prefix)and p.endswith('.yaml'))
  actual=sorted(p.relative_to(ex.upstream).as_posix()for p in(ex.upstream/prefix).rglob('*.yaml'))
  require(paths and actual==paths,'Incomplete/unknown original YAML directory '+prefix)
  for path in paths:
   ex.data(path);sources[path]=ex.sources[path]
   rows.append(dict(role=role,source=path,name=path[len(prefix):-5],sha256=ex.sources[path]))
 require('dest[sub_dir + file_name.trim_suffix(".yaml")] = json_data'in body,'Unknown _load_data insertion')
 require('codegen.opcodes.push_back(GDScriptFunction::OPCODE_RETURN);'in body or 'var res = YAMLParser.parse_file(file_path)'in body,'Unknown native parser source return')
 getters=[]
 for match in re.finditer(r'^func (get_ailment_data|does_ailment_exist|get_all_ailments|get_shop_data|get_battle_skill|does_battle_skill_exist|get_all_battle_skills|get_passive_skill|get_field_skill|get_all_field_skills)\([^\n]*\):?[^\n]*\n(.*?)(?=^func |\Z)',body,re.M|re.S):
  method,part=match[1],match[2]
  cache=re.search(r'(_[a-z_]+)\.(?:get|has|keys)\(',part)
  require(cache,'Missing source getter cache '+method)
  index=next((i for i,(_,member)in enumerate(roles)if member==cache[1]),None)
  require(index is not None,'Unknown source getter Dictionary')
  action=3 if '.keys()'in part else 2 if method.startswith('does_')else 1
  mutation=re.search(r'\["([^"\n]+)"\] = (?:ailment_name|skill_name)',part)
  warning=re.search(r'push_warning\("([^"\n]+)" %',part)
  getters.append(dict(method=method,role=index,action=action,truthiness=('!!'in part),mutation=mutation[1]if mutation else'',warning=warning[1]if warning else''))
 require(len(getters)==10,'Incomplete source cache getters')
 return ex,roles,rows,sources,getters

def prepare(directory):
 ex,roles,rows,sources,getters=source_inputs()
 directory.mkdir(parents=True,exist_ok=True)
 (directory/'project.godot').write_text('config_version=4\n[application]\nconfig/name="Original cache data export"\n[logging]\nfile_logging/enable_logging=false\n')
 (directory/'yaml_parser.gd').write_bytes(ex.data(PARSER))
 (directory/'export.gd').write_bytes((ROOT/'tools/godot_exporter/global_yaml_caches.gd').read_bytes())
 for row in rows:
  if row['role']==4:continue
  p=directory/row['source'];p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(ex.data(row['source']))
 write(directory/'manifest.json',rows)
 print('Data-only original parser project:',len(rows),'source files, five cache documents')

def value(v,depth=0):
 require(depth<=64 and isinstance(v,dict)and type(v.get('kind'))is int,'Invalid native YAML shape/depth')
 k=v['kind'];require(0<=k<=6,'Unknown native YAML Variant type')
 keys={0:{'kind'},1:{'kind','value'},2:{'kind','value'},3:{'kind','f64_le'},4:{'kind','value'},5:{'kind','values'},6:{'kind','entries'}}[k]
 require(set(v)==keys,'Unknown native YAML Variant fields')
 if k==1:require(type(v['value'])is bool,'Native YAML boolean mismatch')
 if k==2:require(isinstance(v['value'],str)and re.fullmatch(r'-?(0|[1-9][0-9]*)',v['value'])and -(1<<63)<=int(v['value'])<(1<<63),'Native YAML int64 rejected')
 if k==3:require(re.fullmatch('[0-9a-f]{16}',v['f64_le'])and math.isfinite(struct.unpack('<d',bytes.fromhex(v['f64_le']))[0]),'Native YAML finite double required')
 if k==4:require(type(v['value'])is str and '\0'not in v['value'],'Native YAML string rejected')
 if k==5:
  require(type(v['values'])is list,'Native YAML Array rejected')
  for x in v['values']:value(x,depth+1)
 if k==6:
  require(type(v['entries'])is list,'Native YAML Dictionary order rejected');seen=set()
  for pair in v['entries']:
   require(type(pair)is list and len(pair)==2 and type(pair[0])is str and pair[0]not in seen and '\0'not in pair[0],'Native YAML Dictionary key/duplicate rejected');seen.add(pair[0]);value(pair[1],depth+1)
 return v

def extract(native):
 ex,roles,rows,sources,getters=source_inputs();receipt=read(native)
 require(set(receipt)=={'schema','engine','source_scene_entered','documents'}and receipt['schema']==1 and receipt['source_scene_entered']is False,'Source-only parser receipt required')
 require(all(receipt['engine'].get(k)==v for k,v in dict(major=3,minor=6,patch=2,status='stable',build='official',hash=ENGINE).items()),'Unknown native cache engine')
 docs={x['source']:value(x['value'])for x in receipt['documents']}
 require(len(docs)==len(receipt['documents'])and set(docs)=={r['source']for r in rows if r['role']!=4},'Incomplete original cache parsed documents')
 policies=[]
 for role,(name,member)in enumerate(roles):
  subset=[x for x in rows if x['role']==role]
  closure=hashlib.sha256(''.join(x['source']+'\0'+x['sha256']+'\n'for x in subset).encode()).hexdigest()
  policies.append(dict(role=role,name=name,member=member,directory='Data/'+name+'/',root_kind=5 if role==3 else 6,closure_sha256=closure))
 for row in rows:
  if row['role']!=4:
   row['value']=docs[row['source']];require(row['value']['kind']==policies[row['role']]['root_kind'],'Source cache root kind mismatch')
 write(RECEIPT,dict(receipt,sources=sources,commit=PIN,exporter_sha256=sha(ROOT/'tools/godot_exporter/global_yaml_caches.gd')))
 d=dict(schema=1,kind='encore.global-yaml-caches.source-ir',commit=PIN,owner=OWNER,source_sha256=sources[OWNER],scene_id=int.from_bytes(hashlib.sha256(('global-yaml:'+OWNER).encode()).digest()[:4],'little'),policies=policies,records=rows,getters=getters,sources=sources,native_receipt_sha256=sha(RECEIPT),semantics=['Source _init flags precede six cache calls; actual Directory insertion and finish cursors required','Canonical manifest is complete source closure, never runtime enumeration order','Native Dictionary key order, int64 and f64 preserved; five caches own live shared values','Getters mutate id on the same source Dictionary; Shops return original Array','Dynamic Variant native return copies result; Dictionary return annotation does not cast Array at runtime','Items fields remain actual independently owned cache; manifest binds complete Directory closure'],whole_globaldata_admitted=False)
 write(IR,d);write(REVIEW,dict(schema=1,commit=PIN,ir_sha256=sha(IR),native_receipt_sha256=sha(RECEIPT),sources=sources,capability=1,whole_globaldata_admitted=False))

def load():
 d=read(IR);review=read(REVIEW);receipt=read(RECEIPT);ex,roles,rows,sources,getters=source_inputs()
 require(set(d)=={'schema','kind','commit','owner','source_sha256','scene_id','policies','records','getters','sources','native_receipt_sha256','semantics','whole_globaldata_admitted'},'Unknown cache IR fields')
 require(receipt['exporter_sha256']==sha(ROOT/'tools/godot_exporter/global_yaml_caches.gd'),'Changed source parser exporter')
 require(d['owner']==OWNER and d['source_sha256']==sources[OWNER] and d['kind']=='encore.global-yaml-caches.source-ir','Unknown cache IR owner')
 require(d['schema']==1 and d['commit']==PIN and d['whole_globaldata_admitted']is False and review['ir_sha256']==sha(IR) and d['native_receipt_sha256']==review['native_receipt_sha256']==sha(RECEIPT),'Stale cache source/review')
 require(d['sources']==review['sources']==receipt['sources']==sources and d['getters']==getters and [(x['name'],x['member'])for x in d['policies']]==roles,'Changed cache source/script closure')
 actual={x['source']:value(x['value'])for x in receipt['documents']}
 require(len(d['records'])==len(rows),'Incomplete six-directory closure')
 for old,row in zip(rows,d['records']):
  require(all(row[k]==v for k,v in old.items())and set(row)==set(old)|({'value'}if old['role']!=4 else set()),'Cache source entry fields changed')
  if old['role']!=4:require(value(row['value'])==actual[old['source']],'Cache native source value changed')
 for index,policy in enumerate(d['policies']):
  require(set(policy)=={'role','name','member','directory','root_kind','closure_sha256'} and policy['role']==index and policy['directory']=='Data/'+roles[index][0]+'/' and policy['root_kind']==(5 if index==3 else 6),'Unknown source cache policy fields/shape')
  subset=[x for x in rows if x['role']==policy['role']]
  require(policy['closure_sha256']==hashlib.sha256(''.join(x['source']+'\0'+x['sha256']+'\n'for x in subset).encode()).hexdigest(),'Cache Directory closure proof stale')
 return d

def encode(d):
 b=bytearray(128)
 def u(*x):b.extend(struct.pack('<'+'I'*len(x),*x))
 def t(s):v=s.encode();u(len(v));b.extend(v)
 def val(v):
  k=v['kind'];u(k)
  if k==1:u(int(v['value']))
  elif k==2:b.extend(struct.pack('<q',int(v['value'])))
  elif k==3:b.extend(bytes.fromhex(v['f64_le']))
  elif k==4:t(v['value'])
  elif k==5:
   u(len(v['values']))
   for x in v['values']:val(x)
  elif k==6:
   u(len(v['entries']))
   for key,x in v['entries']:t(key);val(x)
 t(d['owner']);u(len(d['policies']))
 for x in d['policies']:
  u(x['role'],x['root_kind']);t(x['name']);t(x['member']);t(x['directory']);b.extend(bytes.fromhex(x['closure_sha256']))
 u(len(d['getters']))
 for x in d['getters']:t(x['method']);u(x['role'],x['action'],int(x['truthiness']));t(x['mutation']);t(x['warning'])
 u(len(d['sources']))
 for path,digest in d['sources'].items():t(path);b.extend(bytes.fromhex(digest))
 u(len(d['records']))
 for x in d['records']:
  u(x['role']);t(x['source']);t(x['name']);b.extend(bytes.fromhex(x['sha256']));u(int('value'in x))
  if 'value'in x:val(x['value'])
 struct.pack_into('<8s8I',b,0,b'ENCYAML1',1,128,len(b),0,FAMILY,1,1,d['scene_id']);b[40:60]=bytes.fromhex(PIN);b[60:92]=bytes.fromhex(d['source_sha256']);b[92:124]=bytes.fromhex(sha(IR));struct.pack_into('<I',b,20,zlib.crc32(b[128:]));return bytes(b)

def main():
 p=argparse.ArgumentParser();p.add_argument('action',choices=['prepare','extract','compile','verify']);p.add_argument('--directory',type=Path);p.add_argument('--native',type=Path);a=p.parse_args()
 if a.action=='prepare':require(a.directory,'Explicit private export directory required');prepare(a.directory);return
 if a.action=='extract':require(a.native,'Explicit native parser source receipt required');extract(a.native);return
 b=encode(load())
 if a.action=='compile':PACK.parent.mkdir(parents=True,exist_ok=True);PACK.write_bytes(b)
 else:require(PACK.read_bytes()==b,'Stale cache resource')
 print('Complete source cache manifest:',len(load()['records']),'files;',len(b),'bytes; actual Directory cursors still required')
if __name__=='__main__':
 try:main()
 except(ValueError,KeyError,TypeError,OSError,struct.error,StopIteration)as e:sys.exit('GLOBAL YAML CACHE ERROR: '+str(e))
