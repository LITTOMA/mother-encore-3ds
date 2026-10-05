#!/usr/bin/env python3
"""Bounded, explicit extraction of house doors, initial NPCs, and the bounded Doll story edge."""
import argparse,csv,hashlib,io,json,re,sys
from pathlib import Path
from extract_battle_entry import Extractor,ROOT,PIN,node,one,properties,animation,require
sys.path.insert(0,str(ROOT))
from tools.melody_dialogue import MELODY,GUARD,DEFAULT,AFTER,REPEAT,literal_phrase

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def build(root=ROOT):
 from tools.house_source_bindings import load
 b=load(root);HOUSE=b['source_refs']['house'];supported={row['source_path']:row['id']for row in b['same_scene_doors']}
 ex=Extractor(root);house=ex.text(HOUSE);ds=ex.text(b['source_refs']['door_scene']);dg=ex.text(b['source_refs']['door_script']);fade=ex.text(b['source_refs']['fade_scene'])
 for path in [b['source_refs'][key]for key in ['scene_transition','fade_script','party_script','party_object','character_sprite_script','text_tools','dialogue_script']]:ex.text(path)
 require(node(house,b['nodes']['doors'])=={} and 'position'not in node(house,'.')and 'scale'not in node(house,'.'),'Unexpected ancestor transform')
 shape=properties(one(r'^\[sub_resource type="RectangleShape2D" id='+str(b['door']['shape_id'])+r'\]\n(.*?)(?=^\[)',ds,'door shape',re.M|re.S)[1])['extents']
 inherited=node(ds,b['nodes']['door_shape'])['position'];defaults={}
 for k in ['fade_in_speed','fade_out_speed']:defaults[k]=float(one(r'^export var '+k+r' := ([0-9.]+)',dg,k)[1])
 fi=animation(fade,b['fade_in_resource'],b['source_refs']['fade_scene'],b['door']['fade_in']);fo=animation(fade,b['fade_out_resource'],b['source_refs']['fade_scene'],b['door']['fade_out'])
 require(fi['tracks'][b['door']['fade_value_track']]['keys']['values']==[1.,0.] and fo['tracks'][b['door']['fade_value_track']]['keys']['values']==[0.,1.],'Unsupported fade curves')
 require(fo['tracks'][b['door']['fade_callback_track']]['keys']['values'][0]['method']==b['door']['fade_callback'],'Fade callback changed')
 fade_fields=dict(fade_in_length=fi['length'],fade_in_opaque=fi['tracks'][b['door']['fade_value_track']]['keys']['times'][-1],fade_out_length=fo['length'],fade_out_mostly=fo['tracks'][b['door']['fade_callback_track']]['keys']['times'][0],**defaults,color=b['door']['color'])
 doors=[];boundaries=[]
 names=re.findall(r'^\[node name="([^"]+)" parent="'+re.escape(b['nodes']['doors'])+r'" instance=',house,re.M)
 for name in names:
  path=b['nodes']['doors']+'/'+name;d=node(house,path);pos=d['position'];scale=d.get('scale',[1,1]);child=inherited
  try:child=node(house,path+'/'+b['nodes']['door_shape'])['position']
  except ValueError:pass
  center=[pos[i]+scale[i]*child[i]for i in range(2)];extents=[abs(scale[i])*shape[i]for i in range(2)]
  if path not in supported:
   boundaries.append(dict(id=len(boundaries)+1,source_path=path,kind=2 if d.get('targetScene')else 1,center=center,extents=extents));continue
  require(not d.get('targetScene')and not d.get('flag_set'),'Unsupported door state mutation')
  target=node(house,path+'/'+b['nodes']['door_target'])['position'];destination=[pos[i]+scale[i]*target[i]-b['door']['destination_offset'][i]for i in range(2)]
  sound=lambda k:''if d.get(k,'None')=='None'else b['patterns']['door_sound'].format(sound=d[k])
  doors.append(dict(id=supported[path],source_path=path,start_sound=sound('sound'),end_sound=sound('end_sound'),center=center,extents=extents,destination=destination,direction=d['dir'],**fade_fields))
 doors.sort(key=lambda d:d['id'])
 ns=ex.text(b['source_refs']['npc_scene']);ng=ex.text(b['source_refs']['npc_script']);npc=node(house,b['npc']['primary']);base=node(ns,'.');area=node(ns,b['nodes']['npc_area']);shape_node=node(ns,b['nodes']['npc_shape'])
 ie=properties(one(r'^\[sub_resource type="RectangleShape2D" id='+str(b['npc']['shape_id'])+r'\]\n(.*?)(?=^\[)',ns,'interact shape',re.M|re.S)[1])['extents']
 view=node(ns,b['nodes']['npc_view']);radius=float(one(r'^radius = ([0-9.]+)',ns,'staring radius')[1]);ret=float(one(r'func _on_ViewArea_body_exited[\s\S]*?create_timer\(([0-9.]+)\)',ng,'staring reset delay')[1])
 ir_room=json.loads((Path(root)/'content/native-opening.json').read_text());body=[row for row in ir_room['sections']['BodyRule']if ir_room['strings'][row['source_path_string']]==b['npc']['primary']];require(len(body)==1,'NPC room body binding')
 yaml=ex.yaml(b['patterns']['dialogue'].format(dialogue=npc['dialog']));require(set(yaml)=={b['npc']['primary_phrase']}and set(yaml[b['npc']['primary_phrase']])=={'name','sound','text'},'Unsupported Carol commands')
 translation=ex.text(b['source_refs']['podunk_text']);texts={r['key']:r['en']for r in csv.DictReader(io.StringIO(translation))};phrase=yaml[b['npc']['primary_phrase']];raw=texts[phrase['text']];require(raw.startswith('[@]')and raw.count('[WAIT@]')==b['npc']['primary_segments']-1,'Unreviewed Carol segment controls')
 segments=[]
 for i,part in enumerate(raw[3:].split('[WAIT@]')):
  tokens=[]
  for piece in re.split(r'(\[Ninten\])',part):
   if not piece:continue
   if piece=='[Ninten]':tokens.append(dict(kind=2,text=''))
   else:require('['not in piece and ']'not in piece,'Unknown Carol text token');tokens.append(dict(kind=1,text=piece))
  segments.append(dict(id=i+1,speaker=texts[phrase['name']],voice=b['patterns']['voice'].format(sound=phrase['sound']),tokens=tokens,flags=1|(2 if i<b['npc']['primary_segments']-1 else 4)))
 root_name=one(r'^\[node name="([^"]+)" type="Node2D"\]',house,'source scene root')[1].replace("\\'","'")
 seen='/root/'+root_name+'/'+b['npc']['primary']+'::1:'+npc['dialog']
 primary=next(n for n in b['npcs']if n['source_path']==b['npc']['primary'])
 npc_record=dict(room_actor_index=primary['room_actor'],profile=primary['resource'],dialogue_path=b['patterns']['dialogue'].format(dialogue=npc['dialog']),id=primary['id'],source_path=primary['source_path'],body_id=body[0]['body_id'],primary_resource=primary['resource'],shadow_resource=primary['shadow_resource']or None,first_segment=0,segment_count=len(segments),seen_key=seen,position=npc['position'],interact_center=[npc['position'][i]+area['position'][i]+shape_node['position'][i]for i in range(2)],interact_extents=ie,default_direction=b['npc']['default_direction'],view_center=[npc['position'][i]+view['position'][i]for i in range(2)],view_radius=radius,return_delay=ret,flags=int(npc['staring'])|2|4|8)
 require('export var turn_to_player_on_interact := true'in ng,'NPC default turn contract')
 player=ex.text(b['source_refs']['player_scene']);ray=node(player,b['nodes']['player_ray']);ab=ex.text(b['source_refs']['abstract_script']);gd=ex.text(b['source_refs']['global_data'])
 speed=float(one(r'const TEXT_SPEEDS := \[([0-9.]+)',gd,'default text speed')[1])
 menu={r['key']:r['en']for r in csv.DictReader(io.StringIO(ex.text(b['source_refs']['menu_text'])))}
 pitch=one(r'set_pitch_scale\(rand_range\(([0-9.]+), ([0-9.]+)\)\)',ab,'voice pitch')
 interaction=dict(voice_pitch_min=float(pitch[1]),voice_pitch_max=float(pitch[2]),bullet_string=menu['SYMBOL_BULLET_MAIN'],word_separator=menu['WORD_SEPARATOR'],max_player_name_length=len(menu['LONGEST_POSSIBLE_NAME']),ray_origin=ray['position'],ray_length=ray['cast_to'][1],text_seconds=speed,accept_multiplier=float(one(r'const SPEED_UP_FROM_PRESS_A := ([0-9.]+)',ab,'accept multiplier')[1]),cancel_multiplier=float(one(r'const SPEED_UP_FROM_PRESS_B := ([0-9.]+)',ab,'cancel multiplier')[1]),collision_mask=ray.get('collision_mask',1))
 require(ray['cast_to'][0]==0 and ray['scale'][1]==1,'Unreviewed interaction ray')
 overrides=[dict(npc=0,flag=x[0],dialogue=x[1])for x in npc['_all_dialog']]
 for s in {d[k]for d in doors for k in ['start_sound','end_sound']}|{segments[0]['voice']}:
  if s:ex.data(s)
 shader=ex.text(b['source_refs']['fade_shader']);edge=float(one(r'smoothstep\(cut, cut \+ ([0-9.]+)',shader,'fade shader edge')[1]);size=float(one(r'^shader_param/Size = ([0-9.]+)',fade,'fade material size')[1]);fade_parameters=dict(FadeCuts=fi['tracks'][b['door']['fade_value_track']]['keys']['values']+fo['tracks'][b['door']['fade_value_track']]['keys']['values'],FadeShader=[size,edge,0,0])
 deps={'content/native-opening.json':sha(Path(root)/'content/native-opening.json')}
 extra=extend_source(ex,house,ir_room,ns,ng,segments,npc_record,overrides,root_name,ie,area,shape_node,view,radius,ret,b)
 npc_delay=float(one(r'func stop_interaction\(\):[\s\S]*?create_timer\(([0-9.]+)\)',ng,'NPC interaction direction return delay')[1])
 ir=dict(schema=4,kind='encore.native-house.source-ir',commit=PIN,scope='Six same-scene house warps; original NPCs and openable doors; Doll attack/post-win/melody, Mimmie exit guard, typed NPC program and repeat-dialogue bindings; recoverable telephone boundaries',sources=ex.sources,dependencies=deps,fade_parameters=fade_parameters,npc_parameters=dict(NpcInteractionReturn=[npc_delay,0,0,0]),doors=doors,npcs=extra.pop('npcs'),segments=segments,interaction=interaction,boundaries=boundaries,overrides=overrides,**extra)
 from tools.link_phone_content import link_house
 from tools.link_pillow_content import append_house
 from tools.link_family_followup import append_house as append_family_house
 from tools.link_house_inspections import append_house as append_inspections
 from tools.link_drawer_content import append_house as append_drawer
 from tools.storage_dialogue import append_house as append_storage
 linked=append_house(ex,link_house(ex,ir,ir_room),ir_room)
 linked=append_family_house(ex,linked,ir_room)
 linked=append_inspections(ex,linked,ir_room)
 linked=append_drawer(ex,linked,ir_room)
 linked=append_storage(ex,linked,ir_room)
 from tools.link_basement_content import append_house as append_basement
 return append_basement(ex,linked,ir_room)
