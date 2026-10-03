#!/usr/bin/env python3
"""Bounded original eight-phrase Doll post-win source adapter.

Original Godot3 YAML parsing and animation construction run only offline. The
runtime receives typed external content; this is not a general script adapter.
"""
from __future__ import annotations
import argparse, json, re, sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.doll_dialogue import ROOT, PIN, ANIMATIONS, SOURCES as ATTACK_SOURCES, run_native, decode, sha, require

SOURCE='Data/Dialogue/Podunk/cutscenes/doll_defeated.yaml'
RECEIPT='reports/doll-postwin/native-parser-animation.json'
REVIEW='reports/doll-postwin/source-review.json'
SOUND='Audio/Sound effects/M3/SMAAAASH.wav'
SOURCES=[SOURCE,*ATTACK_SOURCES[1:],SOUND,SOUND+'.import',
         'Graphics/Character Sprites/Npcs/4dir/minnie.png',
         'Graphics/Character Sprites/Npcs/4dir/minnie.png.import',
         'Translations/TranslatedText/dialogue_Podunk_cutscenes - sheet.csv',
         'Scripts/UI/Battle/BattleSystem.gd','Scripts/global/uiManager.gd']

def return_duration(text):
    body=text[text.index('func _end_dialogue():'):text.index('func _clear_dialogue():')]
    matches=re.findall(r'global.currentCamera.return_(?:camera|offset)\(([0-9.]+)\)',body)
    require(len(matches)==2 and matches[0]==matches[1],'Post-win end camera contract')
    return float(matches[0])

def receipt(ex):
    data=ex.document(RECEIPT);review=ex.document(REVIEW)
    require(review['schema']==1 and review['commit']==PIN and review['whole_handler_approved']is False
            and review['sources']==data['sources'],'Unreviewed Doll post-win source scope')
    require(sha(ex.root/RECEIPT)==review['native_receipt_sha256'],'Changed unreviewed post-win native receipt')
    require(data['schema']==1 and data['commit']==PIN and set(data['sources'])==set(SOURCES),'Unreviewed post-win parser receipt')
    for p,h in data['sources'].items():ex.source(p,h)
    require(all(data['godot'].get(k)==v for k,v in {'major':3,'minor':6,'patch':2,'status':'stable','build':'official','hash':'3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8'}.items()),'Post-win parser version')
    data=decode(data)
    import yaml
    for path,parsed in zip([SOURCE,*ANIMATIONS],data['yaml']):
        require(yaml.safe_load(ex.text(path))==parsed,'Post-win native/Python parser mismatch: '+path)
    return data

