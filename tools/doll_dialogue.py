#!/usr/bin/env python3
"""Bounded original Doll source adapter and exact native parser/animation receipt.

The source YAML parser and CharacterSprite animation builder run only offline in
official Godot 3.6.2. This does not interpret scripts or YAML in the game runtime.
"""
from __future__ import annotations
import argparse, hashlib, json, os, re, struct, subprocess, tempfile
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
PIN='7d9246600fffe518408f5830d4848635019005a3'
SOURCE='Data/Dialogue/Podunk/cutscenes/doll_attack.yaml'
RECEIPT='reports/doll-sequence/native-parser-animation.json'
ANIMATIONS=['Data/Animations/PartyMember.yaml','Data/Animations/4dir.yaml','Data/Animations/Floater.yaml']
SOURCES=[SOURCE,*ANIMATIONS,'Scripts/global/yaml_parser.gd','Scripts/Main/character_sprite.gd','Scripts/UI/DialogueBox.gd','Scripts/Main/actor.gd','Scripts/Main/npc.gd','Nodes/Ui/emotes.tscn','Nodes/Reusables/actor.tscn','Nodes/Reusables/npc.tscn','Maps/podunk/Nintens House.tscn','Scripts/Main/roomshaker.gd']

def require(ok,msg):
    if not ok:raise ValueError(msg)
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def decode(value):
    if isinstance(value,dict):
        if set(value)=={'native_f64_decimal','native_f64_le_hex'}:
            return struct.unpack('<d',bytes.fromhex(value['native_f64_le_hex']))[0]
        return {k:decode(v)for k,v in value.items()}
    if isinstance(value,list):return [decode(v)for v in value]
    return value

