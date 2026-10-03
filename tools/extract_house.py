#!/usr/bin/env python3
"""Bounded, explicit extraction of house doors, initial NPCs, and the bounded Doll story edge."""
import argparse,csv,hashlib,io,json,re,sys
from pathlib import Path
from extract_battle_entry import Extractor,ROOT,PIN,node,one,properties,animation,require
sys.path.insert(0,str(ROOT))
from tools.melody_dialogue import MELODY,GUARD,DEFAULT,AFTER,REPEAT,literal_phrase
HOUSE='Maps/podunk/Nintens House.tscn'
SUPPORTED=['Ninten_upstairs','Upstairs_Ninten','Upstairs_Living','Living_Upstairs','Upstair_Sister','Sister_Upstair','Upstair_Mom','Mom_Upstair']
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def build(root=ROOT):
 ex=Extractor(root);house=ex.text(HOUSE);ds=ex.text('Nodes/Overworld/Door.tscn');dg=ex.text('Scripts/Main/Door.gd');fade=ex.text('Nodes/Ui/effects/Fade.tscn')
 for path in ['Scripts/global/SceneTransition.gd','Nodes/Ui/effects/Fade.gd','Scripts/Main/party/Player.gd','Scripts/Main/party/party_object.gd','Scripts/Main/character_sprite.gd','Scripts/global/text_tools.gd','Scripts/UI/DialogueBox.gd']:ex.text(path)
 require('_new_pos.global_position - Vector2(0, 7)'in dg,'Door destination mechanism changed')
 require(node(house,'Doors')=={} and 'position'not in node(house,'.')and 'scale'not in node(house,'.'),'Unexpected ancestor transform')
 shape=properties(one(r'^\[sub_resource type="RectangleShape2D" id=1\]\n(.*?)(?=^\[)',ds,'door shape',re.M|re.S)[1])['extents']
 inherited=node(ds,'CollisionShape2D')['position'];defaults={}
 for k in ['fade_in_speed','fade_out_speed']:defaults[k]=float(one(r'^export var '+k+r' := ([0-9.]+)',dg,k)[1])
 fi=animation(fade,7,'Nodes/Ui/effects/Fade.tscn','Fade In');fo=animation(fade,8,'Nodes/Ui/effects/Fade.tscn','Fade Out')
 require(fi['tracks'][0]['keys']['values']==[1.,0.] and fo['tracks'][0]['keys']['values']==[0.,1.],'Unsupported fade curves')
 require(fo['tracks'][4]['keys']['values'][0]['method']=='_on_fade_out_mostly_done','Fade callback changed')
 fade_fields=dict(fade_in_length=fi['length'],fade_in_opaque=fi['tracks'][0]['keys']['times'][-1],fade_out_length=fo['length'],fade_out_mostly=fo['tracks'][4]['keys']['times'][0],**defaults,color=[0,0,0,1])
 doors=[];boundaries=[]
 names=re.findall(r'^\[node name="([^"]+)" parent="Doors" instance=',house,re.M)
 for name in names:
  path='Doors/'+name;d=node(house,path);pos=d['position'];scale=d.get('scale',[1,1]);child=inherited
  try:child=node(house,path+'/CollisionShape2D')['position']
  except ValueError:pass
  center=[pos[i]+scale[i]*child[i]for i in range(2)];extents=[abs(scale[i])*shape[i]for i in range(2)]
  if name not in SUPPORTED:
   boundaries.append(dict(id=len(boundaries)+1,source_path=path,kind=2 if d.get('targetScene')else 1,center=center,extents=extents));continue
  require(not d.get('targetScene')and not d.get('flag_set'),'Unsupported door state mutation')
  target=node(house,path+'/Position2D')['position'];destination=[pos[i]+scale[i]*target[i]-(7 if i==1 else 0)for i in range(2)]
  sound=lambda k:''if d.get(k,'None')=='None'else 'Audio/Sound effects/'+d[k]
  doors.append(dict(id=SUPPORTED.index(name)+1,source_path=path,start_sound=sound('sound'),end_sound=sound('end_sound'),center=center,extents=extents,destination=destination,direction=d['dir'],**fade_fields))
 doors.sort(key=lambda d:d['id'])
 ns=ex.text('Nodes/Reusables/npc.tscn');ng=ex.text('Scripts/Main/npc.gd');npc=node(house,'Objects/npc');base=node(ns,'.');area=node(ns,'interact');shape_node=node(ns,'interact/CollisionShape2D')
 ie=properties(one(r'^\[sub_resource type="RectangleShape2D" id=3\]\n(.*?)(?=^\[)',ns,'interact shape',re.M|re.S)[1])['extents']
 view=node(ns,'ViewArea/CollisionShape2D2');radius=float(one(r'^radius = ([0-9.]+)',ns,'staring radius')[1]);ret=float(one(r'func _on_ViewArea_body_exited[\s\S]*?create_timer\(([0-9.]+)\)',ng,'staring reset delay')[1])
 ir_room=json.loads((Path(root)/'content/native-opening.json').read_text());body=[b for b in ir_room['sections']['BodyRule']if ir_room['strings'][b['source_path_string']]=='Objects/npc'];require(len(body)==1,'NPC room body binding')
 yaml=ex.yaml('Data/Dialogue/'+npc['dialog']+'.yaml');require(set(yaml)=={'0'}and set(yaml['0'])=={'name','sound','text'},'Unsupported Carol commands')
 translation=ex.text('Translations/TranslatedText/dialogue_Podunk - sheet.csv');texts={r['key']:r['en']for r in csv.DictReader(io.StringIO(translation))};phrase=yaml['0'];raw=texts[phrase['text']];require(raw.startswith('[@]')and raw.count('[WAIT@]')==3,'Unreviewed Carol segment controls')
 segments=[]
 for i,part in enumerate(raw[3:].split('[WAIT@]')):
  tokens=[]
  for piece in re.split(r'(\[Ninten\])',part):
   if not piece:continue
   if piece=='[Ninten]':tokens.append(dict(kind=2,text=''))
   else:require('['not in piece and ']'not in piece,'Unknown Carol text token');tokens.append(dict(kind=1,text=piece))
  segments.append(dict(id=i+1,speaker=texts[phrase['name']],voice='Audio/Sound effects/text/'+phrase['sound']+'.mp3',tokens=tokens,flags=1|(2 if i<3 else 4)))
 root_name=one(r'^\[node name="([^"]+)" type="Node2D"\]',house,'source scene root')[1].replace("\\'","'")
 seen='/root/'+root_name+'/Objects/npc::1:'+npc['dialog']
 npc_record=dict(room_actor_index=0xffffffff,profile='carol',dialogue_path='Data/Dialogue/'+npc['dialog']+'.yaml',id=1,source_path='Objects/npc',body_id=body[0]['body_id'],primary_resource='carol',shadow_resource='shadow',first_segment=0,segment_count=len(segments),seen_key=seen,position=npc['position'],interact_center=[npc['position'][i]+area['position'][i]+shape_node['position'][i]for i in range(2)],interact_extents=ie,default_direction=[0,1],view_center=[npc['position'][i]+view['position'][i]for i in range(2)],view_radius=radius,return_delay=ret,flags=int(npc['staring'])|2|4|8)
 require('initial_dir = Vector2(0,1)'in ng and 'export var turn_to_player_on_interact := true'in ng,'NPC default direction/turn contract')
 player=ex.text('Nodes/Reusables/Player.tscn');ray=node(player,'EventDetector');ab=ex.text('Scripts/UI/AbstractDialogueBox.gd');gd=ex.text('Scripts/global/globalData.gd')
 speed=float(one(r'const TEXT_SPEEDS := \[([0-9.]+)',gd,'default text speed')[1])
 menu={r['key']:r['en']for r in csv.DictReader(io.StringIO(ex.text('Translations/TranslatedText/menus - sheet.csv')))}
 pitch=one(r'set_pitch_scale\(rand_range\(([0-9.]+), ([0-9.]+)\)\)',ab,'voice pitch')
 interaction=dict(voice_pitch_min=float(pitch[1]),voice_pitch_max=float(pitch[2]),bullet_string=menu['SYMBOL_BULLET_MAIN'],word_separator=menu['WORD_SEPARATOR'],max_player_name_length=len(menu['LONGEST_POSSIBLE_NAME']),ray_origin=ray['position'],ray_length=ray['cast_to'][1],text_seconds=speed,accept_multiplier=float(one(r'const SPEED_UP_FROM_PRESS_A := ([0-9.]+)',ab,'accept multiplier')[1]),cancel_multiplier=float(one(r'const SPEED_UP_FROM_PRESS_B := ([0-9.]+)',ab,'cancel multiplier')[1]),collision_mask=ray.get('collision_mask',1))
 require(ray['cast_to'][0]==0 and ray['scale'][1]==1,'Unreviewed interaction ray')
 overrides=[dict(npc=0,flag=x[0],dialogue=x[1])for x in npc['_all_dialog']]
 for s in {d[k]for d in doors for k in ['start_sound','end_sound']}|{segments[0]['voice']}:
  if s:ex.data(s)
 shader=ex.text('Shaders/Fade.shader');edge=float(one(r'smoothstep\(cut, cut \+ ([0-9.]+)',shader,'fade shader edge')[1]);size=float(one(r'^shader_param/Size = ([0-9.]+)',fade,'fade material size')[1]);fade_parameters=dict(FadeCuts=fi['tracks'][0]['keys']['values']+fo['tracks'][0]['keys']['values'],FadeShader=[size,edge,0,0])
 deps={'content/native-opening.json':sha(Path(root)/'content/native-opening.json')}
 extra=extend_source(ex,house,ir_room,ns,ng,segments,npc_record,overrides,root_name,ie,area,shape_node,view,radius,ret)
 npc_delay=float(one(r'func stop_interaction\(\):[\s\S]*?create_timer\(([0-9.]+)\)',ng,'NPC interaction direction return delay')[1])
 ir=dict(schema=4,kind='encore.native-house.source-ir',commit=PIN,scope='Six same-scene house warps; original NPCs and openable doors; Doll attack/post-win/melody, Mimmie exit guard, typed NPC program and repeat-dialogue bindings; recoverable telephone boundaries',sources=ex.sources,dependencies=deps,fade_parameters=fade_parameters,npc_parameters=dict(NpcInteractionReturn=[npc_delay,0,0,0]),doors=doors,npcs=extra.pop('npcs'),segments=segments,interaction=interaction,boundaries=boundaries,overrides=overrides,**extra)
 from tools.link_phone_content import link_house
 from tools.link_pillow_content import append_house
 return append_house(ex,link_house(ex,ir,ir_room),ir_room)
