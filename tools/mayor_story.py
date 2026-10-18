"""Checked offline lowering for the initial Mayor Office conversations.

Unimplemented source overrides remain explicit interaction boundaries. This
candidate builder does not stage a scene, open the door or alter shipped data.
"""
import argparse,copy,csv,hashlib,io,json,re,struct,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.extract_battle_entry import Extractor,PIN,require
from tools import field_scene_npcs as npc,podunk_field as f,native_content as nc
from tools.programme_lowering_recipe import equal
NIL=0xffffffff
RECIPE=ROOT/'content/mayor-story.json'


def read(path=RECIPE):
    def unique(pairs):
        out={}
        for k,v in pairs:require(k not in out,'Duplicate Mayor story field');out[k]=v
        return out
    p=json.loads(Path(path).read_text(encoding='utf8'),object_pairs_hook=unique)
    require(set(p)==set('schema kind commit scene_id documents programmes sources dialogue_first_id locales limitations'.split())and type(p['schema'])is int and p['schema']==1 and p['kind']=='encore.mayor-story.reviewed-lowering'and p['commit']==PIN,'Mayor story recipe schema/pin')
    require(type(p['scene_id'])is int and p['scene_id']==f.load_recipe(ROOT/'content/mayor-field.json')['scene']['id']and p['locales']==['en','zh_CN'],'Mayor story scene/locales')
    require(isinstance(p['programmes'],list)and 1<=len(p['programmes'])<=32 and all(type(x)is str for x in p['programmes'])and set(p['programmes'])==set(p['documents'])and len(set(p['programmes']))==len(p['programmes']),'Mayor programme identities')
    require(type(p['dialogue_first_id'])is int and 0<p['dialogue_first_id']<1000000,'Mayor dialogue identity domain')
    ex=Extractor(ROOT)
    for source,digest in p['sources'].items():require(hashlib.sha256(ex.data(source)).hexdigest()==digest,'Mayor story reviewed source changed: '+source)
    for source,doc in p['documents'].items():require(equal(doc,ex.yaml(source)),'Mayor story reviewed document changed: '+source)
    _,rows,sources=npc.build()
    require(all(p['sources'].get(source)==digest for source,digest in sources.items()),'Mayor story NPC dependency missing or changed')
    return p,ex,rows


def graph(p,flags):
    commands=[];programmes=[];dialogues=[];groups=[];identity=p['dialogue_first_id']
    for source in p['programmes']:
        doc=p['documents'][source];require(isinstance(doc,dict)and '0'in doc and len(doc)<=32,'Mayor dialogue label domain')
        cs=[];labels={};patches=[]
        def emit(kind,phrase,target=NIL,aux=NIL,value=0,duration=0,features=0):
            cs.append(dict(opcode=nc.OPCODES.index(kind),actor_index=65535,phrase=phrase,target_index=target,auxiliary_index=aux,vector=[0,0],value=value,duration=duration,flags=features));return len(cs)-1
        emit('BeginCutscene',0)
        for phrase,(label,row)in enumerate(doc.items()):
            require(type(label)is str and isinstance(row,dict)and set(row)>=set(('name','sound','text'))and not(set(row)-set(('name','sound','text','if','options','goto'))),'Unreviewed Mayor phrase mechanism')
            require(all(type(row[k])is str and row[k]for k in ('name','sound','text')),'Mayor phrase fields')
            labels[label]=len(cs);dialogues.append(dict(id=identity,source=source,label=label));emit('ShowDialogue',phrase,identity,features=1);identity+=1
            if 'options'in row:
                require('if'not in row and 'goto'not in row and isinstance(row['options'],dict)and len(row['options'])==3 and 'cancel'in row['options'],'Mayor choice mechanism')
                options=[(k,v)for k,v in row['options'].items()if k!='cancel'];require(len(options)==2 and all(type(k)is str and type(v)is str and v in doc for k,v in options)and row['options']['cancel']in doc,'Mayor choice target')
                emit('AwaitChoices',phrase,len(groups));groups.append(dict(id='mayor-choice-'+str(len(groups)),program_identity=source.removeprefix('Data/Dialogue/').removesuffix('.yaml'),source_label=label,program_command_count=0,initial_selection=0,cancel_target_pc=row['options']['cancel'],options=[dict(translation_key=k,text='',target_pc=v)for k,v in options]))
            else:
                emit('AwaitDialogue',phrase)
                if 'if'in row:
                    cond=row['if'];require(isinstance(cond,dict)and set(cond)=={'flags','goto'}and isinstance(cond['flags'],dict)and len(cond['flags'])==1 and cond['goto']in doc,'Unreviewed Mayor condition')
                    flag,value=next(iter(cond['flags'].items()));require(flag in flags and type(value)is bool,'Mayor flag condition')
                    patches.append((emit('BranchFlag',phrase,flags[flag],value=int(value)),cond['goto'],'auxiliary_index'))
                target=row.get('goto','end');require(target=='end'or target in doc,'Mayor goto target')
                patches.append((emit('Jump',phrase),target,'target_index'))
        terminal=len(cs);phrase=len(doc)
        emit('StopInteraction',phrase,features=1);emit('SetTalker',phrase);emit('CutsceneEnded',phrase)
        from tools.doll_postwin import return_duration
        duration=return_duration((ROOT/'upstream/MOTHER-Encore/Scripts/UI/DialogueBox.gd').read_text(encoding='utf8'))
        emit('DialogueDone',phrase,duration=duration)
        for index,target,key in patches:cs[index][key]=terminal if target=='end'else labels[target]
        for group in groups:
            if group['program_identity']!=source.removeprefix('Data/Dialogue/').removesuffix('.yaml'):continue
            group['program_command_count']=len(cs);group['cancel_target_pc']=labels[group['cancel_target_pc']]
            for option in group['options']:option['target_pc']=labels[option['target_pc']]
        programmes.append(dict(stable_id=len(programmes)+1,first_command=len(commands),command_count=len(cs),phrase_count=len(doc)+1,source=source))
        commands.extend(cs)
    return commands,programmes,dialogues,groups


