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

def compile_phrase(phrase,index,root=ROOT):
    from tools import programme_lowering_recipe as recipe
    return recipe.lamp_phrase(phrase,index,root)


def compile_receipt(data,review,root=ROOT):
    verify_review(review)
    keys(data,['schema','commit','sources','godot','dialogue','log'])
    if data.get('schema')!=1 or data.get('commit')!=COMMIT or data.get('sources')!=review['sources']:
        raise ValueError('Unreviewed parsed dialogue provenance')
    version=data.get('godot',{})
    if any(version.get(k)!=v for k,v in {'major':3,'minor':6,'patch':2,'status':'stable','build':'official','hash':'3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8'}.items()):
        raise ValueError('Unreviewed Godot parser version')
    doc=data.get('dialogue');keys(doc,[str(i) for i in range(13)])
    if hashlib.sha256(canonical(doc)).hexdigest()!=review['parsed_sha256']: raise ValueError('Changed unreviewed parsed dialogue')
    from tools import programme_lowering_recipe as recipe
    return recipe.execute('lamp',doc,root=root)

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
