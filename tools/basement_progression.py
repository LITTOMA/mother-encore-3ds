#!/usr/bin/env python3
"""Bounded original Mick/key/diary frontend, independent binary policy and linking.

The checked Room scheduler executes the exported typed commands. This module
does not introduce a second VM or translate arbitrary upstream GDScript.
"""
from __future__ import annotations
import argparse,copy,csv,io,json,math,re,struct,sys,zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.extract_battle_entry import Extractor,PIN,require,node
from tools.drawer_program import canonical,digest,read_json,write_json,fields,safe_path
from tools.doll_postwin import return_duration
IR='content/native-basement-progression.json'
REVIEW='reports/basement-progression/source-review.json'
PACK='romfs/data/house.encbasement'
HOUSE='Maps/podunk/Nintens House.tscn';PODUNK='Maps/podunk/podunk.tscn'
NONE=0xffffffff
NAMES=['Strings','KeyItems','Skills','SkillOrder','NpcOverrides','Door','Present','FadeKeys','Parameters','Mick','Music','MusicLifecycle','FadeRect']
FORMATS=[None,'<5I','<4I','<I','<4I','<6I','<6I8f8I','<5f','<If','<3I','<5I8f','<3I','<4f']
STRIDES=[1]+[struct.calcsize(f) for f in FORMATS[1:]]
HEADER=64+16*len(NAMES)

def expected_documents():
    def named(prefix,label,speaker='Mick',**kw):return dict(name='DIALOGUE_'+prefix+'_SPEAKER_'+speaker,text='DIALOGUE_'+prefix+'_'+label,**kw)
    return {
      'Podunk/woof':{'0':named('PODUNK_WOOF','0',options={'DIALOGUE_PODUNK_WOOF_2-OPT_0':'3','DIALOGUE_PODUNK_WOOF_2-OPT_1':'9','cancel':'9'},soundeffect='M3/Woof Woof!.wav'),'3':dict(text='DIALOGUE_PODUNK_WOOF_3',goto='4'),'4':named('PODUNK_WOOF','4',goto='8'),'8':named('PODUNK_WOOF','8',setflags='mick_scratch'),'9':named('PODUNK_WOOF','9',soundeffect='M3/Dog_whining.wav')},
      'Podunk/woof_secret':{'0':named('PODUNK_WOOF_SECRET','0',soundeffect='M3/Woof Woof!.wav')},
      'Podunk/woof_food':{'0':named('PODUNK_WOOF_FOOD','0')},
      'Podunk/woof_deal':{'0':named('PODUNK_WOOF_DEAL','0')},
      'Podunk/woof_key':{'0':named('PODUNK_WOOF_KEY','0',goto='4'),'4':dict(text='DIALOGUE_PODUNK_WOOF_KEY_4',item='KeyBasement',soundeffect='Item Received.mp3',goto='5'),'5':named('PODUNK_WOOF_KEY','5',goto='6'),'6':named('PODUNK_WOOF_KEY','6',setflags='mick_telepathy')},
      'Podunk/cutscenes/ggf_diary':{'0':dict(actors={'ninten':'ninten'},text='DIALOGUE_PODUNK_CUTSCENES_GGF_DIARY_0',goto='1'),'1':dict(text='DIALOGUE_PODUNK_CUTSCENES_GGF_DIARY_1',soundeffect='Item Received.mp3',goto='2'),'2':dict(showbox=False,actorsmove={'ninten':dict(movement=[{'x':128,'y':1040}],speed=32,animation='Walk',type='position')},actorsturn={'ninten':dict(x=0,y=1,queue=True)},wait=1,autoadvance=True,caninput=False,goto='3'),'3':dict(changecam='ninten',returncam=.01,objectsfunction={'AnimationPlayer':'play_anim','MusicArea2':'stop_music'},wait=7,autoadvance=True,caninput=False,goto='4',actorsanim={'ninten':dict(type=1,anim='NintenDiarySleep')}),'4':dict(name='DIALOGUE_PODUNK_CUTSCENES_GGF_DIARY_SPEAKER_???',sound='Female',text='DIALOGUE_PODUNK_CUTSCENES_GGF_DIARY_4',objectsfunction={'melodyBG':'appear'},wait=2,goto='5'),'5':dict(name='DIALOGUE_PODUNK_CUTSCENES_GGF_DIARY_SPEAKER_???',sound='Female',text='DIALOGUE_PODUNK_CUTSCENES_GGF_DIARY_5',wait=2,goto='6'),'6':dict(name='DIALOGUE_PODUNK_CUTSCENES_GGF_DIARY_SPEAKER_???',sound='Female',text='DIALOGUE_PODUNK_CUTSCENES_GGF_DIARY_6',wait=2,goto='7'),'7':dict(showbox=False,wait=3,autoadvance=True,caninput=False,goto='8',actorsanim={'ninten':dict(type=1,anim='NintenDiaryWakeUp')}),'8':dict(text='DIALOGUE_PODUNK_CUTSCENES_GGF_DIARY_8',goto='9'),'9':dict(text='DIALOGUE_PODUNK_CUTSCENES_GGF_DIARY_9',soundeffect='M3/Learned PSI.wav',objectsfunction={'MusicArea2':'play_music','melodyBG':'disappear'},wait=.6,goto='10',learnskills={'ninten':'lifeUpA'}),'10':dict(text='DIALOGUE_PODUNK_CUTSCENES_GGF_DIARY_10')},
      'Reusable/lockopened':{'0':dict(text='DIALOGUE_REUSABLE_LOCKOPENED_0')},
      'Reusable/locklocked':{'0':dict(text='DIALOGUE_REUSABLE_LOCKLOCKED_0')},
      'ItemDialogue/presentempty':{'0':dict(text='DIALOGUE_ITEMDIALOGUE_PRESENTEMPTY_0')},
    }

