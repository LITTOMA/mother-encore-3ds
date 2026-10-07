#!/usr/bin/env python3
"""Checked existing House UI getter/event bindings; never whole UiManager Ready."""
from __future__ import annotations
import argparse,hashlib,re,struct,sys,zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import PIN,read,write,sha,require
from tools.extract_battle_entry import Extractor
UI=ROOT/'content/field-ui-manager.json';IR=ROOT/'content/native-house-ui-continuation.json'
CURVE=ROOT/'reports/house-ui-continuation/fade-curve-native.json'
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
 # Actual source member/method symbols remain independent of C++ execution.
 box_scene='Nodes/Ui/DialogueBox.tscn';abstract_script='Scripts/UI/AbstractDialogueBox.gd'
 ex.text(box_scene);abstract=ex.text(abstract_script)
 box_member=re.search(r'^var (\w+): AbstractDialogueBox = null$',text,re.M)
 canvas_member=re.search(r'^var (\w+): CanvasLayer = null$',text,re.M)
 require(box_member and canvas_member and canvas_member[1]=='_stable_canvas_layer','Unknown source UI dialogue/canvas fields')
 add=re.search(r'^func add_ui\(ui: Node, add_child = true\):\n\t'+stack[1]+r'\.push_front\(ui\)\n\tif add_child:\n\t\t'+canvas_member[1]+r'\.call_deferred\("(\w+)", ui\)\n',text,re.M)
 close_item=re.search(r'^func close_item\(item: Node\):[^\n]*\n\titem\.call\("(\w+)" if item\.has_method\("\1"\) else "(\w+)"\)\n',text,re.M)
 require(add and close_item,'Changed original UI stack deferred-add/close dispatch')
 require(not re.search(r'^func '+re.escape(close_item[1])+r'\(',dialogue+'\n'+abstract,re.M),'DialogueBox now implements close rather than native queue_free')
 for role,name in [(19,'close_item'),(20,'get_dialogue_actors'),(21,'close_dialogue_box')]:
  f=re.search(r'^func '+name+r'\([^\n]*\n.*?(?=^func |\Z)',text,re.M|re.S);require(f,'Missing actual dialogue UI method');methods.append(dict(role=role,name=name,sha=hashlib.sha256(f[0].encode()).hexdigest()))
 require('return '+box_member[1]+'.get_actors() if '+box_member[1]+' else {}'in text,'Changed source active-dialogue actor getter')
 dialogue_policy=dict(stack_member=stack[1],dialogue_member=box_member[1],canvas_member=canvas_member[1],dialogue_scene=box_scene,dialogue_script=dialogue_script,abstract_script=abstract_script,add_child_method=add[1],close_method=close_item[1],queue_free_method=close_item[2])
 actors=re.search(r'^var (\w+) := \{\}$',dialogue,re.M)
 require(actors and actors[1]=='_actors','Unknown source dialogue actors constructor')
 observation={}
 for key,name in [('queued_battle_member','_queued_battle'),('set_respawn_member','_set_respawn')]:
  initial=re.search(r'^var ('+re.escape(name)+r') := (true|false)$',dialogue,re.M);require(initial,'Unknown source dialogue observation declaration')
  observation[key]=initial[1];observation[key.replace('_member','')]=initial[2]=='true'
 dialogue_policy.update(actors_member=actors[1],actor_count=0,**observation)
 # The continued closed models execute only each original false guard.
 widgets=[]
 for role,scene,script in [(1,key_scene,key_script),(2,'Nodes/Ui/CashBox.tscn','Scripts/UI/CashBox.gd'),(3,'Nodes/Ui/PartyInfo.tscn','Scripts/UI/PartyInfo.gd')]:
  body=ex.text(script);resource=ex.text(scene)
  require('res://'+script in resource,'Closed widget source scene/script changed')
  close=re.search(r'^func close\(\):\n.*?(?=^func |\Z)',body,re.M|re.S);require(close,'Missing original widget close')
  guard=re.search(r'\tif (!?)(\w+):',close[0]);require(guard,'Unknown widget close guard')
  initial=re.search(r'^var '+re.escape(guard[2])+r' (?::=|=) (true|false)$',body,re.M)
  require(initial and initial[1]=='false','Closed widget now has a nonclosed constructor')
  if role==1:require('\tif !'+guard[2]+':\n\t\treturn' in close[0],'Changed Key closed return')
  else:require('\tif '+guard[2]+':' in close[0],'Changed closed widget conditional')
  widgets.append(dict(role=role,scene=scene,script=script,member=guard[2],close_method='close',initial_open=False))
 timer=re.search(r'^var (\w+): Timer$',text,re.M);require(timer and timer[1]=='_party_info_timer','Unknown UI info timer null declaration')
 fade_scene='Nodes/Ui/effects/Fade.tscn';fade_script='Nodes/Ui/effects/Fade.gd';fade=ex.text(fade_script);fade_scene_body=ex.text(fade_scene)
 restore=re.search(r'\telse:\n\t\t_fade.set_cut\(([0-9.]+), ([0-9.]+), (\d+), Tween.EASE_IN\)\n\t\tyield\(_fade, "(\w+)"\)\n\t\t_fade.set_spin\(false\)',text)
 require(restore and '.set_trans(Tween.TRANS_QUAD).set_ease(tween_ease)' in fade and 'signal '+restore[4] in fade,'Changed telepathy false restoration')
 spin=re.search(r'^func set_spin\(enabled: bool, speed := 1.0\):.*?\telse:\n\t\tif \$PathAnim.is_playing\(\): \$PathAnim.stop\(\)\n\t\t\$Path2D/PathFollow2D.unit_offset = ([0-9.]+)',fade,re.M|re.S)
 path_anim=re.search(r'\[node name="PathAnim".*?(?=\n\[node|\Z)',fade_scene_body,re.S)
 require(spin and path_anim and 'autoplay'not in path_anim[0],'Changed bounded Fade false spin/default')
 ex.text('Nodes/Ui/Blackbars.tscn');bars=ex.text('Scripts/UI/Blackbars.gd')
 require('var _is_open := false' in bars,'Changed source bars initial body')
 global_script='Scripts/global/global.gd';global_body=ex.text(global_script)
 phone=re.search(r'^func set_phone_location\(value: String\):\n\t(\w+) = value\n',global_body,re.M);require(phone,'Changed source phone location setter')
 audio_script='Scripts/global/audioManager.gd';audio=ex.text(audio_script)
 sound=re.search(r'^func _close_dialog_box\(\):.*?play_sfx_by_name\("([^"]+)", "([^"]+)"\)',dialogue,re.M|re.S);require(sound and sound[1]==sound[2],'Changed close sound call')
 stream=re.search(r'"'+re.escape(sound[1])+r'": load\("res://([^"]+)"\)',audio);require(stream,'Unknown original close sound mapping')
 input_body=re.search(r'^func _action_press\([^\n]*\n.*?(?=^func |\Z)',dialogue,re.M|re.S);require(input_body,'Missing source dialogue action body')
 input_calls=re.findall(r'\$([A-Za-z0-9_/]+)\.play\(\)',input_body[0]);input_nodes=set(input_calls);require(len(input_calls)==2 and len(input_nodes)==1,'Changed actual input sound node')
 show=re.search(r'^func _show_box\([^\n]*\n.*?(?=^func |\Z)',dialogue,re.M|re.S);require(show,'Missing source show-box body')
 open_sound=re.search(r'if show and .*?play_sfx_by_name\("([^"]+)", "([^"]+)"\)',show[0],re.S);require(open_sound and open_sound[1]==open_sound[2],'Changed actual open sound call')
 open_stream=re.search(r'"'+re.escape(open_sound[1])+r'": load\("res://([^"]+)"\)',audio);require(open_stream,'Unknown original open sound mapping')
 dialogue_policy.update(input_sound_node=next(iter(input_nodes)),open_sound_name=open_sound[2],open_sound_source=open_stream[1])
 ended=re.search(r'global.emit_signal\("(\w+)"\)',dialogue);require(ended,'Missing dialogue global completion signal')
 for role,name,body in [(22,'close_key_indicator',text),(23,'toggle_black_bars',text),(24,'get_cash_box',text),(25,'set_telepathy_effect',text),(26,'set_phone_location',global_body),(27,'play_sfx_by_name',audio)]:
  method=re.search(r'^func '+name+r'\([^\n]*\n.*?(?=^func |\Z)',body,re.M|re.S);require(method,'Missing original closed business method')
  methods.append(dict(role=role,name=name,sha=hashlib.sha256(method[0].encode()).hexdigest()))
 business_policy=dict(widgets=widgets,timer_member=timer[1],initial_timer_null=True,fade_scene=fade_scene,fade_script=fade_script,cut_signal=restore[4],global_script=global_script,phone_member=phone[1],end_signal=ended[1],close_sound_source=stream[1],close_sound_name=sound[2],restore_target=float(restore[1]),restore_duration=float(restore[2]),fade_type=int(restore[3]),transition=1,ease=1,spin_stop_unit_offset=float(spin[1]))
 effect=re.search(r'if enabled:\n\t\t_fade.focus_object\(object\)\n\t\t_fade.set_color\(Color\(([^)]+)\)\)\n\t\t_fade.set_cut\(([0-9.]+), ([0-9.]+), (\d+), Tween.EASE_OUT\)\n\t\t_fade.set_spin\(true, ([0-9.]+)\)',text);require(effect,'Changed positive telepathy source sequence')
 color=[float(v.strip())for v in effect[1].split(',')];require(len(color)==4,'Unknown positive source Color')
 screen=re.search(r'const SCREEN_SIZE := Vector2\(([^)]+)\)',fade);require(screen,'Missing original source focus viewport')
 screen_size=[float(v.strip())for v in screen[1].split(',')]
 rotation=re.search(r'\[sub_resource type="Animation" id=2\]\n(.*?)(?=\n\[sub_resource)',fade_scene_body,re.S);require(rotation,'Missing actual PathAnim source clip')
 length=re.search(r'^length = ([0-9.]+)$',rotation[1],re.M);values=re.search(r'"values": \[ ([^]]+) \]',rotation[1]);require(length and values and 'loop = true'in rotation[1] and 'tracks/0/interp = 1'in rotation[1] and 'PathFollow2D:offset'in rotation[1],'Unknown actual Rotate track')
 limits=[float(v.strip())for v in values[1].split(',')];require(len(limits)==2,'Changed source rotate keys')
 times=re.search(r'"times": PoolRealArray\( ([^)]*)\)',rotation[1]);transitions=re.search(r'"transitions": PoolRealArray\( ([^)]*)\)',rotation[1]);require(times and transitions and [float(v.strip())for v in times[1].split(',')]==[0.0,float(length[1])] and all(float(v.strip())==1.0 for v in transitions[1].split(',')),'Changed source Rotate time/easing')
 follow=re.search(r'\[node name="PathFollow2D".*?(?=\n\[node)',fade_scene_body,re.S);require(follow and 'rotate = false'in follow[0],'Unknown nonrotating source follow')
 position=re.search(r'^position = Vector2\(([^)]+)\)$',follow[0],re.M);require(position,'Missing source follow transform')
 native_curve=read(CURVE)
 require(native_curve['commit']==PIN and native_curve['source']==fade_scene and native_curve['source_sha256']==ex.sources[fade_scene] and native_curve['engine_version']=='3.6.2-stable (official)','Changed fixed native Curve2D resource input')
 raw=bytes.fromhex(native_curve['baked_float_hex']);points=[list(v)for v in struct.iter_unpack('<ff',raw)]
 native_length,native_interval=struct.unpack('<ff',bytes.fromhex(native_curve['length_interval_float_hex']))
 require(points==native_curve['baked_points'] and native_length==native_curve['baked_length'] and native_interval==native_curve['bake_interval'] and 2<=len(points)<=4096 and native_length>0 and native_interval>0 and native_curve['cubic_interp']and native_curve['loop'],'Unknown native Curve2D geometry/defaults')
 require(set(native_curve['engine'])=={'curve.cpp','path_2d.cpp'} and all(re.fullmatch('[0-9a-f]{64}',v)for v in native_curve['engine'].values()),'Missing native curve source proof')
 business_policy.update(effect_target=float(effect[2]),effect_duration=float(effect[3]),effect_type=int(effect[4]),effect_ease=2,effect_color=color,spin_speed=float(effect[5]),screen_size=screen_size,spin_length=float(length[1]),spin_from=limits[0],spin_to=limits[1],initial_path_position=[float(v.strip())for v in position[1].split(',')],curve_points=points,curve_length=native_length,curve_interval=native_interval,curve_cubic=native_curve['cubic_interp'],curve_loop=native_curve['loop'],curve_input_sha256=sha(CURVE),curve_engine=native_curve['engine'])
 house_scene='Maps/podunk/Nintens House.tscn';house_source=ex.text(house_scene)
 require('EnemySpawner'not in house_source and 'BasicEnemy'not in house_source,'House now has an unported overworld enemy owner')
 key_policy=dict(member=getter[1],default_count=int(getter[2]),initial_open=False,scene=key_scene,script=key_script,enemy_member='_onScreenEnemies',enemy_initial_count=0,house_scene=house_scene)
 return dict(schema=1,family=FAMILY,commit=PIN,owner=owner,scene_id=ui['scene_id'],source_sha256=ui['source_sha256'],ui_ir_sha256=sha(UI),sources=ex.sources,key_policy=key_policy,stack_policy=stack_policy,dialogue_policy=dialogue_policy,business_policy=business_policy,fields=fields,methods=methods,signals=signals,scope=['Real existing House battle/menu/story owners; no source constructor or seven UI factories replayed','BattleEnded event clears source battlefield state, including PostWinRequested; ReturnStarted dispatches actual battle_to_ov','Cutscene setter is an actual source method on this owning state; House dialogue/story getter remains live','Whole UiManager _ready, stableCanvas construction, generic menus and unsupported party-info owner remain pending'],scene_admitted=False)

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
 for key in ['stack_member','dialogue_member','canvas_member','dialogue_scene','dialogue_script','abstract_script','add_child_method','close_method','queue_free_method']:t(d['dialogue_policy'][key])
 for key in ['actors_member','queued_battle_member','set_respawn_member','input_sound_node','open_sound_name','open_sound_source']:t(d['dialogue_policy'][key])
 u(d['dialogue_policy']['actor_count'],int(d['dialogue_policy']['queued_battle']),int(d['dialogue_policy']['set_respawn']))
 v=d['business_policy'];u(len(v['widgets']))
 for widget in v['widgets']:
  u(widget['role'],int(widget['initial_open']))
  for key in ['scene','script','member','close_method']:t(widget[key])
 for key in ['timer_member','fade_scene','fade_script','cut_signal','global_script','phone_member','end_signal','close_sound_source','close_sound_name']:t(v[key])
 u(int(v['initial_timer_null']),v['fade_type'],v['transition'],v['ease']);b.extend(struct.pack('<3d',v['restore_target'],v['restore_duration'],v['spin_stop_unit_offset']))
 u(v['effect_type'],v['effect_ease'],int(v['curve_cubic']),int(v['curve_loop']))
 for value in [v['effect_target'],v['effect_duration'],v['spin_speed'],v['spin_length'],v['spin_from'],v['spin_to'],v['curve_length'],v['curve_interval'],*v['screen_size'],*v['initial_path_position'],*v['effect_color']]:b.extend(struct.pack('<d',value))
 u(len(v['curve_points']))
 for x,y in v['curve_points']:b.extend(struct.pack('<2f',x,y))
 b.extend(bytes.fromhex(v['curve_input_sha256']))
 for path,source_sha in v['curve_engine'].items():t(path);b.extend(bytes.fromhex(source_sha))
 struct.pack_into('<8s8I',b,0,b'ENCHUIC1',4,128,len(b),zlib.crc32(b[128:]),FAMILY,4,1,d['scene_id']);b[40:60]=bytes.fromhex(PIN);b[60:92]=bytes.fromhex(d['source_sha256']);b[92:124]=bytes.fromhex(sha(IR));return bytes(b)

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
