#!/usr/bin/env python3
"""Pinned House Ladder source/data slice; no scene entry or gameplay probe."""
from __future__ import annotations
import argparse, hashlib, re, struct, sys, zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import PIN,read,write,sha,require,stable
def one(pattern,text,label,flags=re.M):
 rows=list(re.finditer(pattern,text,flags));require(len(rows)==1,'Missing/ambiguous Ladder source selector: '+label);return rows[0]
class Extractor:
 def __init__(self,root):self.root=root;self.sources={};self.inventory=read(root/'compatibility/upstream-inventory.json')['files']
 def text(self,path):
  raw=(self.root/'upstream/MOTHER-Encore'/path).read_bytes();h=hashlib.sha256(raw).hexdigest();require(path in self.inventory and h==self.inventory[path]['sha256'],'Pinned Ladder source changed '+path);self.sources[path]=h;return raw.decode('utf-8')
SCENE='Maps/podunk/Nintens House.tscn'
SCRIPT='Scripts/Main/Ladder.gd'
BASE='Scripts/Main/party/party_object.gd'
PLAYER='Scripts/Main/party/Player.gd'
FOLLOWER='Scripts/Main/party/party_follower.gd'
FAMILY=0x454e0078
IR=ROOT/'content/native-house-return-ladder.json'
REVIEW=ROOT/'reports/house-return-ladder/source-review.json'
PACK=ROOT/'romfs/data/house-return-ladder.encladder'
DEPS=['content/native-house-node-tree.json','content/native-house-geometry.json','content/native-player-initialization.json','content/native-player-ready.json','content/native-player-motion.json']
def method(text,name):
 m=one(r'^func '+re.escape(name)+r'\(([^\n]*)\):\n(.*?)(?=^(?:func |# Overrid)|\Z)',text,name,re.M|re.S)
 return m[2].strip()
def derive():
 ex=Extractor(ROOT);scene=ex.text(SCENE);code=ex.text(SCRIPT);node=ex.text('Nodes/Overworld/Ladder.tscn');base=ex.text(BASE);player=ex.text(PLAYER);follower=ex.text(FOLLOWER)
 require(code.strip()=='extends Area2D\n\n\nfunc _on_Ladder_body_entered(body):\n\tif body is PartyObject:\n\t\tbody.ladder()\n\t\tbody.global_position.x = global_position.x\n\n\nfunc _on_Ladder_body_exited(body):\n\tif body is PartyObject:\n\t\tbody.unladder()','Ladder source body requires new semantic review')
 require(method(base,'ladder')=='_anim_tree.active = false\n\t_anim_player.play("Ladder")\n\t_anim_player.playback_speed = 0\n\t_climbing = true','PartyObject ladder body changed')
 require(method(base,'unladder')=='_anim_tree.active = true\n\t_anim_player.stop()\n\tset_anim_state("Idle")\n\t$Position.position.y = 0\n\t$Shadow.show()\n\t_climbing = false','PartyObject unladder source order changed')
 require(method(player,'ladder')=='.ladder()\n\t_state = MOVE\n\tif audioManager.get_sfx("run") != null:\n\t\taudioManager.get_sfx("run").stop()','Player ladder override changed')
 require(method(player,'unladder')=='.unladder()\n\tif _running:\n\t\t_set_running(true)','Player unladder override changed')
 require(method(follower,'ladder')=='.ladder()\n\t$Shadow.hide()','Follower ladder override changed')
 require(not re.search(r'^func unladder\(',follower,re.M),'Follower unladder override requires implementation')
 require('func set_anim_state(state_name: String):\n\tif !_climbing:' in base,'Unladder climbing guard moved')
 tree=read(ROOT/DEPS[0]);geometry=read(ROOT/DEPS[1]);init=read(ROOT/DEPS[2]);ready=read(ROOT/DEPS[3]);motion=read(ROOT/DEPS[4])
 require(tree['commit']==geometry['commit']==init['commit']==ready['commit']==motion['commit']==PIN,'Ladder dependency pin changed')
 rows=[r for r in tree['records']if r['script']==SCRIPT];require(len(rows)==1 and len(tree['records'])==497,'Complete original House one Ladder required')
 r=rows[0];require(r['script_sha']==ex.sources[SCRIPT] and r['class_index']==tree['classes'].index('Area2D') and r['script_methods']==0,'Ladder structural source callback inventory differs')
 gn=[(i,n)for i,n in enumerate(geometry['nodes'])if n['stable_id']==r['id']];require(len(gn)==1,'Ladder native geometry identity missing');gi,g=gn[0]
 owners=[(i,o)for i,o in enumerate(geometry['owners'])if o['node']==gi];require(len(owners)==1,'Ladder native Area owner missing');oi,o=owners[0]
 require(o['kind']==4 and o['shape_count']==1,'Ladder native area schema changed');sh=geometry['shapes'][o['shape_first']];shapeid=geometry['nodes'][sh['node']]['stable_id']
 require(sh['owner']==oi and tree['records'][0]['id']==r['owner'],'Ladder actual owner/shape identity differs')
 con=[]
 for m in re.finditer(r'^\[connection signal="([^\"]+)" from="\." to="\." method="([^\"]+)"\]$',node,re.M):con.append(dict(signal=m[1],method=m[2],flags=2,arguments=1))
 require(len(con)==2,'Ladder persisted connection closure changed')
 require([c['signal']for c in con]==['body_entered','body_exited'] and [c['method']for c in con]==['_on_Ladder_body_entered','_on_Ladder_body_exited'],'Ladder native signals changed')
 texts=[SCRIPT,BASE,PLAYER,FOLLOWER,con[0]['signal'],con[1]['signal'],con[0]['method'],con[1]['method'],'ladder','unladder',one(r'_anim_player.play\("([^\"]+)"\)',method(base,'ladder'),'Ladder clip')[1],one(r'\$([^\.]+)\.position.y',method(base,'unladder'),'Position')[1],one(r'\$([^\.]+)\.show',method(base,'unladder'),'Shadow')[1],one(r'(_climbing) = true',method(base,'ladder'),'climbing field')[1],one(r'(_anim_tree)\.active',method(base,'ladder'),'tree member')[1],one(r'(_anim_player)\.play\(',method(base,'ladder'),'animation member')[1],re.findall(r'get_sfx\("([^\"]+)"\)',method(player,'ladder'))[0],one(r'set_anim_state\("([^\"]+)"\)',method(base,'unladder'),'Idle animation')[1],'global_position','playback_speed']
 require(motion['fields'][11]==texts[13] and motion['text'][0]==texts[16],'Ladder Player motion fields differ')
 nonparty=[]
 for body in tree['records']:
  native=tree['classes'][body['class_index']]
  if native not in ['StaticBody2D','KinematicBody2D','RigidBody2D']:continue
  if body['script']:
   text=ex.text(body['script']);parent=one(r'^extends ([A-Za-z_][A-Za-z_0-9]*)$',text,'House body native inheritance')[1]
   require(parent==native and body['script_sha']==ex.sources[body['script']],'House body PartyObject type guard needs new inheritance consumer')
  nonparty.append(body['id'])
 for f in [SCENE,SCRIPT]:require(tree['sources'][f]==ex.sources[f],'Ladder tree source differs')
 for f in [BASE,PLAYER]:require(init['sources'][f]==ex.sources[f],'Ladder receiver source differs')
 return dict(schema=1,format=1,family=FAMILY,capability=1,rules=1,commit=PIN,scene=SCENE,scene_id=stable('house-return-ladder:'+SCENE),source_sha256=ex.sources[SCENE],sources=ex.sources,non_party_body_ids=nonparty,dependencies={p:sha(ROOT/p)for p in DEPS},player_initialization_ir_sha256=sha(ROOT/DEPS[2]),player_ready_ir_sha256=sha(ROOT/DEPS[3]),player_motion_ir_sha256=sha(ROOT/DEPS[4]),node=r['node'],id=r['id'],shape_id=shapeid,geometry_owner_index=oi,geometry_shape_index=o['shape_first'],layer=o['layer'],mask=o['mask'],owner_flags=o['flags'],shape_flags=sh['flags'],local=r['local'],world=r['world'],connections=con,texts=texts,stopped_speed=float(one(r'playback_speed = ([0-9.]+)',method(base,'ladder'),'stopped speed')[1]),position_y=float(one(r'position.y = ([0-9.]+)',method(base,'unladder'),'Position Y')[1]),admission=dict(no_source_lifecycle_callbacks=True,no_ready_body=True,actual_native_area_required=True,actual_current_player_receiver_implemented=True,follower_receiver_requires_its_actual_owner=True))
