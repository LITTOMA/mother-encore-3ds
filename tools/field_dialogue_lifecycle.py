#!/usr/bin/env python3
"""Bounded original DialogueBox UI coroutine/Ready lifecycle, no script interpreter."""
from __future__ import annotations
import argparse,hashlib,json,re,struct,sys,zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.extract_battle_entry import Extractor,require,animation
from tools.podunk_scene import PIN,read,write,sha
from tools.field_programme import load as programme_load
from tools.field_node_recipe import load as recipe_load
IR=ROOT/'content/native-field-dialogue-lifecycle.json';REVIEW=ROOT/'reports/field-dialogue-lifecycle/source-review.json';PACK=ROOT/'romfs/data/podunk-dialogue-lifecycle.encdlife'
SCENE='Nodes/Ui/DialogueBox.tscn';UI='Scripts/global/uiManager.gd';BOX='Scripts/UI/DialogueBox.gd';ABSTRACT='Scripts/UI/AbstractDialogueBox.gd'
STAGES=['OpenCreated','Ready','Begin','EndPrefix','AfterStop','EndSignal','ManagerDone','AfterDone','CloseFinished']
OPS=['StoreDialogue','PauseMenuInactive','StackPush','DeferredAdd','UiCutscene','KeyClose','BlackBars','GlobalCutscene','InfoHide','SetProgramme','SetTalker','ConnectName','InputRelease','ArrowHide','VisibleCharacters','TextClear','BulletClear','PhoneLocation','VoiceVolume','TextHide','NameClose','CashClose','TelepathyRestore','KeyUpdate','EmitCutsceneEnded','ResetPhrase','CloseBox','ClearDialogue','UnpauseIfOwned','EmitDone','ReturnCamera','ReturnOffset','RemoveUi','StopTalker']
def function(text,name):
 m=re.search(r'^func '+re.escape(name)+r'\([^\n]*\):?[^\n]*\n(.*?)(?=^func |\Z)',text,re.M|re.S);require(m,'Source function missing '+name);return m[1]
