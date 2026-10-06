#!/usr/bin/env python3
"""Full original cold _load_dict_to_game source policies and native save data.

Original YAML parsing is a data conversion only. This pack does not execute
global._ready, override mutation, Inventory/Character methods, UI or scene
transition. Cold default and new-game documents remain distinct capabilities.
"""
from __future__ import annotations
import argparse, copy, hashlib, re, struct, sys, zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import PIN,read,write,sha,require,stable
from tools.extract_battle_entry import Extractor
from tools.global_yaml_caches import value,ENGINE
OWNER='Scripts/global/global.gd'
DATA='Scripts/global/globalData.gd'
IR=ROOT/'content/native-global-load.json'
RECEIPT=ROOT/'reports/global-load/native-saves.json'
REVIEW=ROOT/'reports/global-load/source-review.json'
PACK=ROOT/'romfs/data/global.encload'
EXPORTER=ROOT/'tools/godot_exporter/global_load.gd'
FAMILY=0x454e0054


def get(v,key):
    require(v['kind']==6,'Original save root is not Dictionary')
    return next((x for k,x in v['entries']if k==key),None)


def literal(s):
    if s=='{}':return dict(kind=6,entries=[])
    if s=='[]':return dict(kind=5,values=[])
    if s in ('true','false'):return dict(kind=1,value=s=='true')
    if re.fullmatch(r'-?\d+',s):return dict(kind=2,value=s)
    raise ValueError('Unknown source LOAD fallback '+s)


def merge(saved,overrides):
    require(saved['kind']==overrides['kind']==6,'Unknown source recursive override root')
    for key,new in overrides['entries']:
        old=get(saved,key)
        if old is not None and old['kind']==new['kind']==6:merge(old,new)
        elif old is not None and old['kind']==new['kind']==5:
            if not new['values'] or(old['values']and old['values'][0]['kind']in(5,6)):
                old['values']=copy.deepcopy(new['values'])
            else:
                require(all(x['kind']==4 for x in old['values']+new['values']),'Unknown original primitive-array merge element equality')
                for x in new['values']:
                    if x not in old['values']:old['values'].append(copy.deepcopy(x))
        elif old is None:saved['entries'].append([key,copy.deepcopy(new)])
        else:
            index=next(i for i,p in enumerate(saved['entries'])if p[0]==key)
            saved['entries'][index][1]=copy.deepcopy(new)
    return saved


