#!/usr/bin/env python3
"""Bounded offline lowering for reviewed field programmes; never a YAML VM."""
from __future__ import annotations
import argparse,csv,hashlib,io,json,re,struct,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.extract_battle_entry import Extractor,PIN,require,properties
from tools import bridge_entry,native_content as nc
POLICY=ROOT/'content/podunk-story.json';OUTPUT='world/podunk/story.encstory'
MAGIC=b'ENCFST01';STRIDES=(1,24,36,40,12,24);NIL=0xffffffff
def read():
 p=bridge_entry.read(POLICY)
 require(set(p)==set('schema kind commit scene_id sources documents actors programmes interactions encounters basic_overrides dialogues emotes voice_root locales limitations'.split()),'Field story recipe fields')
 require(type(p['schema'])is int and p['schema']==2 and p['kind']=='encore.field-story.reviewed-lowering'and p['commit']==PIN,'Field story recipe schema/pin')
 ex=Extractor(ROOT)
 for source,digest in p['sources'].items():require(hashlib.sha256(ex.data(source)).hexdigest()==digest,'Field story source review changed: '+source)
 from tools.programme_lowering_recipe import document
 for source in p['documents']:document(p,source,ex.yaml(source))
 require(type(p['scene_id'])is int and p['scene_id']>0 and all(type(a)is int and 0<=a<64 for a in p['actors'].values())and len(p['actors'])==len(set(p['actors'].values())),'Field story identities')
 require(p['locales']==['en','zh_CN'],'Unreviewed field story locale coverage')
 items=bridge_entry.read(ROOT/'content/native-items.json')['definitions']
 for row in p['basic_overrides']:
  require(set(row)=={'definition','source'}and type(row['definition'])is int and row['definition']>0,'Basic override fields');item=next(d for d in items if d['id']==row['definition']);doc=ex.yaml('Data/Items/'+item['source']+'.yaml')
  require(doc['slot']=='weapon'and row['source']=='Data/BattleSkills/'+doc['basic_skill']+'.yaml','Basic override source differs');ex.data(row['source'])
 for encounter in p['encounters']:
  require(all(type(encounter[k])is int for k in ('id','programme','actor','body_id','basic_slot'))and 0<=encounter['basic_slot']<16,'Field encounter identities/slot')
  require(all(d['equipment_slot']==encounter['basic_slot']for d in items if d['id']in{r['definition']for r in p['basic_overrides']}),'Weapon slot binding differs')
  system=ex.text('Scripts/UI/Battle/BattleSystem.gd');audio=ex.text('Scripts/global/audioManager.gd')
  duration=re.findall(r'audioManager.music_fadein\(0, audioManager.get_audio_player\(0\).volume_db, ([0-9.]+)\)',system)
  require(len(duration)==1 and type(encounter['return_music_seconds'])in(int,float)and encounter['return_music_seconds']==float(duration[0]),'Return music duration differs from source')
  require(encounter['return_music_curve']=='QuartOut'and '_tween.TRANS_QUART,_tween.EASE_OUT'in audio and 'SILENT_SOUND_THRESHOLD, volume, duration'in audio,'Return music source curve changed')
 for rule in p['interactions']:
  require(set(rule)==set('actor path programme flag turn_player turn_npc'.split()) and rule['actor']in p['actors'] and type(rule['programme'])is int and 0<=rule['programme']<len(p['programmes']) and not p['programmes'][rule['programme']]['boundary'] and type(rule['flag'])is str and all(type(rule[k])is bool for k in ('turn_player','turn_npc')),'Field interaction recipe fields')
  owners=bridge_entry.read(ROOT/'content/podunk-npcs.json')['actors'];owner=next((r for r in owners if r['alias']==rule['actor']),None)
  require(owner is not None and owner['path']==rule['path'] and owner['id']==p['actors'][rule['actor']]+1,'NPC source actor identity differs')
  from tools.extract_battle_entry import node
  npc=node(ex.text('Maps/podunk/podunk.tscn'),rule['path'])
  choices=[['',npc['dialog']]]+npc['_all_dialog']
  expected=next((choice[1]for choice in choices if choice[0]==rule['flag']and len(choice)==2),None)
  require(expected is not None and 'Data/Dialogue/'+expected+'.yaml'==p['programmes'][rule['programme']]['source'],'NPC source dialogue selection differs')
  require(npc.get('staring',False) and npc.get('turn_to_player_on_interact',True)==rule['turn_npc'] and npc.get('player_turn',{'x':True,'y':True})=={'x':rule['turn_player'],'y':rule['turn_player']},'NPC interaction turn/restore mode differs')
 return p,ex