def extend_source(ex,house,ir_room,ns,ng,segments,carol,overrides,root_name,ie,area,shape_node,view,radius,ret,b):
 def body(path):
  found=[b['body_id']for b in ir_room['sections']['BodyRule']if ir_room['strings'][b['source_path_string']]==path]
  require(len(found)==1,'Missing/ambiguous source body '+path);return found[0]
 npcs=[carol]
 for binding in b['npcs'][1:]:
  path=binding['source_path'];role=binding['resource']
  n=node(house,path);pos=n['position'];dialog=b['patterns']['dialogue'].format(dialogue=n['dialog']);ex.text(dialog)
  npcs.append(dict(room_actor_index=binding['room_actor'],id=binding['id'],source_path=path,body_id=body(path),primary_resource=role,shadow_resource=binding['shadow_resource']or None,first_segment=0,segment_count=0,seen_key='',position=pos,interact_center=[pos[i]+area['position'][i]+shape_node['position'][i]for i in range(2)],interact_extents=ie,default_direction=b['npc']['default_direction'],view_center=[pos[i]+view['position'][i]for i in range(2)],view_radius=radius,return_delay=ret,flags=int(n.get('staring',False))|2|4|8,profile=role,dialogue_path=dialog))
  overrides.extend(dict(npc=len(npcs)-1,flag=x[0],dialogue=x[1])for x in n.get('_all_dialog',[]))
 odpath=b['source_refs']['openable_scene'];ods=ex.text(odpath);odg=ex.text(b['source_refs']['openable_script'])
 ex.text(b['source_refs']['cutscene_area_script']);cas=ex.text(b['source_refs']['area_scene']);ex.text(b['source_refs']['flag_landmarks_script']);ex.text(b['source_refs']['flag_landmarks_scene'])
 require('globaldata.flags[flag] = true'in odg and 'is_running()'in odg,'Openable blocked policy changed')
 require('_update_positions()'in odg,'Openable geometry policy changed')
 def shape(scene,identity):return properties(one(r'^\[sub_resource type="RectangleShape2D" id='+str(identity)+r'\]\n(.*?)(?=^\[)',scene,'rectangle shape',re.M|re.S)[1])['extents']
 def script_string(name):return one(r'^export .* var '+name+r' (?::=|=) "([^"]*)"',odg,'Openable '+name,re.M)[1]
 defaults={k:script_string(k)for k in ['sound','end_sound','interact_doorblocked','interact_locklocked','interact_lockopened','key','flag','activates_flag','deactivates_flag']}
 blocked_path=b['patterns']['dialogue'].format(dialogue=defaults['interact_doorblocked']);blocked=ex.yaml(blocked_path)
 require(set(blocked)=={'0'}and set(blocked['0'])=={'text'},'Unreviewed blocked dialogue')
 translations={r['key']:r['en']for r in csv.DictReader(io.StringIO(ex.text(b['source_refs']['reusable_text'])))}
 raw=translations[blocked['0']['text']];require(raw.startswith('[@]')and '['not in raw[3:],'Unsupported blocked text control')
 dialogues=[dict(id=b['openable']['blocked_dialogue_id'],source_path=blocked_path,first_segment=len(segments),segment_count=1)]
 segments.append(dict(id=len(segments)+1,speaker='',voice='',tokens=[dict(kind=1,text=raw[3:])],flags=5))
 action=animation(ods,b['openable']['clips']['Action'],odpath,'Action');normal=animation(ods,b['openable']['clips']['Normal'],odpath,'Normal')
 def masks(clip):
  mask=values=0
  for track in clip['tracks']:
   require(track['path']in b['openable']['track_components']and track['keys']['times']==[0.]and len(track['keys']['values'])==1 and type(track['keys']['values'][0])is bool,'Unreviewed openable property track')
   component=b['openable']['track_components'][track['path']];mask|=component
   if track['keys']['values'][0]:values|=component
  return mask,values
 action_mask,action_values=masks(action);normal_mask,normal_values=masks(normal)
 require(list((action_mask,action_values))==b['openable']['expected_masks']['Action']and list((normal_mask,normal_values))==b['openable']['expected_masks']['Normal'],'Changed openable source mask semantics')
 timer=node(ods,b['nodes']['openable_timer']);require(timer['one_shot']and 'process_mode'not in timer,'Unreviewed openable timer domain')
 doors=[]
 for binding in b['openable']['rows']:
  path=binding['source_path'];n=node(house,path);pos=n['position'];offset=n.get('door_offset',node(ods,'.')['door_offset']);base_y=offset[1]+b['openable']['base_y']
  require('scale'not in n,'Unreviewed openable scale')
  def center(child):
   point=node(ods,child).get('position',[0,0]);return [pos[0]+point[0],pos[1]+base_y+point[1]]
  sound=lambda value:''if value=='None'else b['patterns']['door_sound'].format(sound=value)
  props={k:n.get(k,v)for k,v in defaults.items()}
  resource=binding['resource']
  doors.append(dict(id=binding['id'],source_path=path,player_body_id=binding['player_body_id'],nonplayer_body_id=binding['nonplayer_body_id'],sprite_resource=resource,flag=props['flag'],key=props['key'],activates_flag=props['activates_flag'],deactivates_flag=props['deactivates_flag'],blocked_dialogue=0,locked_dialogue=b['patterns']['dialogue'].format(dialogue=props['interact_locklocked']),opened_dialogue=b['patterns']['dialogue'].format(dialogue=props['interact_lockopened']),start_sound=sound(props['sound']),end_sound=sound(props['end_sound']),ram_sound=b['openable']['ram']['sound'],policy=sum(bit for key,bit in [('blocked',1),('locked',2),('remove_key',4),('one_way',8)]if n.get(key,False)),normal_mask=normal_mask,normal_values=normal_values,action_mask=action_mask,action_values=action_values,timer_flags=1,position=pos,trigger_center=center(b['openable']['shape_nodes']['trigger']),trigger_extents=shape(ods,b['openable']['shapes']['trigger']),interact_center=center(b['openable']['shape_nodes']['interact']),interact_extents=shape(ods,b['openable']['shapes']['interact']),collision_center=center(b['openable']['shape_nodes']['collision']),collision_extents=shape(ods,b['openable']['shapes']['collision']),sprite_position=[pos[i]+offset[i]for i in range(2)],sprite_offset=node(ods,b['nodes']['openable_sprite'])['offset'],ram_direction=b['openable']['ram']['direction'],ram_required_y=b['openable']['ram']['required_y'],close_delay=timer['wait_time'],action_length=action['length'],normal_length=normal['length'],ram_strength=b['openable']['ram']['strength'],ram_duration=b['openable']['ram']['duration']))
  for k in ['start_sound','end_sound','ram_sound']:
   if doors[-1][k]:ex.data(doors[-1][k])
  ex.text(doors[-1]['locked_dialogue']);ex.text(doors[-1]['opened_dialogue'])
 trigger_path=b['triggers'][0]['source_path'];n=node(house,trigger_path);pos=n['position'];scale=n['scale'];offset=node(cas,b['nodes']['door_shape'])['position'];extents=shape(cas,b['area_shape_id'])
 dialogue=b['patterns']['dialogue'].format(dialogue=n['dialog']);ex.text(dialogue)
 parent=node(house,b['nodes']['doll_parent']);require(not parent.get('appear_flag')and not n.get('appear_flag'),'Unreviewed Doll appearance requirement')
 conditions=[dict(kind=1,flag=parent['disappear_flag'],value=0),dict(kind=2,flag=n['disappear_flag'],value=0)]
 triggers=[dict(id=b['triggers'][0]['id'],source_path=trigger_path,dialogue=dialogue,center=[pos[i]+scale[i]*offset[i]for i in range(2)],extents=[abs(scale[i])*extents[i]for i in range(2)],first_condition=0,condition_count=2,disposition=b['triggers'][0]['disposition'],program_index=next(i for i,p in enumerate(ir_room['sections']['Program'])if ir_room['strings'][p['source_path_string']]==n['dialog']))]
 doll=ex.yaml(dialogue)
 texts={r['key']:r['en']for r in csv.DictReader(io.StringIO(ex.text(b['source_refs']['cutscene_text'])))}
 def table_for(binding):
  return {r['key']:r['en']for r in csv.DictReader(io.StringIO(ex.text(b['source_refs'][binding['table']])))}
 for binding in [row for row in b['text']['rows']if row['path']==dialogue]:
  phrase_index=binding['label'];stable_id=binding['id'];texts=table_for(binding)
  phrase=doll[phrase_index];raw=texts[phrase['text']]
  require(raw.startswith('[@]')and '['not in raw[3:]and ']'not in raw[3:],'Unsupported Doll text control')
  voice=b['patterns']['voice'].format(sound=phrase['sound']);ex.data(voice)
  dialogues.append(dict(id=stable_id,source_path=dialogue,first_segment=len(segments),segment_count=1))
  segments.append(dict(id=len(segments)+1,speaker=texts[phrase['name']],voice=voice,tokens=[dict(kind=1,text=raw[3:])],flags=5))
 postwin_path=b['text']['post_win'];postwin=ex.yaml(postwin_path)
 for binding in [row for row in b['text']['rows']if row['path']==b['text']['post_win']]:
  phrase_index=binding['label'];stable_id=binding['id'];texts=table_for(binding);part_count=binding['parts']
  phrase=postwin[phrase_index];raw=texts[phrase['text']]
  require(raw.startswith('[@]'),'Post-win bullet control');parts=raw[3:].split('[WAIT@]')
  require(len(parts)==part_count,'Post-win WAIT segment count')
  voice=b['patterns']['voice'].format(sound=phrase['sound']);ex.data(voice)
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
 podunk={r['key']:r['en']for r in csv.DictReader(io.StringIO(ex.text(b['source_refs']['podunk_text'])))}
 def append_text(phrase,table,part_count):
  raw=table[phrase['text']];require(raw.startswith('[@]'),'Melody room bullet control');parts=raw[3:].split('[WAIT@]')
  require(len(parts)==part_count,'Melody room WAIT segment count')
  voice=b['patterns']['voice'].format(sound=phrase['sound'])if phrase.get('sound')else ''
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
 require(ex.yaml(b['source_refs']['new_game'])['party']==[b['npc']['initial_party']],'Melody requires singleton Ninten party')
 require('global.party[0].get_nickname()'in ex.text(b['source_refs']['text_tools']),'PartyLead source replacement changed')
 melody=ex.yaml(b['text']['melody']);guard=ex.yaml(b['text']['guard'])
 for binding in [row for row in b['text']['rows']if row['path']in(b['text']['melody'],b['text']['guard'])]:
  first,count=append_text(ex.yaml(binding['path'])[binding['label']],table_for(binding),binding['parts'])
  dialogues.append(dict(id=binding['id'],source_path=binding['path'],first_segment=first,segment_count=count))
 for binding in [row for row in b['text']['rows']if row['path']in(b['text']['after'],b['text']['repeat'])]:
  path=binding['path'];identity=binding['id'];parts=binding['parts']
  first,count=append_text(literal_phrase(path,ex.yaml(path)),table_for(binding),parts)
  dialogues.append(dict(id=identity,source_path=path,first_segment=first,segment_count=count))
 first,count=append_text(literal_phrase(b['text']['default'],ex.yaml(b['text']['default'])),podunk,b['text']['default_parts'])
 next(n for n in npcs if n['source_path']==b['npc']['mimmie']).update(first_segment=first,segment_count=count,seen_key='/root/'+root_name+'/'+b['npc']['mimmie']+'::1:'+b['npc']['mimmie_seen_dialogue'])
 next(n for n in npcs if n['source_path']==b['npc']['doll']).update(program_index=program_paths[b['npc']['melody_program']],seen_key='/root/'+root_name+'/'+b['npc']['doll']+'::1:'+b['npc']['melody_program'])
 for override in overrides:
  matches=[i for i,d in enumerate(dialogues)if d['source_path']==b['patterns']['dialogue'].format(dialogue=override['dialogue'])]
  require(len(matches)<=1,'Ambiguous literal NPC override')
  override['dialogue_index']=matches[0]if matches else 0xffffffff
  override['seen_key']='/root/'+root_name+'/'+npcs[override['npc']]['source_path']+':'+override['flag']+':1:'+override['dialogue']
 for binding in b['triggers'][1:]:
  name=binding['source_path'];disposition=binding['disposition']
  n=node(house,name);pos=n['position'];scale=n.get('scale',[1,1]);offset=node(cas,b['nodes']['door_shape'])['position'];extents=shape(cas,b['area_shape_id'])
  dialogue=b['patterns']['dialogue'].format(dialogue=n['dialog']);ex.text(dialogue);first=len(conditions)
  require(n.get('appear_flag')and n.get('disappear_flag'),'Melody room trigger conditions')
  conditions.extend([dict(kind=1,flag=n['appear_flag'],value=1),dict(kind=2,flag=n['disappear_flag'],value=0)])
  triggers.append(dict(id=binding['id'],source_path=name,dialogue=dialogue,center=[pos[i]+scale[i]*offset[i]for i in range(2)],extents=[abs(scale[i])*extents[i]for i in range(2)],first_condition=first,condition_count=2,disposition=disposition,program_index=program_paths[n['dialog']]if disposition==2 else 0xffffffff))
 return dict(npcs=npcs,dialogues=dialogues,openable_doors=doors,story_triggers=triggers,story_conditions=conditions)

