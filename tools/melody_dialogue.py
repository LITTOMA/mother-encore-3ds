#!/usr/bin/env python3
"""Bounded source frontend for the first Doll melody and its room interactions.

Official Godot 3 parses the five original YAML files offline. Runtime content is
typed external data, with source labels recorded separately from dense phases.
"""
from __future__ import annotations
import argparse, json, os, subprocess, sys, tempfile
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.doll_dialogue import ROOT, PIN, decode, sha, require
from tools.doll_postwin import return_duration

MELODY='Data/Dialogue/Podunk/dollmelody.yaml'
GUARD='Data/Dialogue/Podunk/cutscenes/mimmie_ignore.yaml'
DEFAULT='Data/Dialogue/Podunk/mimmie_doll_defeated.yaml'
AFTER='Data/Dialogue/Podunk/mimmie_dead_doll.yaml'
REPEAT='Data/Dialogue/Podunk/doll.yaml'
YAMLS=[MELODY,GUARD,DEFAULT,AFTER,REPEAT]
AUDIO=['Audio/Music/Melodies/melody1.mp3','Audio/Sound effects/M3/heal_se.wav','Audio/Music/House.mp3']
RECEIPT='reports/doll-melody/native-parser.json'
REVIEW='reports/doll-melody/source-review.json'
SOURCES=[*YAMLS,'Scripts/global/yaml_parser.gd','Scripts/UI/DialogueBox.gd',
    'Scripts/UI/AbstractDialogueBox.gd','Scripts/Main/actor.gd','Scripts/Main/npc.gd',
    'Scripts/Main/character_sprite.gd','Scripts/Main/CutsceneArea.gd','Scripts/Main/Door.gd',
    'Scripts/Main/Flag Landmarks.gd','Scripts/global/globalData.gd','Scripts/global/text_tools.gd',
    'Scripts/global/audioManager.gd','Scripts/global/uiManager.gd','Scripts/global/global.gd','Data/save_new_game.yaml',
    'Maps/podunk/Nintens House.tscn','Nodes/Reusables/CutsceneArea.tscn',
    'Nodes/Reusables/npc.tscn','Nodes/Ui/DialogueBox.tscn','Nodes/Overworld/MusicChanger.tscn',
    'Nodes/Ui/effects/melodyBG.gd','Nodes/Ui/effects/melodyBG.tscn',
    'Translations/TranslatedText/dialogue_Podunk - sheet.csv',
    'Translations/TranslatedText/dialogue_Podunk_cutscenes - sheet.csv',
    *[p for audio in AUDIO for p in (audio,audio+'.import')]]

def run_native(root,godot):
    root=Path(root);upstream=root/'upstream/MOTHER-Encore'
    inventory=json.loads((root/'compatibility/upstream-inventory.json').read_text())
    sources={p:sha(upstream/p)for p in SOURCES}
    require(inventory['commit']==PIN and all(inventory['files'][p]['sha256']==h for p,h in sources.items()),'Changed melody source')
    with tempfile.TemporaryDirectory(prefix='encore-melody-native-')as td:
        work=Path(td)
        (work/'project.godot').write_text('config_version=4\n[application]\nconfig/name="Doll melody original YAML parser"\n[logging]\nfile_logging/enable_logging=false\n')
        (work/'yaml_parser.gd').write_bytes((upstream/'Scripts/global/yaml_parser.gd').read_bytes())
        for i,path in enumerate(YAMLS):(work/(str(i)+'.yaml')).write_bytes((upstream/path).read_bytes())
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
func _init():
    var parser=load("res://yaml_parser.gd")
    var yaml=[]
    for i in range(5):yaml.append(parser.parse_file("res://%d.yaml"%i))
    var file=File.new()
    file.open("res://result.json",File.WRITE)
    file.store_string(JSON.print(precise({"godot":Engine.get_version_info(),"yaml":yaml})))
    file.close()
    quit()
