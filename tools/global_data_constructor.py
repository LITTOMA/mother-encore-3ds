#!/usr/bin/env python3
"""Original globalData declaration values and constructor/Ready ownership.

The isolated exporter evaluates only copied pure declarations. No original
globalData/Character/Inventory constructor, autoload or scene is executed.
Actual object construction, flags, Directory/cache loops and GodStorage remain
the runtime owners' work. This pack contains no save_default character values.
"""
from __future__ import annotations
import argparse, copy, hashlib, re, struct, sys, zlib
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from tools.podunk_scene import PIN, read, write, sha, require, stable
from tools.extract_battle_entry import Extractor
from tools.field_global_data import defaults
from tools.global_yaml_caches import value, ENGINE
OWNER = 'Scripts/global/globalData.gd'
FAMILY = 0x454e0053
IR = ROOT/'content/native-global-data-constructor.json'
RECEIPT = ROOT/'reports/global-constructor/native-values.json'
REVIEW = ROOT/'reports/global-constructor/source-review.json'
ENGINE_REVIEW = ROOT/'reports/global-constructor/engine-source.json'
PACK = ROOT/'romfs/data/global.encconstructor'
EXPORTER = ROOT/'tools/godot_exporter/global_data_constructor.gd'


def inputs():
    ex = Extractor(ROOT)
    paths = [OWNER, 'Scripts/global/Character.gd', 'Scripts/global/PartyMember.gd',
             'Scripts/global/PartyNPC.gd', 'Scripts/global/Inventory.gd',
             'Scripts/global/uiManager.gd']
    source = {p: ex.text(p) for p in paths}
    body = source[OWNER]
    require(body.startswith('extends Node\n'), 'Unknown globalData native class')
    require(not re.search(r'^func (_enter_tree|_exit_tree|_process|_physics_process|_notification)\(',body,re.M),
            'Unknown globalData native lifecycle callback')
    pm, npc, inv = [source[p] for p in paths[2:5]]
    require('func _init(data := {}):\n\tif data:\n\t\tinit_from_dict(data)' in pm,
            'Unknown empty PartyMember constructor')
    require('func _init(data := {}, constant_data := {}):\n\tinit_from_dict(data)\n\tinit_from_dict(constant_data)' in npc and
            'func init_from_dict(dict: Dictionary):\n\tif dict:' in npc,
            'Unknown empty PartyNPC constructor')
    require(not re.search(r'^func _init\(', source[paths[1]], re.M), 'Character constructor changed')
    require('func _init(type:= InvType.NORMAL, load_data:= []):\n\t_type = type\n\tif _type == InvType.STORAGE_GOD:\n\t\t_init_god_storage()\n\tif load_data:\n\t\tinit_from_serialized(load_data)' in inv,
            'Unknown Inventory constructor')
    legacy = read(ROOT/'content/native-global-data.json')
    caches = read(ROOT/'content/native-global-yaml-caches.json')
    require(legacy['commit'] == caches['commit'] == PIN and legacy['owner'] == caches['owner'] == OWNER,
            'Changed constructor dependencies')
    enum = re.search(r'enum InvType \{([^}]+)\}', inv)[1].replace(' ', '').split(',')
    charblock = body.split('var characters := {\n', 1)[1].split('\n}', 1)[0]
    chars = re.findall(r'\t(PartyMember|PartyNPC)\.(\w+): (PartyMember|PartyNPC)\.new\(\),?', charblock)
    require(len(chars) == 8 and not re.sub(r'\t(?:PartyMember|PartyNPC)\.\w+: (?:PartyMember|PartyNPC)\.new\(\),?|\s', '', charblock), 'Unknown source characters map')
    objects, refs = [], []
    for cls, key, construct in chars:
        require(cls == construct, 'Mismatched empty Character construction')
        script = 'Scripts/global/'+cls+'.gd'
        name = re.search(r'^const '+key+r' := "([^"]+)"$', source[script], re.M)[1]
        fields = defaults(source[paths[1]].split('func ', 1)[0])+defaults(source[script].split('func ', 1)[0])
        getters = []
        if cls == 'PartyMember':
            prop = re.search(r'^var (\w+): Inventory setget ,(\w+)$', pm, re.M)
            require(prop and 'func '+prop[2]+'() -> Inventory:\n\treturn _inventory' in pm, 'Unknown Inventory public getter')
            fields.append(dict(name=prop[1], kind=5, value=''))
            getters.append([prop[1], prop[2]])
        row = dict(id=stable(OWNER+'#characters.'+name), kind=1, role=0 if cls=='PartyMember' else 1,
                   name=name, native='Object', script=script, fields=fields, getters=getters)
        objects.append(row); refs.append([name,row['id']])
    for member, role in re.findall(r'^var (\w+) := Inventory.new\(Inventory.InvType\.(\w+)\)$', body, re.M):
        require(role in ('KEY','STORAGE'), 'Unexpected eager Inventory')
        objects.append(dict(id=stable(OWNER+'#'+member), kind=2, role=enum.index(role), name=member,
                            native='Reference', script=paths[4], getters=[],
                            fields=[dict(name='_type',kind=2,value=enum.index(role)),dict(name='_items',kind=3,value='')]))
    require(len(objects)==len(legacy['declarations']), 'Incomplete native declaration objects')
    for actual, old in zip(objects, legacy['declarations']):
        require(all(actual[k]==old[k] for k in ('id','kind','role','name','native','script')), 'Legacy constructor identity differs')
        require(actual['fields'][:len(old['fields'])]==old['fields'], 'Legacy native defaults differ')
    top = body.split('# TO REMOVE',1)[0]
    top = re.sub(r'var characters := \{\n.*?\n\}', 'var characters := @characters', top, flags=re.S)
    enum_lines = re.findall(r'^enum [^\n]+',top,re.M)
    require(len(enum_lines)==2 and enum_lines[1]=='enum {KEYBOARD, GAMEPAD}', 'Unknown enum declarations')
    rows = [dict(name='BtnStyles', type_hint='', setter='', getter='', constant=True, expression='BtnStyles',adapter=0,owner_role=0),
            dict(name='KEYBOARD',type_hint='',setter='',getter='',constant=True,expression='KEYBOARD',adapter=0,owner_role=0),
            dict(name='GAMEPAD',type_hint='',setter='',getter='',constant=True,expression='GAMEPAD',adapter=0,owner_role=0)]
    declarations = re.findall(r'^(const|var) (\w+)([^\n]*)$',top,re.M)
    cache_roles = {p['member']:p['role'] for p in caches['policies']}
    for decl,name,tail in declarations:
        tail = tail.split('#',1)[0].strip()
        access = tail.split(' setget ',1); tail=access[0]
        setter=getter=''
        if len(access)==2:
            pair=access[1].split(',');require(len(pair)==2,'Unknown setget syntax');setter,getter=[x.strip()for x in pair]
        match = re.fullmatch(r'(?::\s*(\w+))?\s*(?::?=\s*(.*))?',tail)
        require(match,'Unknown declaration expression '+name)
        hint,expr=match[1]or'',match[2]or'null'
        adapter=1 if expr=='@characters' else 2 if expr.startswith('Inventory.new(') else 3 if hint=='Inventory' else 4 if name in cache_roles else 5 if name in ('flags','object_flags','seen_dialogue_flags') else 6 if name=='keys' else 0
        owner_role=cache_roles[name] if adapter==4 else {'flags':0,'object_flags':1,'seen_dialogue_flags':2}.get(name,0)
        row=dict(name=name,type_hint=hint,setter=setter,getter=getter,constant=decl=='const',expression=expr,adapter=adapter,owner_role=owner_role)
        if adapter==1:row.update(kind=9,references=refs)
        elif adapter==2:row.update(kind=8,reference_id=next(x['id']for x in objects if x['name']==name))
        rows.append(row)
    require(len({r['name']for r in rows})==len(rows),'Duplicate declaration names')
    require(not re.sub(r'^extends Node\n|^enum [^\n]+\n|^(?:const|var) [^\n]+\n|^\s*#.*\n|\s+','',top,flags=re.M),'Unclassified declaration source')
    init=body.split('func _init():',1)[1].split('func _ready():',1)[0]
    expected='\n\t_init_flags()\n\t\n\tvar to_load := {\n'+',\n'.join('\t\t"'+p['name']+'": '+p['member']for p in caches['policies'])+'\n\t}\n\tfor key in to_load:\n\t\t_load_data("res://Data/%s/" % key, to_load[key])\n\n'
    require(init==expected,'Unknown source constructor operation/order')
    ready=body.split('func _ready():',1)[1].split('func _init_flags():',1)[0]
    require(ready=='\n\tgod_storage = Inventory.new(Inventory.InvType.STORAGE_GOD)\n\n','Unknown source Ready body')
    speed=re.search(r'func (_set_text_speed)\(value: float\) -> void:\n(.*?)\nfunc (_get_text_speed)\(\) -> float:\n\treturn (\w+)\n',body,re.S)
    require(speed,'Missing text speed body')
    member=speed[4];part=speed[2]
    policy=re.fullmatch(r'\tif value <= ([0-9.]+):\n\t\t'+member+r' = (\w+)\[\2.size\(\) / (\d+)\]\n\t\treturn\n\t# Finding the closest text speed\n\tvar closest := ([0-9.]+)\n\tvar smallest_delta := ([0-9.]+)\n\tfor s in \2:\n\t\tif abs\(s - value\) < smallest_delta:\n\t\t\tsmallest_delta = abs\(s - value\)\n\t\t\tclosest = s\n\t'+member+r' = closest\n',part)
    require(policy,'Unknown text speed executable formula')
    speed_policy=dict(member=member,constant_name=policy[2],setter=speed[1],getter=speed[3],threshold=float(policy[1]),fallback_divisor=int(policy[3]),closest_initial=float(policy[4]),delta_initial=float(policy[5]),comparison=1)
    menu=re.search(r'set_menu_flavors\(globaldata\.(\w+)\)',source[paths[5]])
    require(menu and any(r['name']==menu[1]and r['expression']=='FLAVORS[0]'for r in rows),'Unknown source menu-flavor binding')
    steps=[dict(kind=1,role=0,method='_init_flags',member='',argument='')]
    steps += [dict(kind=2,role=p['role'],method='_load_data',member=p['member'],argument='res://'+p['directory'])for p in caches['policies']]
    ready_steps=[dict(kind=3,role=enum.index('STORAGE_GOD'),method='_init_god_storage',member='god_storage',argument='')]
    return ex, source, rows, enum_lines, objects, steps, ready_steps, speed_policy, menu[1], legacy,caches


