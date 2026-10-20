"""Bounded Iggy graph admission and candidate compiler; no release binding.

The authored command list is checked against the pinned reviewed document,
including every control-flow edge. No source field may silently disappear.
"""
import argparse,copy,csv,hashlib,io,json,struct
from pathlib import Path
from tools import graveyard_tutorial as review,native_content as nc
from tools.extract_battle_entry import require
from tools.programme_lowering_recipe import equal
ROOT=Path(__file__).resolve().parents[1]
RECIPE=ROOT/'content/graveyard-swing-programme.json'
BASE=ROOT/'tests/fixtures/pre-key-grant-room.encroom'

def read(path=RECIPE):
 def unique(pairs):
  out={}
  for k,v in pairs:require(k not in out,'Duplicate tutorial programme field');out[k]=v
  return out
 def invalid(v):raise ValueError('Nonfinite tutorial value')
 p=json.loads(Path(path).read_text(encoding='utf8'),object_pairs_hook=unique,parse_constant=invalid)
 require(type(p)is dict and set(p)==set('schema kind review actors source boundary commands dialogues groups labels terminal scope'.split()),'Tutorial programme fields')
 require(type(p['schema'])is int and p['schema']==1 and p['kind']=='encore.graveyard-tutorial.lowered-candidate'and p['review']=='content/graveyard-tutorial.json','Tutorial programme schema/review')
 require(type(p['scope'])is str and 0<len(p['scope'])<=2048,'Tutorial programme scope')
 r,ex=review.read(ROOT/p['review']);doc=r['documents'][r['source']]
 require(p['source']==r['source']and p['boundary']==r['boundary'],'Tutorial programme source boundary')
 from tools import field_npcs
 # This consumer needs the stable actor binding, not rebuilt sprite sheets or
 # movement clips. Room generation independently admits those NPC consumers.
 policy=field_npcs.load_policy();owner=next(x for x in policy['actors']if x['path']==r['npc'])
 aliases=doc['0']['actors'];require(type(p['actors'])is dict and set(p['actors'])==set(aliases),'Tutorial actor alias coverage')
 for name,source in aliases.items():
  expected=0 if source=='leader'else owner['id']-1 if source==r['npc']else None
  require(expected is not None and type(p['actors'][name])is int and p['actors'][name]==expected,'Tutorial actor source identity')
 require(type(p['dialogues'])is list and p['dialogues'],'Tutorial dialogue bindings')
 ids={};seen=set()
 for row in p['dialogues']:
  require(type(row)is dict and set(row)=={'id','source','label'}and type(row['id'])is int and 0<row['id']<1000000 and row['id']not in seen and row['source']==r['source']and row['label']in doc and 'text'in doc[row['label']]and row['label']not in ids,'Tutorial dialogue binding')
  ids[row['label']]=row['id'];seen.add(row['id'])
 require(list(ids)==[label for label,row in doc.items()if 'text'in row],'Tutorial dialogue order/coverage')
 cs=[];labels={};groups=[];patches=[];talker=None
 def emit(kind,phrase,actor=None,**kw):cs.append(dict(kind=kind,actor=actor,phrase=phrase,**kw));return len(cs)-1
 emit('BeginCutscene',0)
 for actor in aliases:emit('BindActor',0,actor);emit('ActorPersistent',0,actor)
 emit('YieldIdle',0) # DialogueBox actors clause yields after actor_ready.
 for phrase,(label,row)in enumerate(doc.items()):
  require(not(set(row)-set('actors talker name sound text goto changecam returncam showbox actorsmove wait autoadvance caninput actorsdir movecam actorsturn options item soundeffect setflags'.split())),'Unreviewed tutorial phrase mechanism')
  require('actors'not in row or label=='0','Tutorial actor rebinding unsupported')
  labels[label]=len(cs)
  if 'item'in row:
   grants=[g for g in r['item_grants']if g['label']==label];require(len(grants)==1,'Unreviewed tutorial item grant')
   emit('GrantKeyItem',phrase,item=row['item'],value=grants[0]['doses'])
  if 'wait'in row:
   require(type(row['wait'])in(int,float)and 0<row['wait']<=60,'Tutorial wait duration');emit('StartWait',phrase,duration=row['wait'])
  if 'showbox'in row:require(row['showbox']is False,'Unreviewed tutorial box state');emit('HideDialogue',phrase,flags=1)
  if 'talker'in row:
   talker=row['talker'];require(talker is None or talker in aliases,'Unknown tutorial talker');emit('SetTalker',phrase,talker)
  if 'soundeffect'in row:
   audio='Audio/Sound effects/'+row['soundeffect'];ex.data(audio);emit('PlaySound',phrase,audio=audio)
  for actor,v in row.get('actorsdir',{}).items():
   require(actor in aliases and set(v)=={'x','y'},'Tutorial source direction');emit('SetActorDirection',phrase,actor,vector=[v['x'],v['y']])
  for actor,v in row.get('actorsmove',{}).items():
   require(actor in aliases and set(v)=={'movement','speed','animation','type'}and v['animation']=='Walk'and v['type']=='position','Unreviewed tutorial actor movement')
   require(type(v['movement'])is list and 0<len(v['movement'])<=16 and all(type(x)is dict and set(x)=={'x','y'}for x in v['movement']),'Tutorial movement points')
   emit('MoveActorPath',phrase,actor,path=dict(entries=[dict(kind=0,vector=[x['x'],x['y']],duration=0)for x in v['movement']],speed=v['speed'],flags=0,animation=4))
  for actor,v in row.get('actorsturn',{}).items():
   require(actor in aliases and set(v)in({'x','y','speed'},{'x','y','speed','queue'})and type(v.get('queue',False))is bool,'Tutorial source turn')
   emit('TurnActor',phrase,actor,vector=[v['x'],v['y']],duration=v['speed'],flags=int(v.get('queue',False)))
  if 'changecam'in row:
   camera=row['changecam'];require(camera is None or camera in aliases,'Tutorial camera owner');emit('ChangeCamera',phrase,camera,flags=int(camera is None));emit('YieldIdle',phrase)
  if 'movecam'in row:
   camera=row['movecam'];require(set(camera)=={'x','y'},'Unreviewed tutorial camera movement')
   source=ex.text('Scripts/UI/DialogueBox.gd');require('var time = 1.0'in source and '_curr_phrase["movecam"].get("trans", "sine")'in source and '_curr_phrase["movecam"].get("ease", "out")'in source,'Tutorial camera source default changed')
   emit('MoveCamera',phrase,vector=[camera['x'],camera['y']],duration=1,target=1)
  if 'returncam'in row:emit('ReturnCamera',phrase,duration=row['returncam'])
  if 'setflags'in row:require(type(row['setflags'])is str,'Tutorial flag form');emit('SetFlag',phrase,flag=row['setflags'],value=1)
  if 'text'in row:emit('ShowDialogue',phrase,talker,dialogue=ids[label],flags=int(talker is None))
  if 'options'in row:
   choices=row['options'];require('text'in row and 'goto'not in row and len(choices)==3 and 'cancel'in choices,'Tutorial choice mechanism')
   emit('AwaitChoices',phrase,choices=len(groups));groups.append(dict(id='iggy-swing-choice-'+str(len(groups)),program_identity=r['source'].removeprefix('Data/Dialogue/').removesuffix('.yaml'),source_label=label,program_command_count=0,initial_selection=0,cancel_target_pc=choices['cancel'],options=[dict(translation_key=k,text='',target_pc=v)for k,v in choices.items()if k!='cancel']))
  elif 'text'in row:
   require(row.get('autoadvance',False)is False and row.get('caninput',True)is True,'Unreviewed tutorial manual input');emit('AwaitDialogue',phrase,flags=int('wait'in row))
  else:
   require(row.get('autoadvance')is True and row.get('caninput')is False and 'wait'in row,'Unreviewed tutorial automatic input');emit('AwaitTimer',phrase)
  if 'goto'in row:patches.append((emit('Jump',phrase,flags=0,target=0),row['goto']))
 terminal=len(cs);phrase=len(doc)-1
 emit('HideDialogue',phrase,flags=1);emit('SetTalker',phrase)
 for actor in aliases:emit('RestoreActor',phrase,actor)
 emit('CutsceneEnded',phrase)
 from tools.doll_postwin import return_duration
 emit('DialogueDone',phrase,duration=return_duration(ex.text('Scripts/UI/DialogueBox.gd')))
 for pc,label in patches:
  require(label in labels,'Tutorial goto label');target=labels[label];cs[pc]['target']=target;cs[pc]['flags']=int(target<pc)
 for g in groups:
  require(g['cancel_target_pc']in labels and all(o['target_pc']in labels for o in g['options']),'Tutorial choice labels')
  g['program_command_count']=len(cs);g['cancel_target_pc']=labels[g['cancel_target_pc']]
  for o in g['options']:o['target_pc']=labels[o['target_pc']]
 require(equal(p['commands'],cs)and equal(p['labels'],labels)and equal(p['groups'],groups)and type(p['terminal'])is int and p['terminal']==terminal,'Tutorial lowering differs from pinned source graph/order')
 return p,r,ex

