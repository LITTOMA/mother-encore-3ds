#!/usr/bin/env python3
"""Closed-source-graph lowering of the original great-grandfather diary event.

The ordinary room VM owns timing, actors, camera, text, audio and completion.
No game constants, asset names or dialogue are embedded in C++.
"""
from __future__ import annotations
import argparse,copy,re,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from tools.extract_battle_entry import Extractor,PIN,require
from tools.house_presents import read_json,write_json,digest,canonical,table,f32
from tools.godot_text import Document,instantiate
IR='content/diary-story.json'
REVIEW='reports/basement-items/diary-source-review.json'
BINDING='content/diary-story-bindings.json'
NONE=0xffffffff
NO_ACTOR=65535
PATH='Data/Dialogue/Podunk/cutscenes/ggf_diary.yaml'
SCENE='Maps/podunk/Nintens House.tscn'
ANIM='Data/Animations/NintenCutscene.yaml'

def validate_document(doc):
    require(list(doc)==[str(i) for i in range(11)],'Unreviewed diary phrase graph/order')
    keys={0:'actors text goto',1:'text soundeffect goto',2:'showbox actorsmove actorsturn wait autoadvance caninput goto',
          3:'changecam returncam objectsfunction wait autoadvance caninput goto actorsanim',
          4:'name sound text objectsfunction wait goto',5:'name sound text wait goto',6:'name sound text wait goto',
          7:'showbox wait autoadvance caninput goto actorsanim',8:'text goto',9:'text soundeffect objectsfunction wait goto learnskills',10:'text'}
    for i,fields in keys.items():
        p=doc[str(i)];require(set(p)==set(fields.split()),'Unknown/missing diary phrase operation: '+str(i))
        if i<10:require(p['goto']==str(i+1),'Diary graph edge changed')
        if 'wait' in p:require(type(p['wait']) in (int,float) and 0<p['wait']<=60,'Diary wait rejected')
        if 'caninput' in p:require(p['caninput'] is False and p['autoadvance'] is True and p.get('showbox',False) is False,'Unreviewed hidden diary phase')
    require(doc['0']['actors']=={'ninten':'ninten'},'Diary actor binding changed')
    require(doc['2']['actorsmove']=={'ninten':dict(movement=[dict(x=128,y=1040)],speed=32,animation='Walk',type='position')} and
            doc['2']['actorsturn']=={'ninten':dict(x=0,y=1,queue=True)},'Unreviewed diary movement/turn')
    require(doc['3']['changecam']=='ninten' and doc['3']['returncam']==.01 and
            doc['3']['objectsfunction']=={'AnimationPlayer':'play_anim','MusicArea2':'stop_music'} and
            doc['4']['objectsfunction']=={'melodyBG':'appear'} and
            doc['9']['objectsfunction']=={'MusicArea2':'play_music','melodyBG':'disappear'},'Unreviewed diary object/camera calls')
    require(doc['3']['actorsanim']=={'ninten':dict(type=1,anim='NintenDiarySleep')} and
            doc['7']['actorsanim']=={'ninten':dict(type=1,anim='NintenDiaryWakeUp')},'Unreviewed diary animation')
    require(doc['9']['learnskills']=={'ninten':'lifeUpA'},'Unreviewed diary skill effect')
    require(doc['1']['soundeffect']=='Item Received.mp3' and doc['9']['soundeffect']=='M3/Learned PSI.wav','Diary sound changes require review')

