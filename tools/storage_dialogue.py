"""Reviewed Minnie dialogue graph; callback suspension uses the Room scheduler."""
from __future__ import annotations
import argparse, copy, csv, hashlib, io, json, re, sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from tools.extract_battle_entry import Extractor, PIN, node, require
from tools.drawer_program import canonical, read_json
from tools.doll_postwin import return_duration
ROOT=Path(__file__).resolve().parents[1]
IR='content/native-storage-dialogue.json'
SOURCE='Data/Dialogue/Podunk/minnie_storage.yaml'
IDENTITY='Podunk/minnie_storage'
NONE=0xffffffff

def build(root=ROOT):
    ex=Extractor(root); doc=ex.yaml(SOURCE)
    expected={'0':{'name':'DIALOGUE_PODUNK_MINNIE_STORAGE_SPEAKER_Minnie','sound':'Kid','text':'DIALOGUE_PODUNK_MINNIE_STORAGE_0','options':{'DIALOGUE_PODUNK_MINNIE_STORAGE_0-OPT_0':'1','DIALOGUE_PODUNK_MINNIE_STORAGE_0-OPT_1':'3','cancel':'3'}},'1':{'text':'','showbox':False,'open_storage':True,'caninput':False,'goto':'3','cleardialog':True},'2':{'name':'DIALOGUE_PODUNK_MINNIE_STORAGE_SPEAKER_Minnie','sound':'Kid','text':'DIALOGUE_PODUNK_MINNIE_STORAGE_2'},'3':{'name':'DIALOGUE_PODUNK_MINNIE_STORAGE_SPEAKER_Minnie','sound':'Kid','text':'DIALOGUE_PODUNK_MINNIE_STORAGE_3'}}
    require(canonical(doc)==canonical(expected) and list(doc)==list(expected),'Unreviewed Minnie storage graph')
    scene=ex.text('Maps/podunk/Nintens House.tscn'); npc=node(scene,'Objects/npc3')
    require(npc['dialog']=='Podunk/minnie_run_tutorial'
            and npc['_all_dialog']==[['mimmie_door_opened','Podunk/minnie_door_open'],['doll_melody',IDENTITY]]
            and npc['sprite']=='Npcs/4dir/minnie'
            and npc.get('staring') is True,
            'Unreviewed Minnie ordered dialogue override/identity')
    # The map instance inherits its talker, facing and script defaults from
    # this exact original scene. Admit those bytes independently; membership
    # in an override array is insufficient to prove effective precedence.
    inherited='Nodes/Reusables/npc.tscn'
    require(re.findall(r'^\[ext_resource path="res://([^"\n]+)" type="PackedScene" id=11\]$',scene,re.M)==[inherited],
            'Unreviewed Minnie inherited NPC scene')
    base=ex.text(inherited)
    require(node(base,'.')['script']=={'ExtResource':2}
            and re.findall(r'^\[ext_resource path="res://([^"\n]+)" type="Script" id=2\]$',base,re.M)==['Scripts/Main/npc.gd']
            and node(base,'.')['player_turn']=={'x':True,'y':True},
            'Unreviewed Minnie inherited NPC script/facing')
    dialogue=ex.text('Scripts/UI/DialogueBox.gd'); abstract=ex.text('Scripts/UI/AbstractDialogueBox.gd')
    require('open_storage' in dialogue and 'uiManager.open_storage' in dialogue and '_finish_phrase' in abstract,'Storage callback source admission')
    ex.text('Scripts/global/uiManager.gd'); npc_script=ex.text('Scripts/Main/npc.gd')
    require('_all_dialog.push_front(["", dialog])' in npc_script
            and 'for i in dialog_array.size():' in npc_script
            and 'ret = cur_dialog' in npc_script
            and 'global.talker = self' in npc_script
            and 'uiManager.open_dialogue_box(_get_right_dialog(false, true), null, self)' in npc_script
            and 'export var turn_to_player_on_interact := true' in npc_script
            and 'turn_to_player_on_interact' not in npc,
            'Unreviewed Minnie inherited talker/last-matching override semantics')
    table={r['key']:r for r in csv.DictReader(io.StringIO(ex.text('Translations/TranslatedText/dialogue_Podunk - sheet.csv')))}
    texts=[]
    for label,p in doc.items():
        if p['text']:
            # TranslationServer returns the untranslated key for dormant phrase2,
            # which has no CSV record in the pinned original. Preserve it.
            raw=table.get(p['text'],{'en':p['text']})['en']; parts=[[dict(kind=1,text=raw[3:] if raw.startswith('[@]') else raw)]]
            require('[' not in parts[0][0]['text'] and ']' not in parts[0][0]['text'],'Unreviewed Minnie text control')
            texts.append(dict(label=label,id=67+len(texts),key=p['text'],speaker=table[p['name']]['en'],voice='Audio/Sound effects/text/Kid.mp3',raw=raw,parts=parts))
    ex.data(texts[0]['voice'])
    commands=[];labels={}
    def emit(kind,label,**kw):commands.append(dict(kind=kind,phrase=list(doc).index(label),label=label,**kw))
    emit('BeginCutscene','0')
    for label,p in doc.items():
        labels[label]=len(commands)
        if p['text']:emit('ShowDialogue',label,text_id=next(t['id'] for t in texts if t['label']==label),flags=1)
        if 'open_storage' in p:
            emit('HideDialogue',label,flags=1);emit('OpenStorage',label);emit('AwaitSubmenu',label)
        elif 'options' in p:emit('AwaitChoices',label,group=IDENTITY+'::0')
        else:emit('AwaitDialogue',label)
        if 'goto' in p:emit('Jump',label,target_label=p['goto'])
        elif 'options' not in p:
            emit('StopInteraction',label,flags=1);emit('SetTalker',label);emit('CutsceneEnded',label);emit('DialogueDone',label,duration=return_duration(dialogue))
    for c in commands:
        if 'target_label' in c:c['target_pc']=labels[c['target_label']]
    options=doc['0']['options']
    group=dict(id=IDENTITY+'::0',program_identity=IDENTITY,source_label='0',program_command_count=len(commands),initial_selection=0,
               options=[dict(translation_key=k,text=table[k]['en'],target_label=v,target_pc=labels[v])for k,v in options.items()if k!='cancel'],
               cancel_target_label=options['cancel'],cancel_target_pc=labels[options['cancel']])
    return dict(schema=1,kind='encore.storage-dialogue.source-ir',commit=PIN,sources=dict(sorted(ex.sources.items())),source=SOURCE,identity=IDENTITY,npc_source='Objects/npc3',document=doc,texts=texts,commands=commands,labels=labels,choice_group=group)