def review(d):return dict(schema=1,commit=PIN,ir_sha256=sha(IR),producer_sha256=sha(Path(__file__)),sources=d['sources'],dependencies=d['dependencies'],admission=d['admission'])
def load():
 d=read(IR);require(d==derive(),'Ladder source/resource closure changed');require(read(REVIEW)==review(d),'Ladder source review stale');return d
def binary(d):
 b=bytearray(128)
 def u(*v):b.extend(struct.pack('<'+'I'*len(v),*v))
 def t(v):raw=v.encode();u(len(raw));b.extend(raw)
 t(d['scene']);t(d['node']);u(*[d[k]for k in ['id','shape_id','geometry_owner_index','geometry_shape_index','layer','mask','owner_flags','shape_flags']]);b.extend(struct.pack('<14f',*[v for rows in [d['local'],d['world']]for row in rows for v in row],d['stopped_speed'],d['position_y']))
 for k in ['player_initialization_ir_sha256','player_ready_ir_sha256','player_motion_ir_sha256']:b.extend(bytes.fromhex(d[k]))
 u(len(d['texts']));[t(v)for v in d['texts']];u(len(d['connections']))
 for c in d['connections']:t(c['signal']);t(c['method']);u(c['flags'],c['arguments'])
 u(len(d['non_party_body_ids']));u(*d['non_party_body_ids'])
 u(len(d['sources']))
 for p,h in d['sources'].items():t(p);b.extend(bytes.fromhex(h))
 struct.pack_into('<8s8I',b,0,b'ENCHLDR1',1,128,len(b),zlib.crc32(b[128:]),FAMILY,1,1,d['scene_id']);b[40:60]=bytes.fromhex(PIN);b[60:92]=bytes.fromhex(d['source_sha256']);b[92:124]=bytes.fromhex(sha(IR));return bytes(b)
def stage_files(root):
 relative=Path('data/house-return-ladder.encladder');raw=binary(load());require((Path(root)/relative).read_bytes()==raw,'Ladder staged resource changed');return {relative:raw}
def main():
 p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile','verify']);a=p.parse_args()
 if a.action=='extract':d=derive();write(IR,d);write(REVIEW,review(d));return
 raw=binary(load())
 if a.action=='compile':PACK.parent.mkdir(parents=True,exist_ok=True);PACK.write_bytes(raw)
 else:require(PACK.read_bytes()==raw,'Ladder binary stale')
 print('House Ladder checked source resource:',len(raw),'bytes; no scene/lifecycle Ready admission')
if __name__=='__main__':
 try:main()
 except (ValueError,OSError,KeyError,TypeError,struct.error)as e:sys.exit('HOUSE LADDER ERROR: '+str(e))
