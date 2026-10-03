#!/usr/bin/env python3
"""Compile the reviewed lamp_attack YAML through its original Godot parser.

This is a bounded command adapter, not EncoreScript ECS or a GDScript translator.
The source review gates both the parser and the command handler. Compilation of
an offline parsed receipt is supported, but its contents must match the reviewed
native-parser output exactly.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[1]
REVIEW=ROOT/'compatibility/reviews/lamp-dialogue-v0410.json'
SOURCE='Data/Dialogue/Podunk/cutscenes/lamp_attack.yaml'
COMMIT='7d9246600fffe518408f5830d4848635019005a3'
SOURCE_FILES=[SOURCE,'Scripts/UI/DialogueBox.gd','Scripts/global/yaml_parser.gd','Nodes/Ui/DialogueBox.tscn','Scripts/Main/actor.gd','Scripts/Main/npc.gd',
    'Nodes/Overworld/MusicChanger.tscn','Scripts/Main/roomshaker.gd','Nodes/Reusables/roomshaker.tscn','Maps/podunk/Nintens House.tscn','Scripts/global/audioManager.gd']
RECEIPT=ROOT/'reports/m5-lamp-dialogue-reference/dialogue.json'
OUTPUT=ROOT/'build/native/dialogue.json'

def digest(path): return hashlib.sha256(path.read_bytes()).hexdigest()
def canonical(value): return json.dumps(value,ensure_ascii=False,separators=(',',':')).encode()
def verify_review(review):
    if review.get('schema')!=1 or review.get('commit')!=COMMIT or review.get('whole_handler_approved') is not False:
        raise ValueError('Unreviewed dialogue schema/version/scope')
    if review.get('game_version')!='0.4.1.0': raise ValueError('Unreviewed game version')
    if set(review.get('sources',{}))!=set(SOURCE_FILES): raise ValueError('Missing source body gate')
    if any(not isinstance(h,str) or len(h)!=64 or any(c not in '0123456789abcdef' for c in h) for h in review['sources'].values()):
        raise ValueError('Invalid source fingerprint')

def verify_sources(source,review):
    verify_review(review)
    for name,expected in review['sources'].items():
        if digest(source/name)!=expected: raise ValueError('Changed unreviewed source: '+name)

def parse_native(source,godot,review):
    verify_sources(source,review)
    with tempfile.TemporaryDirectory(prefix='encore-lamp-parser-') as tmp:
        work=Path(tmp)
        (work/'project.godot').write_text('config_version=4\n[application]\nconfig/name="Lamp YAML parser reference"\n[logging]\nfile_logging/enable_logging=false\n')
        (work/'yaml_parser.gd').write_bytes((source/'Scripts/global/yaml_parser.gd').read_bytes())
        (work/'lamp.yaml').write_bytes((source/SOURCE).read_bytes())
        (work/'probe.gd').write_text('''extends SceneTree
func _init():
    var parser = load("res://yaml_parser.gd")
    var result = parser.parse_file("res://lamp.yaml")
    var file = File.new()
    file.open("res://result.json", File.WRITE)
    file.store_string(JSON.print({"godot": Engine.get_version_info(), "dialogue": result}))
    file.close()
    quit()
''')
        env=dict(os.environ,XDG_DATA_HOME=str(work/'data'),XDG_CONFIG_HOME=str(work/'config'),XDG_CACHE_HOME=str(work/'cache'))
        result=subprocess.run([str(godot),'--path',str(work),'--script','probe.gd'],capture_output=True,text=True,timeout=30,env=env)
        if result.returncode or 'SCRIPT ERROR' in result.stdout+result.stderr or 'ERROR:' in result.stdout+result.stderr:
            raise ValueError('Native YAML parser failed: '+result.stdout+result.stderr)
        data=json.loads((work/'result.json').read_text())
        data.update(schema=1,commit=COMMIT,sources=review['sources'])
        data['log']=result.stdout+result.stderr
        return data

def keys(value,expected):
    if not isinstance(value,dict) or set(value)!=set(expected): raise ValueError('Unknown/missing dialogue fields: '+str(value))
def number(value,lo=0,hi=1000):
    if type(value) not in (int,float) or not math.isfinite(value) or not lo<=value<=hi:
        raise ValueError('Unreviewed numeric dialogue value')
    return value

def action(kind,phrase=0,actor='None',vector=(0,0),value=0,duration=0,text='',detail=''):
    return dict(kind=kind,phrase=phrase,actor=actor,vector=list(vector),value=value,duration=duration,text=text,detail=detail)

def compile_phrase(phrase,index):
    if type(index) is not int or not 0<=index<=12: raise ValueError('Invalid lamp phrase index')
    allowed={'actors','talker','wait','autoadvance','caninput','goto','actorsanim','music','actorsturn','movecam','changecam',
        'actorsshake','actorsemote','objectsfunction','actorsmove','actorsjump','soundeffect','shakecam','startbattle','ovbattlemusic'}
    if not isinstance(phrase,dict) or set(phrase)-allowed: raise ValueError('Unsupported lamp dialogue command')
    if phrase.get('autoadvance') is not True or phrase.get('caninput') is not False or 'wait' not in phrase:
        raise ValueError('Unsupported lamp phrase input/wait mode')
    if index<12 and phrase.get('goto')!=str(index+1) or index==12 and 'goto' in phrase:
        raise ValueError('Unsupported lamp control flow')
    out=[]
    def emit(kind,**kwargs): out.append(action(kind,index,**kwargs))
    if 'actors' in phrase:
        if index!=0 or phrase['actors']!={'ninten':'leader','lamp':'Objects/lamp'} or list(phrase['actors'])!=['ninten','lamp']:
            raise ValueError('Unreviewed actor bindings/order')
        for name,target in phrase['actors'].items():
            emit('BindActor',actor=name.title(),text=target)
            emit('ActorPersistent',actor=name.title())
        emit('YieldIdle')
    emit('StartWait',duration=number(phrase['wait'],0.001,2))
    # This order comes from DialogueBox._handle_phrase, never YAML key order.
    if 'objectsfunction' in phrase:
        obj=phrase['objectsfunction']
        valid={'Poltergeist/MusicArea':'play_music','Room Shaker':'delayed_start'}
        if not isinstance(obj,dict) or len(obj)!=1 or any(valid.get(k)!=v for k,v in obj.items()):
            raise ValueError('Unreviewed object method/body')
        for target,method in obj.items(): emit('CallObjectDeferred',text=target,detail=method)
    if 'ovbattlemusic' in phrase:
        if phrase['ovbattlemusic'] is not True: raise ValueError('Unreviewed battle music setting')
        emit('OverworldBattleMusic',value=1)
    if 'music' in phrase:
        if phrase['music']!='': raise ValueError('Unreviewed music command')
        emit('MusicFadeOut',value=0,duration=2)
    if 'soundeffect' in phrase:
        if phrase['soundeffect']!='bash.mp3': raise ValueError('Unreviewed sound effect')
        emit('PlaySound',text='res://Audio/Sound effects/bash.mp3',detail='dialogBoxSound')
    if 'talker' in phrase:
        if phrase['talker']!='lamp': raise ValueError('Unreviewed talker')
        emit('SetTalker',actor='Lamp')
    if 'actorsmove' in phrase:
        keys(phrase['actorsmove'],['lamp']); move=phrase['actorsmove']['lamp'];keys(move,['movement','speed','type'])
        if move['type']!='position' or not isinstance(move['movement'],list) or len(move['movement'])!=1: raise ValueError('Unreviewed actor movement')
        target=move['movement'][0];keys(target,['x','y'])
        emit('MoveActor',actor='Lamp',vector=(number(target['x']),number(target['y'])),value=number(move['speed'],1,600),text='position')
    if 'actorsturn' in phrase:
        if phrase['actorsturn']!={'ninten':{'x':1,'y':0}}: raise ValueError('Unreviewed actor turn')
        number(phrase['actorsturn']['ninten']['x'],1,1); number(phrase['actorsturn']['ninten']['y'],0,0)
        emit('TurnActor',actor='Ninten',vector=(1,0),duration=.08)
    if 'actorsshake' in phrase:
        keys(phrase['actorsshake'],['lamp']); shake=phrase['actorsshake']['lamp'];keys(shake,['x','length'])
        emit('ShakeActor',actor='Lamp',vector=(number(shake['x'],0,2),0),duration=number(shake['length'],.001,1))
    if 'actorsjump' in phrase:
        jumps=phrase['actorsjump']
        if not isinstance(jumps,dict) or len(jumps)!=1 or next(iter(jumps)) not in ['ninten','lamp']: raise ValueError('Unreviewed jumping actor')
        for name,jump in jumps.items():
            keys(jump,['height','length'])
            emit('JumpActor',actor=name.title(),value=number(jump['height'],1,24),duration=number(jump['length'],.001,.35))
    if 'actorsanim' in phrase:
        keys(phrase['actorsanim'],['lamp']);anim=phrase['actorsanim']['lamp'];keys(anim,['anim'])
        if anim['anim'] not in ['Idle','Open']: raise ValueError('Unreviewed lamp animation')
        emit('AnimateActor',actor='Lamp',value=1,text=anim['anim'])
    if 'actorsemote' in phrase:
        if phrase['actorsemote']!={'ninten':'surprise'}: raise ValueError('Unreviewed actor emote')
        emit('EmoteActor',actor='Ninten',text='surprise')
    if 'shakecam' in phrase:
        cam=phrase['shakecam']; keys(cam,['length','size'])
        if cam['size']!='small': raise ValueError('Unreviewed camera shake size')
        emit('ShakeCamera',vector=(1,0),value=4,duration=number(cam['length'],.001,.2),text='small')
    if 'changecam' in phrase:
        if phrase['changecam']!='lamp': raise ValueError('Unreviewed camera target')
        emit('ChangeCamera',actor='Lamp');emit('YieldIdle')
    if 'movecam' in phrase:
        cam=phrase['movecam'];keys(cam,['x','y','time'])
        # Reviewed source quirk: `time` is not consumed; only `length` is read.
        # Accept exactly the original inert value, not an arbitrary ignored key.
        if type(cam['time']) not in (float,int) or cam['time']!=1: raise ValueError('Unreviewed inert movecam.time')
        emit('MoveCamera',vector=(number(cam['x']),number(cam['y'])),duration=1,text='sine',detail='out')
    if 'startbattle' in phrase:
        if phrase['startbattle']!={'battlers':[{'lamp':'lamp'}],'winflag':'poltergeist'}: raise ValueError('Unreviewed battle specification')
        emit('QueueBattle',actor='Lamp',text='lamp',detail='poltergeist')
    emit('AwaitTimer')
    return out

def compile_receipt(data,review):
    verify_review(review)
    keys(data,['schema','commit','sources','godot','dialogue','log'])
    if data.get('schema')!=1 or data.get('commit')!=COMMIT or data.get('sources')!=review['sources']:
        raise ValueError('Unreviewed parsed dialogue provenance')
    version=data.get('godot',{})
    if any(version.get(k)!=v for k,v in {'major':3,'minor':6,'patch':2,'status':'stable','build':'official','hash':'3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8'}.items()):
        raise ValueError('Unreviewed Godot parser version')
    doc=data.get('dialogue');keys(doc,[str(i) for i in range(13)])
    if hashlib.sha256(canonical(doc)).hexdigest()!=review['parsed_sha256']: raise ValueError('Changed unreviewed parsed dialogue')
    result=[action('BeginCutscene')]
    for i in range(13): result+=compile_phrase(doc[str(i)],i)
    result += [action('StopInteraction',12,'Lamp'),action('SetTalker',12,'None'),action('RestoreActor',12,'Ninten'),action('ReleaseBattleActor',12,'Lamp'),
        action('CutsceneEnded',12),action('DialogueDone',12),action('RequestBattle',12,'Lamp',text='lamp',detail='poltergeist')]
    return result

def main():
    p=argparse.ArgumentParser();p.add_argument('--source',type=Path);p.add_argument('--godot',type=Path);p.add_argument('--receipt',type=Path,default=RECEIPT);p.add_argument('--output',type=Path,default=OUTPUT)
    args=p.parse_args();review=json.loads(REVIEW.read_text())
    if args.source:
        if not args.godot: p.error('--source requires --godot')
        data=parse_native(args.source,args.godot,review)

    else: data=json.loads(args.receipt.read_text())
    compiled={'schema':1,'commit':COMMIT,'commands':compile_receipt(data,review)}
    if args.output.suffix != '.json':raise ValueError('Dialogue output must be external JSON; C++ content generation is disabled')
    if args.source:
        args.receipt.parent.mkdir(parents=True,exist_ok=True);args.receipt.write_text(json.dumps(data,indent=2)+'\n')
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(compiled,indent=2)+'\n')
    print('Compiled reviewed lamp dialogue to',args.output)
if __name__=='__main__':main()
