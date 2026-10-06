#!/usr/bin/env python3
"""Original global Node2D fields and source scene constructor, not Ready."""
from pathlib import Path
import argparse, hashlib, json, re, struct, sys, zlib
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from tools.extract_battle_entry import Extractor
from tools.podunk_scene import PIN, read, write, require, sha, decode
from tools.scene_reference import quarantine
from tools.field_ui_manager_recipes import extract_recipe, encode_recipe
IR = ROOT / 'content/native-field-global-constructor.json'
REVIEW = ROOT / 'reports/field-global-constructor/source-review.json'
PACK = ROOT / 'romfs/data/global.encnodeconstructor'
SOURCE = 'Scripts/global/global.gd'
SCENE = 'Scripts/global/global.tscn'
TRANSITION = 'Scripts/global/SceneTransition.gd'
FAMILY = 0x454e0055

def stable(path):
    return int.from_bytes(hashlib.sha256(('global-constructor:' + path).encode()).digest()[:4], 'little')

def prepare(out):
    out = Path(out).resolve()
    require(out.is_relative_to((ROOT / 'build').resolve()) and not out.exists(), 'Fresh source constructor reference under build required')
    ex = Extractor(ROOT)
    todo = [SCENE]
    files, attachments, connections = {}, [], []
    payloads = {}
    while todo:
        name = todo.pop()
        if name in files:
            continue
        raw = ex.data(name)
        record = dict(sha256=ex.sources[name], bytes=len(raw))
        if name.endswith('.tscn'):
            raw, refs, scripts, signals = quarantine(raw, name)
            record.update(external_resources=refs, reference_sha256=hashlib.sha256(raw).hexdigest())
            attachments.extend(scripts)
            connections.extend(signals)
            todo.extend(r['path'] for r in refs)
            payloads[name] = raw
        else:
            require(name.endswith('.gd'), 'Unknown global constructor source codec')
            record['quarantined'] = 'Source constructors/Ready are not executed by native conversion'
        files[name] = record
    out.mkdir(parents=True)
    for name, raw in payloads.items():
        p = out / name
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_bytes(raw)
    (out / 'project.godot').write_bytes(b'config_version=4\n[application]\nconfig/name="Source global constructor data"\n[logging]\nfile_logging/enable_logging=false\n')
    for tool in ('scene_data.gd', 'field_ui_manager.gd'):
        (out / tool).write_bytes((ROOT / 'tools/godot_exporter' / tool).read_bytes())
    write(out / 'source.json', dict(schema=1, commit=PIN, scene=SCENE, files=files,
                                  script_attachments=attachments, signal_connections=connections, case_aliases={}, native_compatible=False))

def literal(source):
    source = source.strip()
    if source == 'Vector2.ZERO':
        return dict(kind=7, value=[0, 0])
    if source in ('null', 'false', 'true'):
        return dict(kind={'null':0, 'false':1, 'true':1}[source], value={'null':None, 'false':False, 'true':True}[source])
    value = json.loads(source)
    if type(value) is list:
        require(all(type(x) is str for x in value), 'Unknown source constant Array')
        return dict(kind=5, value=value)
    if type(value) is int:
        return dict(kind=2, value=value)
    if type(value) is float:
        return dict(kind=3, value=value)
    require(type(value) is str, 'Unknown source declaration literal')
    return dict(kind=4, value=value)

def source_fields(source):
    rows = []
    for line in source.split('func ', 1)[0].splitlines():
        line = line.split('#', 1)[0].strip()
        if not line.startswith('var '):
            continue
        m = re.fullmatch(r'var (\w+)(?:\s*:\s*(\w+))?(?:\s*:?=\s*(.*))?', line)
        require(m, 'Unknown source field declaration ' + line)
        name, hint, expression = m.groups()
        if not expression:
            require(hint in (None, 'bool'), 'Unknown source native default ' + line)
            v = dict(kind=1, value=False) if hint == 'bool' else dict(kind=0, value=None)
        elif expression == 'SceneTransition.new()':
            v = dict(kind=8, value=TRANSITION)
        else:
            v = literal(expression)
        rows.append(dict(name=name, type_hint=hint or '', **v))
    return rows