def extend_source(ex,house,ir_room,ns,ng,segments,carol,overrides,root_name,ie,area,shape_node,view,radius,ret):
 def body(path):
  found=[b['body_id']for b in ir_room['sections']['BodyRule']if ir_room['strings'][b['source_path_string']]==path]
  require(len(found)==1,'Missing/ambiguous source body '+path);return found[0]
 npcs=[carol]
 for path,role in [('Objects/npc2','mimmie'),('Objects/npcdoll','doll'),('Objects/npc3','minnie')]:
  n=node(house,path);pos=n['position'];dialog='Data/Dialogue/'+n['dialog']+'.yaml';ex.text(dialog)
  npcs.append(dict(room_actor_index={'mimmie':3,'doll':2,'minnie':4}[role],id=len(npcs)+1,source_path=path,body_id=body(path),primary_resource=role,shadow_resource=None if n.get('no_shadow',False)else 'shadow',first_segment=0,segment_count=0,seen_key='',position=pos,interact_center=[pos[i]+area['position'][i]+shape_node['position'][i]for i in range(2)],interact_extents=ie,default_direction=[0,1],view_center=[pos[i]+view['position'][i]for i in range(2)],view_radius=radius,return_delay=ret,flags=int(n.get('staring',False))|2|4|8,profile=role,dialogue_path=dialog))
  overrides.extend(dict(npc=len(npcs)-1,flag=x[0],dialogue=x[1])for x in n.get('_all_dialog',[]))
 odpath='Nodes/Overworld/Objects/Openable Door.tscn';ods=ex.text(odpath);odg=ex.text('Scripts/Main/Openable Door.gd')
 ex.text('Scripts/Main/CutsceneArea.gd');cas=ex.text('Nodes/Reusables/CutsceneArea.tscn');ex.text('Scripts/Main/Flag Landmarks.gd');ex.text('Nodes/Reusables/flag landmarks.tscn')
 require('globaldata.flags[flag] = true'in odg and 'get_direction().y == -1'in odg and 'is_running()'in odg,'Openable blocked policy changed')
 require('_update_positions()'in odg and 'door_offset.y + 32'in odg,'Openable geometry policy changed')
 require('shake_camera(1, 0.2, Vector2(2,0))'in odg,'Openable ram camera changed')
 def shape(scene,identity):return properties(one(r'^\[sub_resource type="RectangleShape2D" id='+str(identity)+r'\]\n(.*?)(?=^\[)',scene,'rectangle shape',re.M|re.S)[1])['extents']
 def script_string(name):return one(r'^export .* var '+name+r' (?::=|=) "([^"]*)"',odg,'Openable '+name,re.M)[1]
 defaults={k:script_string(k)for k in ['sound','end_sound','interact_doorblocked','interact_locklocked','interact_lockopened','key','flag','activates_flag','deactivates_flag']}
 blocked_path='Data/Dialogue/'+defaults['interact_doorblocked']+'.yaml';blocked=ex.yaml(blocked_path)
 require(set(blocked)=={'0'}and set(blocked['0'])=={'text'},'Unreviewed blocked dialogue')
 translations={r['key']:r['en']for r in csv.DictReader(io.StringIO(ex.text('Translations/TranslatedText/dialogue_Reusable - sheet.csv')))}
 raw=translations[blocked['0']['text']];require(raw.startswith('[@]')and '['not in raw[3:],'Unsupported blocked text control')
 dialogues=[dict(id=1,source_path=blocked_path,first_segment=len(segments),segment_count=1)]
 segments.append(dict(id=len(segments)+1,speaker='',voice='',tokens=[dict(kind=1,text=raw[3:])],flags=5))
 action=animation(ods,1,odpath,'Action');normal=animation(ods,2,odpath,'Normal')
 require([(t['path'],t['keys']['times'],t['keys']['values'])for t in action['tracks']]==[('Sprite:visible',[0.],[False]),('StaticBody2D/CollisionShape2D:disabled',[0.],[True]),('NonPlayerStaticBody2D/CollisionShape2D:disabled',[0.],[True])],'Unreviewed Action tracks')
 require([(t['path'],t['keys']['times'],t['keys']['values'])for t in normal['tracks']]==[('Sprite:visible',[0.],[True]),('NonPlayerStaticBody2D/CollisionShape2D:disabled',[0.],[False])],'Unreviewed Normal tracks')
 timer=node(ods,'Timer');require(timer['one_shot']and 'process_mode'not in timer,'Unreviewed openable timer domain')
 doors=[]
 for index,name in enumerate(['Openable Door','Openable Door2','Openable Door3','Openable Door4']):
  path='Below/'+name;n=node(house,path);pos=n['position'];offset=n.get('door_offset',node(ods,'.')['door_offset']);base_y=offset[1]+32
  require('scale'not in n,'Unreviewed openable scale')
  def center(child):
   point=node(ods,child).get('position',[0,0]);return [pos[0]+point[0],pos[1]+base_y+point[1]]
  sound=lambda value:''if value=='None'else 'Audio/Sound effects/'+value
  props={k:n.get(k,v)for k,v in defaults.items()}
  resource='door_basement'if n['sprite']=={'ExtResource':24}else 'door'
  require(n['sprite']in[{'ExtResource':21},{'ExtResource':24}],'Unreviewed door texture')
  doors.append(dict(id=index+1,source_path=path,player_body_id=body(path+'/StaticBody2D'),nonplayer_body_id=body(path+'/NonPlayerStaticBody2D'),sprite_resource=resource,flag=props['flag'],key=props['key'],activates_flag=props['activates_flag'],deactivates_flag=props['deactivates_flag'],blocked_dialogue=0,locked_dialogue='Data/Dialogue/'+props['interact_locklocked']+'.yaml',opened_dialogue='Data/Dialogue/'+props['interact_lockopened']+'.yaml',start_sound=sound(props['sound']),end_sound=sound(props['end_sound']),ram_sound='Audio/Sound effects/bash.mp3',policy=sum(bit for key,bit in [('blocked',1),('locked',2),('remove_key',4),('one_way',8)]if n.get(key,False)),normal_mask=5,normal_values=1,action_mask=7,action_values=6,timer_flags=1,position=pos,trigger_center=center('Area2D/CollisionShape2D'),trigger_extents=shape(ods,7),interact_center=center('interact/CollisionShape2D'),interact_extents=shape(ods,8),collision_center=center('StaticBody2D/CollisionShape2D'),collision_extents=shape(ods,3),sprite_position=[pos[i]+offset[i]for i in range(2)],sprite_offset=node(ods,'Sprite')['offset'],ram_direction=[2,0],ram_required_y=-1,close_delay=timer['wait_time'],action_length=action['length'],normal_length=normal['length'],ram_strength=1,ram_duration=.2))
  for k in ['start_sound','end_sound','ram_sound']:
   if doors[-1][k]:ex.data(doors[-1][k])
  ex.text(doors[-1]['locked_dialogue']);ex.text(doors[-1]['opened_dialogue'])
 trigger_path='Poltergeist/Cutscene Area2';n=node(house,trigger_path);pos=n['position'];scale=n['scale'];offset=node(cas,'CollisionShape2D')['position'];extents=shape(cas,1)
 dialogue='Data/Dialogue/'+n['dialog']+'.yaml';ex.text(dialogue)
 parent=node(house,'Poltergeist');require(not parent.get('appear_flag')and not n.get('appear_flag'),'Unreviewed Doll appearance requirement')
 conditions=[dict(kind=1,flag=parent['disappear_flag'],value=0),dict(kind=2,flag=n['disappear_flag'],value=0)]
 triggers=[dict(id=1,source_path=trigger_path,dialogue=dialogue,center=[pos[i]+scale[i]*offset[i]for i in range(2)],extents=[abs(scale[i])*extents[i]for i in range(2)],first_condition=0,condition_count=2,disposition=2,program_index=1)]
 doll=ex.yaml(dialogue)
 texts={r['key']:r['en']for r in csv.DictReader(io.StringIO(ex.text('Translations/TranslatedText/dialogue_Podunk_cutscenes - sheet.csv')))}
 for phrase_index,stable_id in [('2',2),('4',3)]:
  phrase=doll[phrase_index];raw=texts[phrase['text']]
  require(raw.startswith('[@]')and '['not in raw[3:]and ']'not in raw[3:],'Unsupported Doll text control')
  voice='Audio/Sound effects/text/'+phrase['sound']+'.mp3';ex.data(voice)
  dialogues.append(dict(id=stable_id,source_path=dialogue,first_segment=len(segments),segment_count=1))
  segments.append(dict(id=len(segments)+1,speaker=texts[phrase['name']],voice=voice,tokens=[dict(kind=1,text=raw[3:])],flags=5))
 postwin_path='Data/Dialogue/Podunk/cutscenes/doll_defeated.yaml';postwin=ex.yaml(postwin_path)
 for phrase_index,stable_id,part_count in [('2',4,1),('7',5,3)]:
  phrase=postwin[phrase_index];raw=texts[phrase['text']]
  require(raw.startswith('[@]'),'Post-win bullet control');parts=raw[3:].split('[WAIT@]')
  require(len(parts)==part_count,'Post-win WAIT segment count')
  voice='Audio/Sound effects/text/'+phrase['sound']+'.mp3';ex.data(voice)
  dialogues.append(dict(id=stable_id,source_path=postwin_path,first_segment=len(segments),segment_count=len(parts)))
  for i,part in enumerate(parts):
   tokens=[]
   for piece in re.split(r'(\[Ninten\])',part):
    if not piece:continue
    if piece=='[Ninten]':tokens.append(dict(kind=2,text=''))
    else:require('['not in piece and ']'not in piece,'Unknown post-win text token');tokens.append(dict(kind=1,text=piece))
   segments.append(dict(id=len(segments)+1,speaker=texts[phrase['name']],voice=voice,tokens=tokens,flags=5 if i==len(parts)-1 else 3))
 # New content appends after all established text IDs/segments. Source program
 # lookup is resolved once offline, never guessed from a runtime ordinal.
 program_paths={ir_room['strings'][p['source_path_string']]:i for i,p in enumerate(ir_room['sections']['Program'])}
 for n in npcs:n['program_index']=0xffffffff
 podunk={r['key']:r['en']for r in csv.DictReader(io.StringIO(ex.text('Translations/TranslatedText/dialogue_Podunk - sheet.csv')))}
 def append_text(phrase,table,part_count):
  raw=table[phrase['text']];require(raw.startswith('[@]'),'Melody room bullet control');parts=raw[3:].split('[WAIT@]')
  require(len(parts)==part_count,'Melody room WAIT segment count')
  voice='Audio/Sound effects/text/'+phrase['sound']+'.mp3'if phrase.get('sound')else ''
  if voice:ex.data(voice)
  first=len(segments)
  for i,part in enumerate(parts):
   tokens=[]
   for piece in re.split(r'(\[(?:Ninten|PartyLead)\])',part):
    if not piece:continue
    if piece in('[Ninten]','[PartyLead]'):tokens.append(dict(kind=2,text=''))
    else:require('['not in piece and ']'not in piece,'Unknown melody room text token');tokens.append(dict(kind=1,text=piece))
   segments.append(dict(id=len(segments)+1,speaker=table[phrase['name']]if 'name'in phrase else '',voice=voice,tokens=tokens,flags=5 if i==len(parts)-1 else 3))
  return first,len(parts)
 # Match the original initial party and nickname replacement before lowering
 # PartyLead to the existing dynamic player-name token.
 require(ex.yaml('Data/save_new_game.yaml')['party']==['ninten'],'Melody requires singleton Ninten party')
 require('global.party[0].get_nickname()'in ex.text('Scripts/global/text_tools.gd'),'PartyLead source replacement changed')
 melody=ex.yaml(MELODY);guard=ex.yaml(GUARD)
 for path,doc,table,labels in [(MELODY,melody,podunk,[('0',6,3),('4',7,1)]),(GUARD,guard,texts,[('0',8,1),('1',9,1)])]:
  for label,identity,parts in labels:
   first,count=append_text(doc[label],table,parts)
   dialogues.append(dict(id=identity,source_path=path,first_segment=first,segment_count=count))
 for path,identity,parts in [(AFTER,10,2),(REPEAT,11,1)]:
  first,count=append_text(literal_phrase(path,ex.yaml(path)),podunk,parts)
  dialogues.append(dict(id=identity,source_path=path,first_segment=first,segment_count=count))
 first,count=append_text(literal_phrase(DEFAULT,ex.yaml(DEFAULT)),podunk,2)
 npcs[1].update(first_segment=first,segment_count=count,seen_key='/root/'+root_name+'/'+npcs[1]['source_path']+'::1:Podunk/mimmie_doll_defeated')
 npcs[2].update(program_index=program_paths['Podunk/dollmelody'],seen_key='/root/'+root_name+'/'+npcs[2]['source_path']+'::1:Podunk/dollmelody')
 for override in overrides:
  matches=[i for i,d in enumerate(dialogues)if d['source_path']=='Data/Dialogue/'+override['dialogue']+'.yaml']
  require(len(matches)<=1,'Ambiguous literal NPC override')
  override['dialogue_index']=matches[0]if matches else 0xffffffff
  override['seen_key']='/root/'+root_name+'/'+npcs[override['npc']]['source_path']+':'+override['flag']+':1:'+override['dialogue']
 for name,disposition in [('Cutscene Area5',2),('Cutscene Area4',1),('Cutscene Area3',1)]:
  n=node(house,name);pos=n['position'];scale=n.get('scale',[1,1]);offset=node(cas,'CollisionShape2D')['position'];extents=shape(cas,1)
  dialogue='Data/Dialogue/'+n['dialog']+'.yaml';ex.text(dialogue);first=len(conditions)
  require(n.get('appear_flag')and n.get('disappear_flag'),'Melody room trigger conditions')
  conditions.extend([dict(kind=1,flag=n['appear_flag'],value=1),dict(kind=2,flag=n['disappear_flag'],value=0)])
  triggers.append(dict(id=len(triggers)+1,source_path=name,dialogue=dialogue,center=[pos[i]+scale[i]*offset[i]for i in range(2)],extents=[abs(scale[i])*extents[i]for i in range(2)],first_condition=first,condition_count=2,disposition=disposition,program_index=program_paths[n['dialog']]if disposition==2 else 0xffffffff))
 return dict(npcs=npcs,dialogues=dialogues,openable_doors=doors,story_triggers=triggers,story_conditions=conditions)