def build():
 ex=Extractor(ROOT);ui=ex.text(UI);box=ex.text(BOX);ab=ex.text(ABSTRACT);scene=ex.text(SCENE);d=programme_load();src={};steps=[]
 def add(stage,kind,body,selector,role=0,value=0,string=''):
  require(selector in body,'Unknown source lifecycle expression '+selector);steps.append(dict(stage=stage,kind=kind,role=role,value=value,string=string,expression=selector))
 op=function(ui,'open_dialogue_box');add_ui=function(ui,'add_ui');begin=function(box,'start_from_dict');end=function(box,'_end_dialogue');close=function(box,'_close_dialog_box')
 require(op.index('is_paused()')<op.index('pause(true, true)')<op.index('DialogueBoxRes.instance()')<op.index('add_ui(_dialogue_box)')<op.index('yield(_dialogue_box, "ready")')<op.index('_dialogue_box.start_from_id(dialogue_id, npc)')<op.index('yield(_dialogue_box, "done")'),'UI source coroutine order changed')
 require('var DialogueBoxRes := preload("res://'+SCENE+'")'in ui and '_stable_canvas_layer.call_deferred("add_child", ui)'in add_ui,'UI actual deferred factory changed')
 ready=function(box,'_ready');ref=lambda var:re.search(r'(?:onready var '+re.escape(var)+r' :=|'+re.escape(var)+r' =) \$([^\s\n]+)',box)[1]
 nodes=[dict(role=1,path='.'),dict(role=2,path=ref('_dialogue_box_node')),dict(role=3,path=ref('_name_label')),dict(role=4,path=ref('_dialogue_label')),dict(role=5,path=ref('_bullet_label')),dict(role=6,path='Dialoguebox/Arrow'),dict(role=7,path='AnimationPlayer'),dict(role=8,path='NameAnim'),dict(role=9,path='AudioStreamPlayer'),dict(role=10,path='WaitTimer'),dict(role=11,path=ref('_camera')),dict(role=12,path=ref('_cursor_down_sprite'))]
 for n in nodes:
  if n['role']not in (1,2,3,4,5,11,12):require('$'+n['path']in box,'Source UI required node missing')
 add('OpenCreated','StoreDialogue',op,'_dialogue_box = DialogueBoxRes.instance()')
 add('OpenCreated','PauseMenuInactive',op,'_pause_menu_active = false')
 add('OpenCreated','StackPush',add_ui,'_ui_stack.push_front(ui)')
 add('OpenCreated','DeferredAdd',add_ui,'_stable_canvas_layer.call_deferred("add_child", ui)',string='add_child')
 for kind,selector,value in [('UiCutscene','_cutscene = true',1),('KeyClose','close_key_indicator()',0),('BlackBars','toggle_black_bars(true)',1)]:add('Ready',kind,op,selector,value=value)
 add('Begin','GlobalCutscene',begin,'global.in_cutscene = true',value=1)
 add('Begin','InfoHide',begin,'uiManager.info_plates_hide()')
 add('Begin','SetProgramme',begin,'_dialog = dialogue_dict')
 add('Begin','SetTalker',begin,'global.talker = npc',value=1)
 add('Begin','ConnectName',begin,'_name_label.connect("item_rect_changed", self, "_set_nametag")',role=3,string='_set_nametag')
 for action in ['ui_cancel','ui_accept']:add('Begin','InputRelease',begin,'Input.action_release("'+action+'")',string=action)
 add('Begin','ArrowHide',begin,'$Dialoguebox/Arrow.hide()',role=6)
 add('Begin','VisibleCharacters',begin,'_dialogue_label.visible_characters = 0',role=4)
 add('Begin','TextClear',begin,'_dialogue_label.bbcode_text = ""',role=4)
 add('Begin','BulletClear',begin,'_bullet_label.bbcode_text = ""',role=5)
 for action in ['ui_cancel','ui_accept']:add('EndPrefix','InputRelease',end,'Input.action_release("'+action+'")',string=action)
 clear=function(box,'_clear_dialogue')
 add('EndPrefix','TextClear',clear,'_dialogue_label.bbcode_text = ""',role=4)
 add('EndPrefix','BulletClear',clear,'_bullet_label.bbcode_text = ""',role=5)
 add('EndPrefix','VisibleCharacters',clear,'_dialogue_label.visible_characters = 0',role=4)
 add('EndPrefix','PhoneLocation',end,'global.set_phone_location("")')
 m=re.search(r'\$AudioStreamPlayer.volume_db = (-?\d+)',end);require(m,'Source voice mute absent');add('EndPrefix','VoiceVolume',end,m[0],role=9,value=int(m[1]))
 add('EndPrefix','TextHide',end,'_dialogue_label.hide()',role=4)
 add('EndPrefix','StopTalker',end,'global.talker.stop_interaction()')
 add('AfterStop','SetTalker',end,'global.talker = null')
 add('AfterStop','NameClose',end,'$NameAnim.play("Close")',role=8,string='Close')
 add('AfterStop','CashClose',end,'uiManager.get_cash_box().close()')
 add('AfterStop','TelepathyRestore',end,'uiManager.set_telepathy_effect(false)')
 add('AfterStop','KeyUpdate',end,'uiManager.update_key_indicator()')
 add('EndSignal','EmitCutsceneEnded',end,'global.emit_signal("cutscene_ended")',string='cutscene_ended')
 add('EndSignal','GlobalCutscene',end,'global.in_cutscene = false')
 add('EndSignal','ResetPhrase',end,'_phrase_num = "0"',string='0')
 add('EndSignal','CloseBox',end,'_close_dialog_box()')
 add('ManagerDone','ClearDialogue',op,'_dialogue_box = null')
 add('ManagerDone','BlackBars',op,'toggle_black_bars(false)')
 add('ManagerDone','UiCutscene',op,'_cutscene = false')
 add('ManagerDone','UnpauseIfOwned',op,'global.get_player().unpause()')
 add('AfterDone','EmitDone',end,'emit_signal("done", _dialog_response)',string='done')
 for kind,method in [('ReturnCamera','return_camera'),('ReturnOffset','return_offset')]:
  m=re.search(r'global.currentCamera\.'+method+r'\(([0-9.]+)\)',end);require(m,'Source camera return missing');add('AfterDone',kind,end,m[0],value=float(m[1]))
 add('CloseFinished','RemoveUi',close,'uiManager.remove_ui(self)')
 anim_node=re.search(r'\$(\w+)\.play\("([^"\n]+)"\)',close);require(anim_node and anim_node[1]=='AnimationPlayer','Source close animation endpoint changed')
 m=re.search(r'\$Dialoguebox.rect_position.y != ([0-9.]+)',close);require(m and 'yield($AnimationPlayer, "animation_finished")'in close,'Source conditional asynchronous close changed')
 closed_y=float(m[1]);closeanim=animation(scene,1,SCENE,anim_node[2]);nameanim=animation(scene,3,SCENE,'Close')
 require(not re.search(r'^func close\(',box+'\n'+ab,re.M)and 'item.call("close" if item.has_method("close") else "queue_free")'in function(ui,'close_item'),'Source remove_ui free dispatch changed')
 require('if _actors.size() != 0:'in end and 'if _queued_battle:'in end and 'if _set_respawn:'in end,'Source closure branches changed')
 require(end.index('_close_dialog_box()')<end.index('emit_signal("done", _dialog_response)')<end.index('global.currentCamera.return_camera('),'Source done/close/camera source order changed')
 for p in d['programs']:
  require(not any(k in phrase for phrase in p['document'].values()for k in ['actors','actorsmove','battle','set_response','setrespawn']),'Programme lifecycle branch outside admitted no-actor slice')
 recipe=recipe_load();require(recipe['scene']==SCENE and recipe['commit']==PIN and recipe['source_sha256']==ex.sources[SCENE]and len(recipe['records'])==47,'Dialogue lifecycle exact official factory differs')
 bypath={r['node']:r for r in recipe['records']}
 for n in nodes:
  r=bypath.get(n['path']);require(r is not None,'Dialogue source native reference absent '+n['path']);n.update(id=r['id'],native_class=r['native_class'],ready=r['ready'])
 for source,h in recipe['sources'].items():ex.data(source);require(ex.sources[source]==h,'Factory original closure changed')
 for source,h in d['sources'].items():ex.sources[source]=h
 return dict(schema=1,kind='encore.field-dialogue-lifecycle.source-ir',commit=PIN,scene=SCENE,scene_sha256=ex.sources[SCENE],factory=dict(scene_id=recipe['scene_id'],ir_sha256=sha(ROOT/'content/dialogue-node-recipe.json'),node_count=len(recipe['records'])),nodes=nodes,stages=STAGES,opcodes=OPS,steps=steps,closed_y=closed_y,close_animation=closeanim,name_close_animation=nameanim,done_signal='done',ready_signal='ready',animation_signal='animation_finished',name_signal='item_rect_changed',was_paused_method='is_paused',pause_arguments=[True,True],initial_response=int(re.search(r'var _dialog_response := (\d+)',box)[1]),programmes=[dict(path=p['identity'],source='Data/Dialogue/'+p['identity']+'.yaml',sha256=d['sources']['Data/Dialogue/'+p['identity']+'.yaml'])for p in d['programs']],sources=dict(sorted(ex.sources.items())),dependencies=dict(programme_ir_sha256=sha(ROOT/'content/native-field-programme.json'),recipe_ir_sha256=sha(ROOT/'content/dialogue-node-recipe.json')),semantics=['Original UI factory comes from independently checked full NodeRecipe; source scripts/native Ready and ObjectDB remain required, no fake object or metadata-only Ready','Actual add_ui pushes stack before SceneTree MessageQueue deferred add_child; await real root ready then UI cutscene/key/bars and checked original Programme','Mick no-actor/no-queued-battle/no-respawn source branch only. Validate exact original programme before PP/mutations; unsupported actor paths remain explicit','Close animation coroutine begins before done signal. done synchronous uiManager waiter clears UI ref/bars/cutscene and conditionally unpauses; original camera returns follow signal callbacks','Prior source boxes may keep closing after done while next box opens; retain genuine ObjectID/generation-bound waiters and deletion owners, never reuse their callbacks for a new instance','No synthetic tick/timer for Ready/animation/done/telepathy restore; actual native signal and source ordering bus provide callbacks; existing DialoguePlayer scheduler unchanged'],unsupported=['Actor dictionary/persistent party path restoration, queued battle or respawn branches require their typed original consumers before admission','Actual native CanvasLayer/Control/AnimationPlayer and complete source script Ready factory supplied by FieldNodeRecipe host, checked ObjectDB references required','Original cash/key/blackbars/input/phone/telepathy/native font/camera endpoints must preflight complete; no blanket method ignore'])