def table(ex,path):return {r['key']:r for r in csv.DictReader(io.StringIO(ex.text(path)))}
def dialogues(p,ex):
 out=[]
 for binding in p['dialogues']:
  require(set(binding)=={'id','source','label'}and type(binding['id'])is int and binding['id']>0,'Field dialogue binding schema')
  doc=p['documents'][binding['source']][binding['label']]
  path='Translations/TranslatedText/'+('dialogue_Podunk_cutscenes'if '/cutscenes/'in binding['source']else'dialogue_Podunk')+' - sheet.csv';rows=table(ex,path)
  for locale in p['locales']:
   def text(key):
    value=rows[key][locale]or rows[key]['en'];value=value.replace('\\n','\n')
    require(all(tag in ('[@]','[WAIT@]','[color]','[/color]','[DELAY]','[Ninten]','[Pippi]')for tag in re.findall(r'\[[^]]+\]',value)),'Unreviewed field dialogue token')
    require(value.count('[color]')==value.count('[/color]'),'Unbalanced field hint color');return value
   voice=p['voice_root']+doc['sound']+'.mp3';ex.data(voice)
   out.append(dict(id=binding['id'],locale='zh_Hans_CN'if locale=='zh_CN'else locale,speaker=text(doc['name']),body=text(doc['text']),voice=voice))
 return out
def audio_sources():
 p,ex=read();rows=dialogues(p,ex);values={r['voice']for r in rows}
 values.update(c['audio']for prog in p['programmes']for c in prog['commands']if 'audio'in c)
 values.update(b['music']for b in p['encounters'])
 return sorted(values)
def emote(ex,name,number,sections,frame_count):
 source=ex.text('Nodes/Ui/emotes.tscn');pattern=r'^\[sub_resource type="Animation" id='+str(number)+r'\]\n(.*?)(?=^\[|\Z)';match=re.search(pattern,source,re.M|re.S)
 require(match is not None,'Missing source emote');data=properties(match[1]);require(data['resource_name']==name and data.get('loop',False)==False,'Emote binding changed')
 tracks={}
 for key,value in data.items():
  if key.startswith('tracks/'):
   _,index,field=key.split('/');tracks.setdefault(int(index),{})[field]=value
 require(set(tracks)==set(range(len(tracks)))and len(tracks)in(1,2),'Emote tracks changed')
 for index,t in tracks.items():
  require(t['enabled']and not t['imported']and t['type']=='value'and t['path']==('.:frame'if index==0 else'.:offset'),'Unsupported emote track')
  if index:require(t['keys']['values']==[[0,10],[0,0]]and t['keys']['times']==[0,data['length']],'Changed reviewed visual offset difference')
 keys=tracks[0]['keys'];first=len(sections['Key']);sections['Key'].extend(dict(time=t,frame=f)for t,f in zip(keys['times'],keys['values']))
 index=len(sections['Clip']);sections['Clip'].append(dict(stable_id=max((r['stable_id']for r in sections['Clip']),default=0)+1,length=data['length'],first_key=first,key_count=len(keys['times']),flags=16 if keys['times'][0]>0 else 0,visibility=0,frame_count=frame_count,channel=1));return index
