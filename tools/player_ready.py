#!/usr/bin/env python3
"""Reviewed Player Ready and existing 0056 animation graph execution projection."""
from __future__ import annotations
import argparse, hashlib, json, re, struct, sys, zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import PIN,read,write,sha,require
FAMILY=0x454e005a
IR=ROOT/'content/native-player-ready.json';REVIEW=ROOT/'reports/player-ready/source-review.json';PACK=ROOT/'romfs/data/player.encready'
INIT=ROOT/'content/native-player-initialization.json'
REVIEWED_METHODS={'Scripts/Main/party/party_object.gd#_ready': 'ff49d11e8d4ea4a3e89bfb031766273e1d5e6a436c766fab50667024959092d7', 'Scripts/Main/party/party_object.gd#update_party_member': 'c480bda79a62a78f1840fd08d301a434d926a62a55709d745f9166e177530168', 'Scripts/Main/party/party_object.gd#_refresh_party_member_connections': '3ba5ddc23a1fe3294d4ddad1018d08302b231136ce1045a51acc89480440739c', 'Scripts/Main/party/party_object.gd#_refresh_status': '8d8a8e55d3134b80febb1e7e846af69744d09ef0befc474ff076b3a78aed6d17', 'Scripts/Main/party/party_object.gd#blend_animation': '10a5533e52062cb459080551b925d3f3c06ef823f8855cfbec1b45c13f0e8fd1', 'Scripts/Main/party/party_object.gd#set_anim_state': '84eba2c47c862dec34c7b30b3ed0b5c15b2cc1d65bc3b6a6dbaed66a68e1e3be', 'Scripts/Main/party/Player.gd#_ready': '5173fa6c14083ccb2604d33ea55d8b4aaba997bbb20e657ecedbbaf2a374ff7a', 'Scripts/Main/party/Player.gd#update_party_member': '6b8f493c1dff1a2bcd23ab90080d729869382be45f05cb3ad25574873032c3a5', 'Scripts/Main/party/Player.gd#spritesheet': '61d5539b414d33e56631dd51bc225787fe7fb6d371f962ac19f8d2000d052dd5', 'Scripts/Main/party/Player.gd#blend_position': '6570ea47862ea70e129f822d0823e7b482fd753d0df3db14010cc635d397b0bb'}
BASE='Scripts/Main/party/party_object.gd';PLAYER='Scripts/Main/party/Player.gd';GLOBAL='Scripts/global/global.gd'
KEYS='party_member steps tap_run crouch direction last_step run_sound costume party_signal update_method status_signal refresh_method sweat_effect shadow_animation sweat_path sprite_path special_path camera_path tree_path animation_path normal_format special_format snow_format fallback_texture snow_costume footsteps_format footsteps_voice blend_format climbing'.split()
def body(text,name):
 m=re.search(r'^func '+re.escape(name)+r'\([^\n]*\)[^\n]*:\n(.*?)(?=^func |\Z)',text,re.M|re.S);require(m,'Missing reviewed Player method '+name);return m[0].rstrip()
def scalar(v):
 if isinstance(v,dict):
  require(v.get('type')in('int64','real'),'Unsupported numeric native tag');return float(v['value'])if v['type']=='real'else int(v['value'])
 return v
def vec(v):
 require(v['type']=='Vector2','Expected source Vector2');return [scalar(v['x']),scalar(v['y'])]
