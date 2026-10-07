#!/usr/bin/env python3
"""Pinned bus layout/settings and reviewed native AudioServer semantics."""
from pathlib import Path
import argparse, hashlib, json, re, struct, sys, zlib
ROOT=Path(__file__).resolve().parents[1]
PIN='7d9246600fffe518408f5830d4848635019005a3'
FAMILY=0x454e006e
IR=ROOT/'content/native-audio-server.json'
REVIEW=ROOT/'reports/audio-server/source-review.json'
PACK=ROOT/'romfs/data/audio-server.encbuses'
LAYOUT='Audio/Audio Buses/default_bus_layout.tres'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def require(v,m):
 if not v:raise ValueError(m)
def write(p,d):
 p.parent.mkdir(parents=True,exist_ok=True);p.write_text(json.dumps(d,indent=2,ensure_ascii=False)+'\n',encoding='utf8')
def extract(engine):
 up=ROOT/'upstream/MOTHER-Encore';cpp=Path(engine)/'servers-audio_server.cpp';hpp=Path(engine)/'servers-audio_server.h'
 c=cpp.read_text(encoding='utf8');h=hpp.read_text(encoding='utf8')
 require('bus->name = "Master";'in c and 'send->index_cache >= bus->index_cache'in c and 'E->get().callback(E->get().userdata)'in c,'Reviewed native route/mixing changed')
 require('callback == p_item.callback' in h or 'callback == p_other.callback' in h or 'callback == p_callback.callback' in h or 'callback == p_cb.callback' in h or 'callback == p_ci.callback' in h,'Reviewed native callback comparison changed')
 layout=(up/LAYOUT).read_text(encoding='utf8');project=(up/'project.godot').read_text(encoding='utf8');globalgd=(up/'Scripts/global/global.gd').read_text(encoding='utf8')
 require('default_bus_layout="res://'+LAYOUT+'"'in project,'Project source bus binding changed')
 buses={};effect={};section='';known_effect=False
 for line in layout.splitlines():
  line=line.strip()
  if not line:continue
  if line.startswith('['):
   section=line;continue
  key,value=[v.strip()for v in line.split('=',1)]
  if section.startswith('[sub_resource'):
   require(section=='[sub_resource type="AudioEffectFilter" id=2]','Unknown source effect class')
   require(key in ('resource_name','cutoff_hz','resonance'),'Unknown source effect field')
   effect[key]=json.loads(value);known_effect=True;continue
  require(section=='[resource]','Unexpected source layout section')
  m=re.fullmatch(r'bus/(\d+)/(name|solo|mute|bypass_fx|volume_db|send|effect/0/effect|effect/0/enabled)',key);require(m,'Unknown source bus property '+key)
  i=int(m[1]);k=m[2];b=buses.setdefault(i,dict(name='',send='',volume_db=0.0,solo=False,mute=False,bypass_fx=False,effects=[]))
  if k=='effect/0/effect':require(value=='SubResource( 2 )','Unknown source effect binding');b['effects']=[dict(id=2,native_class='AudioEffectFilter',enabled=False)]
  elif k=='effect/0/enabled':require(b['effects'],'Effect property source order');b['effects'][0]['enabled']=json.loads(value)
  else:b[k]=json.loads(value)
 require(sorted(buses)==list(range(len(buses)))and known_effect,'Source bus indices/effect closure')
 buses[0]['name']='Master';buses[0]['send']=''
 for b in buses.values():
  for fx in b['effects']:fx.update(effect)
 settings=[]
 for method,member,bus,signal in re.findall(r'func (set_\w+_volume)\(volume: int\):\s*globaldata\.(\w+) = volume\s*AudioServer\.set_bus_volume_db\(AudioServer\.get_bus_index\("([^"]+)"\), volume\)\s*global\.emit_signal\("([^"]+)"\)',globalgd):
  settings.append(dict(method=method,member=member,bus=bus,signal=signal))
 require(len(settings)==3,'Source setting setter closure changed')
 player=(up/'Nodes/Reusables/Player.tscn').read_text(encoding='utf8')
 sources={p:sha(up/p)for p in [LAYOUT,'project.godot','Scripts/global/global.gd','Nodes/Reusables/Player.tscn']}
 # Native Node connects this signal/method; these are execution names from
 # reviewed engine source, not new game content embedded in the consumer.
 asp=Path(engine)/'audio_stream_player.cpp';a=asp.read_text(encoding='utf8')
 require('"bus_layout_changed"'in a and '"_bus_layout_changed"'in a,'Source native layout connection changed')
 engines={'servers/audio_server.cpp':dict(sha256=sha(cpp),url='https://github.com/godotengine/godot/blob/3.6.2-stable/servers/audio_server.cpp'),'servers/audio_server.h':dict(sha256=sha(hpp),url='https://github.com/godotengine/godot/blob/3.6.2-stable/servers/audio_server.h'),'scene/audio/audio_stream_player.cpp':dict(sha256=sha(asp),url='https://github.com/godotengine/godot/blob/3.6.2-stable/scene/audio/audio_stream_player.cpp')}
 d=dict(schema=1,format=1,family=FAMILY,capability=1,rules=1,commit=PIN,engine_source='servers/audio_server.cpp',source_sha256=sha(cpp),source_id=int.from_bytes(hashlib.sha256(('AudioServer:'+LAYOUT).encode()).digest()[:4],'little'),layout_signal='bus_layout_changed',layout_method='_bus_layout_changed',buses=list(buses.values()),settings=settings,sources=sources,engine=engines,unsupported_at_use=['AudioEffectFilter DSP processing'],source_ready_granted=False)
 write(IR,d);write(REVIEW,dict(schema=1,commit=PIN,ir_sha256=sha(IR),sources=sources,engine=engines,semantics=['native callback/function userdata ordered Set','set_bus_layout/set_bus_volume_db do not emit bus_layout_changed','native forward/self-send fallback to bus0','actual scalar mute/solo/send route; effect execution refuses until implemented'],source_ready_granted=False));return d