def lower(p,r,ex,ir,choice_first):
 s=ir['sections'];strings=ir['strings'];flags={strings[v['name_string']]:i for i,v in enumerate(s['Flag'])}
 def S(v):
  if v not in strings:strings.append(v)
  return strings.index(v)
 audio={};first=len(s['Command'])
 for row in p['commands']:
  c=dict(opcode=nc.OPCODES.index(row['kind']),actor_index=nc.NO_ACTOR if row['actor']is None else p['actors'][row['actor']],phrase=row['phrase'],target_index=row.get('target',nc.NONE),auxiliary_index=nc.NONE,flags=row.get('flags',0),vector=row.get('vector',[0,0]),value=row.get('value',0),duration=row.get('duration',0))
  for field in ('dialogue','choices','item','flag'):
   if field in row:c['target_index']={'dialogue':lambda:row[field],'choices':lambda:row[field]+choice_first,'item':lambda:S(row[field]),'flag':lambda:flags[row[field]]}[field]()
  if 'path'in row:
   path=row['path'];start=len(s['MovementEntry']);s['MovementEntry'].extend(copy.deepcopy(path['entries']));c['target_index']=len(s['MovementPath']);s['MovementPath'].append(dict(stable_id=max((x['stable_id']for x in s['MovementPath']),default=0)+1,first_entry=start,entry_count=len(path['entries']),flags=path['flags'],animation_motion=path['animation'],speed=path['speed']))
  if 'audio'in row:
   path=row['audio']
   if path not in audio:
    audio[path]=len(s['Resource']);s['Resource'].append(dict(stable_id=max(x['stable_id']for x in s['Resource'])+1,path_string=S('res://'+path),width=0,height=0,columns=0,rows=0,kind=2,flags=0,sha256=hashlib.sha256(ex.data(path)).hexdigest()))
   c['target_index']=audio[path]
  s['Command'].append(c)
 programme=len(s['Program']);s['Program'].append(dict(stable_id=max(x['stable_id']for x in s['Program'])+1,first_command=first,command_count=len(p['commands']),phrase_count=len(r['documents'][r['source']]),source_path_string=S(r['source'].removeprefix('Data/Dialogue/').removesuffix('.yaml'))))
 return programme