def derive():
 d=read(INIT);require(d['commit']==PIN and d['family']==0x454e0056,'Player source dependency changed')
 source={p:(ROOT/'upstream/MOTHER-Encore'/p).read_text(encoding='utf-8-sig')for p in(BASE,PLAYER,GLOBAL)}
 methods={BASE:[' _ready'.strip(),'update_party_member','_refresh_party_member_connections','_refresh_status','blend_animation','set_anim_state'],PLAYER:['_ready','update_party_member','spritesheet','blend_position']}
 proofs=[]
 for p,names in methods.items():
  for name in names:proofs.append(dict(source=p,method=name,sha256=hashlib.sha256(body(source[p],name).encode()).hexdigest()))
 require({x['source']+'#'+x['method']:x['sha256']for x in proofs}==REVIEWED_METHODS,'Unknown Player Ready method; semantic review required')
 b=source[BASE];p=source[PLAYER];ready=body(p,'_ready');refresh=body(b,'_refresh_status');update=body(p,'update_party_member');sprite=body(p,'spritesheet');blend=body(p,'blend_position')
 require('global.currentCamera = camera' in ready and 'for i in global.partySpace.size():'in ready and 'global.partySpace.push_front(position)\n\t\tglobal.partySpace.pop_back()'in ready,'Unknown Player Ready array semantics')
 require('global.connect("party_changed", self, "update_party_member")'in body(b,'_ready'),'Unknown base Ready')
 names=dict(climbing=re.search(r'if !(\w+):',body(b,'set_anim_state'))[1],party_member=re.search(r'\t(\w+) = global.party\[0\]',update)[1],steps=re.search(r'\t(\w+) = 0',refresh)[1],tap_run=re.search(r'\t(\w+) = false\n\t_crouch',ready)[1],crouch=re.search(r'\t(\w+) = false\n\t_direction',ready)[1],direction=re.search(r'\t(\w+) = Vector2',ready)[1],last_step=re.search(r'\t(\w+) = position',ready)[1],run_sound=re.search(r'% (\w+)\)',ready)[1],costume=re.search(r'if (\w+) == "Snow"',sprite)[1],party_signal='party_changed',update_method='update_party_member',status_signal='status_changed',refresh_method='_refresh_status',sweat_effect=re.search(r'get_combined_status_effect\("([^"\n]+)"',refresh)[1],shadow_animation=re.search(r'set_shadow\("([^"\n]+)"',ready)[1],sweat_path=re.search(r'\$([^ .\n]+)\.playing',refresh)[1],sprite_path=next(x['path']for x in d['onready']if x['name']=='sprite'),special_path=next(x['path']for x in d['onready']if x['name']=='_special'),camera_path=next(x['path']for x in d['onready']if x['name']=='camera'),tree_path=next(x['path']for x in d['onready']if x['name']=='_anim_tree'),animation_path=next(x['path']for x in d['onready']if x['name']=='_anim_player'),normal_format=re.search(r'normal_texture: String = "([^"\n]+)"',sprite)[1],special_format=re.search(r'special_texture: String = "([^"\n]+)"',sprite)[1],snow_format=re.search(r'snow_texture: String = "([^"\n]+)"',sprite)[1],fallback_texture=re.findall(r'sprite.texture = ResourceLoader.load\("([^"\n]+)"',sprite)[0],snow_costume='Snow',footsteps_format=re.search(r'load\("([^"\n]+)" % run_sound',ready)[1],footsteps_voice=re.search(r'add_sfx\(sfx, "([^"\n]+)"',ready)[1],blend_format=re.search(r'_anim_tree.set\("([^"\n]+)" % animation',b)[1])
 entries=re.search(r'for param in (\[[^\n]+\]):',blend);require(entries,'Unknown Ready blend list');blend_names=json.loads(entries[1])
 direction=[int(x)for x in re.search(r'_direction = Vector2\((\d+),(\d+)\)',ready).groups()]
 resize=int(re.search(r'partySpace.resize\((\d+)\)',source[GLOBAL])[1]);height_divisor=int(re.search(r'texture.get_height\(\)/(\d+) \+ (\d+)' ,sprite)[1]);height_offset=int(re.search(r'texture.get_height\(\)/(\d+) \+ (\d+)' ,sprite)[2])
 n=d['native_snapshot'];resources={x['id']:x for x in n['resources']};props=next(x['properties']for x in n['nodes']if x['path']==names['tree_path']);machine=resources[props['tree_root']['id']];require(machine['class']=='AnimationNodeStateMachine','Unknown source tree root');mp=machine['properties'];states=[]
 ap=next(x['properties']for x in n['nodes']if x['path']==names['animation_path'])
 for key,value in mp.items():
  if not key.startswith('states/')or not key.endswith('/node'):continue
  name=key[7:-5];node=resources[value['id']];bp=name;scale_key='';scale=1
  if node['class']=='AnimationNodeBlendTree':
   q=node['properties'];connections=q['node_connections']['value'];require(len(connections)==6 and scalar(connections[1])==scalar(connections[4])==0 and connections[3]=='output'and connections[5]==connections[0],'Unsupported Player BlendTree topology')
   scaler=resources[q['nodes/'+connections[0]+'/node']['id']];require(scaler['class']=='AnimationNodeTimeScale','Unsupported Player scale node')
   bp=name+'/'+connections[2];scale_key='parameters/'+name+'/'+connections[0]+'/scale';scale=scalar(props[scale_key]);node=resources[q['nodes/'+connections[2]+'/node']['id']]
  if node['class']=='AnimationNodeBlendSpace2D':
   q=node['properties'];require(scalar(q['blend_mode'])==1,'Only actual discrete Player BlendSpace supported');points=[]
   for i in range(64):
    k='blend_point_'+str(i)+'/node'
    if k not in q:break
    anim=resources[q[k]['id']];require(anim['class']=='AnimationNodeAnimation'and anim['properties']['filter_enabled']is False and not anim['properties']['filters']['value'],'Unknown filtered graph leaf')
    clip=anim['properties']['animation'];clipid=ap['anims/'+clip]['id'];ar=resources[clipid];require(ar['class']=='Animation','Unknown source clip')
    points.append(dict(position=vec(q['blend_point_'+str(i)+'/pos']),clip=clip,node_id=anim['id'],clip_id=clipid,length=scalar(ar['properties']['length']),loop=ar['properties']['loop']))
   param='parameters/'+bp+'/blend_position';initial=vec(props[param])
  elif node['class']=='AnimationNodeAnimation':
   clip=node['properties']['animation'];clipid=ap['anims/'+clip]['id'];ar=resources[clipid];param='';initial=[0,0];points=[dict(position=[0,0],clip=clip,node_id=node['id'],clip_id=clipid,length=scalar(ar['properties']['length']),loop=ar['properties']['loop'])]
  else:raise ValueError('Unsupported Player graph leaf '+node['class'])
  states.append(dict(name=name,position=vec(mp['states/'+name+'/position']),parameter=param,initial=initial,scale_parameter=scale_key,scale=scale,points=points))
 edges=[];v=mp['transitions']['value'];require(len(v)%3==0,'Malformed native transitions')
 for i in range(0,len(v),3):
  q=resources[v[i+2]['id']]['properties'];require(scalar(q['xfade_time'])==0 and not q['advance_condition']and scalar(q['switch_mode'])in(0,2),'Unsupported transition crossfade/condition/mode');edges.append(dict(source=v[i],target=v[i+1],mode=scalar(q['switch_mode']),priority=scalar(q['priority']),auto=q['auto_advance'],disabled=q['disabled']))
 require(mp['end_node']=='','Source end node policy changed');require(all(x in [s['name']for s in states]for x in(mp['start_node'],)),'Unknown native start state')
 rule_body=body(b,'set_anim_state');rules=[]
 for names_row,target in re.findall(r'\t\t\t\t((?:"[^"]+"(?:, )?)+):\n\t\t\t\t\tstate_name = "([^"]+)"',rule_body):
  rules.extend([[v,target]for v in re.findall(r'"([^"]+)"',names_row)])
 require(rules and '_anim_state.travel(state_name)'in rule_body,'Unknown incapacitated animation rules')
 prefix=re.search(r'state_name = "([^"]+)" \+ state_name',rule_body)[1]
 return dict(schema=1,kind='encore.player-ready.source-ir',commit=PIN,family=FAMILY,initialization_ir_sha256=sha(INIT),sources={x:sha(ROOT/'upstream/MOTHER-Encore'/x)for x in source},method_proofs=proofs,bindings=names,blend_names=blend_names,direction=direction,party_space_size=resize,height_divisor=height_divisor,height_offset=height_offset,start=mp['start_node'],incapacitated_rules=rules,fainted_prefix=prefix,process_mode=scalar(props['process_mode']),playback_resource=props['parameters/playback']['id'],states=states,edges=edges,whole_player_ready=False)