def add_room(sections,strings,S):
 p,ex=read();require(p['scene_id']==sections['Scene'][0]['stable_id'],'Field story scene differs')
 actors=p['actors'];require(set(actors.values())==set(range(len(sections['ActorInstance']))),'Field story actor owner differs')
 flags={strings[f['name_string']]:i for i,f in enumerate(sections['Flag'])};clips={};battle_indices={}
 def resource(path,kind):
  index=len(sections['Resource']);file=ROOT/'romfs'/path if kind!=2 else ex.upstream/path;require(file.is_file(),'Missing field story resource '+path)
  if kind==2:ex.data(path)
  sections['Resource'].append(dict(stable_id=max(r['stable_id']for r in sections['Resource'])+1,path_string=S(('res://'if kind==2 else'')+path),width=0,height=0,columns=0,rows=0,kind=kind,flags=0,sha256=hashlib.sha256(file.read_bytes()).hexdigest()));return index
 audio={}
 for b in p['encounters']:
  require(set(b)==set('id programme actor enemy winflag entry round basic music body_id basic_slot return_music_seconds return_music_curve'.split()),'Encounter recipe schema')
  index=len(sections['Battle']);battle_indices[b['id']]=index;sections['Battle'].append(dict(stable_id=b['id'],enemy_string=S(b['enemy']),actor_instance_index=b['actor'],win_flag_index=flags[b['winflag']],advantage=0,flags=8,win_cutscene_string=0,battle_resource_index=resource(b['entry'],3)))
  owner=next(v for k,v in p['documents'][p['programmes'][b['programme']]['source']]['0']['actors'].items()if actors[k]==b['actor'])
  sections['BodyRule'].append(dict(body_id=b['body_id'],source_path_string=S(owner),initially_enabled=1,flags=0))
 sections['Scene'][0]['body_rule_count']=len(sections['BodyRule'])
 for number,prog in enumerate(p['programmes']):
  require(set(prog)=={'source','boundary','commands'}and prog['source']in p['documents'],'Programme recipe schema');first=len(sections['Command'])
  for source in prog['commands']:
   require(set(source)<=set('kind actor phrase vector value duration flags target emote audio path flag dialogue battle'.split())and {'kind','actor','phrase'}<=set(source),'Field command recipe schema')
   kind=source['kind'];require(kind in nc.OPCODES,'Unknown field opcode')
   c=dict(opcode=nc.OPCODES.index(kind),actor_index=65535 if source['actor']is None else actors[source['actor']],phrase=source['phrase'],target_index=source.get('target',NIL),flags=source.get('flags',0),vector=source.get('vector',[0,0]),value=source.get('value',0),duration=source.get('duration',0),auxiliary_index=NIL)
   if 'emote'in source:
    name=source['emote'];profile=sections['ActorProfile'][sections['ActorInstance'][c['actor_index']]['profile_index']];r=sections['Resource'][profile['emote_resource']];frames=r['columns']*r['rows']
    if name not in clips:clips[name]=emote(ex,name,p['emotes'][name],sections,frames)
    c['target_index']=clips[name]
   if 'audio'in source:
    path=source['audio']
    if path not in audio:audio[path]=resource(path,2)
    c['target_index']=audio[path]
   if 'flag'in source:c['target_index']=flags[source['flag']]
   if 'dialogue'in source:c['target_index']=source['dialogue']
   if 'battle'in source:c['target_index']=battle_indices[source['battle']]
   if 'path'in source:
    path=source['path'];require(set(path)=={'entries','speed','flags','animation'},'Field movement recipe fields');start=len(sections['MovementEntry']);sections['MovementEntry'].extend(path['entries']);c['target_index']=len(sections['MovementPath']);sections['MovementPath'].append(dict(stable_id=c['target_index']+1,first_entry=start,entry_count=len(path['entries']),flags=path['flags'],animation_motion=path['animation'],speed=path['speed']))
   sections['Command'].append(c)
  sections['Program'].append(dict(stable_id=number+1,first_command=first,command_count=len(prog['commands']),phrase_count=max(len(p['documents'][prog['source']]),1+max(c['phrase']for c in prog['commands'])),source_path_string=S(prog['source'].removeprefix('Data/Dialogue/').removesuffix('.yaml'))))
 return {**{'upstream/MOTHER-Encore/'+s:h for s,h in ex.sources.items()},'content/podunk-story.json':bridge_entry.sha(POLICY),'tools/field_story.py':bridge_entry.sha(__file__)}
def build():
 from tools.podunk_field import pack_container,unpack_container
 p,ex=read();pool=bytearray();cache={}
 def ref(value):
  if not value:return (0,0)
  if value not in cache:cache[value]=(len(pool),len(value.encode()));pool.extend(value.encode())
  return cache[value]
 programmes=[struct.pack('<5I I',i,*ref(row['boundary']),*ref(row['source'].removeprefix('Data/Dialogue/').removesuffix('.yaml')),0 if row['boundary']else 1)for i,row in enumerate(p['programmes'])]
 texts=[struct.pack('<9I',r['id'],*ref(r['locale']),*ref(r['speaker']),*ref(r['body']),*ref(r['voice']))for r in dialogues(p,ex)]
 encounters=[struct.pack('<8IfI',b['id'],b['actor'],*ref(b['basic']),*ref(b['music']),b['basic_slot'],b['programme'],b['return_music_seconds'],{'Linear':1,'QuartIn':2,'QuartOut':3}[b['return_music_curve']])for b in p['encounters']]
 overrides=[struct.pack('<3I',r['definition'],*ref(r['source']))for r in p['basic_overrides']]
 interactions=[struct.pack('<6I',p['actors'][r['actor']],r['programme'],*ref(r['flag']),int(r['turn_player'])|int(r['turn_npc'])<<1,0)for r in p['interactions']]
 blob=pack_container(MAGIC,p['scene_id'],[bytes(pool),b''.join(programmes),b''.join(texts),b''.join(encounters),b''.join(overrides),b''.join(interactions)],STRIDES,128*1024,capability=2)
 unpack_container(blob,MAGIC,STRIDES,128*1024,capability=2);return blob
def stage_files(source_root=ROOT/'romfs'):
 blob=build();require((source_root/OUTPUT).read_bytes()==blob,'Stale field story pack');return {Path(OUTPUT):blob}
def main():
 parser=argparse.ArgumentParser();parser.add_argument('action',choices=('compile','verify'));args=parser.parse_args();blob=build();path=ROOT/'romfs'/OUTPUT
 if args.action=='verify':require(path.read_bytes()==blob,'Stale field story resource')
 else:path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(blob)
 print('Field story:',args.action,len(blob),'bytes; checked source programmes and text only')
if __name__=='__main__':
 try:main()
 except (ValueError,KeyError,TypeError,OSError)as e:print('FIELD STORY ERROR:',e,file=sys.stderr);raise SystemExit(1)
