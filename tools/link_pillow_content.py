#!/usr/bin/env python3
"""Append the reviewed optional Pillow/Minnie branch without remapping existing identities."""
from __future__ import annotations
import copy,csv,io,json,re
from pathlib import Path
from tools.pillow_dialogue import *
from tools.world_geometry import f32
NONE=0xffffffff
from tools.pillow_source_bindings import load as pillow_bindings

def texts(ex,docs):
 binding=pillow_bindings(ex.root)['append'];tables={}
 for name in binding['text_tables']:
  tables.update({r['key']:r['en']for r in csv.DictReader(io.StringIO(ex.text(name)))})
 out=[]
 for path in PATHS:
  for label,p in docs[path].items():
   if'text'not in p:continue
   raw=tables[p['text']]
   if path==TUTORIAL:
    raw=re.sub(r'\[if input:gamepad\](.*?)\[else\].*?\[/if\]',r'\1',raw)
    if'[ui_toggle]'in raw:
     adapter_path=ex.root/'content/pillow-input.json';adapter=json.loads(adapter_path.read_text())
     require(adapter=={'schema':1,'input_type':'gamepad','action':'ui_toggle','label':'B'},'Unreviewed tutorial platform binding')
     if hasattr(ex,'file'):ex.file('content/pillow-input.json')
     raw=raw.replace('[ui_toggle]',adapter['label'])
   segments=text_segments(raw)
   out.append(dict(id=binding['dialogue_first_id']+len(out),identity=path+'::'+label,source_path=path,speaker=tables[p['name']],voice=binding['voice_pattern'].format(sound=p['sound']),segments=segments))
 return out,tables