def text_segments(raw):
    require(raw.startswith('[@]'),'Basement dialogue source bullet missing')
    result=[]
    for part in re.split(r'\[(?:WAIT@|W@)\]',raw[3:]):
        tokens=[];color=False
        for token in re.split(r'(\[[^\]]+\])',part):
            if not token:continue
            if token in ('[Ninten]','[PartyLead]'):tokens.append(dict(kind='PlayerName'))
            elif token=='[FavFood]':tokens.append(dict(kind='FavoriteFood'))
            elif token=='[ItemName]':tokens.append(dict(kind='CurrentItemName'))
            elif token=='[ItemArt1]':tokens.append(dict(kind='CurrentItemArticle1'))
            elif token in ('[ui_select]','[ui_toggle]','[ui_accept]'):tokens.append(dict(kind='InputAction',action=token[1:-1]))
            elif token=='[color]':require(not color,'Nested basement hint');color=True;tokens.append(dict(kind='HintStart'))
            elif token=='[/color]':require(color,'Basement hint close');color=False;tokens.append(dict(kind='HintEnd'))
            else:require('[' not in token and ']' not in token,'Unknown basement text control: '+token);tokens.append(dict(kind='Literal',text=token))
        require(not color,'Basement hint crosses segment');result.append(dict(bullet=True,tokens=tokens))
    return result

