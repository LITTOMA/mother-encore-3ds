#!/usr/bin/env python3
"""Bounded original Pillow/Minnie source graph, native receipts and dialogue lowering."""
from __future__ import annotations
import argparse, csv, hashlib, io, json, re
from pathlib import Path
import sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT));sys.path.insert(0,str(ROOT/'tools'))
from tools.doll_dialogue import PIN, SOURCES as BASE_SOURCES, run_native, decode, require
from tools.doll_postwin import return_duration
from tools.phone_dialogue import text_segments
ATTACK='Data/Dialogue/Podunk/cutscenes/pillow_attack.yaml'
LEAVE='Data/Dialogue/Podunk/cutscenes/minnie_leave.yaml'
DOOR='Data/Dialogue/Podunk/cutscenes/minnie_door.yaml'
TUTORIAL='Data/Dialogue/Podunk/minnie_run_tutorial.yaml'
OPEN='Data/Dialogue/Podunk/minnie_door_open.yaml'
PATHS=(ATTACK,LEAVE,DOOR,TUTORIAL,OPEN)
REPORT=ROOT/'reports/pillow-sequence'

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def identity(path):return path.removeprefix('Data/Dialogue/').removesuffix('.yaml')
def load_documents(ex):
 import yaml
 docs={path:yaml.safe_load(ex.text(path))for path in PATHS}
 require(list(docs[ATTACK])==list(map(str,range(7))),'Pillow phrase labels')
 require(list(docs[LEAVE])==['0','1','3','4','5','6'],'Minnie leave phrase labels')
 require(list(docs[DOOR])==['0','1','2'],'Minnie door phrase labels')
 require(list(docs[TUTORIAL])==['0','3','7']and list(docs[OPEN])==['0'],'Minnie tutorial/open labels')
 for path,emote in [(ATTACK,'surprise'),(LEAVE,'surprise'),(DOOR,'sweat')]:
  receipt_path='reports/pillow-sequence/'+Path(path).stem+'-native.json'
  receipt=ex.document(receipt_path)if hasattr(ex,'document')else json.loads((ex.root/receipt_path).read_text())
  require(receipt['commit']==PIN and receipt['godot']['major']==3 and receipt['godot']['minor']==6 and receipt['godot']['patch']==2,'Pillow native engine/pin')
  for source,digest in receipt['sources'].items():
   if hasattr(ex,'source'):ex.source(source,digest)
   else:require(sha(ex.upstream/source)==digest,'Changed Pillow native source '+source);ex.data(source)
  require(decode(receipt)['yaml'][0]==docs[path],'Pillow original/Python parser disagreement')
 return docs

def generate(godot):
 REPORT.mkdir(parents=True,exist_ok=True)
 for path,emote in [(ATTACK,'surprise'),(LEAVE,'surprise'),(DOOR,'sweat')]:
  sources=[path,*BASE_SOURCES[1:]]
  data=run_native(ROOT,godot,path,emote,sources)
  (REPORT/(Path(path).stem+'-native.json')).write_text(json.dumps(data,indent=2)+'\n')
 review=dict(commit=PIN,scope='Pillow attack, Minnie leave/door and tutorial; bounded source handlers, no generic interpreter',
  handlers=['text','actors/ready/idle','wait','talker','teleport','direction','stop_loop','move_queue','turn','shake','jump','animation','emote','camera','flags','battle','text/timer gate','ordered asynchronous restoration'],
  invariants=['Pillow branch optional; no melody prerequisite','Poltergeist parent disappears on doll_melody','minnie_leave only at source phrase6','minnie_door only at source phrase2','NPC event_positions apply on ready only; live teleport remains distinct','Looping movement and shake share source _looping','Repeated jumps wait after final jump too'],
  receipts={p.name:sha(p)for p in REPORT.glob('*-native.json')})
 (REPORT/'source-review.json').write_text(json.dumps(review,indent=2)+'\n')