def inputs():
    ex=Extractor(ROOT)
    sources=[OWNER,DATA,'Scripts/global/Inventory.gd','Scripts/global/Item.gd','Scripts/global/PartyMember.gd','Scripts/global/PartyNPC.gd','Scripts/global/uiManager.gd','Scripts/global/yaml_parser.gd']
    text={p:ex.text(p)for p in sources};global_body=text[OWNER]
    require('var file := File.new()\n\tif file.file_exists(file_path):\n\t\tfile.open(file_path, File.READ)\n\t\tvar file_content := file.get_as_text()\n\t\tvar res = YAMLParser.parse_file(file_path)'in text[DATA],'Unknown actual get_json_data File source order')
    require('Item.new(item["item_name"], item.get("equipped", false),\\\n\t\t\t\titem.get("doses", 1), int(item.get("uid", Item.get_uid(Item.used_uids_tab))))'in text[sources[2]],'Unknown eager serialized Item constructor arguments')
    cold=re.search(r'^const SAVE_DEFAULT_PATH := "res://([^"]+)"$',global_body,re.M)[1]
    new=re.search(r'^const SAVE_NEW_GAME_PATH := "res://([^"]+)"$',global_body,re.M)[1]
    override=re.search(r'_override_save_dict\(save_data, globaldata.get_json_data\("res://([^"]+)"\)\)',global_body)[1]
    require('func _load_default_save():\n\tvar save_data: Dictionary = globaldata.get_json_data(SAVE_DEFAULT_PATH)\n\t_load_dict_to_game(save_data, false)'in global_body,'Unknown cold-default invocation')
    original_merge=global_body.split('func _override_save_dict(save: Dictionary, overrides: Dictionary):\n',1)[1].split('\nfunc _load_dict_to_game',1)[0]
    require('var fully_replace_array: bool = overrides[key].empty() or (save[key].size() > 0 and typeof(save[key][0]) in [TYPE_DICTIONARY, TYPE_ARRAY])'in original_merge and
            'if !item in save[key]:\n\t\t\t\t\t\tsave[key].append(item)'in original_merge and
            'if save.has(key) and save[key] is Dictionary and overrides[key] is Dictionary:'in original_merge,
            'Unknown actual source override rules')
    body=global_body.split('func _load_dict_to_game(save_data: Dictionary, goto_game := true):\n',1)[1].split('\nfunc erase_save',1)[0]
    prefix,suffix=body.split('\tif goto_game:\n',1)
    lines=[s.strip()for s in prefix.splitlines()if s.strip()and not s.strip().startswith('#')]
    require(lines[0]=='_override_save_dict(save_data, globaldata.get_json_data("res://'+override+'"))','Unknown first override cursor')
    constructor=read(ROOT/'content/native-global-data-constructor.json')
    char=read(ROOT/'content/native-field-character-load.json')
    flags=read(ROOT/'content/native-global-flags.json')
    definitions=read(ROOT/'content/native-global-item-definitions.json')
    require(all(x['commit']==PIN for x in (constructor,char,flags,definitions)),'Changed LOAD source dependencies')
    require(char['source_save']==cold,'Eight-character capability is not cold default')
    for p in (cold,new,override):ex.data(p)
    rows=[];steps=[dict(kind=1,index=0,owner=OWNER,method='_override_save_dict',member='')]
    cursor=1
    while cursor<len(lines)and re.match(r'globaldata\.\w+ = ',lines[cursor]):
        line=lines[cursor];cursor+=1
        match=re.fullmatch(r'globaldata\.(\w+) = (.*)',line);member,expr=match.groups()
        vector=re.fullmatch(r'Vector2\(save_data.get\("([^"]+)", ([^()]+)\), save_data.get\("([^"]+)", \2\)\)',expr)
        if vector:
            row=dict(member=member,kind=2,key=vector[1],key_y=vector[3],coercion=0,fallback_kind=1,
                     fallback=literal(vector[2]),fallback_member='',constant='',constant_index=0)
        else:
            cast=expr.startswith('int(')
            if cast:require(expr.endswith(')'),'Unknown int coercion');expr=expr[4:-1]
            m=re.fullmatch(r'save_data.get\("([^"]+)", (.*)\)',expr);require(m,'Unknown scalar LOAD '+line)
            key,fallback=m.groups();same=re.fullmatch(r'globaldata\.(\w+)',fallback);constant=re.fullmatch(r'globaldata\.(\w+)\[(\d+)\]',fallback)
            row=dict(member=member,kind=1,key=key,key_y='',coercion=int(cast),fallback_kind=2 if same else 3 if constant else 1,
                     fallback=literal(fallback)if not same and not constant else dict(kind=0),fallback_member=same[1]if same else'',constant=constant[1]if constant else'',constant_index=int(constant[2])if constant else 0)
            require(not same or same[1]==member,'Unknown source live member fallback')
        decl=next((d for d in constructor['declarations']if d['name']==member),None)
        require(decl and not decl['constant']and decl['adapter']not in(1,2,3,4),'Unknown mutable globaldata source destination')
        steps.append(dict(kind=2,index=len(rows),owner=DATA,method='',member=member));rows.append(row)
    inventories=[]
    while cursor<len(lines):
        m=re.fullmatch(r'globaldata\.(\w+)\.(\w+)\(save_data.get\("([^"]+)", \[\]\)\)',lines[cursor])
        if not m:break
        cursor+=1;member,method,key=m.groups()
        decl=next((o for o in constructor['objects']if o['kind']==2 and o['name']==member),None)
        require(decl and method=='init_from_serialized','Unknown Inventory load cursor')
        inventories.append(dict(member=member,method=method,source_key=key,id=decl['id'],role=decl['role']))
        steps.append(dict(kind=3,index=len(inventories)-1,owner=decl['script'],method=method,member=member))
    require(len(inventories)==2,'Incomplete KEY/STORAGE source load')
    expected=['for char_id in globaldata.characters:','globaldata.characters[char_id].init_from_dict(save_data.get(char_id, {}))','party.clear()','partyNpcs.clear()','for i in save_data["party"]:','if i in POSSIBLE_PLAYABLE_MEMBERS:','party.append(globaldata.characters.get(i))','else:','partyNpcs.append(globaldata.characters.get(i))','uiManager.set_menu_flavors(globaldata.menu_flavor)','for flag in globaldata.flags:','globaldata.flags[flag] = save_data.get("flags", {}).get(flag, false)']
    require(lines[cursor:]==expected,'Unknown actual source Character/party/UI/flags suffix')
    steps += [dict(kind=4,index=0,owner=DATA,method='init_from_dict',member='characters'),dict(kind=5,index=0,owner=OWNER,method='clear',member='party'),dict(kind=6,index=0,owner=OWNER,method='clear',member='partyNpcs'),dict(kind=7,index=0,owner=OWNER,method='append',member=''),dict(kind=8,index=0,owner='Scripts/global/uiManager.gd',method='set_menu_flavors',member=constructor['menu_flavor_member']),dict(kind=9,index=0,owner=DATA,method='',member='flags'),dict(kind=10,index=0,owner=OWNER,method='',member='goto_game')]
    require('for i in 8:\n\t\t_menu_flavor_shader.set_shader_param('in text[sources[6]],'Unknown actual Ui flavor method owner')
    ready=global_body.split('func _ready():\n',1)[1].split('\nfunc _init_player():',1)[0]
    require(ready=='\t_set_localized_default_inputs()\n\t_load_settings()\n\tpartySpace.resize(255)\n\tadd_child(scene_transition)\n\t_init_player()\n\t_load_default_save()\n','Unknown source cold LOAD predecessor order')
    pre=[dict(kind=1,source=OWNER,method='_init_player',member='partyObjects'),
         dict(kind=2,source=DATA,method='_ready',member=constructor['ready_steps'][0]['member']),
         dict(kind=3,source=sources[6],method='set_menu_flavors',member='_menu_flavor_shader'),
         dict(kind=4,source=DATA,method='_init_flags',member='flags')]
    playable=re.search(r'^const POSSIBLE_PLAYABLE_MEMBERS := \[([^]]+)\]$',global_body,re.M)[1].split(', ')
    names=[]
    for ref in playable:
        m=re.fullmatch(r'PartyMember\.(\w+)',ref);require(m,'Unknown source playable member identity')
        names.append(re.search(r'^const '+m[1]+r' := "([^"]+)"$',text[sources[4]],re.M)[1])
    # Bind full original definition source closure, not the old 19-item slice.
    def_sources={}
    for definition in definitions['definitions']:
        source=definition['source'];ex.data(source);def_sources[source]=ex.sources[source]
    inventory=read(ROOT/'compatibility/upstream-inventory.json')['files']
    require(set(def_sources)=={p for p in inventory if p.startswith('Data/Items/')and p.endswith('.yaml')}and
            len(def_sources)==len(definitions['definitions'])and definitions['constructor_sources']==['Scripts/global/Item.gd','Scripts/global/Inventory.gd',DATA],
            'Full Item source constructor coverage required')
    return ex,rows,inventories,steps,pre,names,cold,new,override,char,flags,definitions,constructor,def_sources,original_merge,suffix