def validate(d):
 require(d['schema']==1 and d['kind']=='encore.field-dialogue-lifecycle.source-ir'and d['commit']==PIN and d['scene']==SCENE and d['stages']==STAGES and d['opcodes']==OPS,'Dialogue lifecycle source/schema')
 require(len(d['nodes'])==12 and [n['role']for n in d['nodes']]==list(range(1,13)),'Lifecycle native refs')
 require(all(s['stage']in STAGES and s['kind']in OPS and 0<=s['role']<=12 for s in d['steps']),'Unknown lifecycle source operation')
 require(all(any(s['stage']==stage for s in d['steps'])for stage in STAGES)and len(d['programmes'])==5,'Lifecycle complete stages/programmes')
 return d
def load():
 d=validate(read(IR));r=read(REVIEW);require(r['commit']==PIN and r['ir_sha256']==sha(IR)and r['producer_sha256']==sha(Path(__file__))and r['sources']==d['sources'],'Lifecycle semantic review stale');require(d==build(),'Lifecycle source lowering differs');return d
def encode(d):
 validate(d);b=bytearray(64)
 def u(*v):b.extend(struct.pack('<'+'I'*len(v),*v))
 def f(v):b.extend(struct.pack('<d',v))
 def t(v):raw=v.encode();u(len(raw));b.extend(raw)
 t(d['scene']);b.extend(bytes.fromhex(d['scene_sha256']));u(d['factory']['scene_id'],d['factory']['node_count']);b.extend(bytes.fromhex(d['factory']['ir_sha256']));f(d['closed_y']);u(d['initial_response'],*[int(x)for x in d['pause_arguments']])
 for name in ['done_signal','ready_signal','animation_signal','name_signal','was_paused_method']:t(d[name])
 u(len(d['nodes']))
 for n in d['nodes']:u(n['role'],n['id'],n['ready']);t(n['path']);t(n['native_class'])
 u(len(d['steps']))
 for s in d['steps']:u(STAGES.index(s['stage']),OPS.index(s['kind']),s['role']);f(s['value']);t(s['string'])
 u(len(d['programmes']))
 for p in d['programmes']:t(p['path']);t(p['source']);b.extend(bytes.fromhex(p['sha256']))
 # Native clips remain typed binary keys; no runtime JSON parser.
 for name in ['close_animation','name_close_animation']:
  a=d[name];require(len(a['tracks'])==1 and a['tracks'][0]['type']=='value','Unknown close native animation track')
  k=a['tracks'][0];require(k['enabled']and not k['imported']and k['keys']['update']==0 and all(len(v)==2 for v in k['keys']['values']),'Unknown close native animation codec')
  t(a['name']);t(k['path']);u(a['source_resource_id'],int(a['loop']),k['interp'],int(k['loop_wrap']));f(a['length']);f(a['step']);u(len(k['keys']['times']))
  for time,transition,value in zip(k['keys']['times'],k['keys']['transitions'],k['keys']['values']):f(time);f(transition);f(value[0]);f(value[1])
 u(len(d['sources']))
 for source,h in d['sources'].items():t(source);b.extend(bytes.fromhex(h))
 struct.pack_into('<8s6I20s12x',b,0,b'ENCDLIF1',1,len(b),0,1,1,len(d['steps']),bytes.fromhex(PIN));struct.pack_into('<I',b,16,zlib.crc32(b)&0xffffffff);return bytes(b)
def stage_files(source):
 raw=encode(load());p=Path('data/podunk-dialogue-lifecycle.encdlife');require((Path(source)/p).read_bytes()==raw,'Stale staged lifecycle');return{p:raw}
def main():
 a=argparse.ArgumentParser();a.add_argument('action',choices=['extract','compile']);args=a.parse_args()
 if args.action=='extract':
  d=validate(build());write(IR,d);write(REVIEW,dict(schema=1,commit=PIN,ir_sha256=sha(IR),producer_sha256=sha(Path(__file__)),sources=d['sources'],semantics=d['semantics'],unsupported=d['unsupported']));print('Dialogue lifecycle source',len(d['steps']),'operations');return
 raw=encode(load());PACK.parent.mkdir(parents=True,exist_ok=True);PACK.write_bytes(raw);print('Dialogue lifecycle binary',len(raw),'bytes')
if __name__=='__main__':
 try:main()
 except(ValueError,KeyError,OSError,TypeError,struct.error,AttributeError)as e:sys.exit('FIELD DIALOGUE LIFECYCLE ERROR: '+str(e))