def source_review(ir,path):
 return {'schema':ir['schema'],'commit':PIN,'sources':ir['sources'],'dependencies':ir['dependencies'],'ir_sha256':sha(path),'mechanisms':['ancestor-scaled door shape and target minus (0,7)','same scene preserves rewards and flags','WAIT sections and source singleton-party dynamic name','NPC body binding and directional ray interaction','typed NPC program and override text bindings; unsupported later paths recoverable','Openable Action/Normal exact visibility and collider masks','blocked running-upward unlock without flags_updated','source Doll and Mimmie guard programs with explicit source flag conditions','original Doll inherited talker and original override-specific seen keys','source NPC stop-interaction direction return delay','six reviewed Carol/Phone linear programs and typed hint color tokens','original Carol Room actor binding; Area3/4 exact inherited geometry and contact/flag gates','Dad-normal graph retained separately until branch/menu integration']}
def main():
 p=argparse.ArgumentParser();p.add_argument('--out',type=Path,default=ROOT/'content/native-house.json');a=p.parse_args()
 ir=build();a.out.write_text(json.dumps(ir,indent=2,ensure_ascii=False)+'\n');r=ROOT/'reports/house-data';r.mkdir(parents=True,exist_ok=True);(r/'source-review.json').write_text(json.dumps(source_review(ir,a.out),indent=2,ensure_ascii=False)+'\n');print('Extracted',len(ir['doors']),'doors,',len(ir['boundaries']),'unsupported routes,',len(ir['segments']),'dialogue sections')
if __name__=='__main__':main()