def build(root=ROOT):
    ex=Extractor(root);scene=ex.text(HOUSE);town=ex.text(PODUNK)
    from tools.basement_music_regions import build as music_build
    music=music_build(root,ex)
    npc=ex.text('Scripts/Main/npc.gd');inventory=ex.text('Scripts/global/Inventory.gd');item=ex.text('Scripts/global/Item.gd')
    holder=ex.text('Scripts/Main/ItemHolder.gd');present=ex.text('Scripts/Main/Present.gd');door=ex.text('Scripts/Main/Openable Door.gd')
    dialogue=ex.text('Scripts/UI/DialogueBox.gd');party=ex.text('Scripts/global/PartyMember.gd');save=ex.yaml('Data/save_new_game.yaml')
    for p in ('Scripts/UI/AbstractDialogueBox.gd','Scripts/global/text_tools.gd','Scripts/global/globalData.gd','Scripts/global/global.gd','Scripts/global/uiManager.gd','Scripts/Main/actor.gd','Scripts/Main/Camera2D.gd','Scripts/Main/FlaggableObject.gd','Scripts/misc/Anim_start.gd','Maps/podunk/Ninten_s room.gd','Nodes/Reusables/npc.tscn','Nodes/Overworld/Objects/Present.tscn','Nodes/Ui/effects/melodyBG.gd','Nodes/Ui/effects/melodyBG.tscn','Nodes/Overworld/MusicChanger.tscn','LICENSE'):ex.text(p)
    require(save['party']==['ninten'],'Basement active party scope changed')
    require('ret = cur_dialog' in npc and '_all_thoughts.push_front(["", _thoughts])' in npc and '_all_dialog.push_front(["", dialog])' in npc,'Mick ordered override source changed')
    require('uiManager.open_dialogue_box(_get_right_dialog(true, true))' in npc and 'uiManager.set_telepathy_effect(true, self)' in npc,'Mick telepathy source changed')
    require('return globaldata.key_items.add_item_by_name(item_name)' in inventory and 'var item := Item.new(item_name)' in inventory and 'self.doses = get_data().get("doses", 1)' in item,'Key item construction source changed')
    require(holder.index('_play_interact()',holder.index('func _check_item'))<holder.index('Inventory.add_item_available(item)')<holder.index('_set_flag_status()',holder.index('func _check_item'))<holder.index('uiManager.open_dialogue_box(dialog)'),'Present item/flag/dialogue source order changed')
    require('$AnimationPlayer.play("Unwrapped")' in present and '$Sprite.frame = 4' in present,'Present animation source changed')
    require('globaldata.flags[flag] = true\n\topen(); unlock()' in door and 'if remove_key: Inventory.drop_item_from_party(key_item)' in door and 'global.item = key_item' in door,'Key door source changed')
    require('globaldata.characters[member].add_skill(skill)' in dialogue and 'if !skill_name in _learned_skills:' in party and '_learned_skills.sort_custom(self, "_sort_skills")' in party,'Diary source skill semantics changed')
    mick=node(town,'Objects/NPCS/npc21');require(mick['sprite']=='Npcs/4dir/mick' and mick['dialog']=='Podunk/woof' and mick['_thoughts']=='Podunk/woof_food','Mick source binding changed')
    require(mick['_all_dialog']==[['mick_scratch','Podunk/woof_secret'],['mick_telepathy','Podunk/woof_deal'],['got_dog_treats','Podunk/woof_treats'],['gave_treats','Podunk/woof_animals']] and mick['_all_thoughts']==[['mick_scratch','Podunk/woof_key'],['mick_telepathy','Podunk/woof_food']],'Mick precedence changed')
    original_door=node(scene,'Below/Openable Door3');original_present=node(scene,'Objects/Present1')
    inherited=ex.text('Nodes/Overworld/Objects/Present.tscn');shape=re.findall(r'\[sub_resource type="RectangleShape2D" id=6\]\nextents = Vector2\( ([^)]+) \)',inherited);require(len(shape)==1,'Present inherited interaction shape changed');extents=[float(v)for v in shape[0].split(',')];offset=node(inherited,'interact/CollisionShape2D')['position'];scale=node(scene,'Objects/Present1/interact')['scale'];pos=original_present['position'];interaction=[pos[i]+offset[i]*scale[i]for i in(0,1)]+[extents[i]*scale[i]for i in(0,1)]
    require(original_door['key']=='KeyBasement' and original_door['flag']=='ninten_basement_door' and 'remove_key' not in original_door,'Basement door identity/removal changed')
    require(original_present['item']=='GGFDiary' and original_present['flag']=='got_diary' and original_present['dialog']=='Podunk/cutscenes/ggf_diary' and original_present['type']=='briefcase','Diary present identity changed')
    flaggable=ex.text('Scripts/Main/FlaggableObject.gd');require('export (bool) var can_pickup := true' in holder and 'var player_turn := { \n\t"y": true,\n\t"x": true\n}' in holder,'Present pickup/turn defaults changed');require(all('export (bool) var '+name+' := false' in flaggable for name in ('is_object_flag','emit_flag_updated_signal','reset_when_leaving_region','reset_when_leaving_area'))and 'reset_when_leaving_region = false' in holder,'Present flag/reset defaults changed');present_policy={name:original_present.get(name,False) for name in ('emit_flag_updated_signal','is_object_flag','reset_when_leaving_area','reset_when_leaving_region')};require(not any(present_policy[name] for name in ('is_object_flag','reset_when_leaving_area','reset_when_leaving_region')),'Unsupported Present object flag/reset side effect');present_policy.update(can_pickup=original_present.get('can_pickup',True),player_turn=3)
    documents=expected_documents()
    for identity,expected in documents.items():
        doc=ex.yaml('Data/Dialogue/'+identity+'.yaml');require(canonical(doc)==canonical(expected) and list(doc)==list(expected),'Unreviewed basement graph: '+identity)
    tables={}
    for name in ('dialogue_Podunk','dialogue_Podunk_cutscenes','dialogue_Reusable','dialogue_ItemDialogue','items'):
        for row in csv.DictReader(io.StringIO(ex.text('Translations/TranslatedText/'+name+' - sheet.csv'))):
            require(row['key'] not in tables,'Ambiguous basement translation key');tables[row['key']]=row
    keyitems=[]
    for stable,name,grant in ((1,'KeyBasement',True),(2,'GGFDiary',True),(3,save['key_items'][0]['item_name'],False)):
        doc=ex.yaml('Data/Items/'+name+'.yaml');require(doc['keyitem'] is True and doc.get('doses',1)==1 and doc.get('transform','')=='' and not doc.get('actions') and not any(doc['boost'].values()) and doc['HPrecover']==doc['PPrecover']==0,'Unreviewed basement key item effect')
        keyitems.append(dict(id=stable,source=name,doses=doc.get('doses',1),grant=grant,name_key=doc['name'],names={lang:tables[doc['name']][col] for lang,col in [('en','en'),('zh_Hans_CN','zh_CN')]},article_key=doc['article'],articles={lang:tables[doc['article']][col] for lang,col in [('en','en'),('zh_Hans_CN','zh_CN')]}))
    source_order=re.search(r'const SKILLS_ORDER[^=]*=\s*\[([\s\S]*?)\]',party);require(source_order is not None,'Source skill sort order missing')
    skill_order=re.findall(r'"([^"]+)"',source_order[1]);require(len(skill_order)==len(set(skill_order)) and 'telepathy' in skill_order and 'lifeUpA' in skill_order,'Source skill order ambiguous')
    skill=ex.yaml('Data/BattleSkills/lifeUpA.yaml');require(skill['level']==0 and skill['skill_type']=='psi','Diary learned skill source changed')
    field_skill=ex.yaml('Data/FieldSkills/telepathy.yaml');require(field_skill.get('battle_skill')=='telepathy','Telepathy learned skill prerequisite changed')
    animations=ex.yaml('Data/Animations/NintenCutscene.yaml')
    diary_animations={name:animations['animations'][name] for name in ('NintenDiarySleep','NintenDiaryWakeUp')}
    for name,animation in diary_animations.items():require(animation['type']==1 and len(animation['directions'])==1 and animation['directions'][0][0]==0,'Unreviewed diary animation: '+name)
    # Only the reviewed white fade track is accepted. Anim_start.play_anim plays
    # the configured animation; house's animation_finished reacts to cutscene only.
    block=re.search(r'\[sub_resource type="Animation" id=100\]\n([\s\S]*?)(?=\n\[)',scene);require(block is not None,'Diary white fade absent');body=block[1]
    require('tracks/0/path = NodePath("ColorRect:modulate")' in body and 'tracks/0/interp = 1' in body and '"update": 0' in body and len(re.findall(r'tracks/\d+/type',body))==1,'Unknown diary fade track')
    times=[float(x.strip()) for x in re.search(r'"times": PoolRealArray\( ([^)]+) \)',body)[1].split(',')]
    colors=[[float(v.strip()) for v in color.split(',')] for color in re.findall(r'Color\( ([^)]+) \)',body)]
    require(len(times)==len(colors)==5 and times[0]==0 and all(a<b for a,b in zip(times,times[1:])) and all(len(c)==4 and all(0<=v<=1 for v in c) for c in colors),'Diary fade keys changed')
    fade_node=re.search(r'\[node name="ColorRect" type="ColorRect" parent="."\]\n([\s\S]*?)(?=\n\[)',scene);require(fade_node is not None,'Diary fade node absent')
    margins=[float(re.search(r'^margin_'+side+r' = ([-0-9.]+)$',fade_node[1],re.M)[1])for side in('left','top','right','bottom')];fade_rect=[margins[0],margins[1],margins[2]-margins[0],margins[3]-margins[1]]
    require('modulate = Color( 1, 1, 1, 0 )'in fade_node[1],'Unknown diary fade base color')
    fade_length=float(re.search(r'^length = ([0-9.]+)',body,re.M)[1]);require(times[-1]==fade_length,'Diary fade duration mismatch')
    require(scene.count('"anims/white fade"')==1,'Diary quoted animation property changed')
    ap=node(scene.replace('"anims/white fade"','anims/white_fade'),'AnimationPlayer');require(ap['animation']=='white fade' and ap['play_on_start'] is False and ap['anims/white_fade']=={'SubResource':100},'Diary AnimationPlayer source configuration changed')
    turn=re.findall(r'^func turn_to\(newDir, rotSpeed = ([0-9.]+), queue = false\):$',ex.text('Scripts/Main/actor.gd'),re.M);require(len(turn)==1,'Diary actor turn default changed');turn_seconds=float(turn[0])
    text_source=ex.text('Scripts/global/text_tools.gd');hint=re.search(r'^const DIALOG_HINT_COLOR := "([a-f0-9]{6})"',text_source,re.M);require(hint is not None,'Basement hint color missing')
    texts=[];commands=[];programs=[];choices=[];text_id=70
    for identity,doc in documents.items():
        rows=[];labels={}
        def emit(kind,label,**kw):rows.append(dict(kind=kind,phrase=list(doc).index(label),label=label,**kw))
        emit('BeginCutscene','0')
        for label,p in doc.items():
            labels[label]=len(rows)
            if 'item' in p:emit('GrantKeyItem',label,key_id=next(x['id'] for x in keyitems if x['source']==p['item']))
            if p.get('text'):
                segments={lang:text_segments(tables[p['text']][col]) for lang,col in [('en','en'),('zh_Hans_CN','zh_CN')]}
                voice='Audio/Sound effects/text/'+p['sound']+'.mp3' if p.get('sound') else ''
                texts.append(dict(id=text_id,source='Data/Dialogue/'+identity+'.yaml',label=label,key=p['text'],speaker_key=p.get('name',''),speakers={lang:tables[p['name']][col] if p.get('name') else '' for lang,col in [('en','en'),('zh_Hans_CN','zh_CN')]},voice=voice,segments=segments));emit('ShowDialogue',label,text_id=text_id,flags=1);text_id+=1
                if voice:ex.data(voice);ex.data(voice+'.import')
            if p.get('actors'):
                emit('BindActor',label,actor_source='ninten');emit('ActorPersistent',label,actor_source='ninten');emit('YieldIdle',label)
            if 'wait' in p:emit('StartWait',label,duration=p['wait'])
            if 'showbox' in p:emit('HideDialogue',label,flags=1)
            for object_name,method in p.get('objectsfunction',{}).items():emit('CallObjectDeferred',label,object_source=object_name,method=method)
            if p.get('soundeffect'):
                path='Audio/Sound effects/'+p['soundeffect'];ex.data(path);ex.data(path+'.import');emit('PlaySound',label,sound_source=path)
            for actor,movement in p.get('actorsmove',{}).items():emit('MoveActor',label,actor_source=actor,position=[movement['movement'][0]['x'],movement['movement'][0]['y']],speed=movement['speed'],animation=movement['animation'])
            for actor,direction in p.get('actorsturn',{}).items():emit('TurnActor',label,actor_source=actor,direction=[direction['x'],direction['y']],queue=direction.get('queue',False),duration=turn_seconds)
            for actor,animation in p.get('actorsanim',{}).items():emit('AnimateSpecialActor',label,actor_source=actor,animation_source=animation['anim'],animation_id=1 if animation['anim']=='NintenDiarySleep'else 2)
            if 'changecam' in p:emit('ChangeCamera',label,actor_source=p['changecam']);emit('YieldIdle',label)
            if 'returncam' in p:emit('ReturnCamera',label,duration=p['returncam'])
            if 'setflags' in p:emit('SetFlag',label,flag_id=p['setflags'],value=1)
            for character,skill_id in p.get('learnskills',{}).items():emit('LearnSkill',label,skill_id=1)
            if p.get('options'):
                emit('AwaitChoices',label,group=identity+'::'+label)
            elif p.get('text'):emit('AwaitDialogue',label,flags=(1 if p.get('wait') else 0))
            else:emit('AwaitTimer',label)
            if 'goto' in p:emit('Jump',label,target_label=p['goto'])
            elif not p.get('options'):
                emit('StopInteraction',label,flags=1);emit('SetTalker',label)
                if identity=='Podunk/cutscenes/ggf_diary':emit('RestoreActor',label,actor_source='ninten')
                emit('CutsceneEnded',label);emit('DialogueDone',label,duration=return_duration(dialogue))
        for row in rows:
            if 'target_label' in row:row['target_pc']=labels[row['target_label']]
        for label,p in doc.items():
            if p.get('options'):choices.append(dict(id=identity+'::'+label,program_identity=identity,source_label=label,program_command_count=len(rows),initial_selection=0,options=[dict(translation_key=k,text=tables[k]['en'],target_label=v,target_pc=labels[v]) for k,v in p['options'].items() if k!='cancel'],cancel_target_label=p['options']['cancel'],cancel_target_pc=labels[p['options']['cancel']]))
        programs.append(dict(identity=identity,document=doc,commands=rows,labels=labels))
    override_rows=[dict(thoughts=thoughts,flag=flag,program=path,supported=path in documents) for thoughts,array in [(False,[['',mick['dialog']]]+mick['_all_dialog']),(True,[['',mick['_thoughts']]]+mick['_all_thoughts'])] for flag,path in array]
    return dict(schema=1,kind='encore.basement-progression.source-ir',commit=PIN,scope='Original Mick scratch/Telepathy key chain, kept basement key, diary present and full source dream/learn-LIFEUP graph; no generic VM',sources=dict(sorted(ex.sources.items())),key_items=keyitems,skills=[dict(id=1,character='ninten',skill='lifeUpA',sort_rank=skill_order.index('lifeUpA'))],skill_order=skill_order,mick=dict(scene=PODUNK,node='Objects/NPCS/npc21',telepathy_skill=field_skill['battle_skill'],overrides=override_rows),door=dict(scene=HOUSE,node='Below/Openable Door3',key_id=1,flag='ninten_basement_door',remove_key=False,opened='Reusable/lockopened',locked='Reusable/locklocked'),present=dict(stable_id=zlib.crc32((HOUSE+':Objects/Present1').encode())&NONE,scene=HOUSE,node='Objects/Present1',key_id=2,flag='got_diary',dialogue='Podunk/cutscenes/ggf_diary',empty='ItemDialogue/presentempty',position=original_present['position'],interaction_scale=node(scene,'Objects/Present1/interact')['scale'],closed_frame=0,opened_frame=4,interaction=interaction,policy=present_policy),diary_animations=diary_animations,white_fade=dict(length=fade_length,rect=fade_rect,keys=[dict(time=t,color=c) for t,c in zip(times,colors)]),hint_color=hint[1],music=music,texts=texts,programs=programs,choice_groups=choices)