def run_native(root,godot,source=SOURCE,emote_name='exclamation',source_paths=None):
    root=Path(root);upstream=root/'upstream/MOTHER-Encore'
    inventory=json.loads((root/'compatibility/upstream-inventory.json').read_text())
    require(inventory['commit']==PIN,'Doll native source pin')
    sources={p:sha(upstream/p)for p in (SOURCES if source_paths is None else source_paths)}
    require(all(h==inventory['files'][p]['sha256']for p,h in sources.items()),'Doll changed source')
    with tempfile.TemporaryDirectory(prefix='encore-doll-native-')as td:
        work=Path(td)
        (work/'project.godot').write_text('config_version=4\n[application]\nconfig/name="Doll original parser and animation reference"\n[logging]\nfile_logging/enable_logging=false\n')
        (work/'yaml_parser.gd').write_bytes((upstream/'Scripts/global/yaml_parser.gd').read_bytes())
        for i,p in enumerate([source,*ANIMATIONS]):(work/(str(i)+'.yaml')).write_bytes((upstream/p).read_bytes())
        code=(upstream/'Scripts/Main/character_sprite.gd').read_text()
        mechanism=code[code.index('func _create_animations('):code.index('# Sets the sprite texture')]
        (work/'native_builder.gd').write_text('extends Sprite\nvar _json_data\nvar _anim_player=AnimationPlayer.new()\nvar _sprite_frame_path=NodePath(".:frame")\nvar _directional_tags=[]\nvar dir=0\n'+mechanism+'\nfunc _create_tree(_tags, _single, _connections):\n    pass\n')
        emotes=(upstream/'Nodes/Ui/emotes.tscn').read_text()
        for name in [emote_name]:
            match=re.search(r'^\[sub_resource type="Animation" id=\d+\]\nresource_name = "'+name+r'"\n(.*?)(?=^\[|\Z)',emotes,re.M|re.S)
            require(match is not None,'Missing native emote '+name)
            (work/(name+'.tres')).write_text('[gd_resource type="Animation" format=2]\n\n[resource]\nresource_name = "'+name+'"\n'+match[1])
        (work/'probe.gd').write_text('''extends SceneTree
func precise(value):
    if typeof(value)==TYPE_REAL:
        var bits=StreamPeerBuffer.new()
        bits.put_double(value)
        return {"native_f64_decimal":"%.17f"%value,"native_f64_le_hex":bits.data_array.hex_encode()}
    if value is Array:
        var out=[]
        for x in value:out.append(precise(x))
        return out
    if value is Dictionary:
        var out={}
        for k in value:out[k]=precise(value[k])
        return out
    return value
func clip(anim):
    var keys=[]
    for i in anim.track_get_key_count(0):keys.append([anim.track_get_key_time(0,i),anim.track_get_key_value(0,i)])
    return {"length":anim.length,"loop":anim.loop,"keys":keys}
func _init():
    var parser=load("res://yaml_parser.gd")
    var yaml=[]
    var animations=[]
    for i in range(4):yaml.append(parser.parse_file("res://%d.yaml"%i))
    for i in range(1,4):
        var builder=load("res://native_builder.gd").new()
        builder._json_data=yaml[i]
        builder._create_animations()
        var clips={}
        for name in builder._anim_player.get_animation_list():clips[name]=clip(builder._anim_player.get_animation(name))
        animations.append(clips)
        builder._anim_player.free()
        builder.free()
    var emote=load("res://exclamation.tres")
    var file=File.new()
    file.open("res://result.json",File.WRITE)
    file.store_string(JSON.print(precise({"godot":Engine.get_version_info(),"yaml":yaml,"animations":animations,"exclamation":clip(emote)})))
    file.close()
    quit()
'''.replace('exclamation',emote_name))
        env=dict(os.environ,XDG_DATA_HOME=str(work/'data'),XDG_CONFIG_HOME=str(work/'config'),XDG_CACHE_HOME=str(work/'cache'))
        p=subprocess.run([str(godot),'--path',str(work),'--script','probe.gd'],capture_output=True,text=True,timeout=60,env=env)
        require(p.returncode==0 and 'SCRIPT ERROR'not in p.stdout+p.stderr and 'ERROR:'not in p.stdout+p.stderr,'Doll native parser failed: '+p.stdout+p.stderr)
        result=json.loads((work/'result.json').read_text());result.update(schema=1,commit=PIN,sources=sources,log=p.stdout+p.stderr)
        return result

def receipt(ex):
    data=ex.document(RECEIPT)
    review=ex.document('reports/doll-sequence/source-review.json')
    require(review['schema']==1 and review['commit']==PIN and review['whole_handler_approved']is False and review['sources']==data['sources'],'Unreviewed Doll source scope')
    require(sha(ex.root/RECEIPT)==review['native_receipt_sha256'],'Changed unreviewed native Doll receipt')
    require(data['schema']==1 and data['commit']==PIN and set(data['sources'])==set(SOURCES),'Unreviewed Doll parser receipt')
    for p,h in data['sources'].items():ex.source(p,h)
    require(all(data['godot'].get(k)==v for k,v in {'major':3,'minor':6,'patch':2,'status':'stable','build':'official','hash':'3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8'}.items()),'Doll parser version')
    data=decode(data)
    import yaml
    for path,parsed in zip([SOURCE,*ANIMATIONS],data['yaml']):require(yaml.safe_load(ex.text(path))==parsed,'Doll native/Python parser mismatch: '+path)
    return data

def compile_dialogue(doc,root=ROOT):
    from tools import programme_lowering_recipe as recipe
    return recipe.execute('doll',doc,root=root)


def main():
    parser=argparse.ArgumentParser();parser.add_argument('--godot',type=Path,required=True);a=parser.parse_args()
    data=run_native(ROOT,a.godot);out=ROOT/RECEIPT;out.parent.mkdir(parents=True,exist_ok=True);out.write_text(json.dumps(data,indent=2)+'\n')
    print('Recorded original Doll parser, original CharacterSprite builder, native emote Animation:',out)
if __name__=='__main__':main()
