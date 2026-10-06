#!/usr/bin/env python3
"""Original globaldata boolean dictionaries; a save projection, not a full save."""
from __future__ import annotations
import argparse,json,re,struct,sys,zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import PIN,require,read,sha,write
from tools.extract_battle_entry import Extractor
IR=ROOT/'content/native-global-flags.json'
REVIEW=ROOT/'compatibility/reviews/native-global-flags-v0410.json'
PACK=ROOT/'romfs/data/global.encflags'
FAMILY=0x454e0047

def extract():
 ex=Extractor(ROOT);script=ex.text('Scripts/global/globalData.gd');glob=ex.text('Scripts/global/global.gd')
 block=script.split('func _init_flags():\n',1)[1].split('\nfunc _load_data',1)[0]
 require(block.endswith('\tfor flag in flag_names:\n\t\tflags[flag] = false\n'),'Flags constructor unknown execution')
 declaration=block.split('var flag_names := [',1)[1].split('\n\t]',1)[0]
 clean='\n'.join(line.split('#',1)[0]for line in declaration.splitlines());names=re.findall(r'"([a-z0-9_]+)"',clean)
 require(len(names)==len(set(names)) and len(names)>100 and not re.sub(r'"[a-z0-9_]+"|[\s,]','',clean),'Flags declaration grammar/duplicate')
 require('var object_flags := {}'in script and 'var seen_dialogue_flags := {}'in script,'Flags empty constructor dictionaries')
 require('func set_flag(flag_name: String, value: bool, emit_signal := true):\n\tif flags.has(flag_name):\n\t\tflags[flag_name] = value\n\t\tif emit_signal: global.emit_signal("flags_updated")'in script,'Normal flag setter changed')
 require('func set_object_flag(flag_name: String, value: bool, emit_signal := true):\n\tobject_flags[flag_name] = value\n\tif emit_signal: global.emit_signal("flags_updated")'in script,'Object flag setter changed')
 require('for flag in globaldata.flags:\n\t\tglobaldata.flags[flag] = save_data.get("flags", {}).get(flag, false)'in glob,'Save normal registered flag projection changed')
 for n in ('object_flags','seen_dialogue_flags'):require('globaldata.'+n+' = save_data.get("'+n+'", {})'in glob,'Save dictionary replacement changed '+n)
 profiles=[]
 overrides=ex.yaml('Data/save_overrides.yaml')
 require(not set(overrides)&{'flags','object_flags','seen_dialogue_flags'},'Overrides flag semantics require a new review')
 for source in ('Data/save_new_game.yaml','Data/save_default.yaml'):
  d=ex.yaml(source);maps=[d.get(k,{})for k in ('flags','object_flags','seen_dialogue_flags')]
  require(all(type(m)is dict and all(type(k)is str and type(v)is bool for k,v in m.items())for m in maps),'Nonboolean source save flag projection')
  profiles.append(dict(source=source,normal=[bool(maps[0].get(n,False))for n in names],objects=list(maps[1].items()),seen=list(maps[2].items())))
 # These consumers write actual path-qualified seen keys, never a source ID.
 for source in ('Scripts/Main/npc.gd','Scripts/Main/door_npc.gd'):
  text=ex.text(source);require('globaldata.seen_dialogue_flags[last_dialog_hash] = true'in text,'Seen dialogue mutation changed')
 d=dict(schema=1,kind='encore.global-flags.source-ir',commit=PIN,family=FAMILY,owner='Scripts/global/globalData.gd',sources=ex.sources,registered=[dict(name=n,initial=False)for n in names],profiles=profiles,policy=dict(normal_set='registered-only',object_set='insert',source_load='replace-dynamic-project-registered',seen_set='actual-dialogue-key',emit='after-write-even-unchanged'),scope=['All original registered flags, including source unfinished future-area declarations; declaring a flag does not admit that area','Only boolean flags/object_flags/seen_dialogue_flags save projection; no scene/party/inventory/save-slot schema replacement','Source setter unknown normal key is a known no-op; dynamic object and actual path-qualified seen keys are preserved','Flag emission belongs to the real global signal owner; this consumer never approves global or globaldata Ready'])
 validate(d);write(IR,d);write(REVIEW,dict(schema=1,commit=PIN,ir_sha256=sha(IR),sources=d['sources'],scope=d['scope']));return d

