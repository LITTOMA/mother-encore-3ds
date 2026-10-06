#!/usr/bin/env python3
"""Two original boolean status queries over the real source Status Array."""
import argparse,hashlib,re,struct,sys,zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import PIN,read,write,sha,require,stable
from tools.extract_battle_entry import Extractor
from tools.house_global_bridge import load as bridge_load,IR as BRIDGE
from tools.player_ready import derive as ready_derive,IR as READY
IR=ROOT/'content/native-house-status-effects.json';REVIEW=ROOT/'reports/house-status-effects/source-review.json';PACK=ROOT/'romfs/data/house.encstatuseffects'
FAMILY=0x454e0062
CHAR='Scripts/global/Character.gd'
METHODS={'get_status_effects': '5a58fc88e8d04e859cce84a28ae2c1dab1de47d8a0bc66196ac52d0ce714c46e', '_pick_status_effects': 'c9ee09eaef0e47b33844746aae30fb2cd7f1aa826f01a98a90f74b8451142fc7', 'get_combined_status_effect': 'f0f8f7c7fd6c91856d8c78e49c685f71a4f8dabf4c1807ba914226efac272988', 'is_incapacitated': '27244338738bd3de7228df957533711791f67715540d1a5fc6c84081b8056238'}

def derive():
 bridge=bridge_load();ready=ready_derive();require(ready==read(READY),'Player Ready dependency stale')
 ex=Extractor(ROOT);source=ex.text(CHAR)
 for n,h in METHODS.items():
  match=re.search(r'^func '+n+r'\([^\n]*\)[^\n]*:\n(.*?)(?=^func |\Z)',source,re.M|re.S)
  require(match and hashlib.sha256(match[0].rstrip().encode()).hexdigest()==h,'Changed reviewed Character status method '+n)
 require('combined_effect = combined_effect or effects[i]'in source,'Unknown boolean combination')
 effect_key=re.search(r'!ailment_info.has\("([^"\n]+)"\)',source)[1]
 any_key=re.search(r'\t\t\t"([^"\n]+)":\n\t\t\t\tcondition = true',source)[1]
 incap=re.search(r'func is_incapacitated\(\) -> bool:\n\treturn get_combined_status_effect\("([^"\n]+)"\)',source)[1]
 boolean_line=re.search(r'\t\t((?:"[^"\n]+"(?:, )?)+):\n\t\t\tcombined_effect = false',source)[1]
 boolean_names=re.findall(r'"([^"\n]+)"',boolean_line)
 sweat=ready['bindings']['sweat_effect'];require(sweat in boolean_names and incap in boolean_names,'Unknown Ready boolean query')
 policies=[]
 for p in bridge['status']['statuses']:
  raw=ex.yaml(p['source']);require(ex.sources[p['source']]==p['source_sha256'],'Status dependency SHA differs')
  cases=raw[effect_key];require(list(cases)==[any_key],'Unreviewed status case requires a new typed selector')
  require(all(k not in cases[any_key]or type(cases[any_key][k])is bool for k in (sweat,incap)),'Unsupported boolean status effect type')
  policies.append(dict(id=p['id'],source=p['source'],source_sha256=p['source_sha256'],data=raw))
 require([p['id']for p in policies]==['asthma'],'Unreviewed status scope')
 for path in [bridge['status']['script'],ready['sources'].keys().__iter__().__next__()]:ex.data(path)
 return dict(schema=1,format=1,capability=1,rules=1,family=FAMILY,commit=PIN,scene_id=stable('native-house-status-effects'),owner=CHAR,sources=ex.sources,dependencies=dict(bridge=sha(BRIDGE),ready=sha(READY)),methods=METHODS,bindings=dict(effects=effect_key,any_case=any_key,sweat=sweat,incapacitated=incap),statuses=policies,scope='Actual saved House asthma Node owners and their complete original cache Dictionary. Only source boolean OR sweat/incap queries; no battle status executor or Ready receipt.')

def load():
 d=read(IR);require(d==derive(),'Status effects IR/dependencies changed');v=read(REVIEW);require(v['ir_sha256']==sha(IR)and v['sources']==d['sources'],'Status effects review stale');return d

def encode(d):
 b=bytearray(128)
 def u(*v):b.extend(struct.pack('<'+'I'*len(v),*v))
 def t(s):raw=s.encode();u(len(raw));b.extend(raw)
 def value(v):
  if v is None:u(0)
  elif type(v)is bool:u(1,int(v))
  elif type(v)is int:u(2);b.extend(struct.pack('<q',v))
  elif type(v)is str:u(4);t(v)
  elif type(v)is list:u(5,len(v));[value(x)for x in v]
  elif type(v)is dict:
   u(6,len(v))
   for k,x in v.items():t(k);value(x)
  else:raise ValueError('Unknown source YAML value')
 for k in ('bridge','ready'):b.extend(bytes.fromhex(d['dependencies'][k]))
 for k in ('effects','any_case','sweat','incapacitated'):t(d['bindings'][k])
 u(len(d['statuses']))
 for s in d['statuses']:t(s['id']);t(s['source']);b.extend(bytes.fromhex(s['source_sha256']));value(s['data'])
 u(len(d['sources']))
 for p,h in d['sources'].items():t(p);b.extend(bytes.fromhex(h))
 struct.pack_into('<8s8I',b,0,b'ENCHSE01',1,128,len(b),zlib.crc32(b[128:]),FAMILY,1,1,d['scene_id']);b[40:60]=bytes.fromhex(PIN);b[60:92]=bytes.fromhex(d['sources'][CHAR]);b[92:124]=bytes.fromhex(sha(IR));return bytes(b)

def stage_files(root):
 d=load();p=Path('data/house.encstatuseffects');raw=(Path(root)/p).read_bytes();require(raw==encode(d),'Stale staged status effects');return {p:raw}

def main():
 p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile','verify']);a=p.parse_args()
 if a.action=='extract':d=derive();write(IR,d);write(REVIEW,dict(schema=1,commit=PIN,ir_sha256=sha(IR),sources=d['sources'],methods=d['methods'],scope=d['scope']));return
 d=load();raw=encode(d)
 if a.action=='compile':PACK.parent.mkdir(parents=True,exist_ok=True);PACK.write_bytes(raw)
 else:require(PACK.read_bytes()==raw,'Stale status effects pack')
 print('Actual House Status boolean queries:',len(raw),'bytes')
if __name__=='__main__':
 try:main()
 except(ValueError,KeyError,TypeError,OSError,struct.error)as e:sys.exit('HOUSE STATUS EFFECTS ERROR: '+str(e))