def extract(root=ROOT):
    root=Path(root);ir=build(root);write_json(root/IR,ir);write_json(root/REVIEW,dict(schema=1,commit=PIN,ir_sha256=digest(root/IR),sources=ir['sources'],scope=ir['scope'],semantics=['Dialogue key grant occurs before text; key item constructor consumes ordinary source UID/random/clock policy','Normal scratch choice writes mick_scratch; thought overrides use last matching source order and grant KeyBasement before mick_telepathy','Openable uses key, writes flag, opens/unlocks, keeps key, then shows source item-name feedback','Present animates first, grants GGFDiary independently of ordinary inventory capacity, writes got_diary, stops sparkles, then starts complete diary graph','Diary actor motion/sleep/wake, deferred white fade and owned music/melody effects, waits, camera and lifeUpA learning retain typed source phases','Skill add is idempotent and sorted by independent source SKILL_ORDER'],unsupported=['DogTreats and gave_treats Mick branches; selecting these active overrides fails closed','Using newly learned LifeUp through unimplemented battle or PSI UI','Other key items/party/diary variants'],unverified=['Manual suites not run','3DS build pending integration','Emulator and hardware gameplay/visual/audio parity pending']));return ir

def load(root=ROOT):
    root=Path(root);ir=read_json(root/IR);require(canonical(ir)==canonical(build(root)),'Stale/unreviewed basement IR');r=read_json(root/REVIEW);require(r['schema']==1 and r['commit']==PIN and r['sources']==ir['sources'] and r['ir_sha256']==digest(root/IR),'Basement source review mismatch');return ir