def append_room(ex,clip_names,add_clip):
 from tools.extract_native_content import node_block,prop,pair
 from tools.native_content import OPCODES
 from tools.doll_dialogue import decode
 binding=pillow_bindings(ex.root);a=binding['append'];s=ex.sections;require(len(s['ActorInstance'])==a['prefix_actors'] and len(s['Battle'])==a['prefix_battles'],'Pillow append-only prefix')
 docs=load_documents(ex);text_rows,translations=texts(ex,docs);ids={t['identity']:t['id']for t in text_rows}
 world=ex.document(a['world_manifest'])
 for path,digest in world['sources'].items():ex.source(path,digest)
 resource=ex.add_resource(world['path'],world['width'],world['height'],world['columns'],world['rows'],expected=world['sha256'])
 attack_native=decode(ex.document(a['attack_receipt']))
 doll_native=decode(ex.document(a['template_receipt']))
 require(attack_native['yaml'][a['yaml_slot']]==doll_native['yaml'][a['yaml_slot']]and attack_native['animations'][a['animation_slot']]==doll_native['animations'][a['animation_slot']],'Pillow Floater differs from checked native tracks')
 # Same original Floater YAML/Animation tracks, independent source image/origin.
 profile=copy.deepcopy(s['ActorProfile'][a['template_profile_index']]);profile.update(stable_id=a['profile_id'],primary_resource=resource,flags=0,
  animation_binding_first=len(s['AnimationBinding']),animation_binding_count=0,direction_first=len(s['DirectionFrame']),direction_count=0,
  sprite_offset=[f32(attack_native['yaml'][a['yaml_slot']]['offset'][0]),f32(-int(world['height']/(world['rows']*2))+attack_native['yaml'][a['yaml_slot']]['offset'][1])])
 s['ActorProfile'].append(profile)
 source=node_block(ex.text(a['scene']),a['node'])
 require(json.loads(prop(source,'sprite'))==a['sprite']and prop(source,'no_shadow')=='true','Pillow source profile')
 s['ActorInstance'].append(dict(stable_id=a['actor_id'],profile_index=a['profile_index'],binding_kind=a['binding_kind'],flags=a['actor_flags'],display_name_string=ex.string(a['name']),position=pair(prop(source,'position')),direction=a['direction'],initial_clip=profile['idle_clip']))
 clip_names[a['clip_alias']]=clip_names[a['template_clip']]
 door_native=decode(ex.document(a['door_receipt']));clip=door_native[a['emote']]
 emote=s['Resource'][profile['emote_resource']]
 clip_names[a['emote']]=add_clip(clip['length'],clip['keys'],int(clip['loop'])|8|(16 if clip['keys'][0][0]>0 else 0),frame_count=emote['columns']*emote['rows'],channel=1)
 battle_resource=ex.add_resource(a['battle_path'],kind=3)
 s['Battle'].append(dict(stable_id=a['battle_id'],enemy_string=ex.string(a['battle_enemy']),actor_instance_index=a['actor_index'],win_flag_index=NONE,advantage=s['Battle'][a['battle_template']]['advantage'],flags=s['Battle'][a['battle_template']]['flags']&~a['flag_remove'],win_cutscene_string=ex.string(identity(LEAVE)),battle_resource_index=battle_resource))
 script=ex.text(a['dialogue_script']);actor=ex.text(a['actor_script'])
 end=return_duration(script)
 turn,move,jump,loop,repeat=(binding['facts'][key]['value']for key in ['turn','move','jump','loop','repeat'])
 s['Rule'].extend([dict(key=26,scalar_type=2,value=loop),dict(key=27,scalar_type=2,value=repeat)])
 flags={ex.strings[f['name_string']]:i for i,f in enumerate(s['Flag'])}
 graph=tutorial_graph(docs[TUTORIAL],translations,end,root=ex.root)
 for path,commands,labels in [(p,compile_linear(p,docs[p],end,ids,turn,move,jump,root=ex.root),list(docs[p]))for p in (ATTACK,LEAVE,DOOR)]+[(TUTORIAL,graph['commands'],graph['source_labels'])]:
  first=len(s['Command'])
  for a in commands:
   kind=a['kind'];target=a.get('target_index',NONE)
   if kind=='MoveActorPath':
    p=a['path'];target=len(s['MovementPath']);entry=len(s['MovementEntry'])
    for e in p['movement']:s['MovementEntry'].append(dict(kind=int('wait'in e),vector=[0,0]if'wait'in e else[f32(e['x']),f32(e['y'])],duration=e.get('wait',0)))
    s['MovementPath'].append(dict(stable_id=target+1,first_entry=entry,entry_count=len(p['movement']),flags=int(p['type']=='step')|int(p.get('loop',False))*4|int(p.get('queue',False))*8,animation_motion=binding['append']['motion_id'] if p.get('animation')==binding['append']['motion']else 65535,speed=p['speed']))
   elif kind in('AnimateActor','EmoteActor'):target=clip_names[a['clip']]
   elif kind in('QueueBattle','RequestBattle'):target=binding['append']['battle_index']
   elif kind=='SetFlag':target=flags[a['flag']]
   elif kind=='ShowDialogue':target=a['dialogue_id']if'dialogue_id'in a else ids[a['dialogue_key']]
   elif kind=='AwaitChoices':target=binding['append']['choices_index']
   elif kind=='Jump':target=a['target_pc']
   s['Command'].append(dict(opcode=OPCODES.index(kind),actor_index=binding['append']['actors'][a['actor']],phrase=a['phrase'],target_index=target,flags=a.get('flags',0),vector=[f32(v)for v in a.get('vector',[0,0])],value=a.get('value',0),duration=a.get('duration',0),auxiliary_index=NONE))
  s['Program'].append(dict(stable_id=len(s['Program'])+1,first_command=first,command_count=len(commands),phrase_count=len(labels),source_path_string=ex.string(identity(path))))
 ex.record_map('Pillow/Minnie Programs/Actor/MovementPaths/Rules', ';'.join(PATHS),
  'Original native-parser graphs,14-point loop,queued path,stop_loop,looping shake,repeated jump,strictly source-ordered flags',
  'Append actor6/battle3; original Minnie actor4 reused. No fabricated event positions or optional-path prerequisite')
 for p in ('tools/pillow_dialogue.py','tools/link_pillow_content.py','tools/pillow_source_bindings.py','content/pillow-source-bindings.json'):ex.file(p)

