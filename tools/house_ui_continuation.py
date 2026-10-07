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
 # Door continuation executes the real zero-key branch, retaining the
 # existing KeyNumber closed state rather than replaying UiManager._ready.
 key_scene='Nodes/Ui/KeyCount.tscn';key_script='Scripts/UI/KeyNumber.gd'
 key_text=ex.text(key_script);key_resource=ex.text(key_scene)
 require('script = ExtResource( 2 )' in key_resource and 'res://'+key_script in key_resource,'Changed KeyCount native script binding')
 for role,name,body in [(11,'get_key_count',text),(12,'update_key_indicator',text),(13,'close',key_text)]:
  f=re.search(r'^func '+name+r'\([^\n]*\n.*?(?=^func |\Z)',body,re.M|re.S);require(f,'Missing actual key indicator method');methods.append(dict(role=role,name=name,sha=hashlib.sha256(f[0].encode()).hexdigest()))
 getter=re.search(r'globaldata\.(\w+)\.get\(global\.currentScene\.get_region_name\(\), (\d+)\)',text);require(getter and int(getter[2])==0,'Unknown source regional key lookup')
 require('if get_key_count() <= 0:\n\t\t_key.close()\n\telse:\n\t\t_key.open()'in text,'Unknown key indicator source branch')
 require('var _is_open = false' in key_text and 'func close():\n\tif !_is_open:\n\t\treturn' in key_text,'Unknown KeyNumber source close guard')
 enemy_method=re.search(r'^func clear_on_screen_enemies\([^\n]*\n.*?(?=^func |\Z)',text,re.M|re.S);require(enemy_method and '_onScreenEnemies.clear()'in enemy_method[0] and 'var _onScreenEnemies := []'in text,'Unknown source on-screen enemy clearing')
 methods.append(dict(role=14,name='clear_on_screen_enemies',sha=hashlib.sha256(enemy_method[0].encode()).hexdigest()))
 stack=re.search(r'^var (\w+) := \[\]$',text,re.M);require(stack and stack[1]=='_ui_stack','Unknown source UI stack declaration')
 require('func is_stack_empty() -> bool:\n\treturn '+stack[1]+'.size() == 0' in text and stack[1]+'.push_front(ui)'in text and stack[1]+'.erase(ui)'in text,'Unknown UI stack getter/push/erase')
 dialogue_script='Scripts/UI/DialogueBox.gd';dialogue=ex.text(dialogue_script)
 require('uiManager.remove_ui(self)'in dialogue,'Source DialogueBox close no longer removes its UI entry')
 for role,name,body in [(15,'is_stack_empty',text),(16,'add_ui',text),(17,'remove_ui',text),(18,'_close_dialog_box',dialogue)]:
  f=re.search(r'^func '+name+r'\([^\n]*\n.*?(?=^func |\Z)',body,re.M|re.S);require(f,'Missing actual source stack lifecycle');methods.append(dict(role=role,name=name,sha=hashlib.sha256(f[0].encode()).hexdigest()))
 stack_policy=dict(member=stack[1],initial_count=0,dialogue_script=dialogue_script)
 house_scene='Maps/podunk/Nintens House.tscn';house_source=ex.text(house_scene)
 require('EnemySpawner'not in house_source and 'BasicEnemy'not in house_source,'House now has an unported overworld enemy owner')
 key_policy=dict(member=getter[1],default_count=int(getter[2]),initial_open=False,scene=key_scene,script=key_script,enemy_member='_onScreenEnemies',enemy_initial_count=0,house_scene=house_scene)
 return dict(schema=1,family=FAMILY,commit=PIN,owner=owner,scene_id=ui['scene_id'],source_sha256=ui['source_sha256'],ui_ir_sha256=sha(UI),sources=ex.sources,key_policy=key_policy,stack_policy=stack_policy,fields=fields,methods=methods,signals=signals,scope=['Real existing House battle/menu/story owners; no source constructor or seven UI factories replayed','BattleEnded event clears source battlefield state, including PostWinRequested; ReturnStarted dispatches actual battle_to_ov','Cutscene setter is an actual source method on this owning state; House dialogue/story getter remains live','Whole UiManager _ready, stableCanvas construction, generic menus and unsupported party-info owner remain pending'],scene_admitted=False)

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
 t(d['key_policy']['member']);u(d['key_policy']['default_count'],int(d['key_policy']['initial_open']));t(d['key_policy']['scene']);t(d['key_policy']['script']);t(d['key_policy']['enemy_member']);u(d['key_policy']['enemy_initial_count']);t(d['key_policy']['house_scene'])
 t(d['stack_policy']['member']);u(d['stack_policy']['initial_count']);t(d['stack_policy']['dialogue_script'])
 struct.pack_into('<8s8I',b,0,b'ENCHUIC1',3,128,len(b),zlib.crc32(b[128:]),FAMILY,3,1,d['scene_id']);b[40:60]=bytes.fromhex(PIN);b[60:92]=bytes.fromhex(d['source_sha256']);b[92:124]=bytes.fromhex(sha(IR));return bytes(b)

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