def lower(ir):
    fields(ir,('schema','kind','commit','scope','sources','key_items','skills','skill_order','mick','door','present','diary_animations','white_fade','hint_color','music','texts','programs','choice_groups'),'Basement IR')
    require(type(ir['schema']) is int and ir['schema']==1 and ir['kind']=='encore.basement-progression.source-ir' and ir['commit']==PIN,'Basement source schema/pin')
    pool=bytearray(b'\0');offsets={'':0};out={n:[] for n in NAMES}
    def string(v):
        require(type(v) is str and '\0' not in v and len(v.encode())<=4096,'Basement string invalid')
        if v not in offsets:offsets[v]=len(pool);pool.extend(v.encode()+b'\0')
        return offsets[v]
    out['KeyItems']=[[k['id'],string(k['source']),k['doses'],int(k['grant']),string(k['name_key'])] for k in ir['key_items']]
    out['Skills']=[[k['id'],string(k['character']),string(k['skill']),k['sort_rank']] for k in ir['skills']]
    out['SkillOrder']=[[string(s)] for s in ir['skill_order']]
    out['NpcOverrides']=[[int(r['thoughts']),string(r['flag']),string(r['program']),int(r['supported'])] for r in ir['mick']['overrides']]
    d=ir['door'];out['Door']=[[string(d['node']),d['key_id'],string(d['flag']),int(d['remove_key']),string(d['opened']),string(d['locked'])]]
    p=ir['present'];out['Present']=[[string(p['node']),p['key_id'],string(p['flag']),string(p['dialogue']),string(p['empty']),p['opened_frame'],*p['position'],*p['interaction_scale'],*p['interaction'],int(p['policy']['can_pickup']),p['policy']['player_turn'],int(p['policy']['emit_flag_updated_signal']),int(p['policy']['is_object_flag']),int(p['policy']['reset_when_leaving_area']),int(p['policy']['reset_when_leaving_region']),p['closed_frame'],p['stable_id']]]
    def f32(v):return struct.unpack('<f',struct.pack('<f',v))[0]
    out['FadeKeys']=[[f32(v) for v in [k['time'],*k['color']]] for k in ir['white_fade']['keys']]
    m=ir['music'];regions=[dict(region=m['region'],geometry=m['geometry'],track=m['track'],parent_disappear_flag=m['parent_disappear_flag'],source_ordinal=m['source_ordinal'])]+m['additional_regions'];out['Music']=[[string(m['scene']),string(row['region']['source_path']),string(row['track']['source_path']),row['region']['track_id'],row['region']['id'],*[f32(v)for v in [m['default_stop_seconds'],row['region']['volume_db'],row['region']['fadein_seconds'],row['region']['fadeout_seconds'],*row['geometry']]]]for row in regions]
    out['MusicLifecycle']=[[row['region']['id'],string(row['parent_disappear_flag']),row['source_ordinal']]for row in regions]
    out['FadeRect']=[[f32(v)for v in ir['white_fade']['rect']]]
    out['Parameters']=[[1,f32(ir['white_fade']['length'])]];out['Mick']=[[string(ir['mick']['scene']),string(ir['mick']['node']),string(ir['mick']['telepathy_skill'])]];out['Strings']=bytes(pool);validate(out);return out