def encode(d):
 b=bytearray(128)
 def u(*v):b.extend(struct.pack('<'+'I'*len(v),*v))
 def f(*v):b.extend(struct.pack('<'+'f'*len(v),*v))
 def text(v):z=v.encode();u(len(z));b.extend(z)
 b.extend(bytes.fromhex(d['initialization_ir_sha256']));u(len(d['sources']))
 for p,h in d['sources'].items():text(p);b.extend(bytes.fromhex(h))
 for k in KEYS:text(d['bindings'][k])
 u(d['party_space_size'],d['height_divisor'],d['height_offset'],d['process_mode'],d['playback_resource']);f(*d['direction']);u(len(d['blend_names']))
 for x in d['blend_names']:text(d['bindings']['blend_format'].replace('%s',x))
 text(d['fainted_prefix']);u(len(d['incapacitated_rules']))
 for a,b_rule in d['incapacitated_rules']:text(a);text(b_rule)
 text(d['start']);u(len(d['states']))
 for s in d['states']:
  text(s['name']);f(*s['position']);text(s['parameter']);f(*s['initial']);text(s['scale_parameter']);f(s['scale']);u(len(s['points']))
  for p in s['points']:f(*p['position']);text(p['clip']);u(p['node_id'],p['clip_id']);f(p['length']);u(p['loop'])
 u(len(d['edges']))
 for e in d['edges']:text(e['source']);text(e['target']);u(e['mode'],e['priority'],e['auto'],e['disabled'])
 struct.pack_into('<8s8I',b,0,b'ENCPRDY1',1,128,len(b),zlib.crc32(b[128:]),FAMILY,1,1,read(INIT)['scene_id']);b[40:60]=bytes.fromhex(PIN);b[60:92]=bytes.fromhex(read(INIT)['source_sha256']);b[92:124]=bytes.fromhex(sha(IR));return bytes(b)