def pure_source(rows,enums):
    declarations=[]
    for row in rows:
        if row['name']in ('BtnStyles','KEYBOARD','GAMEPAD'):continue
        if row['adapter']in (1,2,3):continue
        kind='const' if row['constant']else'var'
        hint=(': '+row['type_hint'])if row['type_hint']else''
        declarations.append(kind+' '+row['name']+hint+' = '+row['expression'])
    names=[r['name']for r in rows if r['adapter']not in (1,2,3)]
    pure='extends Reference\n'+'\n'.join(enums+declarations)+'\nfunc declaration_values():\n\treturn ['+','.join('['+'"'+n+'", '+n+']'for n in names)+']\n'
    require('.new('not in pure and 'preload('not in pure and 'load('not in pure and '_init('not in pure and '_ready('not in pure,'Original lifecycle entered data exporter')
    return pure, names


def prepare(directory):
    ex,source,rows,enums,*_=inputs()
    directory.mkdir(parents=True,exist_ok=True)
    (directory/'project.godot').write_text('config_version=4\n[application]\nconfig/name="Original declaration data export"\n[logging]\nfile_logging/enable_logging=false\n')
    pure,names=pure_source(rows,enums)
    (directory/'pure.gd').write_bytes(pure.encode('utf-8'))
    (directory/'export.gd').write_bytes(EXPORTER.read_bytes())
    write(directory/'manifest.json',dict(sources=ex.sources,pure_sha256=sha(directory/'pure.gd')))
    print('Pure original declaration conversion:',len(names),'values; no game constructor/Ready')


