"""Source-limited getshovel candidate lowering; no shipping/frontend binding."""
import copy,csv,hashlib,io,json,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import graveyard_shovel as review,native_content as nc
from tools.extract_battle_entry import require

def lowered(p,ex,base):
 from tools import field_npcs
 from tools.doll_postwin import return_duration
 require(base['scene_id']==p['scene'],'Shovel Room source scene differs')
 doc=p['documents'][p['source']];npc=field_npcs.load_policy();require(npc['scene']in p['sources'],'Shovel NPC source map differs')
 owner=next((x for x in npc['actors']if x['path']==p['npc']),None);require(owner is not None,'Shovel source NPC has no admitted actor')
 actors={'ninten':base['sections']['Scene'][0]['player_instance_index'],'iggy':owner['id']-1}
 require(actors['iggy']<len(base['sections']['ActorInstance'])and base['sections']['ActorInstance'][actors['iggy']]['binding_kind']==2,'Shovel Iggy replacement binding differs')
 commands=[];labels={};patches=[];dialogues=[];talker=None;live=set(actors)
 def emit(kind,phrase,actor=None,**values):commands.append(dict(kind=kind,phrase=phrase,actor=actor,**values));return len(commands)-1
 emit('BeginCutscene',0)
 for actor in doc['0']['actors']:emit('BindActor',0,actor);emit('ActorPersistent',0,actor)
 emit('YieldIdle',0) # DialogueBox's actors clause yields after actor_ready.
 allowed=set('actors talker name sound text actorsdir setflags goto actorsturn changecam returncam showbox actorsmove wait autoadvance caninput objectsfunction eraseactors'.split())
 for phrase,(label,row)in enumerate(doc.items()):
  require(type(row)is dict and not(set(row)-allowed),'Unmapped shovel source phrase mechanism')
  require('actors'not in row or label=='0','Unmapped shovel actor rebinding');labels[label]=len(commands)
  if 'wait'in row:
   require(type(row['wait'])in(int,float)and 0<row['wait']<=60,'Shovel source wait');emit('StartWait',phrase,duration=row['wait'])
  if 'showbox'in row:require(row['showbox']is False,'Unmapped shovel box state');emit('HideDialogue',phrase,flags=1)
  if 'objectsfunction'in row:
   require(row['objectsfunction']=={p['object']:'queue_free'},'Unmapped shovel object method');emit('QueueFreeStoryObject',phrase,object_path=p['object'])
  if 'talker'in row:
   talker=row['talker'];require(talker in live,'Unknown shovel source talker');emit('SetTalker',phrase,talker)
  for actor,v in row.get('actorsdir',{}).items():
   require(actor in live and set(v)=={'x','y'},'Unmapped shovel direct actor direction');emit('SetActorDirection',phrase,actor,vector=[v['x'],v['y']])
  for actor,v in row.get('actorsmove',{}).items():
   require(actor in live and type(v)is dict and set(v)=={'movement','speed','animation','type'}and v['animation']=='Walk'and v['type']=='position','Unmapped shovel actor movement')
   require(type(v['movement'])is list and 0<len(v['movement'])<=16,'Shovel movement entry capacity');entries=[]
   for point in v['movement']:
    require(type(point)is dict,'Shovel movement entry type')
    if set(point)=={'wait'}:
     require(type(point['wait'])in(int,float)and 0<point['wait']<=60,'Shovel source movement wait');entries.append(dict(kind=1,vector=[0,0],duration=point['wait']))
    else:require(set(point)=={'x','y'},'Unmapped shovel movement operation');entries.append(dict(kind=0,vector=[point['x'],point['y']],duration=0))
   emit('MoveActorPath',phrase,actor,path=dict(entries=entries,speed=v['speed'],flags=0,animation=4))
  for actor,v in row.get('actorsturn',{}).items():
   require(actor in live and type(v)is dict and set(v)in({'x','y','speed'},{'x','y','speed','queue'})and type(v.get('queue',False))is bool,'Unmapped shovel source turn')
   emit('TurnActor',phrase,actor,vector=[v['x'],v['y']],duration=v['speed'],flags=int(v.get('queue',False)))
  for actor,value in row.get('eraseactors',{}).items():
   require(actor in live and actor!='ninten'and value is True,'Unmapped shovel actor erasure');emit('EraseActor',phrase,actor);live.remove(actor)
   if talker==actor:talker=None
  if 'changecam'in row:
   camera=row['changecam'];require(camera is None or camera in live,'Shovel source camera owner');emit('ChangeCamera',phrase,camera,flags=int(camera is None));emit('YieldIdle',phrase)
  if 'returncam'in row:emit('ReturnCamera',phrase,duration=row['returncam'])
  if 'setflags'in row:require(type(row['setflags'])is str,'Unmapped shovel flag form');emit('SetFlag',phrase,flag=row['setflags'],value=1)
  if 'text'in row:
   require(row.get('autoadvance',False)is False and row.get('caninput',True)is True,'Unmapped shovel manual dialogue')
   identity=p['dialogue_first_id']+len(dialogues);dialogues.append(dict(id=identity,source=p['source'],label=label));emit('ShowDialogue',phrase,talker,dialogue=identity,flags=int(talker is None));emit('AwaitDialogue',phrase,flags=int('wait'in row))
  elif 'wait'in row:
   require(row.get('autoadvance')is True and row.get('caninput')is False,'Unmapped shovel automatic wait');emit('AwaitTimer',phrase)
  else:require(label==list(doc)[-1]and 'goto'not in row,'Unmapped shovel automatic no-text phase')
  if 'goto'in row:require(type(row['goto'])is str and row['goto']in doc,'Shovel source goto');patches.append((emit('Jump',phrase,target=0),row['goto']))
 terminal=len(commands);phrase=len(doc)-1;emit('HideDialogue',phrase,flags=1);emit('SetTalker',phrase)
 for actor in actors:
  if actor in live:emit('RestoreActor',phrase,actor)
 emit('CutsceneEnded',phrase);emit('DialogueDone',phrase,duration=return_duration(ex.text('Scripts/UI/DialogueBox.gd')))
 for index,label in patches:
  require(labels[label]>index,'Shovel backward/unknown source goto');commands[index]['target']=labels[label]
 require(len(dialogues)==5 and live=={'ninten'},'Shovel complete text/live actor domain')
 return dict(actors=actors,source=p['source'],boundary=p['boundary'],commands=commands,dialogues=dialogues,labels=labels,terminal=terminal,groups=[])