def append_house(ex,ir,room):
 from tools.extract_battle_entry import node,properties,one
 from tools.house_source_bindings import load
 binding=load(ex.root)
 docs=load_documents(ex);rows,translations=texts(ex,docs)
 require(max(d['id']for d in ir['dialogues'])==min(binding['pillow']['text_ids'].values())-1,'Pillow dialogue identity prefix')
 hint=one(r'^const DIALOG_HINT_COLOR := "([0-9a-f]{6})"',ex.text(binding['source_refs']['text_tools']),'hint color')[1]
 kinds={'Literal':1,'PlayerName':2,'HintStart':3,'HintEnd':4}
 for t in rows:
  first=len(ir['segments']);ex.data(t['voice'])
  for n,segment in enumerate(t['segments']):
   tokens=[dict(kind=kinds[x['kind']],text=x.get('text',hint if x['kind']=='HintStart'else''))for x in segment['tokens']]
   ir['segments'].append(dict(id=len(ir['segments'])+1,speaker=t['speaker'],voice=t['voice'],tokens=tokens,flags=3 if n+1<len(t['segments'])else 5))
  ir['dialogues'].append(dict(id=binding['pillow']['text_ids'][t['identity']],source_path=t['source_path'],first_segment=first,segment_count=len(t['segments'])))
 programs={room['strings'][p['source_path_string']]:i for i,p in enumerate(room['sections']['Program'])}
 minnie=next(n for n in ir['npcs']if n['source_path']==binding['pillow']['npc_source'])
 minnie['program_index']=programs[binding['pillow']['tutorial']]
 minnie['seen_key']='/root/'+binding['scene_root']+'/'+binding['pillow']['npc_source']+'::1:'+binding['pillow']['tutorial']
 for override in ir['overrides']:
  if override['npc']==ir['npcs'].index(minnie)and override['dialogue']==binding['pillow']['opened']:
   override['dialogue_index']=next(i for i,d in enumerate(ir['dialogues'])if d['source_path']==OPEN)
 scene=ex.text(binding['source_refs']['house']);base=ex.text(binding['source_refs']['area_scene'])
 offset=node(base,binding['nodes']['area_shape'])['position'];shape=properties(one(r'^\[sub_resource type="RectangleShape2D" id='+str(binding['area_shape_id'])+r'\]\n(.*?)(?=^\[)',base,'Pillow trigger shape',re.M|re.S)[1])['extents']
 parent=node(scene,binding['nodes']['doll_parent'])
 for path in binding['pillow']['triggers']:
  n=node(scene,path);pos=n['position'];scale=n.get('scale',[1,1]);first=len(ir['story_conditions'])
  ir['story_conditions'].append(dict(kind=1,flag=parent['disappear_flag'],value=0))
  if n.get('appear_flag'):ir['story_conditions'].append(dict(kind=1,flag=n['appear_flag'],value=1))
  ir['story_conditions'].append(dict(kind=2,flag=n['disappear_flag'],value=0))
  ir['story_triggers'].append(dict(id=len(ir['story_triggers'])+1,source_path=path,dialogue=binding['patterns']['dialogue'].format(dialogue=n['dialog']),center=[pos[i]+scale[i]*offset[i]for i in range(2)],extents=[abs(scale[i])*shape[i]for i in range(2)],first_condition=first,condition_count=len(ir['story_conditions'])-first,disposition=2,program_index=programs[n['dialog']]))
 ir['scope']+='; original Mom-room warps and optional Pillow/Minnie event chain/tutorial'
 ir['sources']=dict(sorted(ex.sources.items()))
 return ir