def extract(native, tree, receipt):
    ex = Extractor(ROOT)
    body = ex.text(SOURCE)
    pm = ex.text('Scripts/global/PartyMember.gd')
    transition = ex.text(TRANSITION)
    require(body.startswith('extends Node2D\n') and 'func _init(' not in body, 'Unknown global superclass/_init')
    require(transition.startswith('extends Node\nclass_name SceneTransition\n') and not re.search(r'^var |^func _init\(|^func _ready\(', transition, re.M), 'Unknown SceneTransition constructor/Ready')
    declarations = source_fields(body)
    require(len(declarations) == 14, 'Global complete fourteen declarations changed')
    for role, row in enumerate(declarations, 1):
        row['role'] = role
    require([r['kind'] for r in declarations] == [5]*5+[0,0,8,0,0,1,1,1,4], 'Global source constructor types/order changed')
    constants = []
    for line in body.split('func ', 1)[0].splitlines():
        line = line.split('#', 1)[0].strip()
        if not line.startswith('const '):
            continue
        m = re.fullmatch(r'const (\w+) := (.*)', line)
        require(m, 'Unknown source constant')
        expression = m[2]
        if 'PartyMember.' in expression:
            names = re.fullmatch(r'\[(PartyMember\.\w+(?:, PartyMember\.\w+)*)\]', expression)
            require(names, 'Unknown source playable references')
            values=[]
            for key in re.findall(r'PartyMember\.(\w+)', expression):
                values.append(re.search(r'^const '+key+r' := "([^"]+)"$', pm, re.M)[1])
            v = dict(kind=5, value=values)
        else:
            v = literal(expression)
        constants.append(dict(name=m[1], **v))
    require(len(constants)==8, 'Global constant closure changed')
    recipe = extract_recipe(SCENE, native, tree, receipt)
    require([(r['node'],r['native_class']) for r in recipe['records']] == [('.', 'Node2D'), ('Playtimer','Timer'), ('Slowmo','Node'), ('MouseHider','Node')], 'Global complete native constructor subtree changed')
    require(recipe['records'][0]['pause']==2 and all(not r['native_generated']for r in recipe['records']), 'Unknown global native property order')
    defaults=[]
    for row in recipe['records'][1:]:
        if row['script']:
            text = ex.text(row['script'])
            require('func _init(' not in text, 'Unknown global child constructor')
            defaults.append(dict(id=row['id'], script=row['script'], fields=source_fields(text)))
    original = read(native)
    timer=decode(next(n['properties']for n in original['nodes']if n['path']=='Playtimer'))
    require(timer['process_mode']in (0,1) and type(timer['one_shot'])is bool and type(timer['autostart'])is bool and timer['wait_time']>0, 'Unknown source Playtimer properties')
    steps=[x.strip()for x in body.split('func _ready():',1)[1].split('\nfunc ',1)[0].splitlines()if x.strip()]
    require(steps==['_set_localized_default_inputs()', '_load_settings()', 'partySpace.resize(255)', 'add_child(scene_transition)', '_init_player()', '_load_default_save()'], 'Unknown global Ready source cursor')
    source_map=dict(recipe['sources']);source_map.update(ex.sources)
    transition_node=dict(recipe['records'][2]);transition_node.update(id=stable(TRANSITION), node='.', name='', parent=0, owner=0, canvas_parent=0,index=-1,ready=0,script=TRANSITION,script_sha=source_map[TRANSITION],script_methods=0)
    document=dict(schema=1, format=1, capability=1, rules=1, family=FAMILY, commit=PIN,
                  owner=SOURCE, scene=SCENE, source_sha256=source_map[SOURCE], scene_id=stable(SOURCE),
                  declarations=declarations, constants=constants, signals=re.findall(r'^signal (\w+)$',body,re.M),
                  child_defaults=defaults, recipe=recipe,
                  timer=dict(id=recipe['records'][1]['id'], scene=recipe['scene_id'], scene_sha=recipe['source_sha256'],
                             mode=timer['process_mode'], wait=float(timer['wait_time']), flags=int(timer['one_shot'])|int(timer['autostart'])<<1, script_sha='0'*64),
                  transition=dict(source=TRANSITION, id=stable(TRANSITION), native='Node', name='', script_methods=0,descriptor=transition_node),
                  ready_steps=steps, sources=source_map, scene_admitted=False,
                  pending=['First Ready _set_localized_default_inputs and following _load_settings/_init_player require actual independent owners',
                           'Slowmo and MouseHider source Ready/process are retained, not constructor admission of their later actions'])
    write(IR,document)
    write(REVIEW,dict(schema=1, commit=PIN, ir_sha256=sha(IR), sources=source_map,
                     scope='Actual global declaration/constant source closure and four-node native PackedScene constructor; SceneTransition.new blank native Node',
                     native_sha256=sha(native), tree_sha256=sha(tree), source_receipt_sha256=sha(receipt), scene_admitted=False))