def choices_blob(groups,ex):
    from tools import dialogue_choice_assets as choices
    r=json.loads(choices.RECIPE.read_bytes());choices.verify_recipe(r)
    rows={x['key']:x for x in csv.DictReader(io.StringIO(ex.text('Translations/TranslatedText/dialogue_Podunk - sheet.csv')))}
    r=copy.deepcopy(r);r['groups']=copy.deepcopy(groups);r['extra_option_rects']=[r['option_rects']for _ in groups[1:]]
    require(bool(groups),'Mayor source choices are required by this candidate capability')
    for group in r['groups']:
        for option in group['options']:option['text']=rows[option['translation_key']]['en']
    return choices.encode(r)


def add_room(sections,strings,S,*unused):
    p,ex,rows=read();flags={strings[x['name_string']]:i for i,x in enumerate(sections['Flag'])}
    commands,programmes,dialogues,groups=graph(p,flags)
    require(not sections['Command']and not sections['Program'],'Mayor story requires an owned empty programme table')
    sections['Command'].extend(commands)
    for row in programmes:
        source=row.pop('source');row['source_path_string']=S(source.removeprefix('Data/Dialogue/').removesuffix('.yaml'));sections['Program'].append(row)
    return {**{'upstream/MOTHER-Encore/'+s:h for s,h in ex.sources.items()},'content/mayor-story.json':hashlib.sha256(RECIPE.read_bytes()).hexdigest(),'tools/mayor_story.py':hashlib.sha256(Path(__file__).read_bytes()).hexdigest()}