def load(root=ROOT):
    d=read_json(Path(root)/IR);require(canonical(d)==canonical(build(root)),'Stale/unreviewed storage dialogue IR');return d

def append_room(ex):
    from tools.native_content import OPCODES
    d=load(ex.root);s=ex.sections
    require(len(s['Program'])==18,'Storage stable program prefix changed')
    for p,h in d['sources'].items():ex.source(p,h)
    ex.file(IR);ex.file('tools/storage_dialogue.py')
    choices=json.loads((Path(ex.root)/'content/native-dialogue-choices.json').read_text());groups={g['id']:i for i,g in enumerate(choices['groups'])}
    require(d['choice_group']['id'] in groups,'Storage choices not generated')
    first=len(s['Command'])
    for c in d['commands']:
        target=c.get('text_id',c.get('target_pc',NONE))
        if c['kind']=='AwaitChoices':target=groups[c['group']]
        s['Command'].append(dict(opcode=OPCODES.index(c['kind']),actor_index=65535,phrase=c['phrase'],target_index=target,flags=c.get('flags',0),vector=[0,0],value=0,duration=c.get('duration',0),auxiliary_index=NONE))
    s['Program'].append(dict(stable_id=19,first_command=first,command_count=len(d['commands']),phrase_count=len(d['document']),source_path_string=ex.string(d['identity'])))
    ex.record_map('Storage dialogue',IR,'Original choice, hidden submenu callback, dormant phrase2 and goodbye3','Append program19, text67..69; no RNG or gameplay rules change')

def append_house(ex,house,room):
    d=load(ex.root);out=copy.deepcopy(house)
    require(max(t['id']for t in out['dialogues'])==66,'Storage text prefix changed')
    for p,h in d['sources'].items():ex.data(p);require(ex.sources[p]==h,'Storage House source fingerprint')
    for text in d['texts']:
        first=len(out['segments'])
        for i,tokens in enumerate(text['parts']):
            out['segments'].append(dict(id=len(out['segments'])+1,speaker=text['speaker'],voice=text['voice'],tokens=tokens,flags=3 if i+1<len(text['parts']) else 5))
        out['dialogues'].append(dict(id=text['id'],source_path=d['source'],first_segment=first,segment_count=len(text['parts'])))
    # Room programme resolution owns the effective override; a multi-phrase
    # effect programme must never masquerade as a literal dialogue index.
    require(any(room['strings'][p['source_path_string']]==d['identity']for p in room['sections']['Program']),'Storage programme absent')
    out['sources']=dict(sorted(ex.sources.items()));out['scope']+='; original Minnie storage choice and callback dialogue'
    return out

def main():
    p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','verify']);a=p.parse_args()
    if a.action=='extract':
        path=ROOT/IR;path.write_bytes((json.dumps(build(),ensure_ascii=False,indent=2)+'\n').encode('utf8'))
    else:load()
if __name__=='__main__':main()