def engine_review(directory):
    files={'scene/main/node.cpp':'node.cpp','modules/gdscript/gdscript_compiler.cpp':'gdscript_compiler.cpp',
           'modules/gdscript/gdscript.cpp':'gdscript.cpp'}
    bodies={path:(directory/file).read_text(encoding='utf-8')for path,file in files.items()}
    node=bodies['scene/main/node.cpp'];compiler=bodies['modules/gdscript/gdscript_compiler.cpp']
    require('data.ready_notified = true;'in node and 'notification(NOTIFICATION_POST_ENTER_TREE);\n\n\tif (data.ready_first) {\n\t\tdata.ready_first = false;\n\t\tnotification(NOTIFICATION_READY);\n\t\temit_signal(SceneStringNames::get_singleton()->ready);'in node,
            'Unknown official native Ready lifecycle')
    require('get_script_instance()->call_multilevel_reversed(SceneStringNames::get_singleton()->_ready, nullptr, 0);'in node,
            'Unknown official script Ready dispatch')
    require('void Node::request_ready() {\n\tdata.ready_first = true;\n}'in node,'Unknown request_ready rearm')
    exit_body=node.split('void Node::_propagate_exit_tree()',1)[1].split('\nvoid Node::',1)[0]
    require('ready_first'not in exit_body,'Exit re-arms script Ready')
    require('return idx | (GDScriptFunction::ADDR_TYPE_MEMBER << GDScriptFunction::ADDR_BITS);'in compiler and
            'Error err = _parse_block(codegen, p_class->initializer, stack_level);'in compiler,
            'Unknown direct member constructor assignment')
    proofs=[]
    for path,file in files.items():
        text=bodies[path]
        needle='void Node::_propagate_ready()'if path.endswith('node.cpp')else 'Parse initializer'if path.endswith('compiler.cpp')else 'instance->members.resize'
        line=next((i+1 for i,x in enumerate(text.splitlines())if needle in x),None)
        require(line,'Missing reviewed official engine range')
        proofs.append(dict(source=path,sha256=sha(directory/file),url='https://github.com/godotengine/godot/blob/'+ENGINE+'/'+path,
                           first_line=line,excerpt='\n'.join(text.splitlines()[line-1:line+35])))
    write(ENGINE_REVIEW,dict(schema=1,engine_commit=ENGINE,files=proofs,
         native_ready=dict(post_enter_each_entry=True,ready_notified_before_children=True,ready_body_and_signal_first_only=True,
                           exit_rearms_ready=False,request_ready_rearms=True),
         source_globaldata_callbacks_absent=['_enter_tree','_exit_tree','_process','_physics_process','_notification'],
         constructor_assignment='GDScript class initializer targets ADDR_TYPE_MEMBER directly; local direct accesses bypass external setget',
         behaviour_probe_executed=False))