def prepare(directory):
    ex,*tail=inputs();cold,new,override=tail[5:8]
    directory.mkdir(parents=True,exist_ok=True)
    (directory/'project.godot').write_bytes(b'config_version=4\n[application]\nconfig/name="Original LOAD data export"\n[logging]\nfile_logging/enable_logging=false\n')
    parser='Scripts/global/yaml_parser.gd'
    (directory/'yaml_parser.gd').write_bytes(ex.data(parser));(directory/'export.gd').write_bytes(EXPORTER.read_bytes())
    for source in(cold,new,override):
        p=directory/source;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(ex.data(source))
    write(directory/'manifest.json',[dict(source=p)for p in(cold,new,override)])
    print('Original source parser export: three separate save documents; no lifecycle')


def typed_items(v,definitions):
    require(v['kind']==5,'Unknown saved Inventory type');rows=[]
    names={d['item_name']for d in definitions['definitions']}
    for entry in v['values']:
        require(entry['kind']==6 and {k for k,_ in entry['entries']}<=set('item_name equipped doses uid'.split()),'Unknown source serialized Item fields')
        name=get(entry,'item_name');require(name and name['kind']==4 and name['value']in names,'Saved Item outside full definitions')
        equipped=get(entry,'equipped')or dict(kind=1,value=False);doses=get(entry,'doses')or dict(kind=2,value='1');uid=get(entry,'uid')
        require(equipped['kind']==1 and doses['kind']==2 and 0<int(doses['value'])<=65535 and(uid is None or(uid['kind']==2 and 0<=int(uid['value'])<=0xffffffff)),'Unknown source saved Item values')
        rows.append(dict(name=name['value'],equipped=equipped['value'],doses=int(doses['value']),has_uid=uid is not None,uid=int(uid['value'])if uid else 0))
    return rows


