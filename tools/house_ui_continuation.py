#!/usr/bin/env python3
"""Checked existing House UI getter/event bindings; never whole UiManager Ready."""
from __future__ import annotations
import argparse,hashlib,re,struct,sys,zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import PIN,read,write,sha,require
from tools.extract_battle_entry import Extractor
UI=ROOT/'content/field-ui-manager.json';IR=ROOT/'content/native-house-ui-continuation.json'
REVIEW=ROOT/'reports/house-ui-continuation/source-review.json';PACK=ROOT/'romfs/data/house.encuicontinuation';FAMILY=0x454e0064

def derive():
 ex=Extractor(ROOT);ui=read(UI);owner=ui['scene'];text=ex.text(owner);battle_path='Scripts/UI/Battle/BattleSystem.gd';battle=ex.text(battle_path)
 require(ui['commit']==PIN and ui['source_sha256']==ex.sources[owner],'Changed original existing UI source')
 methods=[];fields=[]
 for role,method in [(1,'is_in_battle'),(2,'is_pause_menu_active'),(3,'is_in_cutscene')]:
  f=re.search(r'^func '+method+r'\(\) -> bool:\n\treturn (\w+)\n',text,re.M);require(f,'Unknown actual UI boolean getter')
  member=f[1];initial=re.search(r'^var '+re.escape(member)+r' := (true|false)$',text,re.M);require(initial,'Unknown source UI boolean declaration')
  fields.append(dict(role=role,member=member,initial=initial[1]=='true'));methods.append(dict(role=role,name=method,sha=hashlib.sha256(f[0].encode()).hexdigest()))
 for role,method in [(4,'set_cutscene'),(5,'info_plates_hide')]:
  f=re.search(r'^func '+method+r'\([^\n]*\n.*?(?=^func |\Z)',text,re.M|re.S);require(f,'Missing actual UI source method');methods.append(dict(role=role,name=method,sha=hashlib.sha256(f[0].encode()).hexdigest()))
 require('func set_cutscene(value: bool):\n\t'+fields[2]['member']+' = value' in text,'Unknown source cutscene setter')
 signal_names=re.findall(r'^signal (\w+)$',text,re.M);require(len(signal_names)==3,'Changed source UI signal roster')
 signals=[dict(role=i+1,name=n,arity=0)for i,n in enumerate(signal_names)]
 require('emit_signal("'+signal_names[1]+'")'in text,'Missing actual battle-start signal')
 require('_battle_ui.connect("'+signal_names[2]+'", self, "emit_signal", ["'+signal_names[2]+'"], CONNECT_ONESHOT)'in text,'Unknown actual return-signal forwarding')
 require('emit_signal("'+signal_names[2]+'")'in battle and '\n\tyield($AnimScene, "animation_finished")\n\t\n\t# Release player or play cutscene\n\temit_signal("battle_ended", battle_result)'in battle,'Changed original return/end order')
 require('func _on_battle_ended(result: int, battle_ui: Node):\n\t'+fields[0]['member']+' = false' in text,'Changed original battlefield end assignment')
 for name in ['start_battle','_on_battle_ended','open_commands_menu','close_commands_menu','open_dialogue_box']:
  f=re.search(r'^func '+name+r'\([^\n]*\n.*?(?=^func |\Z)',text,re.M|re.S);require(f,'Missing original UI event body');methods.append(dict(role=len(methods)+1,name=name,sha=hashlib.sha256(f[0].encode()).hexdigest()))
 return dict(schema=1,family=FAMILY,commit=PIN,owner=owner,scene_id=ui['scene_id'],source_sha256=ui['source_sha256'],ui_ir_sha256=sha(UI),sources=ex.sources,fields=fields,methods=methods,signals=signals,scope=['Real existing House battle/menu/story owners; no source constructor or seven UI factories replayed','BattleEnded event clears source battlefield state, including PostWinRequested; ReturnStarted dispatches actual battle_to_ov','Cutscene setter is an actual source method on this owning state; House dialogue/story getter remains live','Whole UiManager _ready, stableCanvas construction, generic menus and unsupported party-info owner remain pending'],scene_admitted=False)

def extract():
 d=derive();write(IR,d);write(REVIEW,dict(schema=1,commit=PIN,ir_sha256=sha(IR),sources=d['sources'],scope=d['scope']))
def load():
 d=read(IR);require(d==derive(),'Changed actual House continuation source bindings');r=read(REVIEW);require(r['ir_sha256']==sha(IR)and r['sources']==d['sources'],'Stale actual continuation review');return d

def encode(d):
 b=bytearray(128)
 def u(*v):b.extend(struct.pack('<'+'I'*len(v),*v))
 def t(s):v=s.encode();u(len(v));b.extend(v)
 b.extend(bytes.fromhex(d['ui_ir_sha256']));t(d['owner']);u(len(d['sources']))
 for p,h in d['sources'].items():t(p);b.extend(bytes.fromhex(h))
 u(len(d['fields']))
 for x in d['fields']:u(x['role'],int(x['initial']));t(x['member'])
 u(len(d['methods']))
 for x in d['methods']:u(x['role']);t(x['name']);b.extend(bytes.fromhex(x['sha']))
 u(len(d['signals']))
 for x in d['signals']:u(x['role'],x['arity']);t(x['name'])
 struct.pack_into('<8s8I',b,0,b'ENCHUIC1',1,128,len(b),zlib.crc32(b[128:]),FAMILY,1,1,d['scene_id']);b[40:60]=bytes.fromhex(PIN);b[60:92]=bytes.fromhex(d['source_sha256']);b[92:124]=bytes.fromhex(sha(IR));return bytes(b)

def main():
 p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile','verify']);a=p.parse_args()
 if a.action=='extract':extract();return
 b=encode(load())
 if a.action=='compile':PACK.write_bytes(b)
 else:require(PACK.read_bytes()==b,'Stale actual continuation resource')
 print('Existing House UI continuation source:',len(b),'bytes; no scene execution')
if __name__=='__main__':
 try:main()
 except(ValueError,KeyError,OSError,TypeError,StopIteration,AttributeError)as e:sys.exit('HOUSE UI SOURCE ERROR: '+str(e))