def source_review(ir,path):
 return {'schema':ir['schema'],'commit':PIN,'sources':ir['sources'],'dependencies':ir['dependencies'],'ir_sha256':sha(path),'mechanisms':['ancestor-scaled door shape and target minus (0,7)','same scene preserves rewards and flags','WAIT sections and source singleton-party dynamic name','NPC body binding and directional ray interaction','typed NPC program and override text bindings; unsupported later paths recoverable','Openable Action/Normal exact visibility and collider masks','blocked running-upward unlock without flags_updated','source Doll and Mimmie guard programs with explicit source flag conditions','original Doll inherited talker and original override-specific seen keys','source NPC stop-interaction direction return delay','six reviewed Carol/Phone linear programs and typed hint color tokens','original Carol Room actor binding; Area3/4 exact inherited geometry and contact/flag gates','Dad-normal graph retained separately until branch/menu integration']}
def main():
 p=argparse.ArgumentParser();p.add_argument('--out',type=Path,default=ROOT/'content/native-house.json');a=p.parse_args()
 ir=build();a.out.write_text(json.dumps(ir,indent=2,ensure_ascii=False)+'\n');r=ROOT/'reports/house-data';r.mkdir(parents=True,exist_ok=True);(r/'source-review.json').write_text(json.dumps(source_review(ir,a.out),indent=2,ensure_ascii=False)+'\n');print('Extracted',len(ir['doors']),'doors,',len(ir['boundaries']),'unsupported routes,',len(ir['segments']),'dialogue sections')
if __name__=='__main__':main()