''')
        env=dict(os.environ,XDG_DATA_HOME=str(work/'data'),XDG_CONFIG_HOME=str(work/'config'),XDG_CACHE_HOME=str(work/'cache'))
        result=subprocess.run([str(godot),'--path',str(work),'--script','probe.gd'],capture_output=True,text=True,timeout=60,env=env)
        require(result.returncode==0 and 'SCRIPT ERROR'not in result.stdout+result.stderr and 'ERROR:'not in result.stdout+result.stderr,'Melody native parser failed: '+result.stdout+result.stderr)
        data=json.loads((work/'result.json').read_text())
        data.update(schema=1,commit=PIN,sources=sources,log=result.stdout+result.stderr)
        return data

def receipt(ex):
    data=ex.document(RECEIPT);review=ex.document(REVIEW)
    require(review['schema']==1 and review['commit']==PIN and review['whole_handler_approved']is False and review['sources']==data['sources'],'Unreviewed melody scope')
    require(sha(ex.root/RECEIPT)==review['native_receipt_sha256'],'Changed unreviewed melody native receipt')
    require(data['schema']==1 and data['commit']==PIN and set(data['sources'])==set(SOURCES),'Unreviewed melody parser receipt')
    for path,digest in data['sources'].items():ex.source(path,digest)
    require(all(data['godot'].get(k)==v for k,v in {'major':3,'minor':6,'patch':2,'status':'stable','build':'official','hash':'3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8'}.items()),'Melody parser version')
    data=decode(data)
    import yaml
    require(len(data['yaml'])==len(YAMLS),'Melody parser document count')
    for path,parsed in zip(YAMLS,data['yaml']):require(yaml.safe_load(ex.text(path))==parsed,'Melody native/Python parser mismatch: '+path)
    for path,doc in zip(YAMLS[2:],data['yaml'][2:]):literal_phrase(path,doc)
    return data

def literal_phrase(path,doc,root=ROOT):
    from tools import programme_lowering_recipe as recipe
    value=recipe.load(root)
    recipe.document(value,path,doc)
    require(list(doc)==['0'],'Literal melody-room graph')
    return doc['0']


def compile_melody(doc,end_duration,root=ROOT):
    from tools import programme_lowering_recipe as recipe
    return recipe.execute('melody',doc,end_duration,root)


def compile_guard(doc,end_duration,root=ROOT):
    from tools import programme_lowering_recipe as recipe
    return recipe.execute('guard',doc,end_duration,root)


def main():
    p=argparse.ArgumentParser();p.add_argument('--godot',type=Path,required=True);a=p.parse_args()
    data=run_native(ROOT,a.godot);out=ROOT/RECEIPT;out.parent.mkdir(parents=True,exist_ok=True);out.write_text(json.dumps(data,indent=2)+'\n')
    review=dict(schema=1,commit=PIN,whole_handler_approved=False,sources=data['sources'],native_receipt_sha256=sha(out),
        scope='Three-phrase Doll melody, original Mimmie default/repeat and Doll repeat interactions, two-phrase Mimmie exit guard; no Pillow, Minnie or telephone execution',
        engine_contract=dict(room_rules=4,room_capabilities=4,house_schema=4,house_npc_stride=104,house_override_stride=20,npc_interaction_return_parameter=19,
            source_phase_labels={'Podunk/dollmelody':['0','3','4'],'Podunk/cutscenes/mimmie_ignore':['0','1']}),
        order=['text begins','actor creation/ready/persistent and idle','wait/input policy','queue no-argument object functions','synchronous music','voice selection','synchronous soundeffect','talker','actor moves','camera plus idle','synchronous flags','dialogue or timer gate','stop original talker','restore only bound actors','cutscene ended and done','source camera return'],
        constraints=['Melody uses inherited original Doll NPC; only leader Actor exists','Silent melody/repeat text consumes no voice RNG','Root MusicArea stop targets House ownership and must not stop newly-started melody','Final wait gates input concurrently with printing; no auto-close','PartyLead resolves to configured Ninten player name in this reviewed singleton-party slice','No minnie_leave mutation or init-only NPC event-position replay'],
        not_verified_here=['whole game startup','general MusicChanger multiplexing','effect visual parity','emulator/hardware','audio audibility'])
    (ROOT/REVIEW).write_text(json.dumps(review,indent=2)+'\n');print('Recorded original five-file melody/room YAML parser:',out)

if __name__=='__main__':main()
