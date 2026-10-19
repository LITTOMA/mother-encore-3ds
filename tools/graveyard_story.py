"""Source-limited cemetery conversations; quest/item actions stay fail-closed."""
import csv,hashlib,io,json
from pathlib import Path
from tools.extract_battle_entry import Extractor,PIN,require,node
from tools.programme_lowering_recipe import equal
from tools.mayor_story import graph,choices_blob

ROOT=Path(__file__).resolve().parents[1]

def checked_text(value):
    require(type(value)is str and 0<len(value.encode())<=8192,'Cemetery text size/type')
    allowed={'[@]','[WAIT@]','[color]','[/color]','[DELAY]','[Ninten]','[Pippi]','[ui_cancel]'}
    at=0
    while at<len(value):
        start=value.find('[',at)
        if start<0:require(']'not in value[at:],'Unmatched cemetery text bracket');break
        require(']'not in value[at:start],'Unmatched cemetery text bracket')
        end=value.find(']',start);require(end>=0,'Unclosed cemetery text token');tag=value[start:end+1]
        if tag=='[if input:gamepad]':
            alternate=value.find('[else]',end+1);close=value.find('[/if]',end+1)
            require(0<=alternate<close,'Malformed cemetery input conditional')
            branches=(value[end+1:alternate],value[alternate+6:close])
            require(all(b and '['not in b and ']'not in b for b in branches),'Unreviewed cemetery input conditional body')
            at=close+5;continue
        require(tag in allowed,'Cemetery text token outside reviewed scope');at=end+1
    return value

def read(path):
    def unique(pairs):
        out={}
        for k,v in pairs:require(k not in out,'Duplicate cemetery story field');out[k]=v
        return out
    path=Path(path);p=json.loads(path.read_text(encoding='utf8'),object_pairs_hook=unique)
    require(set(p)==set('schema kind commit scene_id npc_path documents programmes unavailable dialogue_first_id locales sources limitations'.split())and type(p['schema'])is int and p['schema']==1 and p['kind']=='encore.graveyard-story.reviewed-lowering'and p['commit']==PIN,'Cemetery story schema/pin')
    require(type(p['scene_id'])is int and p['scene_id']>0 and type(p['npc_path'])is str and p['locales']==['en','zh_CN'],'Cemetery story identity/locales')
    require(type(p['dialogue_first_id'])is int and 0<p['dialogue_first_id']<1000000,'Cemetery dialogue identity')
    require(type(p['documents'])is dict and type(p['sources'])is dict and p['sources']and all(type(s)is str and type(h)is str and len(h)==64 for s,h in p['sources'].items()),'Cemetery source/document fields')
    require(type(p['programmes'])is list and 0<len(p['programmes'])<=16 and all(type(s)is str for s in p['programmes'])and len(set(p['programmes']))==len(p['programmes'])and set(p['programmes'])==set(p['documents']),'Cemetery programme coverage')
    require(type(p['unavailable'])is list and all(type(s)is str for s in p['unavailable'])and len(set(p['unavailable']))==len(p['unavailable'])and not(set(p['programmes'])&set(p['unavailable'])),'Cemetery unavailable dialogue coverage')
    ex=Extractor(ROOT)
    for source,digest in p['sources'].items():require(hashlib.sha256(ex.data(source)).hexdigest()==digest,'Cemetery reviewed source changed: '+source)
    for source,doc in p['documents'].items():require(source in p['sources']and equal(doc,ex.yaml(source))and list(doc)==list(ex.yaml(source)),'Cemetery reviewed document/order changed')
    from tools import field_npcs
    policy,rows,_=field_npcs.build();owner=next((r for r in rows if r['binding']['path']==p['npc_path']),None)
    require(owner is not None and owner['binding'].get('interaction_policy')is True and policy['scene']in p['sources'],'Cemetery source NPC policy missing')
    original=node(ex.text(policy['scene']),p['npc_path'])
    require(original.get('player_turn',{'x':True,'y':True})=={'x':True,'y':True}and original.get('turn_to_player_on_interact',True)is True,'Cemetery interaction turn policy changed')
    selections=[['',original['dialog']]]+original.get('_all_dialog',[])
    require(all(type(x)is list and len(x)==2 and all(type(y)is str for y in x)for x in selections),'Cemetery dynamic dialogue sequence is not reviewed')
    actual={'Data/Dialogue/'+x[1]+'.yaml'for x in selections}
    require(actual==set(p['programmes'])|set(p['unavailable'])and actual<=set(p['sources']),'Cemetery dialogue source coverage differs')
    return p,ex,owner,selections

def lowered(path,flags):
    p,ex,owner,selections=read(path)
    commands,programmes,dialogues,groups=graph(p,flags)
    require(bool(groups),'Cemetery plain conversation requires its reviewed choices')
    return p,ex,owner,selections,commands,programmes,dialogues,groups

def add_room(path,sections,strings,S):
    flags={strings[r['name_string']]:i for i,r in enumerate(sections['Flag'])}
    p,ex,owner,_,commands,programmes,_,_=lowered(path,flags)
    require(p['scene_id']==sections['Scene'][0]['stable_id']and owner['binding']['id']<=len(sections['ActorInstance']),'Cemetery Room source owner differs')
    first=len(sections['Command']);sections['Command'].extend(commands)
    for row in programmes:
        row=dict(row);source=row.pop('source');row.update(stable_id=len(sections['Program'])+1,first_command=first+row['first_command'],source_path_string=S(source.removeprefix('Data/Dialogue/').removesuffix('.yaml')))
        sections['Program'].append(row)
    return {**{'upstream/MOTHER-Encore/'+s:h for s,h in ex.sources.items()},Path(path).resolve().relative_to(ROOT).as_posix():hashlib.sha256(Path(path).read_bytes()).hexdigest(),'tools/graveyard_story.py':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),'tools/mayor_story.py':hashlib.sha256((ROOT/'tools/mayor_story.py').read_bytes()).hexdigest()}

def payload(path,flags,first_programme):
    p,ex,owner,selections,_,programmes,dialogues,groups=lowered(path,flags)
    table={r['key']:r for r in csv.DictReader(io.StringIO(ex.text('Translations/TranslatedText/dialogue_Podunk - sheet.csv')))}
    texts=[]
    for binding in dialogues:
        source=p['documents'][binding['source']][binding['label']]
        for locale in p['locales']:
            def text(key):
                require(key in table,'Cemetery translation key missing');value=table[key][locale]or table[key]['en'];value=value.replace('\\n','\n')
                return checked_text(value)
            voice='Audio/Sound effects/text/'+source['sound']+'.mp3';ex.data(voice)
            texts.append(dict(id=binding['id'],locale='zh_Hans_CN'if locale=='zh_CN'else locale,speaker=text(source['name']),body=text(source['text']),voice=voice))
    admitted={source:first_programme+i for i,source in enumerate(p['programmes'])};interactions=[]
    for flag,source in selections:
        require(not flag or flag in flags,'Cemetery override flag missing')
        programme=admitted.get('Data/Dialogue/'+source+'.yaml',0xffffffff)
        interactions.append(dict(actor=owner['binding']['id']-1,programme=programme,flag=flag,turns=3,status=int(programme==0xffffffff),source=source))
    return p,programmes,texts,interactions,choices_blob(groups,ex)