def derive(native):
    ex,rows,inventories,steps,pre,playable,cold,new,override,char,flags,definitions,constructor,def_sources,merge_source,goto_source=inputs()
    receipt=read(native)
    require(set(receipt)=={'schema','engine','source_scene_entered','documents'}and receipt['schema']==1 and receipt['source_scene_entered']is False,'Unknown source-only save parser receipt')
    require(all(receipt['engine'].get(k)==v for k,v in dict(major=3,minor=6,patch=2,status='stable',build='official',hash=ENGINE).items()),'Unknown original parser engine')
    docs={x['source']:value(x['value'])for x in receipt['documents']}
    require(len(docs)==len(receipt['documents'])and set(docs)=={cold,new,override}and all(v['kind']==6 for v in docs.values()),'Incomplete native save closure')
    file_ir=read(ROOT/'content/native-global-yaml-file.json')
    require(file_ir['commit']==PIN and file_ir['owner']==DATA and file_ir['parser']=='Scripts/global/yaml_parser.gd','Unknown actual File/SmartReader dependency')
    for source,h in file_ir['sources'].items():require(hashlib.sha256(ex.data(source)).hexdigest()==h,'Changed source File/SmartReader proof')
    documents=[]
    require('func load_new_game(auto_name := false, goto_game := false):\n\tvar save_data: Dictionary = globaldata.get_json_data(SAVE_NEW_GAME_PATH)'in ex.text(OWNER),'Unknown original new-game file read source')
    for kind,source,method in [(0,cold,'_load_default_save'),(1,new,'load_new_game'),(2,override,'_load_dict_to_game')]:
        raw=ex.data(source);raw.decode('utf-8')
        documents.append(dict(kind=kind,caller_source=OWNER,caller_method=method,getter_source=DATA,getter_method='get_json_data',parser_source=file_ir['parser'],parser_method='parse_file',file=dict(role=kind,source=source,bytes_hex=raw.hex(),sha256=hashlib.sha256(raw).hexdigest(),parsed=docs[source])))
    merged=merge(copy.deepcopy(docs[cold]),docs[override])
    for inv in inventories:inv['items']=typed_items(get(merged,inv['source_key'])or dict(kind=5,values=[]),definitions)
    chars=[]
    for row in char['rows']:
        saved=get(merged,row['name']);require(saved and saved['kind']==6,'Missing actual cold character Dictionary')
        require(get(saved,'name')['value']==row['display_name']and int(get(saved,'level')['value'])==row['level']and int(get(saved,'exp')['value'])==row['exp'],'Existing eight-character cold source differs')
        require(typed_items(get(saved,'inventory')or dict(kind=5,values=[]),definitions)==row['inventory'],'Existing character inventory source differs')
        chars.append(dict(id=row['id'],role=row['role'],name=row['name'],saved=saved))
    party_value=get(merged,'party');require(party_value and party_value['kind']==5,'Unknown source party type')
    byname={r['name']:r for r in chars};party=[]
    for v in party_value['values']:
        require(v['kind']==4 and v['value']in byname,'Unknown source party member')
        row=byname[v['value']];party.append(dict(name=row['name'],id=row['id'],playable=row['name']in playable))
    savedflags=get(merged,'flags')or dict(kind=6,entries=[]);require(savedflags['kind']==6,'Unknown source normal flags type')
    projected=[]
    for row in flags['registered']:
        v=get(savedflags,row['name'])or dict(kind=1,value=False);require(v['kind']==1,'Unknown original flag boolean');projected.append([row['name'],v['value']])
    profile=next(p for p in flags['profiles']if p['source']==cold);require([v for _,v in projected]==profile['normal'],'Independent registered flag projection differs')
    return dict(schema=1,kind='encore.global-load.source-ir',commit=PIN,family=FAMILY,owner=OWNER,
                scene_id=stable('global-load:'+cold),source_sha256=ex.sources[OWNER],sources=ex.sources,
                cold_source=cold,new_game_source=new,overrides_source=override,cold_original=docs[cold],new_game_original=docs[new],overrides=docs[override],cold_merged=merged,
                assignments=rows,inventories=inventories,characters=chars,playable_names=playable,party=party,flags=projected,
                saved_item_keys=['item_name','equipped','doses','uid'],saved_item_default_doses=1,saved_item_default_equipped=False,
                normal_flags_member='flags',normal_flags_source_key='flags',party_source_key='party',menu_flavor_member=constructor['menu_flavor_member'],steps=steps,preconditions=pre,
                constructor_ir_sha256=sha(ROOT/'content/native-global-data-constructor.json'),characters_ir_sha256=sha(ROOT/'content/native-field-character-load.json'),flags_ir_sha256=sha(ROOT/'content/native-global-flags.json'),
                definition_sources=def_sources,native_receipt_sha256=sha(native),source_merge_sha256=hashlib.sha256(merge_source.encode()).hexdigest(),source_goto_sha256=hashlib.sha256(goto_source.encode()).hexdigest(),
                documents=documents,file_ir_sha256=sha(ROOT/'content/native-global-yaml-file.json'),
                cold_default_admitted=True,new_game_admitted=False,goto_game_admitted=False,
                source_order=['Original override runs first against actual mutable save Dictionary','Scalar fallbacks read live globaldata members after actual _init_player/set_respawn','KEY then STORAGE; actual Item constructors evaluate eager UID fallback before each constructor','All eight actual characters in dictionary insertion order, including inactive members','Clear both party arrays then classify source ordered saved party with actual playable constants','Actual Ui ShaderMaterial constructor owns set_menu_flavors; UiManager Ready is not a prerequisite','Registered normal flags mutate directly without flags_updated emission','goto_game=false skips entire scene/player suffix; this capability does not execute new-game naming/transition'])