def build(root=ROOT):
    ex=Extractor(root);binding=read_json(Path(root)/BINDING)
    require(set(binding)=={'schema','kind','commit','source_path','text_ids','menu_button'} and binding['schema']==1 and
            binding['kind']=='encore.diary-story.bindings' and binding['commit']==PIN and binding['source_path']==PATH and
            set(binding['text_ids'])=={'0','1','4','5','6','8','9','10'} and len(set(binding['text_ids'].values()))==8 and
            all(type(i) is int and i>0 for i in binding['text_ids'].values()),'Unreviewed diary text binding')
    require(binding['menu_button']==dict(action='ui_select',label='START',mask=8),'Unreviewed native menu input adaptation')
    doc=ex.yaml(PATH);validate_document(doc)
    scripts={p:ex.text(p) for p in ['Scripts/UI/DialogueBox.gd','Scripts/Main/actor.gd','Scripts/Main/character_sprite.gd',
        'Scripts/global/PartyMember.gd','Scripts/misc/Anim_start.gd','Scripts/global/text_tools.gd']}
    require('var obj = _special_sprite if type == 1 else character_sprite' in scripts['Scripts/Main/actor.gd'] and
            'obj.travel(anim)' in scripts['Scripts/Main/actor.gd'] and
            'frame_index + 1][0]-1' in scripts['Scripts/Main/character_sprite.gd'],'Special animation source mapping changed')
    party=scripts['Scripts/global/PartyMember.gd']
    require('_learned_skills.append(skill_name)' in party and '_learned_skills.sort_custom(self, "_sort_skills")' in party,'Source skill learning changed')
    order=re.search(r'const SKILLS_ORDER := \[(.*?)\n\]',party,re.S)
    require(order is not None,'Missing source skill order')
    skill_order=re.findall(r'"([^"]+)"',re.sub(r'#[^\n]*','',order[1]))
    require('lifeUpA' in skill_order and 'telepathy' in skill_order,'Unknown diary skill ordering')
    from tools.doll_postwin import return_duration
    end_duration=return_duration(scripts['Scripts/UI/DialogueBox.gd'])
    require('play(animation)' in scripts['Scripts/misc/Anim_start.gd'],'Animation trigger semantics changed')
    docs={}
    def load(p):
        if p not in docs:docs[p]=Document(p,ex.text(p))
        return docs[p]
    nodes={n.path:n for n in instantiate(load,SCENE)}
    actor={n.path:n for n in instantiate(load,'Nodes/Reusables/actor.tscn')}
    special=actor['SpecialSprite'].props
    require(special['_auto_offset'] is False and special.get('centered',True) is True,'Special sprite origin changed')
    player=nodes['AnimationPlayer'];anim=load(SCENE).sub_resource(player.props['anims/'+player.props['animation']],'Animation')
    require(anim['tracks/0/path'].path=='ColorRect:modulate' and anim['tracks/0/interp']==1 and
            anim['tracks/0/keys']['update']==0 and not any(k.startswith('tracks/1/') for k in anim),'Unreviewed screen fade animation')
    values=anim['tracks/0/keys']['values']
    require(all(v.r==v.g==v.b==1 for v in values),'Unreviewed fade color')
    overlay=dict(length=f32(anim['length']),keys=[[f32(t),round(v.a*65534)] for t,v in zip(anim['tracks/0/keys']['times'],values)],rgb=0xffffff)
    animation=ex.yaml(ANIM);require(animation['size']==[20,10] and animation['offset']==[0,0],'Special animation grid/offset changed')
    clips={}
    from tools.basement_items import load as items
    frames=items(root)['assets'][1]['frames']
    for name in [doc['3']['actorsanim']['ninten']['anim'],doc['7']['actorsanim']['ninten']['anim']]:
        clip=animation['animations'][name]
        require(clip['type']==1 and len(clip['directions'])==1 and clip['directions'][0][0]==0,'Unreviewed special animation playback')
        elapsed=0;keys=[]
        for frame,seconds in clip['directions'][0][1:]:
            require(frame-1 in frames and type(seconds) in (int,float) and seconds>0,'Unknown special frame/duration')
            keys.append([f32(elapsed),frames.index(frame-1)]);elapsed+=seconds
        clips[name]=dict(length=f32(elapsed),keys=keys)
    rows=table(ex,'Translations/TranslatedText/dialogue_Podunk_cutscenes - sheet.csv');texts=[]
    for label,identity in binding['text_ids'].items():
        p=doc[label];key=p['text'];voice='Audio/Sound effects/text/'+p['sound']+'.mp3' if p.get('sound') else ''
        if voice:ex.data(voice)
        texts.append(dict(id=identity,label=label,source_path=PATH,translation_key=key,raw=rows[key]['en'].replace('[ui_select]',binding['menu_button']['label']),
            localized={lang:rows[key][col].replace('[ui_select]',binding['menu_button']['label']) for lang,col in [('en','en'),('zh_Hans_CN','zh_CN')]},
            speaker=rows[p['name']]['en'] if 'name' in p else '',voice=voice))
    for p in doc.values():
        if p.get('soundeffect'):ex.data('Audio/Sound effects/'+p['soundeffect'])
    hint=re.search(r'^const DIALOG_HINT_COLOR := "([0-9a-f]{6})"',scripts['Scripts/global/text_tools.gd'],re.M)
    require(hint is not None,'Diary hint color missing')
    return dict(hint_color=hint[1],schema=1,kind='encore.diary-story.source-ir',commit=PIN,sources=ex.sources,document=doc,texts=texts,clips=clips,
        overlay=overlay,sprite_position=[special['position'].x,special['position'].y],sprite_offset=[special['offset'].x,special['offset'].y],
        menu_button=binding['menu_button'],skill_order=skill_order,end_duration=end_duration,dependencies={BINDING:digest(Path(root)/BINDING)})

