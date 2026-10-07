#!/usr/bin/env python3
"""Original House inspection YAML appended to the SAME checked Room VM.

No new interpreter, copied text fragment, invented programme index, or native
toolchain is involved. Existing Room and House prefixes remain immutable.
"""
from __future__ import annotations
import argparse, copy, csv, hashlib, io, json, struct, sys
from pathlib import Path
import yaml
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from tools.extract_battle_entry import PIN, require
from tools.drawer_program import validate_document, text_segments
from tools.doll_postwin import return_duration
from tools import native_content as room
IR='content/native-house-return-inspection-programme.json'
ROOM_IR='content/native-house-return-room-programmes.json'
REVIEW='reports/house-return-inspection-programme/source-review.json'
PACK='romfs/data/house-return-room.encroom'
MANIFEST='reports/house-return-inspection-programme/pack-manifest.json'
BASE='content/native-opening.json'
HOUSE='content/native-house.json'
DRAWER='content/native-drawer-program.json'
INTERACT='content/native-house-return-interact-dialog.json'
TOOL='tools/house_return_inspection_programme.py'
NONE=room.NONE

def digest(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def read(path):return json.loads(Path(path).read_text(encoding='utf8'))
def write(path,value):
    path=Path(path);path.parent.mkdir(parents=True,exist_ok=True)
    path.write_bytes((json.dumps(value,ensure_ascii=False,indent=2)+'\n').encode('utf8'))

class Sources:
    """Inventory-checked pinned reads; deliberately does not run Git."""
    def __init__(self,root):
        self.root=Path(root);self.inventory=read(self.root/'compatibility/upstream-inventory.json')
        require(read(self.root/'upstream.lock')['commit']==self.inventory['commit']==PIN,'Source pin mismatch')
        self.sources={}
    def data(self,path):
        require(path in self.inventory['files'],'Uninventoried inspection source '+path)
        raw=(self.root/'upstream/MOTHER-Encore'/path).read_bytes();sha=hashlib.sha256(raw).hexdigest()
        require(sha==self.inventory['files'][path]['sha256'],'Changed inspection source '+path)
        self.sources[path]=sha;return raw
    def text(self,path):return self.data(path).decode('utf8')
    def yaml(self,path):return yaml.safe_load(self.text(path))

def source_ir(root=ROOT):
    root=Path(root);ex=Sources(root);base=read(root/BASE);house=read(root/HOUSE)
    drawer=read(root/DRAWER);interact=read(root/INTERACT)
    require(base['upstream_commit']==house['commit']==drawer['commit']==interact['commit']==PIN,'Inspection resource pin')
    # Select every actual default/override from the complete source instances.
    rows=interact['records']
    paths=sorted({p for r in rows for p in [r['dialogue'],*[x[1] for x in r['choices']]] if p})
    require(len(rows)==8 and len(paths)==9,'Unreviewed complete House inspection scope')
    dialogue=ex.text('Scripts/UI/DialogueBox.gd');abstract=ex.text('Scripts/UI/AbstractDialogueBox.gd')
    ui=ex.text('Scripts/global/uiManager.gd');script=ex.text(interact['script'])
    require('uiManager.open_dialogue_box(_get_right_dialog())' in script and
            'func open_dialogue_box(' in ui,'Inspection actual nullable source open')
    itempos=dialogue.index('if _curr_phrase.has("item"):');textpos=dialogue.index('if _curr_phrase.has("text"):')
    soundpos=dialogue.index('if _curr_phrase.has("soundeffect"):');flagpos=dialogue.index('if _curr_phrase.has("setflags"):')
    require(itempos<textpos<soundpos<flagpos and
            'global.item = Inventory.add_item_available(_curr_phrase["item"])' in dialogue,
            'Inspection source effect order changed')
    body=dialogue[dialogue.index('func _next_phrase('):dialogue.index('func _handle_gotos(')]
    require('Inventory.has_inventory_space() != curr_if["invspace"]' in body and
            'for curr_if in all_ifs:' in body and
            '_handle_gotos(curr_if, with_sound)\n\t\t\t\treturn' in body,
            'Inspection post-acknowledgement first-match conditional order changed')
    # Reuse the actual reviewed Drawer effect schema, not its second scheduler.
    for path,sha in drawer['sources'].items():require(hashlib.sha256(ex.data(path)).hexdigest()==sha,'Stale Drawer source '+path)
    for path,sha in interact['sources'].items():require(hashlib.sha256(ex.data(path)).hexdigest()==sha,'Stale Interact source '+path)
    programs=[]
    for identity in paths:
        source='Data/Dialogue/'+identity+'.yaml';doc=ex.yaml(source)
        isdrawer=source==drawer['source_path']
        if isdrawer:validate_document(doc)
        else:require(len(doc)==1 and list(doc)==['0'] and set(doc['0'])=={'text'},'Unknown inspection YAML opcode/label')
        table='Translations/TranslatedText/dialogue_'+('Reusable' if identity.startswith('Reusable/') else "Podunk_non-person text_Ninten's house")+' - sheet.csv'
        translations={r['key']:r for r in csv.DictReader(io.StringIO(ex.text(table)))}
        texts=[]
        for label,phrase in doc.items():
            require(phrase['text'] in translations and 'en' in translations[phrase['text']],'Missing inspection translation')
            raw=translations[phrase['text']]['en'];parts=text_segments(raw)
            candidates=[x for x in house['dialogues'] if x['source_path']==source]
            if isdrawer:
                matches=[x for x in drawer['texts'] if x['label']==label and x['raw']==raw]
                require(len(matches)==1,'Drawer actual label/text mapping')
                candidates=[x for x in candidates if x['id']==matches[0]['id']]
            require(len(candidates)==1,'Ambiguous existing House source text identity')
            text=candidates[0];segments=house['segments'][text['first_segment']:text['first_segment']+text['segment_count']]
            require(len(parts)==len(segments),'House inspection WAIT span mismatch')
            # Hint RGB is already reviewed in the immutable House text resource.
            for tokens,segment in zip(parts,segments):
                actual=segment['tokens'];require(len(actual)==len(tokens),'House inspection tokens mismatch')
                require(all(t['kind']==a['kind'] and (t['kind']==3 or t['text']==a['text']) for t,a in zip(tokens,actual)),
                        'House inspection text/receiver source mismatch')
            texts.append(dict(label=label,key=phrase['text'],id=text['id'],raw=raw,first_segment=text['first_segment'],segment_count=text['segment_count']))
        programs.append(dict(identity=identity,source=source,document=doc,texts=texts,drawer=isdrawer))
    return dict(schema=1,kind='encore.house-return-inspection-programme.source-ir',commit=PIN,
        dependencies={p:digest(root/p) for p in (BASE,HOUSE,DRAWER,INTERACT,TOOL)},
        sources=dict(sorted(ex.sources.items())),programmes=programs,
        return_duration=return_duration(dialogue),inventory_template_index=0,
        scope='All eight actual Interact nodes; nine YAML programmes; same Room VM/native DialogueBox; no Ready grant')

def append(base,source):
    out=copy.deepcopy(base);s=out['sections'];strings=out['strings']
    require(out['rules']==8 and out['capabilities']==9,'Original Room capability changed')
    require(next(r['value'] for r in s['Rule'] if r['key']==11)==source['return_duration'],
            'Actual dialogue ending differs from existing Room camera-return source rule')
    def string(value):
        if value not in strings:strings.append(value)
        return strings.index(value)
    flags={strings[x['name_string']]:i for i,x in enumerate(s['Flag'])}
    sounds={strings[x['path_string']]:i for i,x in enumerate(s['Resource']) if x['kind']==2}
    existing={strings[x['source_path_string']] for x in s['Program']}
    for program in source['programmes']:
        require(program['identity'] not in existing,'Inspection programme already linked')
        doc=program['document'];commands=[];labels={};fixups=[]
        def emit(op,label,**kw):
            c=dict(opcode=room.OPCODES.index(op),actor_index=room.NO_ACTOR,phrase=list(doc).index(label),
                target_index=NONE,flags=0,vector=[0,0],value=0,duration=0,auxiliary_index=NONE)
            c.update(kw);commands.append(c);return len(commands)-1
        emit('BeginCutscene','0')
        for label,phrase in doc.items():
            labels[label]=len(commands)
            if 'item' in phrase:emit('GrantInventoryItem',label,target_index=source['inventory_template_index'])
            emit('ShowDialogue',label,target_index=next(t['id'] for t in program['texts'] if t['label']==label),flags=1)
            if 'soundeffect' in phrase:
                path='res://Audio/Sound effects/'+phrase['soundeffect'];require(path in sounds,'Drawer sound absent from actual Room')
                emit('PlaySound',label,target_index=sounds[path])
            if 'setflags' in phrase:
                require(phrase['setflags'] in flags,'Drawer flag absent from actual Room')
                emit('SetFlag',label,target_index=flags[phrase['setflags']],value=1)
            emit('AwaitDialogue',label)
            for condition in phrase.get('if',[]):
                if 'flags' in condition:
                    name,value=next(iter(condition['flags'].items()));require(name in flags,'Drawer conditional flag absent')
                    pc=emit('BranchFlag',label,target_index=flags[name],value=int(value))
                else:pc=emit('BranchInventorySpace',label,value=int(condition['invspace']))
                fixups.append((pc,'auxiliary_index',condition['goto']))
            if 'goto' in phrase:fixups.append((emit('Jump',label),'target_index',phrase['goto']))
            else:
                emit('StopInteraction',label,flags=1);emit('SetTalker',label)
                emit('CutsceneEnded',label);emit('DialogueDone',label,duration=source['return_duration'])
        for pc,field,label in fixups:commands[pc][field]=labels[label]
        s['Program'].append(dict(stable_id=max(p['stable_id'] for p in s['Program'])+1,
            first_command=len(s['Command']),command_count=len(commands),phrase_count=len(doc),source_path_string=string(program['identity'])))
        s['Command'].extend(commands)
    out['rules']=8;out['capabilities']=10
    return out

def build_room(root=ROOT):
    root=Path(root);source=source_ir(root)
    require(read(root/IR)==source,'Stale inspection programme source IR')
    out=append(read(root/BASE),source)
    for path,sha in source['sources'].items():out['provenance']['sources']['upstream/MOTHER-Encore/'+path]=sha
    for path in (BASE,HOUSE,DRAWER,INTERACT,TOOL,IR,'tools/native_content.py'):
        out['provenance']['sources'][path]=digest(root/path)
    return out

def verify_extension(ir,root=ROOT):
    require(ir==build_room(root),'Stale/unreviewed House inspection Room programme extension')

def bundle_context(d):
    scene=d['strings'][d['sections']['Scene'][0]['source_scene_string']].removeprefix('res://')
    return dict(scene=scene,scene_id=d['scene_id'],source_sha256=d['provenance']['sources']['upstream/MOTHER-Encore/'+scene])

def stage_files(root):
    out=read(ROOT/ROOM_IR);verify_extension(out);room.verify_provenance(out)
    blob,_=room.compile_ir(out);path=Path('data/house-return-room.encroom')
    require((Path(root)/path).read_bytes()==blob,'Inspection complete Room staged pack differs')
    return {path:blob}

def main():
    p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile']);a=p.parse_args()
    if a.action=='extract':
        source=source_ir();write(ROOT/IR,source);out=build_room();write(ROOT/ROOM_IR,out)
        write(ROOT/REVIEW,dict(schema=1,commit=PIN,sources=source['sources'],dependencies=source['dependencies'],
            opcodes=['BeginCutscene','ShowDialogue','AwaitDialogue','BranchFlag','BranchInventorySpace','Jump',
                     'GrantInventoryItem','PlaySound','SetFlag','StopInteraction','SetTalker','CutsceneEnded','DialogueDone'],
            source_order='GrantItem -> text -> sound -> flag -> entire text acknowledgement -> ordered first-match branches',
            old_room_prefix='All original strings/sections preserved; only Program/Command/StringRef/StringBytes append',
            compatibility='ENCRMD01 format1 unchanged; unchanged rules8; independent cap10; old executable rejects new resource',
            tests_run=False,build_run=False,whole_dialogue_handler_approved=False))
    else:
        out=read(ROOT/ROOM_IR);verify_extension(out);room.verify_provenance(out)
        blob,manifest=room.compile_ir(out);path=ROOT/PACK;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(blob)
        write(ROOT/MANIFEST,manifest)
        print(json.dumps(dict(pack=PACK,bytes=len(blob),sha256=hashlib.sha256(blob).hexdigest(),
            original_programmes=len(read(ROOT/BASE)['sections']['Program']),programmes=len(out['sections']['Program']),
            rules=8,capabilities=10,tests_run=False,build_run=False)))
if __name__=='__main__':main()