def load():
 d=json.loads(IR.read_text(encoding='utf8'));r=json.loads(REVIEW.read_text(encoding='utf8'))
 require(d['commit']==PIN and d['family']==FAMILY and d['format']==d['capability']==d['rules']==1 and r['ir_sha256']==sha(IR),'AudioServer review/version mismatch')
 require(d['sources']==r['sources']and d['engine']==r['engine'],'AudioServer source review mismatch')
 for p,h in d['sources'].items():require(sha(ROOT/'upstream/MOTHER-Encore'/p)==h,'AudioServer source changed '+p)
 return d
def encode(d):
 b=bytearray(128)
 def u(*v):b.extend(struct.pack('<'+'I'*len(v),*v))
 def t(s):v=s.encode();u(len(v));b.extend(v)
 def f(v):b.extend(struct.pack('<f',v))
 t(d['engine_source']);t(d['layout_signal']);t(d['layout_method']);proof={**d['sources'],**{p:v['sha256']for p,v in d['engine'].items()}};u(len(proof))
 for p,h in proof.items():t(p);b.extend(bytes.fromhex(h))
 u(len(d['buses']))
 for x in d['buses']:
  t(x['name']);t(x['send']);f(x['volume_db']);u(x['solo'],x['mute'],x['bypass_fx'],len(x['effects']))
  for fx in x['effects']:u(fx['id']);t(fx['native_class']);t(fx['resource_name']);f(fx['cutoff_hz']);f(fx['resonance']);u(fx['enabled'])
 u(len(d['settings']))
 for x in d['settings']:
  for k in ('method','member','bus','signal'):t(x[k])
 struct.pack_into('<8s8I',b,0,b'ENCBUS01',1,128,len(b),zlib.crc32(b[128:]),FAMILY,1,1,d['source_id']);b[40:60]=bytes.fromhex(PIN);b[60:92]=bytes.fromhex(d['source_sha256']);b[92:124]=bytes.fromhex(sha(IR));return bytes(b)
def stage_files(root):
 d=load();b=encode(d);require((Path(root)/'data/audio-server.encbuses').read_bytes()==b,'AudioServer generated pack changed');return {Path('data/audio-server.encbuses'):b}
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile']);p.add_argument('--engine',type=Path);a=p.parse_args()
 try:
  d=extract(a.engine)if a.action=='extract'else load();PACK.parent.mkdir(parents=True,exist_ok=True);PACK.write_bytes(encode(d));print('AudioServer checked buses:',len(d['buses']),'; settings:',len(d['settings']),'; no Node/Ready grant')
 except(ValueError,KeyError,OSError,TypeError,struct.error)as e:sys.exit('AUDIO SERVER ERROR: '+str(e))