def compile_dialogue(doc,end_duration):
    require(isinstance(doc,dict)and list(doc)==[str(i)for i in range(8)],'Post-win phrase graph')
    require(end_duration>0,'Post-win camera return duration')
    result=[]
    def emit(kind,phrase=0,actor='None',**payload):result.append(dict(kind=kind,phrase=phrase,actor=actor,**payload))
    emit('BeginCutscene')
    allowed={'actors','actorsdir','actorsmove','teleportactors','changecam','returncam','wait','autoadvance','caninput','goto','actorsanim','talker','name','sound','text','actorsemote','actorsturn','showbox','actorsjump','actorsshake','setflags','unsetflags','soundeffect','ovbattlemusic'}
    for i in range(8):
        p=doc[str(i)]
        require(isinstance(p,dict)and not(set(p)-allowed),'Unknown post-win phrase field')
        require(p.get('goto')==(str(i+1)if i<7 else None),'Post-win control flow')
        if 'text'in p:
            require(i in(2,7)and p['sound']=='Kid'and 'wait'not in p and p['name']=='DIALOGUE_PODUNK_CUTSCENES_DOLL_DEFEATED_SPEAKER_Mimmie'
                    and p['text']=='DIALOGUE_PODUNK_CUTSCENES_DOLL_DEFEATED_'+str(i),'Post-win text mode')
            emit('ShowDialogue',i,'Mimmie',dialogue_id=4 if i==2 else 5)
        else:
            require(p.get('autoadvance')is True and p.get('caninput')is False and 'wait'in p,'Post-win wait mode')
        if 'actors'in p:
            require(i==0 and list(p['actors'].items())==[('ninten','leader'),('doll','Objects/npcdoll'),('mimmie','Objects/npc2'),('minnie','Objects/npc3')],'Post-win bindings/order')
            for actor in p['actors']:
                emit('BindActor',i,actor.title());emit('ActorPersistent',i,actor.title())
            emit('YieldIdle',i)
        if 'wait'in p:emit('StartWait',i,duration=p['wait'])
        if 'showbox'in p:require(i==3 and p['showbox']is False,'Post-win showbox mode')
        if 'ovbattlemusic'in p:
            require(i==0 and p['ovbattlemusic']is False,'Post-win music policy');emit('OverworldBattleMusic',i,value=0)
        if 'soundeffect'in p:
            require(i==0 and p['soundeffect']=='M3/SMAAAASH.wav','Post-win sound');emit('PlaySound',i,resource='res://'+SOUND)
        if 'talker'in p:
            require(i==2 and p['talker']=='mimmie','Post-win talker');emit('SetTalker',i,'Mimmie')
        for actor,v in p.get('teleportactors',{}).items():
            require(i==0 and actor=='minnie'and set(v)=={'x','y'},'Post-win teleport fields')
            emit('TeleportActor',i,actor.title(),vector=[v['x'],v['y']])
        for actor,v in p.get('actorsdir',{}).items():
            require(actor in('ninten','mimmie')and set(v)=={'x','y'},'Post-win direction fields')
            emit('SetActorDirection',i,actor.title(),vector=[v['x'],v['y']])
        for actor,v in p.get('actorsmove',{}).items():
            require(actor in('doll','mimmie')and set(v)>={'movement','speed','type'}and not(set(v)-{'movement','speed','type','animation'}),'Post-win move fields')
            require(v['type']in('step','position')and v.get('animation','')in('','Walk'),'Post-win move mode')
            require(v['movement']and all(set(e)=={'x','y'}for e in v['movement']),'Post-win move entry')
            emit('MoveActorPath',i,actor.title(),path=v)
        for actor,v in p.get('actorsturn',{}).items():
            require(actor=='ninten'and set(v)=={'x','y','speed'},'Post-win turn fields')
            emit('TurnActor',i,'Ninten',vector=[v['x'],v['y']],duration=v['speed'])
        for actor,v in p.get('actorsshake',{}).items():
            require(i==2 and actor=='mimmie'and set(v)=={'x','length'},'Post-win shake fields')
            emit('ShakeActor',i,'Mimmie',vector=[v['x'],0],duration=v['length'])
        for actor,v in p.get('actorsjump',{}).items():
            require(actor in('doll','mimmie')and set(v)in({'height','speed'},{'height','length'}),'Post-win jump fields')
            emit('JumpActor',i,actor.title(),value=v['height'],duration=v.get('length',v.get('speed')))
        for actor,v in p.get('actorsanim',{}).items():
            require(i==0 and actor=='doll'and v=={'anim':'Idle'},'Post-win animation');emit('AnimateActor',i,'Doll',clip='Doll Idle')
        for actor,v in p.get('actorsemote',{}).items():
            require(i==2 and actor=='mimmie'and v=='dot','Post-win emote');emit('EmoteActor',i,'Mimmie',clip='dot')
        if 'changecam'in p:
            require((i,p['changecam'])in[(0,'doll'),(2,'mimmie')],'Post-win camera actor')
            emit('ChangeCamera',i,p['changecam'].title());emit('YieldIdle',i)
        if 'returncam'in p:emit('ReturnCamera',i,duration=p['returncam'])
        if 'setflags'in p:
            require((i,p['setflags'])in[(0,'doll_defeated'),(1,'pillow_attack')],'Post-win flag')
            emit('SetFlag',i,flag=p['setflags'],value=1)
        if 'unsetflags'in p:
            require(i==0 and p['unsetflags']=='poltergeist','Post-win unset flag');emit('SetFlag',i,flag=p['unsetflags'],value=0)
        emit('AwaitDialogue'if'text'in p else'AwaitTimer',i)
    emit('StopInteraction',7,'Mimmie');emit('SetTalker',7)
    for actor in ('Ninten','Doll','Mimmie','Minnie'):emit('RestoreActor',7,actor)
    emit('CutsceneEnded',7);emit('DialogueDone',7,duration=end_duration)
    return result

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--godot',type=Path,required=True);a=parser.parse_args()
    data=run_native(ROOT,a.godot,SOURCE,'dot',SOURCES)
    out=ROOT/RECEIPT;out.parent.mkdir(parents=True,exist_ok=True);out.write_text(json.dumps(data,indent=2)+'\n')
    review=dict(schema=1,commit=PIN,whole_handler_approved=False,sources=data['sources'],native_receipt_sha256=sha(out),
        scope='Only original doll_defeated eight phrases, four source actors, dot emote, two localized text phrases and camera/world return; no melody/Pillow execution',
        handler_order=['text','actor ready/persistent and idle yields','wait','ovbattlemusic','soundeffect','talker','teleportactors','actorsdir','actorsmove','actorsturn','actorsshake','actorsjump','actorsanim','actorsemote','changecam and idle yield','returncam','setflags','unsetflags','dialogue/timer gate','stop talker','ordered asynchronous NPC restoration','cutscene_ended','done and camera return'],
        source_edges={'jump_speed':'speed and length name the same source jump duration; length overrides speed when both appear; bounded source uses one at a time',
            'idle_play_signal':'Existing Doll Idle clip retains its identity and gains flag4: original Actor.play_anim emits finished_action immediately for idle; original attack only plays Float and initialization does not invoke this event',
            'flags':'doll_defeated true then poltergeist false after camera idle yield in phrase0; pillow_attack true in phrase1; no melody or tutorial flag',
            'minnie':'Objects/npc3 remains at original (472,88) initially and receives explicit phrase0 teleport; event_positions is only applied by npc._ready',
            'restoration':'update_npcs copies position/direction immediately then waits for replacement frame_changed and idle; DialogueBox does not await these coroutines',
            'terminal':'final DialogueDone duration carries original return_camera/return_offset duration; existing battle-program DialogueDone stays zero'},
        not_verified_here=['full Godot game startup','whole DialogueBox or Actor implementation','emulator/hardware','audio audibility'])
    (ROOT/REVIEW).write_text(json.dumps(review,indent=2)+'\n')
    print('Recorded original post-win parser, animation builder and dot emote:',out)
if __name__=='__main__':main()