def extract(native):
    d=derive(native);write(RECEIPT,read(native));d['native_receipt_sha256']=sha(RECEIPT);write(IR,d)
    write(REVIEW,dict(schema=1,commit=PIN,ir_sha256=sha(IR),native_receipt_sha256=sha(RECEIPT),exporter_sha256=sha(EXPORTER),sources=d['sources'],capability=1,new_game_admitted=False,goto_game_admitted=False))


def load():
    d=read(IR);require(d==derive(RECEIPT),'Changed original LOAD source/native documents/dependencies')
    require(read(REVIEW)==dict(schema=1,commit=PIN,ir_sha256=sha(IR),native_receipt_sha256=sha(RECEIPT),exporter_sha256=sha(EXPORTER),sources=d['sources'],capability=1,new_game_admitted=False,goto_game_admitted=False),'Stale complete cold LOAD source review')
    return d


def encode(d):
    b=bytearray(128)
    def u(*x):b.extend(struct.pack('<'+'I'*len(x),*x))
    def t(s):v=s.encode();u(len(v));b.extend(v)
    def val(v):
        k=v['kind'];u(k)
        if k==1:u(int(v['value']))
        elif k==2:b.extend(struct.pack('<q',int(v['value'])))
        elif k==3:b.extend(bytes.fromhex(v['f64_le']))
        elif k==4:t(v['value'])
        elif k==5:
            u(len(v['values']))
            for x in v['values']:val(x)
        elif k==6:
            u(len(v['entries']))
            for key,x in v['entries']:t(key);val(x)
    for k in('owner','cold_source','new_game_source','overrides_source','normal_flags_member','normal_flags_source_key','menu_flavor_member','party_source_key'):t(d[k])
    for k in('constructor_ir_sha256','characters_ir_sha256','flags_ir_sha256'):b.extend(bytes.fromhex(d[k]))
    b.extend(bytes.fromhex(d['file_ir_sha256']))
    for k in('sources','definition_sources'):
        u(len(d[k]))
        for p,h in d[k].items():t(p);b.extend(bytes.fromhex(h))
    for k in('cold_original','new_game_original','overrides','cold_merged'):val(d[k])
    u(len(d['documents']))
    for row in d['documents']:
        u(row['kind'])
        for k in('caller_source','caller_method','getter_source','getter_method','parser_source','parser_method'):t(row[k])
        f=row['file'];u(f['role']);t(f['source']);raw=bytes.fromhex(f['bytes_hex']);u(len(raw));b.extend(raw);b.extend(bytes.fromhex(f['sha256']));val(f['parsed'])
    u(len(d['saved_item_keys']));[t(s)for s in d['saved_item_keys']];u(d['saved_item_default_doses'],int(d['saved_item_default_equipped']))
    u(len(d['assignments']))
    for r in d['assignments']:
        u(r['kind'],r['coercion'],r['fallback_kind'],r['constant_index'])
        for k in('member','key','key_y','fallback_member','constant'):t(r[k])
        val(r['fallback'])
    u(len(d['inventories']))
    for r in d['inventories']:
        u(r['id'],r['role']);t(r['member']);t(r['source_key']);t(r['method']);u(len(r['items']))
        for x in r['items']:t(x['name']);u(int(x['equipped']),x['doses'],int(x['has_uid']),x['uid'])
    u(len(d['characters']))
    for r in d['characters']:u(r['id'],r['role']);t(r['name']);val(r['saved'])
    u(len(d['playable_names']));[t(s)for s in d['playable_names']]
    u(len(d['party']))
    for r in d['party']:u(r['id'],int(r['playable']));t(r['name'])
    u(len(d['flags']))
    for n,v in d['flags']:t(n);u(int(v))
    u(len(d['steps']))
    for r in d['steps']:u(r['kind'],r['index']);t(r['owner']);t(r['method']);t(r['member'])
    u(len(d['preconditions']))
    for r in d['preconditions']:u(r['kind']);t(r['source']);t(r['method']);t(r['member'])
    struct.pack_into('<8s8I',b,0,b'ENCGLD01',1,128,len(b),0,FAMILY,1,1,d['scene_id']);b[40:60]=bytes.fromhex(PIN);b[60:92]=bytes.fromhex(d['source_sha256']);b[92:124]=bytes.fromhex(sha(IR));struct.pack_into('<I',b,20,zlib.crc32(b[128:]));return bytes(b)


def main():
    p=argparse.ArgumentParser();p.add_argument('action',choices=['prepare','extract','compile','verify']);p.add_argument('--directory',type=Path);p.add_argument('--native',type=Path);a=p.parse_args()
    if a.action=='prepare':require(a.directory,'Explicit private source export directory required');prepare(a.directory);return
    if a.action=='extract':require(a.native,'Explicit native source save receipt required');extract(a.native);return
    d=load();b=encode(d)
    if a.action=='compile':PACK.parent.mkdir(parents=True,exist_ok=True);PACK.write_bytes(b)
    else:require(PACK.read_bytes()==b,'Stale global LOAD binary')
    print('Original cold LOAD:',len(d['assignments']),'assignments;',sum(len(x['items'])for x in d['inventories']),'KEY/STORAGE Items;',len(d['characters']),'characters;',len(d['steps']),'source cursors;',len(b),'bytes')
if __name__=='__main__':
    try:main()
    except(ValueError,KeyError,TypeError,OSError,struct.error,StopIteration,IndexError)as e:sys.exit('GLOBAL LOAD ERROR: '+str(e))