def build_story(room):
    p,ex,rows=read();flags={room['strings'][x['name_string']]:i for i,x in enumerate(room['sections']['Flag'])}
    _,programmes,dialogues,groups=graph(p,flags);choice=choices_blob(groups,ex);pool=bytearray();cache={}
    def ref(text):
        if not text:return (0,0)
        if text not in cache:cache[text]=(len(pool),len(text.encode()));pool.extend(text.encode())
        return cache[text]
    progs=[struct.pack('<6I',i,0,0,*ref(row['source'].removeprefix('Data/Dialogue/').removesuffix('.yaml')),1)for i,row in enumerate(programmes)]
    table={x['key']:x for x in csv.DictReader(io.StringIO(ex.text('Translations/TranslatedText/dialogue_Podunk - sheet.csv')))}
    texts=[]
    for binding in dialogues:
        doc=p['documents'][binding['source']][binding['label']]
        for locale in p['locales']:
            def text(key):
                if key.startswith('['):
                    require(key in ('[Pippi]','[Ninten]'),'Unreviewed Mayor literal name token');return key
                require(key in table,'Mayor translation key missing');value=table[key][locale]or table[key]['en'];value=value.replace('\\n','\n')
                require(all(tag in ('[@]','[WAIT@]','[color]','[/color]','[DELAY]','[Ninten]','[Pippi]')for tag in re.findall(r'\[[^]]+\]',value)),'Unreviewed Mayor text token');return value
            voice='Audio/Sound effects/text/'+doc['sound']+'.mp3';ex.data(voice)
            texts.append(struct.pack('<9I',binding['id'],*ref('zh_Hans_CN'if locale=='zh_CN'else locale),*ref(text(doc['name'])),*ref(text(doc['text'])),*ref(voice)))
    admitted={source:i for i,source in enumerate(p['programmes'])};interactions=[]
    for row in rows:
        if not row['has_initial_dialogue']:continue
        choices=[['',row['dialogue']]]+row['alternatives']
        for selected in choices:
            require(len(selected)==2 and all(type(v)is str for v in selected),'Unreviewed multi-choice seen-dialogue sequence')
            flag,source=selected;path='Data/Dialogue/'+source+'.yaml';ex.data(path)
            require(path in p['sources'],'Unreviewed Mayor interaction source')
            require(not flag or flag in flags,'Mayor override flag missing')
            # Source default both player axes and NPC turning are reviewed here.
            from tools.extract_battle_entry import node
            original=node(ex.text('Maps/podunk/Mayors Office.tscn'),row['binding']['path'])
            require(original.get('player_turn',{'x':True,'y':True})=={'x':True,'y':True}and original.get('turn_to_player_on_interact',True)is True,'Mayor interaction turn policy differs')
            programme=admitted.get(path,NIL);interactions.append(struct.pack('<8I',row['binding']['id']-1,programme,*ref(flag),3,int(programme==NIL),*ref(source)))
    sections=[bytes(pool),b''.join(progs),b''.join(texts),b'',b'',b''.join(interactions),choice]
    blob=f.pack_container(b'ENCFST01',p['scene_id'],sections,(1,24,36,40,12,32,1),128*1024,capability=3)
    return blob,dict(source_commit=PIN,sources=ex.sources,programme_count=len(programmes),dialogues=len(dialogues),choices=groups,unavailable_interactions=sum(struct.unpack('<8I',x)[5]for x in interactions),release_staged=False)


def main():
    a=argparse.ArgumentParser();a.add_argument('--out',type=Path,required=True);a.add_argument('--npc-assets',type=Path,default=ROOT/'build/mayor-import/npc-candidate');o=a.parse_args();out=o.out.resolve()
    require(out.is_relative_to(ROOT/'build')and out!=ROOT/'build','Mayor story candidate output must remain under build')
    recipe=f.load_recipe(ROOT/'content/mayor-field.json');model=f.Model(recipe)
    room=f.build_room(recipe,model,recipe['house']['room_ir'],npc_adder=lambda *args:npc.add_room(*args,asset_root=o.npc_assets.resolve()),story_adder=add_room,recipe_path=ROOT/'content/mayor-field.json')
    parsed=nc.parse_pack(room);story,receipt=build_story(parsed);out.mkdir(parents=True,exist_ok=True)
    (out/'mayor.encroom').write_bytes(room);(out/'mayor.encstory').write_bytes(story)
    receipt['producer_sha256']=hashlib.sha256(Path(__file__).read_bytes()).hexdigest();receipt['dependencies']={name:hashlib.sha256((ROOT/name).read_bytes()).hexdigest()for name in ['content/mayor-story.json','content/mayor-field.json','content/mayor-npcs.json','content/native-dialogue-choices.json','tools/dialogue_choice_assets.py','tools/menu_audio_binding.py','reports/dad-record/native-layout.json','tools/field_scene_npcs.py','tools/podunk_field.py','tools/native_content.py']};receipt['outputs']={p.name:hashlib.sha256(p.read_bytes()).hexdigest()for p in out.iterdir()if p.is_file()and p.suffix in('.encroom','.encstory')}
    (out/'source.json').write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf8');print('Mayor story candidate:',len(room),len(story),'bytes; not staged')
if __name__=='__main__':main()