def texts(p,r,ex):
 from tools.graveyard_story import checked_text
 rows={x['key']:x for x in csv.DictReader(io.StringIO(ex.text('Translations/TranslatedText/dialogue_Podunk_cutscenes - sheet.csv')))};result=[]
 for b in p['dialogues']:
  doc=r['documents'][r['source']][b['label']]
  for locale in ('en','zh_CN'):
   def text(k):
    require(k in rows and locale in rows[k]and bool(rows[k][locale]),'Tutorial translation missing');return checked_text(rows[k][locale].replace('\\n','\n'))
   voice='Audio/Sound effects/text/'+doc['sound']+'.mp3'if 'sound'in doc else''
   if voice:ex.data(voice)
   result.append(dict(id=b['id'],locale='zh_Hans_CN'if locale=='zh_CN'else locale,speaker=text(doc['name'])if 'name'in doc else'',body=text(doc['text']),voice=voice))
 return result

def choices(groups,ex):
 from tools import dialogue_choice_assets as c
 r=json.loads(c.RECIPE.read_bytes());c.verify_recipe(r);r=copy.deepcopy(r);r['groups']=copy.deepcopy(groups);r['extra_option_rects']=[r['option_rects']for _ in groups[1:]]
 tables={}
 for g in r['groups']:
  sheet='dialogue_Podunk_cutscenes'if '/cutscenes/'in g['program_identity']else'dialogue_Podunk'
  if sheet not in tables:tables[sheet]={x['key']:x for x in csv.DictReader(io.StringIO(ex.text('Translations/TranslatedText/'+sheet+' - sheet.csv')))}
  for o in g['options']:
   row=tables[sheet].get(o['translation_key']);require(row is not None and row['en'],'Tutorial choice translation missing');o['text']=row['en']
 return c.encode(r)