def validate(t):
    fields(t,NAMES,'Basement sections');pool=t['Strings'];require(type(pool) is bytes and 0<len(pool)<=65536 and pool[0]==pool[-1]==0,'Basement string pool')
    starts=set();at=0
    while at<len(pool):starts.add(at);end=pool.index(0,at);pool[at:end].decode('utf-8');at=end+1
    def string(i):require(type(i) is int and i in starts,'Basement string reference');return pool[i:pool.index(0,i)].decode()
    def identity(i):return bool(re.fullmatch('[A-Za-z0-9_]+',string(i)))
    require(len(t['KeyItems'])==3 and len(t['Skills'])==1 and 0<len(t['SkillOrder'])<=512 and len(t['Door'])==len(t['Present'])==len(t['Parameters'])==len(t['Mick'])==1 and 1<=len(t['Music'])<=16 and 0<len(t['NpcOverrides'])<=32 and 2<=len(t['FadeKeys'])<=64,'Basement capacity')
    ids=set();sources=set()
    for k in t['KeyItems']:require(len(k)==5 and type(k[0]) is int and k[0]>0 and k[0] not in ids and identity(k[1]) and string(k[1]) not in sources and type(k[2]) is int and 0<k[2]<=65535 and k[3] in (0,1) and identity(k[4]),'Basement key item');ids.add(k[0]);sources.add(string(k[1]))
    order=[string(k[0]) for k in t['SkillOrder']];require(len(set(order))==len(order) and all(re.fullmatch('[A-Za-z0-9_]+',s) for s in order),'Basement skill order')
    for s in t['Skills']:require(len(s)==4 and s[0]>0 and identity(s[1]) and identity(s[2]) and type(s[3]) is int and 0<=s[3]<len(order) and order[s[3]]==string(s[2]),'Basement skill binding')
    defaults=set()
    for r in t['NpcOverrides']:
        require(len(r)==4 and r[0] in (0,1) and (not string(r[1]) or identity(r[1])) and safe_path(string(r[2])) and r[3] in (0,1),'Basement npc override')
        if not string(r[1]):require(r[0] not in defaults,'Duplicate default Mick path');defaults.add(r[0])
    require(defaults=={0,1},'Mick default path missing')
    require(len(t['Mick'][0])==3 and all(safe_path(string(i)) for i in t['Mick'][0][:2]) and identity(t['Mick'][0][2]) and string(t['Mick'][0][2]) in order,'Mick source/skill binding')
    d=t['Door'][0];require(len(d)==6 and safe_path(string(d[0])) and d[1] in ids and identity(d[2]) and d[3] in (0,1) and safe_path(string(d[4])) and safe_path(string(d[5])),'Basement door')
    p=t['Present'][0];require(len(p)==22 and safe_path(string(p[0])) and p[1] in ids and identity(p[2]) and safe_path(string(p[3])) and safe_path(string(p[4])) and type(p[5]) is int and 0<=p[5]<65536 and all(type(v) in (int,float) and math.isfinite(v) and abs(v)<=8192 for v in p[6:14]) and all(p[i]>0 for i in(8,9,12,13)) and p[14]in(0,1) and 0<=p[15]<=3 and p[16]in(0,1) and p[17:20]==[0,0,0] and type(p[20])is int and 0<=p[20]<65536 and type(p[21])is int and 0<p[21]<=NONE,'Basement present')
    music_ids=set();music_nodes=set()
    for m in t['Music']:
        require(len(m)==13 and safe_path(string(m[0])) and safe_path(string(m[1])) and string(m[2]).startswith('res://Audio/Music/') and safe_path(string(m[2])[6:]) and all(type(i)is int and 0<i<=NONE for i in m[3:5]) and m[4]not in music_ids and string(m[1])not in music_nodes and all(type(v)in(int,float) and math.isfinite(v) for v in m[5:]) and 0<=m[5]<=60 and -120<=m[6]<=24 and all(0<=v<=60 for v in m[7:9]) and all(abs(v)<=8192 for v in m[9:]) and all(v>0 for v in m[11:]),'Basement music binding');music_ids.add(m[4]);music_nodes.add(string(m[1]))
    require(len(t['MusicLifecycle'])==len(t['Music'])and len(t['FadeRect'])==1,'Basement music lifecycle/fade geometry capacity')
    ordinals=set()
    for row,m in zip(t['MusicLifecycle'],t['Music']):require(len(row)==3 and row[0]==m[4]and(not string(row[1])or identity(row[1]))and type(row[2])is int and row[2]>0 and row[2]not in ordinals,'Music source lifecycle/order');ordinals.add(row[2])
    rect=t['FadeRect'][0];require(len(rect)==4 and all(type(v)in(int,float)and math.isfinite(v)and abs(v)<=8192 for v in rect)and rect[2]>0 and rect[3]>0,'Diary fade rectangle')
    last=-1
    for k in t['FadeKeys']:require(len(k)==5 and all(type(v) in (int,float) and math.isfinite(v) for v in k) and k[0]>last and k[0]>=0 and all(0<=v<=1 for v in k[1:]),'Basement fade key');last=k[0]
    require(t['FadeKeys'][0][0]==0 and t['Parameters'][0][0]==1 and math.isfinite(t['Parameters'][0][1]) and 0<t['Parameters'][0][1]<=120 and abs(last-t['Parameters'][0][1])<.00001,'Basement fade duration')