def derive(native):
    ex,source,rows,enums,objects,steps,ready,speed,menu,legacy,caches=inputs()
    receipt=read(native)
    engine=read(ENGINE_REVIEW)
    require(engine['schema']==1 and engine['engine_commit']==ENGINE and engine['behaviour_probe_executed']is False,
            'Unknown constructor official engine review')
    require(set(receipt)=={'schema','engine','source_scene_entered','original_constructor_entered','pure_sha256','values'},'Unknown pure declaration receipt')
    require(receipt['schema']==1 and receipt['source_scene_entered']is False and receipt['original_constructor_entered']is False,'Original constructor cannot enter data exporter')
    require(all(receipt['engine'].get(k)==v for k,v in dict(major=3,minor=6,patch=2,status='stable',build='official',hash=ENGINE).items()),'Unknown declaration conversion engine')
    require(receipt['pure_sha256']==hashlib.sha256(pure_source(rows,enums)[0].encode()).hexdigest(),'Changed copied pure source declaration expressions')
    values={p[0]:p[1]for p in receipt['values']};require(len(values)==len(receipt['values']),'Duplicate native declaration receipt')
    require(set(values)=={r['name']for r in rows if r['adapter']not in (1,2,3)},'Incomplete native declaration values')
    for row in rows:
        if row['adapter']in (1,2):continue
        if row['adapter']==3:row.update(kind=0,value={'kind':0});continue
        v=values[row['name']]
        if v.get('kind')==7:
            require(set(v)=={'kind','coordinates'}and len(v['coordinates'])==2,'Unknown source Vector2')
            for x in v['coordinates']:value(dict(kind=3,f64_le=x))
            row.update(kind=7,vector=v['coordinates'])
        else:row.update(kind=value(v)['kind'],value=v)
    speed['speeds']=[v['f64_le']for v in values[speed['constant_name']]['values']]
    require(values[speed['constant_name']]['kind']==5 and all(v['kind']==3 for v in values[speed['constant_name']]['values']),'Native text speed type differs')
    d=dict(schema=1,kind='encore.global-data-constructor.source-ir',commit=PIN,family=FAMILY,owner=OWNER,
           source_sha256=ex.sources[OWNER],scene_id=stable('global-constructor:'+OWNER),sources=ex.sources,
           declarations=rows,objects=objects,constructor_steps=steps,ready_steps=ready,text_speed=speed,menu_flavor_member=menu,
           legacy_ir_sha256=sha(ROOT/'content/native-global-data.json'),cache_ir_sha256=sha(ROOT/'content/native-global-yaml-caches.json'),
           engine_review_sha256=sha(ENGINE_REVIEW),
           native_receipt_sha256=sha(native),source_functions=['PartyMember.new({}) skips conditional initializer','PartyNPC.new({}, {}) calls both empty guarded initializers','Inventory.new(KEY/STORAGE,[]) sets type only','_init_flags then actual six Directory loops, closure owner required','_ready creates actual STORAGE_GOD Inventory after complete Items cache'],
           gameplay_methods_admitted=False)
    return d