def build(path=RECIPE):
 from tools import field_story,graveyard_story,podunk_field as f
 p,r,ex=read(path);base=nc.parse_pack(BASE.read_bytes());ir=copy.deepcopy(base);flags={ir['strings'][x['name_string']]:i for i,x in enumerate(ir['sections']['Flag'])}
 fp,fe=field_story.read();require(base['scene_id']==r['scene']==fp['scene_id'],'Tutorial frozen/source scene differs');programmes=[dict(programme=i,boundary=v['boundary'],source=v['source'].removeprefix('Data/Dialogue/').removesuffix('.yaml'),flags=int(not v['boundary']))for i,v in enumerate(fp['programmes'])]
 text=field_story.dialogues(fp,fe);interactions=[dict(actor=fp['actors'][v['actor']],programme=v['programme'],flag=v['flag'],turns=int(v['turn_player'])|int(v['turn_npc'])<<1,status=0,source=fp['programmes'][v['programme']]['source'].removeprefix('Data/Dialogue/').removesuffix('.yaml'))for v in fp['interactions']]
 supplement,extra,tx,ix,_=graveyard_story.payload(ROOT/fp['supplement'],flags,len(programmes));_,ge,_,_,_,_,_,groups=graveyard_story.lowered(ROOT/fp['supplement'],flags)
 for v in extra:programmes.append(dict(programme=len(programmes),boundary='',source=v['source'].removeprefix('Data/Dialogue/').removesuffix('.yaml'),flags=1))
 text.extend(tx);interactions.extend(ix)
 for v in fp['continuations']:programmes.append(dict(programme=len(programmes),boundary=v['boundary'],source=v['source'].removeprefix('Data/Dialogue/').removesuffix('.yaml'),flags=0))
 require(len(programmes)==len(ir['sections']['Program']),'Tutorial frozen Room/programme prefix differs')
 for i,v in enumerate(programmes):require(ir['strings'][ir['sections']['Program'][i]['source_path_string']]==v['source'],'Tutorial source prefix identity differs')
 index=lower(p,r,ex,ir,len(groups));programmes.append(dict(programme=index,boundary=p['boundary'],source=p['source'].removeprefix('Data/Dialogue/').removesuffix('.yaml'),flags=0));text.extend(texts(p,r,ex));groups+=copy.deepcopy(p['groups'])
 require(len({(v['id'],v['locale'])for v in text})==len(text),'Tutorial duplicate text namespace')
 sources={**fe.sources,**ge.sources,**ex.sources};dependencies={name:hashlib.sha256((ROOT/name).read_bytes()).hexdigest()for name in ['content/podunk-story.json',fp['supplement'],'content/graveyard-tutorial.json','content/graveyard-swing-programme.json','tests/fixtures/pre-key-grant-room.encroom','tools/graveyard_tutorial.py','tools/graveyard_tutorial_programme.py','tools/field_story.py','tools/graveyard_story.py','tools/mayor_story.py','tools/native_content.py']}
 ir.pop('build_input_sha256');ir['capabilities']=13;ir['provenance']={'sources':{**{'upstream/MOTHER-Encore/'+k:v for k,v in sources.items()},**dependencies}}
 room,_=nc.compile_ir(ir);nc.parse_pack(room)
 pool=bytearray();cache={}
 def ref(v):
  if not v:return(0,0)
  if v not in cache:cache[v]=(len(pool),len(v.encode()));pool.extend(v.encode())
  return cache[v]
 ps=[struct.pack('<6I',v['programme'],*ref(v['boundary']),*ref(v['source']),v['flags'])for v in programmes]
 ts=[struct.pack('<9I',v['id'],*ref(v['locale']),*ref(v['speaker']),*ref(v['body']),*ref(v['voice']))for v in text]
 es=[struct.pack('<8IfI',v['id'],v['actor'],*ref(v['basic']),*ref(v['music']),v['basic_slot'],v['programme'],v['return_music_seconds'],{'Linear':1,'QuartIn':2,'QuartOut':3}[v['return_music_curve']])for v in fp['encounters']]
 os=[struct.pack('<3I',v['definition'],*ref(v['source']))for v in fp['basic_overrides']]
 xs=[struct.pack('<8I',v['actor'],v['programme'],*ref(v['flag']),v['turns'],v['status'],*ref(v['source']))for v in interactions]
 choice=choices(groups,ex)
 story=f.pack_container(b'ENCFST01',r['scene'],[bytes(pool),b''.join(ps),b''.join(ts),b''.join(es),b''.join(os),b''.join(xs),choice],(1,24,36,40,12,32,1),128*1024,capability=4)
 return room,story,dict(scope='Candidate only: full source tutorial graph; no action/render/contact or release binding',programme=index,commands=len(p['commands']),phrases=len(p['labels']),dialogues=len(p['dialogues']),choice_first=len(groups)-len(p['groups']),base_sha256=hashlib.sha256(BASE.read_bytes()).hexdigest(),sources=sources,dependencies=dependencies)

def main():
 a=argparse.ArgumentParser();a.add_argument('--out',type=Path,required=True);o=a.parse_args();out=o.out.resolve();require(out.is_relative_to(ROOT/'build')and out!=ROOT/'build','Tutorial output must remain in owned build directory')
 room,story,proof=build();out.mkdir(parents=True,exist_ok=True)
 for name,blob in [('podunk.encroom',room),('story.encstory',story)]:
  (out/name).write_bytes(blob);proof.setdefault('outputs',{})[name]=dict(bytes=len(blob),sha256=hashlib.sha256(blob).hexdigest())
 (out/'source.json').write_text(json.dumps(proof,indent=2)+'\n',encoding='utf8');print('Iggy source tutorial candidate:',len(room),len(story),'bytes; not staged')
if __name__=='__main__':main()