def load():
    d=read(IR);r=read(REVIEW);require(d['schema']==d['format']==d['capability']==d['rules']==1 and d['family']==FAMILY and d['commit']==PIN and d['scene_admitted']is False and r['ir_sha256']==sha(IR), 'Global constructor review/version changed')
    ex=Extractor(ROOT)
    for name,h in d['sources'].items():
        ex.data(name);require(ex.sources[name]==h,'Changed global constructor source '+name)
    return d

def encode(d):
    b=bytearray(128)
    def u(*v):b.extend(struct.pack('<'+'I'*len(v),*v))
    def t(value):raw=value.encode();u(len(raw));b.extend(raw)
    def fields(rows,roles=False):
        u(len(rows))
        for row in rows:
            if roles:u(row['role'])
            t(row['name']);t(row.get('type_hint',''));u(row['kind'])
            value=row['value']
            if row['kind']==1:u(int(value))
            elif row['kind']==2:b.extend(struct.pack('<q',value))
            elif row['kind']==3:b.extend(struct.pack('<d',value))
            elif row['kind']in (4,8):t(value)
            elif row['kind']==5:u(len(value));[t(x)for x in value]
            elif row['kind']==7:b.extend(struct.pack('<2d',*value))
            else:require(row['kind']==0 and value is None,'Unknown field opcode')
    t(d['owner']);t(d['scene']);fields(d['declarations'],True);fields(d['constants'])
    u(len(d['signals']));[t(x)for x in d['signals']]
    u(len(d['child_defaults']))
    for row in d['child_defaults']:u(row['id']);t(row['script']);fields(row['fields'])
    t(d['transition']['source']);u(d['transition']['id']);t(d['transition']['native']);t(d['transition']['name']);u(d['transition']['script_methods'])
    row=d['transition']['descriptor'];u(*[row[k]for k in ['class_index','ready','pause','flags','light_mask']]);b.extend(struct.pack('<3i',row['index'],row['priority'],row['z']));b.extend(struct.pack('<20f',*[v for a in row['local']for v in a],*[v for a in row['world']for v in a],*row['modulate'],*row['self_modulate']));t(row['node']);u(len(row['groups']));[t(v)for v in row['groups']]
    u(len(d['ready_steps']));[t(x)for x in d['ready_steps']]
    u(len(d['sources']))
    for name,h in d['sources'].items():t(name);b.extend(bytes.fromhex(h))
    recipe=encode_recipe(d['recipe']);u(len(recipe));b.extend(recipe)
    timer=d['timer'];payload=bytes.fromhex(PIN)+struct.pack('<I4If32s32s',1,timer['id'],timer['scene'],timer['mode'],timer['flags'],timer['wait'],bytes.fromhex(timer['scene_sha']),bytes.fromhex(timer['script_sha']))
    packed=struct.pack('<8s6I',b'ENCFNT01',1,32+len(payload),zlib.crc32(payload),0x454e0044,1,1)+payload
    u(len(packed));b.extend(packed)
    struct.pack_into('<8s8I',b,0,b'ENCGCON1',1,128,len(b),zlib.crc32(b[128:]),FAMILY,1,1,d['scene_id'])
    b[40:60]=bytes.fromhex(PIN);b[60:92]=bytes.fromhex(d['source_sha256']);b[92:124]=bytes.fromhex(sha(IR));return bytes(b)

def stage_files(source):
    raw=encode(load());require((Path(source)/'data/global.encnodeconstructor').read_bytes()==raw,'Staged global node constructor differs');return {Path('data/global.encnodeconstructor'):raw}

def main():
    p=argparse.ArgumentParser();p.add_argument('action',choices=['prepare','extract','compile','verify']);[p.add_argument('--'+name,type=Path)for name in ['out','native','tree','source']];a=p.parse_args()
    if a.action=='prepare':prepare(a.out);return
    if a.action=='extract':extract(a.native,a.tree,a.source);return
    raw=encode(load())
    if a.action=='compile':PACK.parent.mkdir(parents=True,exist_ok=True);PACK.write_bytes(raw)
    else:require(PACK.read_bytes()==raw,'Stale global constructor')
    print('Actual global constructor:',len(raw),'bytes; source Ready remains gated')
if __name__=='__main__':
    try:main()
    except(ValueError,KeyError,TypeError,OSError,struct.error)as error:sys.exit('GLOBAL CONSTRUCTOR ERROR: '+str(error))