def encode(t):
    validate(t);b=bytearray(HEADER);struct.pack_into('<8s6I20s12x',b,0,b'ENCBASM1',1,0,0,1,1,len(NAMES),bytes.fromhex(PIN))
    for i,n in enumerate(NAMES):
        while len(b)%4:b.append(0)
        struct.pack_into('<4I',b,64+i*16,i+1,len(b),len(t[n]),STRIDES[i]);b.extend(t[n] if i==0 else b''.join(struct.pack(FORMATS[i],*r) for r in t[n]))
    struct.pack_into('<I',b,12,len(b));struct.pack_into('<I',b,16,zlib.crc32(b)&NONE);return bytes(b)

def parse_pack(b):
    require(type(b) in (bytes,bytearray) and HEADER<=len(b)<=1024*1024,'Basement pack size');m,v,n,c,cap,rules,count,pin=struct.unpack_from('<8s6I20s',b);require(m==b'ENCBASM1' and v==cap==rules==1 and n==len(b) and count==len(NAMES) and pin.hex()==PIN and not any(b[52:64]),'Basement header/version');copy=bytearray(b);copy[16:20]=b'\0'*4;require(zlib.crc32(copy)&NONE==c,'Basement CRC');t={};end=HEADER
    for i,name in enumerate(NAMES):
        kind,start,num,stride=struct.unpack_from('<4I',b,64+i*16);require(kind==i+1 and start==(end+3)//4*4 and not any(b[end:start]) and stride==STRIDES[i] and start+num*stride<=len(b),'Basement directory');raw=b[start:start+num*stride];t[name]=bytes(raw) if i==0 else [list(struct.unpack_from(FORMATS[i],raw,j*stride)) for j in range(num)];end=start+num*stride
    require(end==len(b),'Basement trailing bytes');validate(t);return t

def stage_files(source):
    raw=(Path(source)/'data/house.encbasement').read_bytes();require(raw==encode(lower(load(ROOT))),'Stale basement staged binary');parse_pack(raw);return {Path('data/house.encbasement'):raw}

def append_room(ex,resolve_actor,resolve_clip,resolve_binding,resolve_resource,resolve_choice):
    """Append typed programmes using the existing Room scheduler and resolvers.

    All indices are admitted by the normal Room loader. Resolver callbacks must
    reject absent original resources, animations or scene binding methods.
    """
    from tools.native_content import OPCODES
    d=load(ex.root);s=ex.sections;flags={ex.strings[r['name_string']]:i for i,r in enumerate(s['Flag'])}
    require(all(n in OPCODES for n in ('GrantKeyItem','LearnSkill','AnimateSpecialActor')),'Basement typed Room capabilities missing')
    for path,h in d['sources'].items():ex.source(path,h)
    ex.file(IR);ex.file('tools/basement_progression.py')
    for program in d['programs']:
        first=len(s['Command'])
        for c in program['commands']:
            kind=c['kind'];target=NONE;actor=65535;aux=NONE;value=0;duration=c.get('duration',0);vector=[0,0];f=c.get('flags',0)
            if 'actor_source' in c:actor=resolve_actor(c['actor_source'])
            if kind=='ShowDialogue':target=c['text_id']
            elif kind=='GrantKeyItem':target=c['key_id']
            elif kind=='LearnSkill':target=c['skill_id']
            elif kind=='Jump':target=c['target_pc']
            elif kind=='SetFlag':require(c['flag_id'] in flags,'Basement flag not linked');target=flags[c['flag_id']];value=c['value']
            elif kind=='PlaySound':target=resolve_resource(c['sound_source'])
            elif kind=='CallObjectDeferred':target=resolve_binding(c['object_source'],c['method'])
            elif kind=='AnimateSpecialActor':target=c['animation_id']
            elif kind=='MoveActor':vector=c['position'];value=c['speed']
            elif kind=='TurnActor':vector=c['direction'];f=int(c['queue'])
            elif kind=='AwaitChoices':target=resolve_choice(c['group'])
            s['Command'].append(dict(opcode=OPCODES.index(kind),actor_index=actor,phrase=c['phrase'],target_index=target,flags=f,vector=vector,value=value,duration=duration,auxiliary_index=aux))
        s['Program'].append(dict(stable_id=max((p['stable_id'] for p in s['Program']),default=0)+1,first_command=first,command_count=len(program['commands']),phrase_count=len(program['document']),source_path_string=ex.string(program['identity'])))
    ex.record_map('Basement original programmes',IR,'Mick source options/thoughts/key grant and complete diary dream/learn graph','Reuse checked Room scheduler; key and skill effects bind independent ENCBASM1')

def append_house(ex,house,room,input_action_labels,current_item_labels,locale='en'):
    d=load(ex.root);out=copy.deepcopy(house);require(max(t['id'] for t in out['dialogues'])==69,'Basement stable text prefix changed');require(locale in ('en','zh_Hans_CN'),'Basement locale not admitted')
    for path,h in d['sources'].items():ex.data(path);require(ex.sources[path]==h,'Basement House source mismatch')
    kinds={'Literal':1,'PlayerName':2,'HintStart':3,'HintEnd':4,'FavoriteFood':11}
    for text in d['texts']:
        first=len(out['segments'])
        for i,part in enumerate(text['segments'][locale]):
            tokens=[]
            for t in part['tokens']:
                if t['kind']=='InputAction':require(t['action'] in input_action_labels,'Basement input action unresolved');tokens.append(dict(kind=1,text=input_action_labels[t['action']]))
                elif t['kind']=='CurrentItemName':require(text['id'] in current_item_labels,'Basement item label unresolved');tokens.append(dict(kind=1,text=current_item_labels[text['id']]))
                elif t['kind']=='CurrentItemArticle1':
                    require(text['source']=='Data/Dialogue/Reusable/lockopened.yaml','Unreviewed current-item article context')
                    article=d['key_items'][0]['articles'][locale].split(',');require(len(article)>1,'Basement article form missing');tokens.append(dict(kind=1,text=article[1]))
                else:tokens.append(dict(kind=kinds[t['kind']],text=t.get('text',d['hint_color'] if t['kind']=='HintStart' else '')))
            out['segments'].append(dict(id=len(out['segments'])+1,speaker=text['speakers'][locale],voice=text['voice'],tokens=tokens,flags=3 if i+1<len(text['segments'][locale]) else 5))
        out['dialogues'].append(dict(id=text['id'],source_path=text['source'],first_segment=first,segment_count=len(text['segments'][locale])))
    require(all(any(room['strings'][p['source_path_string']]==g['identity'] for p in room['sections']['Program']) for g in d['programs']),'Basement programme linking missing');out['sources']=dict(sorted(ex.sources.items()));out['scope']+='; original key/diary dialogues';return out

def main():
    p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile','verify']);a=p.parse_args()
    try:
        if a.action=='extract':extract();print('Basement progression source IR extracted');return 0
        blob=encode(lower(load()));parse_pack(blob)
        if a.action=='compile':target=ROOT/PACK;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(blob)
        else:require((ROOT/PACK).read_bytes()==blob,'Stale basement binary')
        print('Basement progression binary:',len(blob),'bytes; source key/skill policy, no second VM')
    except (ValueError,KeyError,OSError,TypeError,struct.error) as e:print('Basement progression rejected:',e,file=sys.stderr);return 1
    return 0
if __name__=='__main__':raise SystemExit(main())