def main():
 p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile','verify']);a=p.parse_args();d=derive()
 if a.action=='extract':write(IR,d);write(REVIEW,dict(schema=1,commit=PIN,ir_sha256=sha(IR),sources=d['sources'],methods=d['method_proofs'],scope='Base then derived actual Ready body, discrete Player graph with zero crossfade; no generic AnimationTree, whole Player/native subtree Ready pending',execution='runtime/player_ready.cpp and runtime/player_ready_animation.cpp',engine_tag='3.6.2-stable',engine_sources={'scene/animation/animation_node_state_machine.cpp':'08cd288491d7d26a878cb648236c0c3634b8c8b380b6ec0f93a13767f5edf15d','scene/animation/animation_blend_space_2d.cpp':'b69dc753037f04c3d81a7a9cecb4c156b0a8cb1dc41f9da3d48478da7134ca71','scene/animation/animation_blend_tree.cpp':'103331401ae49fe2dfc33dcc4724cdac4ab4f8db52b1ad37f84c2006a02e1610','scene/animation/animation_tree.cpp':'879954bec8eca3940381b87382a4e1aa423c759e2de805787064e026ec5176e9'}));return
 require(read(IR)==d and read(REVIEW)['ir_sha256']==sha(IR),'Player Ready source/IR review changed');v=encode(d)
 if a.action=='compile':PACK.parent.mkdir(parents=True,exist_ok=True);PACK.write_bytes(v)
 else:require(PACK.read_bytes()==v,'Stale Player Ready pack')
 print('Player Ready:',len(d['states']),'states;',len(d['edges']),'transitions;',len(v),'bytes; whole Ready pending')
if __name__=='__main__':
 try:main()
 except (ValueError,KeyError,TypeError,OSError,struct.error,AttributeError)as e:sys.exit('PLAYER READY ERROR: '+str(e))
