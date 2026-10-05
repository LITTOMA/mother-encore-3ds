#!/usr/bin/env python3
"""Checked append-only adapter for the two post-phone family conversations."""
from __future__ import annotations
import argparse, copy, csv, hashlib, io, json, re
from pathlib import Path
from tools.extract_battle_entry import ROOT, PIN, Extractor, node, require
from tools.doll_postwin import return_duration
from tools.phone_dialogue import text_segments

IR='content/family-followup-bindings.json'
REVIEW='reports/family-followup/source-review.json'
SCENE='Maps/podunk/Nintens House.tscn'
TABLE='Translations/TranslatedText/dialogue_Podunk - sheet.csv'
NONE=0xffffffff

def digest(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def build(root=ROOT):
    ex=Extractor(root)
    rows=[]
    for person,source,flag,last,voice,start in [
            ('carol','Objects/npc','carol_ask_key','4','Female',48),
            ('mimmie','Objects/npc2','mimmie_ask_key','7','Kid',51)]:
        speaker='DIALOGUE_PODUNK_'+person.upper()+'_KEY_SPEAKER_'+person.capitalize()
        key='DIALOGUE_PODUNK_'+person.upper()+'_KEY_'
        path='Data/Dialogue/Podunk/'+person+'_key.yaml'
        repeat='Data/Dialogue/Podunk/'+person+'_key_again.yaml'
        expected={'0':dict(name=speaker,sound=voice,text=key+'0',goto=last),
                  last:dict(name=speaker,sound=voice,text=key+last,setflags=flag)}
        again={'0':dict(name=speaker.replace('_KEY_SPEAKER_', '_KEY_AGAIN_SPEAKER_'),sound=voice,text=key+'AGAIN_0')}
        require(ex.yaml(path)==expected and ex.yaml(repeat)==again,
                'Unreviewed family phrase fields/graph: '+person)
        original=node(ex.text(SCENE),source)['_all_dialog']
        identity='Podunk/'+person+'_key'
        selected=[x for x in original if x[0] in ('talked_to_dad',flag)]
        require(selected==[['talked_to_dad',identity],[flag,identity+'_again']],
                'Family source override order changed')
        rows.append(dict(npc=source,flag=flag,path=path,repeat_path=repeat,
                         dialogue_first_id=start,program_id=17+len(rows),
                         document=expected,repeat_document=again,
                         overrides=original))
    dialogue=ex.text('Scripts/UI/DialogueBox.gd')
    require(dialogue.index('if _curr_phrase.has("text"):') < dialogue.index('if _curr_phrase.has("setflags"):')
            and '_change_flags(_curr_phrase["setflags"], true)' in dialogue
            and 'globaldata.set_flag(flag, value)' in dialogue,
            'Family source text/flag order changed')
    end=return_duration(dialogue)
    ex.text('Scripts/Main/npc.gd'); ex.text('Scripts/global/globalData.gd')
    hint=ex.text('Scripts/global/text_tools.gd')
    color=re.search(r'^const DIALOG_HINT_COLOR := "([0-9a-f]{6})"',hint,re.M)
    require(color is not None,'Family hint color source')
    table={r['key']:r['en'] for r in csv.DictReader(io.StringIO(ex.text(TABLE)))}
    texts=[]
    for row in rows:
        for path,doc in [(row['path'],row['document']),(row['repeat_path'],row['repeat_document'])]:
            for label,p in doc.items():
                voice='Audio/Sound effects/text/'+p['sound']+'.mp3'; ex.data(voice)
                texts.append(dict(id=row['dialogue_first_id']+(len(texts)%3),source_path=path,label=label,
                                  speaker=table[p['name']],voice=voice,
                                  segments=text_segments(table[p['text']],player_token='[Ninten]')))
    return dict(schema=1,kind='encore.family-followup.source-ir',commit=PIN,
                scope='Two post-Dad key requests and repeats only; no storage/item/Podunk continuation',
                sources=dict(sorted(ex.sources.items())),families=rows,texts=texts,
                hint_color=color[1],end_duration=end)

def load(root=ROOT):
    root=Path(root); data=json.loads((root/IR).read_text(encoding='utf-8'))
    require(data==build(root),'Stale/unreviewed family followup IR')
    review=json.loads((root/REVIEW).read_text(encoding='utf-8'))
    require(review['schema']==1 and review['commit']==PIN and review['sources']==data['sources']
            and review['ir_sha256']==digest(root/IR) and review['whole_handler_approved'] is False,
            'Unreviewed family followup source scope')
    return data

def adopt_sources(ex,data):
    for path,sha in data['sources'].items():
        if path.startswith('Audio/'):
            ex.data(path) if hasattr(ex,'data') else ex.source(path)
        else: ex.text(path)
        require(ex.sources.get(path,ex.sources.get('upstream/MOTHER-Encore/'+path))==sha,
                'Family linking source mismatch: '+path)

def append_room(ex,clip_names=None,add_clip=None):
    from tools.native_content import OPCODES
    data=load(ex.root); adopt_sources(ex,data)
    s=ex.sections
    require(len(s['Program'])==16,'Family program identity prefix changed')
    flags={ex.strings[f['name_string']]:i for i,f in enumerate(s['Flag'])}
    require(all(r['flag'] in flags for r in data['families']),'Family flag missing')
    pending=[]; programs=[]
    for row in data['families']:
        commands=[]
        def emit(kind,phrase,target=NONE,flags=0,value=0,duration=0):
            commands.append(dict(opcode=OPCODES.index(kind),actor_index=65535,phrase=phrase,
                                 target_index=target,flags=flags,vector=[0,0],value=value,
                                 duration=duration,auxiliary_index=NONE))
        emit('BeginCutscene',0)
        for phrase,label in enumerate(row['document']):
            emit('ShowDialogue',phrase,row['dialogue_first_id']+phrase,flags=1)
            if 'setflags' in row['document'][label]:
                emit('SetFlag',phrase,flags[row['flag']],value=1)
            emit('AwaitDialogue',phrase)
        phrase=len(row['document'])-1
        emit('StopInteraction',phrase,flags=1); emit('SetTalker',phrase)
        emit('CutsceneEnded',phrase); emit('DialogueDone',phrase,duration=data['end_duration'])
        programs.append(dict(stable_id=row['program_id'],first_command=len(s['Command'])+len(pending),
                             command_count=len(commands),phrase_count=len(row['document']),
                             source_path_string=ex.string(row['path'].removeprefix('Data/Dialogue/').removesuffix('.yaml'))))
        pending.extend(commands)
    s['Command'].extend(pending); s['Program'].extend(programs)
    ex.file(IR); ex.file('tools/link_family_followup.py')
    ex.record_map('Family post-phone programs',IR,
                  'Carol 0→4 / Mimmie 0→7; ShowDialogue→SetFlag→AwaitDialogue',
                  'Append programs17/18 and text48..53; retain all flag identities/defaults; normal NPC end protocol')

def append_house(ex,house,room):
    data=load(ex.root); adopt_sources(ex,data)
    require(max(d['id'] for d in house['dialogues'])==47,'Family dialogue identity prefix changed')
    out=copy.deepcopy(house); kinds={'Literal':1,'PlayerName':2,'HintStart':3,'HintEnd':4}
    for text in data['texts']:
        first=len(out['segments'])
        for i,segment in enumerate(text['segments']):
            tokens=[dict(kind=kinds[t['kind']],text=t.get('text',data['hint_color'] if t['kind']=='HintStart' else '')) for t in segment['tokens']]
            out['segments'].append(dict(id=len(out['segments'])+1,speaker=text['speaker'],voice=text['voice'],
                                        tokens=tokens,flags=3 if i+1<len(text['segments']) else 5))
        out['dialogues'].append(dict(id=text['id'],source_path=text['source_path'],first_segment=first,
                                    segment_count=len(text['segments'])))
    programs={room['strings'][p['source_path_string']]:p['stable_id'] for p in room['sections']['Program']}
    for row in data['families']:
        identity=row['path'].removeprefix('Data/Dialogue/').removesuffix('.yaml')
        require(programs.get(identity)==row['program_id'],'Missing/mismatched family source program')
        npc=next(i for i,n in enumerate(out['npcs']) if n['source_path']==row['npc'])
        for flag,path,index in [('talked_to_dad',identity,row['dialogue_first_id']),
                                (row['flag'],identity+'_again',row['dialogue_first_id']+2)]:
            matches=[o for o in out['overrides'] if o['npc']==npc and o['flag']==flag and o['dialogue']==path]
            require(len(matches)==1,'Family override binding missing/ambiguous')
            matches[0]['dialogue_index']=next(i for i,d in enumerate(out['dialogues']) if d['id']==index)
    out['sources']=dict(sorted(ex.sources.items())); out['scope']+='; checked post-phone family key requests and repeats'
    return out

def main():
    p=argparse.ArgumentParser(); p.add_argument('action',choices=['extract','verify']); a=p.parse_args()
    if a.action=='verify': load(); print('Family followup: source/IR admitted'); return
    data=build(); (ROOT/IR).write_bytes((json.dumps(data,indent=2,ensure_ascii=False)+'\n').encode('utf-8'))
    review=dict(schema=1,commit=PIN,whole_handler_approved=False,sources=data['sources'],ir_sha256=digest(ROOT/IR),
                scope=data['scope'],semantics={'phrase_order':'Source text starts before setflags; flag is set before the input gate',
                'override_order':'Last matching NPC override wins; source order retained',
                'save_identity':'Existing flag stable IDs and seen keys retained; session mutable flags derive from SetFlag',
                'termination':'Existing StopInteraction/SetTalker/CutsceneEnded/DialogueDone with source camera return duration'},
                unsupported=['Minnie storage choices','inventory/item giving','later family programs'],
                unverified=['manual test suites','emulator','hardware','audio audibility'])
    (ROOT/REVIEW).parent.mkdir(parents=True,exist_ok=True)
    (ROOT/REVIEW).write_bytes((json.dumps(review,indent=2)+'\n').encode('utf-8'))
    print('Family followup source IR extracted')
if __name__=='__main__': main()
