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

def compile_dialogue(doc,end_duration,root=ROOT):
    from tools import programme_lowering_recipe as recipe
    return recipe.execute('postwin',doc,end_duration,root)


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