def load(root=ROOT):
    root=Path(root);ir=read_json(root/IR);require(canonical(ir)==canonical(build(root)),'Stale/unreviewed diary source IR')
    require(read_json(root/REVIEW)==dict(schema=1,commit=PIN,ir_sha256=digest(root/IR),sources=ir['sources'],document=ir['document'],
        semantics='Original grant precedes dialogue; move/queued turn; seven-second sleep, white fade and music stop; three delayed voice phrases; wake, music restore, lifeUpA learning; original completion and camera return. Alpha keys quantized to 16-bit, special pixels losslessly repacked.'), 'Diary source review mismatch')
    return ir

def append_room(ex):
    from tools.native_content import OPCODES
    from tools.basement_items import load as items,verify_receipt
    ir=load(ex.root);data=items(ex.root);receipt=verify_receipt(ex.root);s=ex.sections
    for p,sha in ir['sources'].items():ex.source(p,sha)
    asset=data['assets'][1];resource=ex.add_resource(asset['output'],*asset['size'],*asset['grid'],expected=receipt['outputs'][asset['output']]['sha256'])
    def clip(value,channel=0):
        index=len(s['Clip']);first=len(s['Key']);s['Key'].extend(dict(time=t,frame=f) for t,f in value['keys'])
        s['Clip'].append(dict(stable_id=index+1,length=value['length'],first_key=first,key_count=len(value['keys']),flags=2 if channel==0 else 0,
            visibility=0,frame_count=56 if channel==0 else 65535,channel=channel));return index
    clips={name:clip(value) for name,value in ir['clips'].items()};overlay=clip(ir['overlay'],2)
    player=s['Scene'][0]['player_instance_index'];base=s['ActorInstance'][player]['profile_index'];profile=copy.deepcopy(s['ActorProfile'][base]);special=len(s['ActorProfile'])
    profile.update(stable_id=special+1,execution_kind=2,primary_resource=resource,animation_binding_first=len(s['AnimationBinding']),animation_binding_count=0,
        direction_first=len(s['DirectionFrame']),direction_count=0,initial_frame=0,idle_clip=next(iter(clips.values())),sprite_position=ir['sprite_position'],sprite_offset=ir['sprite_offset'])
    s['ActorProfile'].append(profile)
    def audio(path):
        path='res://Audio/Sound effects/'+path
        matches=[i for i,r in enumerate(s['Resource']) if ex.strings[r['path_string']]==path]
        return matches[0] if len(matches)==1 else ex.add_resource(path,kind=2)
    # Same already checked MusicArea2 and melodyBG methods as Doll's melody.
    bindings={}
    for key,kind in [('stop_music',5),('appear',6),('play_music',1),('disappear',7)]:
        matches=[i for i,b in enumerate(s['Binding']) if b['kind']==kind and
                 (kind in (6,7) or ex.strings[s['Resource'][b['target_index']]['path_string']]=='res://Audio/Music/House.mp3')]
        require(len(matches)==1,'Ambiguous diary object method binding: '+key);bindings[key]=matches[0]
    commands=[]
    def emit(kind,phrase,actor=NO_ACTOR,target=NONE,flags=0,vector=None,value=0,duration=0,aux=NONE):
        commands.append(dict(opcode=OPCODES.index(kind),actor_index=actor,phrase=phrase,target_index=target,flags=flags,vector=vector or [0,0],value=value,duration=duration,auxiliary_index=aux))
    emit('BeginCutscene',0);emit('BindActor',0,player)
    ids={t['label']:t['id'] for t in ir['texts']}
    for label,p in ir['document'].items():
        n=int(label)
        if 'wait' in p:emit('StartWait',n,duration=p['wait'])
        if p.get('showbox') is False:emit('HideDialogue',n,flags=1)
        for obj,method in p.get('objectsfunction',{}).items():
            if obj=='AnimationPlayer':emit('ScreenOverlay',n,target=overlay,value=ir['overlay']['rgb'])
            else:emit('CallObjectDeferred',n,target=bindings[method])
        if 'soundeffect' in p:emit('PlaySound',n,target=audio(p['soundeffect']))
        if 'actorsmove' in p:
            move=p['actorsmove']['ninten'];point=move['movement'][0]
            first=len(s['MovementEntry']);s['MovementEntry'].append(dict(kind=0,vector=[point['x'],point['y']],duration=0))
            path=len(s['MovementPath']);s['MovementPath'].append(dict(stable_id=path+1,first_entry=first,entry_count=1,flags=0,animation_motion=4,speed=move['speed']))
            emit('MoveActorPath',n,player,path)
        if 'actorsturn' in p:
            turn=p['actorsturn']['ninten'];emit('TurnActor',n,player,flags=1,vector=[turn['x'],turn['y']],duration=next(r['value'] for r in s['Rule'] if r['key']==10))
        if 'actorsanim' in p:
            if label=='3':emit('SetActorProfile',n,player,special)
            emit('AnimateActor',n,player,clips[p['actorsanim']['ninten']['anim']],flags=1,aux=special)
        if 'changecam' in p:emit('ChangeCamera',n,player)
        if 'returncam' in p:emit('ReturnCamera',n,duration=p['returncam'])
        if 'text' in p:emit('ShowDialogue',n,target=ids[label],flags=1)
        if 'learnskills' in p:emit('LearnSkill',n,target=ex.string(p['learnskills']['ninten']))
        if 'text' in p:emit('AwaitDialogue',n,flags=1 if 'wait' in p else 0)
        elif 'wait' in p:emit('AwaitTimer',n)
    n=10;emit('StopInteraction',n,flags=1);emit('SetTalker',n);emit('SetActorProfile',n,player,base)
    emit('RestoreActor',n,player);emit('CutsceneEnded',n);emit('DialogueDone',n,duration=ir['end_duration'])
    s['Program'].append(dict(stable_id=len(s['Program'])+1,first_command=len(s['Command']),command_count=len(commands),phrase_count=11,
        source_path_string=ex.string(PATH.removeprefix('Data/Dialogue/').removesuffix('.yaml'))));s['Command'].extend(commands)
    for p in (IR,BINDING,'tools/diary_story.py','content/basement-items.json','content/asset-receipts/graphics/basement/source.json'):ex.file(p)
    ex.record_map('Basement diary programme',PATH,'Closed eleven-phrase source graph, original scene white fade and special sprite animation tracks; source skill ordering',
        'Independent room capability 9; alpha samples use 16-bit normalized interpolation; preserved source waits and completion')