def extract(native,engine_directory):
    engine_review(engine_directory)
    d=derive(native);write(RECEIPT,read(native));d['native_receipt_sha256']=sha(RECEIPT)
    write(IR,d);write(REVIEW,dict(schema=1,commit=PIN,ir_sha256=sha(IR),native_receipt_sha256=sha(RECEIPT),exporter_sha256=sha(EXPORTER),sources=d['sources'],capability=1,gameplay_methods_admitted=False))


def load():
    d=read(IR);review=read(REVIEW)
    require(d==derive(RECEIPT),'Changed original constructor source/values/dependencies')
    require(review==dict(schema=1,commit=PIN,ir_sha256=sha(IR),native_receipt_sha256=sha(RECEIPT),exporter_sha256=sha(EXPORTER),sources=d['sources'],capability=1,gameplay_methods_admitted=False),'Stale constructor source review')
    return d


def encode(d):
    b=bytearray(128)
    def u(*x):b.extend(struct.pack('<'+'I'*len(x),*x))
    def t(s):v=s.encode();u(len(v));b.extend(v)
    def f(x):b.extend(struct.pack('<d',x))
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
    t(d['owner']);b.extend(bytes.fromhex(d['legacy_ir_sha256']));b.extend(bytes.fromhex(d['cache_ir_sha256']));b.extend(bytes.fromhex(d['engine_review_sha256']));t(d['menu_flavor_member'])
    u(len(d['sources']))
    for p,h in d['sources'].items():t(p);b.extend(bytes.fromhex(h))
    u(len(d['objects']))
    for o in d['objects']:
        u(o['id'],o['kind'],o['role']);t(o['name']);t(o['native']);t(o['script']);u(len(o['fields']))
        for field in o['fields']:
            t(field['name']);u(field['kind']);t(field['value']if isinstance(field['value'],str)else'');b.extend(struct.pack('<q',field['value']if type(field['value'])is int else 0))
        u(len(o['getters']))
        for n,g in o['getters']:t(n);t(g)
    u(len(d['declarations']))
    for r in d['declarations']:
        for key in ('name','type_hint','setter','getter'):t(r[key])
        u(int(r['constant']),r['kind'],r['adapter'],r['owner_role'],r.get('reference_id',0))
        if r['kind']<=6:val(r['value'])
        elif r['kind']==7:
            for v in r['vector']:b.extend(bytes.fromhex(v))
        elif r['kind']==9:
            u(len(r['references']))
            for n,i in r['references']:t(n);u(i)
    for key in ('constructor_steps','ready_steps'):
        u(len(d[key]))
        for s in d[key]:u(s['kind'],s['role']);t(s['method']);t(s['member']);t(s['argument'])
    s=d['text_speed']
    for key in ('member','constant_name','setter','getter'):t(s[key])
    u(s['fallback_divisor'],s['comparison']);f(s['threshold']);f(s['closest_initial']);f(s['delta_initial']);u(len(s['speeds']))
    for v in s['speeds']:b.extend(bytes.fromhex(v))
    struct.pack_into('<8s8I',b,0,b'ENCGDC01',1,128,len(b),0,FAMILY,1,1,d['scene_id'])
    b[40:60]=bytes.fromhex(PIN);b[60:92]=bytes.fromhex(d['source_sha256']);b[92:124]=bytes.fromhex(sha(IR));struct.pack_into('<I',b,20,zlib.crc32(b[128:]));return bytes(b)


def main():
    p=argparse.ArgumentParser();p.add_argument('action',choices=['prepare','extract','compile','verify']);p.add_argument('--directory',type=Path);p.add_argument('--native',type=Path);p.add_argument('--engine-directory',type=Path);a=p.parse_args()
    if a.action=='prepare':require(a.directory,'Explicit private export path required');prepare(a.directory);return
    if a.action=='extract':require(a.native and a.engine_directory,'Explicit native values receipt and official engine directory required');extract(a.native,a.engine_directory);return
    d=load();b=encode(d)
    if a.action=='compile':PACK.parent.mkdir(parents=True,exist_ok=True);PACK.write_bytes(b)
    else:require(PACK.read_bytes()==b,'Stale constructor resource')
    print('Original constructor:',len(d['declarations']),'declarations;',len(d['objects']),'empty objects;',len(b),'bytes; actual owner lifecycle required')
if __name__=='__main__':
    try:main()
    except(ValueError,KeyError,TypeError,OSError,struct.error,StopIteration,IndexError)as e:sys.exit('GLOBAL CONSTRUCTOR ERROR: '+str(e))