def compile_linear(path,doc,end_duration,text_ids,turn_default,move_default,jump_default):
 labels=list(doc);out=[]
 def emit(kind,label='0',actor='None',**kw):out.append(dict(kind=kind,phrase=labels.index(label),actor=actor,**kw))
 emit('BeginCutscene')
 actors=doc['0']['actors'];require(actors==({'ninten':'leader','minnie':'Objects/npc3','pillow':'Objects/pillow'}if path==ATTACK else{'ninten':'leader','minnie':'Objects/npc3'}),'Pillow actor binding/order')
 allowed={'actors','actorsdir','actorsmove','teleportactors','wait','autoadvance','caninput','goto','actorsanim','actorsshake','actorsemote','actorsturn','changecam','returncam','stopactorsloop','talker','name','sound','text','actorsjump','showbox','setflags','startbattle','movecam'}
 for index,label in enumerate(labels):
  p=doc[label];require(not(set(p)-allowed),'Unknown Pillow source phrase field')
  require(p.get('goto')==(labels[index+1]if index+1<len(labels)else None),'Pillow source control flow')
  if 'text'in p:emit('ShowDialogue',label,p.get('talker','None').title(),dialogue_id=text_ids[path+'::'+label],flags=0 if'talker'in p else 1)
  if 'actors'in p:
   for actor in actors:emit('BindActor',label,actor.title());emit('ActorPersistent',label,actor.title())
   emit('YieldIdle',label)
  if 'wait'in p:emit('StartWait',label,duration=p['wait'])
  if 'talker'in p:emit('SetTalker',label,p['talker'].title())
  for actor,v in p.get('teleportactors',{}).items():
   require(set(v)=={'x','y'},'Pillow teleport');emit('TeleportActor',label,actor.title(),vector=[v['x'],v['y']])
  for actor,v in p.get('actorsdir',{}).items():
   require(set(v)=={'x','y'},'Pillow direction');emit('SetActorDirection',label,actor.title(),vector=[v['x'],v['y']])
  for actor,v in p.get('stopactorsloop',{}).items():
   require(v is None,'Pillow stop_loop payload');emit('StopActorLoop',label,actor.title())
  for actor,v in p.get('actorsmove',{}).items():
   require(set(v)>={'movement','speed','type'}and not(set(v)-{'movement','speed','type','animation','loop','queue'}),'Pillow move fields')
   require(v['type']in('position','step')and v.get('animation','')in('','Walk')and all(set(x)in({'x','y'},{'wait'})for x in v['movement']),'Pillow move entries')
   emit('MoveActorPath',label,actor.title(),path=v)
  for actor,v in p.get('actorsturn',{}).items():
   require(not(set(v)-{'x','y','speed','queue'})and {'x','y'}&set(v),'Pillow turn fields')
   emit('TurnActor',label,actor.title(),vector=[v.get('x',0),v.get('y',0)],duration=v.get('speed',turn_default),flags=int(v.get('queue',False)))
  for actor,v in p.get('actorsshake',{}).items():
   require(set(v)=={'x','length'},'Pillow shake fields');emit('ShakeActor',label,actor.title(),vector=[v['x'],0],duration=v['length'])
  for actor,v in p.get('actorsjump',{}).items():
   require(not(set(v)-{'height','length','times'})and 'height'in v,'Pillow jump fields')
   times=v.get('times',1);require(type(times)is int and 1<=times<=16,'Pillow jump count')
   emit('JumpActor',label,actor.title(),value=v['height'],duration=v.get('length',jump_default),flags=times-1)
  for actor,v in p.get('actorsanim',{}).items():
   require(actor=='pillow'and v=={'anim':'Idle'},'Pillow animation');emit('AnimateActor',label,'Pillow',clip='Pillow Idle')
  for actor,v in p.get('actorsemote',{}).items():
   require(actor=='minnie'and v in('surprise','sweat'),'Pillow emote');emit('EmoteActor',label,'Minnie',clip=v)
  if'changecam'in p:emit('ChangeCamera',label,p['changecam'].title());emit('YieldIdle',label)
  if'movecam'in p:
   require(set(p['movecam'])=={'x'},'Minnie partial camera');emit('MoveCamera',label,vector=[p['movecam']['x'],0],duration=move_default,flags=1,target_index=1)
  if'returncam'in p:emit('ReturnCamera',label,duration=p['returncam'])
  if'setflags'in p:
   require((path,label,p['setflags'])in[(ATTACK,'6','pillow_attack'),(LEAVE,'6','minnie_leave'),(DOOR,'2','minnie_door')],'Pillow flag timing')
   emit('SetFlag',label,flag=p['setflags'],value=1)
  if'startbattle'in p:
   require(path==ATTACK and label=='6'and p['startbattle']=={'battlers':[{'pillow':'pillow'}],'wincutscene':'Podunk/cutscenes/minnie_leave'},'Pillow battle boundary')
   emit('QueueBattle',label,'Pillow')
  if'text'in p:emit('AwaitDialogue',label)
  elif'wait'in p:
   require(p.get('autoadvance')is True and p.get('caninput')is False,'Pillow timer gate');emit('AwaitTimer',label)
  else:require(path==LEAVE and label=='6','Pillow terminal no-text phrase')
 last=labels[-1];talker='Pillow'if path==ATTACK else'Minnie'
 emit('StopInteraction',last,talker);emit('SetTalker',last)
 for actor in actors:emit('ReleaseBattleActor'if actor=='pillow'else'RestoreActor',last,actor.title())
 emit('CutsceneEnded',last);emit('DialogueDone',last,duration=0 if path==ATTACK else end_duration)
 if path==ATTACK:emit('RequestBattle',last,'Pillow')
 return out