def validate(d):
 require(set(d)=={'schema','kind','commit','family','owner','sources','registered','profiles','policy','scope'}and d['schema']==1 and d['kind']=='encore.global-flags.source-ir'and d['commit']==PIN and d['family']==FAMILY and d['owner']=='Scripts/global/globalData.gd','Global flags schema/pin')
 require(d['policy']==dict(normal_set='registered-only',object_set='insert',source_load='replace-dynamic-project-registered',seen_set='actual-dialogue-key',emit='after-write-even-unchanged'),'Unsupported flag policy')
 require(0<len(d['registered'])<=4096 and len(d['profiles'])==2,'Flags source bounds')
 names=set()
 for row in d['registered']:
  require(set(row)=={'name','initial'}and type(row['initial'])is bool and re.fullmatch('[a-z0-9_]+',row['name'])and row['name']not in names,'Flags registered declaration');names.add(row['name'])
 for p in d['profiles']:
  require(set(p)=={'source','normal','objects','seen'}and p['source']in('Data/save_new_game.yaml','Data/save_default.yaml')and len(p['normal'])==len(names)and all(type(v)is bool for v in p['normal']),'Source flags profile')
  for name in ('objects','seen'):
   keys=set()
   for k,v in p[name]:require(type(k)is str and '\0'not in k and len(k.encode())<=4096 and k not in keys and type(v)is bool,'Dynamic source flags');keys.add(k)
 require(len({p['source']for p in d['profiles']})==2,'Duplicate source profile')
 return d

def load():
 d=validate(read(IR));review=read(REVIEW);require(review['schema']==1 and review['commit']==PIN and review['ir_sha256']==sha(IR) and review['sources']==d['sources'],'Global flags semantic review differs')
 inv=read(ROOT/'compatibility/upstream-inventory.json');require(inv['commit']==PIN,'Flags source inventory pin')
 for p,h in d['sources'].items():require(inv['files'][p]['sha256']==h==sha(ROOT/'upstream/MOTHER-Encore'/p),'Changed flag source '+p)
 return d

def encode(d):
 validate(d);body=bytearray(bytes.fromhex(PIN)+bytes.fromhex(sha(IR)))
 def u(v):body.extend(struct.pack('<I',v))
 def s(v):raw=v.encode();u(len(raw));body.extend(raw)
 s(d['owner'])
 u(len(d['registered']));u(len(d['profiles']));u(len(d['sources']))
 for row in d['registered']:s(row['name']);u(int(row['initial']))
 for p in d['profiles']:
  s(p['source'])
  for v in p['normal']:u(int(v))
  for name in ('objects','seen'):
   u(len(p[name]))
   for k,v in p[name]:s(k);u(int(v))
 for p,h in d['sources'].items():s(p);body.extend(bytes.fromhex(h))
 return struct.pack('<8s6I',b'ENCFGS01',1,32+len(body),zlib.crc32(body),FAMILY,1,1)+body

def stage_files(source):
 raw=encode(load());p=Path('data/global.encflags');require((Path(source)/p).read_bytes()==raw,'Stale flags binary');return{p:raw}

if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile']);a=p.parse_args()
 try:
  if a.action=='extract':print('Original registered flags:',len(extract()['registered']))
  else:PACK.write_bytes(encode(load()));print('Global flags binary:',PACK.stat().st_size)
 except(ValueError,KeyError,TypeError,OSError,struct.error)as e:sys.exit('GLOBAL FLAGS ERROR: '+str(e))