def candidate(path=review.RECIPE):
 from tools import graveyard_tutorial_programme as existing
 review_path=Path(path).resolve();require(review_path.is_relative_to(ROOT),'Shovel review must have a checked workspace identity');review_name=review_path.relative_to(ROOT).as_posix()
 p,ex=review.read(path);base,_,base_proof=existing.build();ir=copy.deepcopy(nc.parse_pack(base));programme=lowered(p,ex,ir)
 existing_ids={row['target_index']for row in ir['sections']['Command']if row['opcode']==nc.OPCODES.index('ShowDialogue')};require(existing_ids.isdisjoint(v['id']for v in programme['dialogues']),'Shovel dialogue identity overlaps existing content')
 flags={ir['strings'][row['name_string']]for row in ir['sections']['Flag']};require({'got_shovel','returned_shovel'}<=flags,'Shovel source flags are not admitted')
 # The existing closed serializer owns typed actor/path/timer encoding. This
 # candidate adds only the checked object-path target before serialization.
 sections=ir['sections'];strings=ir['strings'];first=len(sections['Command'])
 for command in programme['commands']:
  if 'object_path'in command:
   name=command['object_path']
   if name not in strings:strings.append(name)
   command['target']=strings.index(name)
 index=existing.lower(programme,p,ex,ir,0);require(index==len(ir['sections']['Program'])-1 and sections['Command'][first+programme['labels']['3']]['opcode']==49,'Shovel candidate programme/delete ownership')
 texts(programme,p,ex) # Admit every locale/voice before freezing provenance.
 ir.pop('build_input_sha256');ir['capabilities']=14
 ir['provenance']={'sources':{**{'upstream/MOTHER-Encore/'+n:h for n,h in {**base_proof['sources'],**ex.sources}.items()},**base_proof['dependencies'],**{n:hashlib.sha256((ROOT/n).read_bytes()).hexdigest()for n in [review_name,'content/podunk-npcs.json','tools/graveyard_shovel.py','tools/graveyard_shovel_programme.py','tools/graveyard_tutorial_programme.py','tools/native_content.py']}}}
 blob,_=nc.compile_ir(ir);nc.parse_pack(blob)
 return blob,programme,p,ex

def texts(programme,p,ex):
 from tools.graveyard_story import checked_text
 rows={v['key']:v for v in csv.DictReader(io.StringIO(ex.text('Translations/TranslatedText/dialogue_Podunk_cutscenes - sheet.csv')))};out=[]
 for binding in programme['dialogues']:
  source=p['documents'][p['source']][binding['label']]
  for locale in ['en','zh_CN']:
   def text(key):require(key in rows and bool(rows[key].get(locale)),'Missing shovel translation');return checked_text(rows[key][locale].replace('\\n','\n'))
   voice='Audio/Sound effects/text/'+source['sound']+'.mp3'if 'sound'in source else''
   if voice:ex.data(voice)
   out.append(dict(id=binding['id'],locale='zh_Hans_CN'if locale=='zh_CN'else locale,speaker=text(source['name'])if 'name'in source else'',body=text(source['text']),voice=voice))
 return out

def source_texts(path=review.RECIPE):
 p,ex=review.read(path);bindings=[dict(id=p['dialogue_first_id']+i,source=p['source'],label=label)for i,(label,row)in enumerate((v for v in p['documents'][p['source']].items()if 'text'in v[1]))]
 require(len(bindings)==5,'Shovel complete dialogue domain')
 return texts(dict(dialogues=bindings),p,ex)

def add_room(path,scene,sections,strings,S):
 """Append the reviewed programme through the existing typed serializer."""
 from tools import graveyard_tutorial_programme as existing
 p,ex=review.read(path);programme=lowered(p,ex,dict(scene_id=scene,sections=sections,strings=strings))
 existing_ids={row['target_index']for row in sections['Command']if row['opcode']==nc.OPCODES.index('ShowDialogue')}
 require(existing_ids.isdisjoint(v['id']for v in programme['dialogues']),'Shovel dialogue identity overlaps existing content')
 for command in programme['commands']:
  if 'object_path'in command:command['target']=S(command['object_path'])
 index=existing.lower(programme,p,ex,dict(sections=sections,strings=strings),0)
 texts(programme,p,ex)
 name=Path(path).resolve().relative_to(ROOT).as_posix()
 dependencies={n:hashlib.sha256((ROOT/n).read_bytes()).hexdigest()for n in(name,'content/podunk-npcs.json','tools/graveyard_shovel.py','tools/graveyard_shovel_programme.py','tools/graveyard_tutorial_programme.py')}
 return index,{**{'upstream/MOTHER-Encore/'+n:h for n,h in ex.sources.items()},**dependencies}