def tutorial_graph(doc,translations,end):
 require(doc['0']['options']=={'DIALOGUE_PODUNK_MINNIE_RUN_TUTORIAL_2-OPT_0':'3','DIALOGUE_PODUNK_MINNIE_RUN_TUTORIAL_2-OPT_1':'7','cancel':'7'},'Minnie choices changed')
 out=[];labels={};phase=list(doc)
 def emit(kind,label,**kw):out.append(dict(kind=kind,phrase=phase.index(label),actor='None',**kw))
 emit('BeginCutscene','0')
 for label,p in doc.items():
  require(set(p)==({'name','sound','text','options'}if label=='0'else{'name','sound','text','goto'}if label=='3'else{'name','sound','text'}),'Minnie tutorial fields')
  labels[label]=len(out);emit('ShowDialogue',label,dialogue_key=TUTORIAL+'::'+label,flags=1)
  if label=='0':emit('AwaitChoices',label,choice_group=identity(TUTORIAL)+'::0')
  else:emit('AwaitDialogue',label)
  if label=='3':require(p['goto']=='7','Minnie tutorial edge');emit('Jump',label,target_label='7')
  elif label=='7':emit('SetTalker',label);emit('CutsceneEnded',label);emit('DialogueDone',label,duration=end)
 for c in out:
  if'target_label'in c:c['target_pc']=labels[c['target_label']]
 group=dict(id=identity(TUTORIAL)+'::0',program_identity=identity(TUTORIAL),source_label='0',program_command_count=len(out),initial_selection=0,cancel_target_label='7',cancel_target_pc=labels['7'],options=[dict(translation_key=k,text=translations[k],target_label=v,target_pc=labels[v])for k,v in doc['0']['options'].items()if k!='cancel'])
 return dict(identity=identity(TUTORIAL),commands=out,source_labels=phase,choice_groups=[group])

if __name__=='__main__':
 parser=argparse.ArgumentParser();parser.add_argument('--godot',type=Path,required=True);a=parser.parse_args();generate(a.godot)