def append_house(ex,house):
    from tools.link_house_inspections import text_segments
    ir=load(ex.root);require(max(d['id'] for d in house['dialogues'])==80,'Diary stable dialogue prefix changed')
    out=copy.deepcopy(house)
    for p,sha in ir['sources'].items():ex.data(p);require(ex.sources[p]==sha,'Diary linked source mismatch')
    for text in ir['texts']:
        first=len(out['segments']);parts=text_segments(text['raw'])
        for tokens in parts:
            for token in tokens:
                if token['kind']==3:token['text']=ir['hint_color']
        for i,tokens in enumerate(parts):out['segments'].append(dict(id=len(out['segments'])+1,speaker=text['speaker'],voice=text['voice'],tokens=tokens,flags=3 if i+1<len(parts) else 5))
        out['dialogues'].append(dict(id=text['id'],source_path=PATH,first_segment=first,segment_count=len(parts)))
    out['schema']=8;out['sources']=ex.sources;out['scope']+='; complete original basement diary event text';return out

def main():
    p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','verify']);a=p.parse_args()
    if a.action=='extract':
        ir=build();write_json(ROOT/IR,ir);write_json(ROOT/REVIEW,dict(schema=1,commit=PIN,ir_sha256=digest(ROOT/IR),sources=ir['sources'],document=ir['document'],
            semantics='Original grant precedes dialogue; move/queued turn; seven-second sleep, white fade and music stop; three delayed voice phrases; wake, music restore, lifeUpA learning; original completion and camera return. Alpha keys quantized to 16-bit, special pixels losslessly repacked.'))
    else:load()
    return 0
if __name__=='__main__':raise SystemExit(main())
