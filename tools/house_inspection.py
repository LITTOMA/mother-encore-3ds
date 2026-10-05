#!/usr/bin/env python3
"""Audited house inspection source adapter and checked ENCHIN01 compiler.

JSON is offline source/IR only. Runtime loads the independent binary bindings;
dialogue text stays in the existing checked House resource. Drawer item logic is
retained as an explicit unsupported capability, never flattened into prose.
"""
from __future__ import annotations
import argparse, csv, hashlib, io, json, math, re, struct, sys, zlib
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT)); sys.path.insert(0, str(ROOT / 'tools'))
from tools.extract_battle_entry import Extractor, PIN, node, one, properties, require

IR = 'content/native-house-inspections.json'
PACK = 'romfs/data/opening.encinspect'
REPORT = 'reports/house-inspections/source-review.json'
HOUSE = 'Maps/podunk/Nintens House.tscn'
SCENE = 'Nodes/Reusables/interact_dialog.tscn'
SCRIPT = 'Scripts/Main/Interact Dialog.gd'
PROMPT = 'Scripts/UI/Button Prompt.gd'
PARTY = 'Scripts/Main/party/party_object.gd'
PLAYER = 'Scripts/Main/party/Player.gd'
GLOBAL = 'Scripts/global/globalData.gd'
TABLE = "Translations/TranslatedText/dialogue_Podunk_non-person text_Ninten's house - sheet.csv"
REUSABLE = 'Translations/TranslatedText/dialogue_Reusable - sheet.csv'
DRAWER = "Data/Dialogue/Podunk/non-person text/Ninten's house/Bedside Drawer.yaml"
NONE = 0xffffffff
NAMES = ('Strings', 'Objects', 'Overrides')
FORMATS = (None, '<11I8f', '<4I')
STRIDES = (1, 76, 16)
HEADER = 112

def digest(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def read_json(path): return json.loads(Path(path).read_text(encoding='utf-8'))
def write_json(path, value):
    path = Path(path); path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes((json.dumps(value, indent=2, ensure_ascii=False) + '\n').encode('utf-8'))
def safe_path(value):
    return isinstance(value, str) and bool(value) and not value.startswith('/') and ':' not in value and '\\' not in value and all(p not in ('', '.', '..') for p in value.split('/'))
def fields(value, keys, label): require(isinstance(value, dict) and set(value) == set(keys), 'Unknown/missing ' + label + ' fields')
def f32(value): return struct.unpack('<f', struct.pack('<f', value))[0]

def build(root=ROOT):
    root = Path(root); ex = Extractor(root)
    house = ex.text(HOUSE).replace("\\'", "'")
    scene = ex.text(SCENE); script = ex.text(SCRIPT)
    prompt = ex.text(PROMPT); party = ex.text(PARTY); player = ex.text(PLAYER); global_data = ex.text(GLOBAL)
    ex.text('Nodes/Ui/ButtonPrompt.tscn'); ex.text('Nodes/Reusables/Player.tscn')
    ex.text('Scripts/global/text_tools.gd'); ex.text('Scripts/UI/DialogueBox.gd')
    require(ex.yaml('Data/save_new_game.yaml')['party']==['ninten'],'Inspection dynamic-name mapping requires singleton Ninten party')
    require('seen_dialogue_flags' not in script and 'ret = flags[1]' in script and 'for flags in _all_dialog:' in script, 'Inspection ordered override/seen policy changed')
    require('queue_free()' in script and 'check_appear_disappear_flags(appear_flag, disappear_flag)' in script, 'Inspection lifetime policy changed')
    require('position.x = position.x / get_parent().scale.x' in prompt and 'position.y = position.y / get_parent().scale.y' in prompt, 'Inspection prompt scale cancellation changed')
    require('if not target_turn_constraints.x and not target_turn_constraints.y:' in party, 'Inspection turn admission changed')
    turn_body = one(r'^func _turn_to\(.*?\n(.*?)(?=^export|^func)', party, 'party turn', re.M | re.S)[1]
    require('_direction = which_direction' in turn_body and 'axis_constraints.' not in turn_body, 'Inspection turn vector policy changed')
    require('if (abs(which_direction.x) > abs(which_direction.y) or !axis_constraints.y) and which_direction.x != 0 and axis_constraints.x:' in player and 'elif axis_constraints.y and which_direction.y != 0:' in player, 'Player cardinal turn override changed')
    require('var collide = eventRayCaster.get_collider()' in player and 'if "interact" in collide.name:' in player, 'Inspection ray selection changed')
    require('flags.get(appear_flag, false)' in global_data and '!flags.get(disappear_flag, false)' in global_data, 'Inspection flag visibility changed')
    root_props = node(scene, '.'); shape_node = node(scene, 'CollisionShape2D')
    shape = properties(one(r'^\[sub_resource type="RectangleShape2D" id=3\]\n(.*?)(?=^\[)', scene, 'inspection shape', re.M | re.S)[1])['extents']
    require(shape == [4, 4] and root_props['player_turn'] == {'x': True, 'y': True}, 'Inspection inherited defaults changed')
    require('path="res://'+SCENE+'" type="PackedScene" id=14' in house,'Inspection inherited scene binding changed')
    require(not any(k in node(house, p) for p in ('.', 'Objects') for k in ('position', 'scale', 'rotation')), 'Inspection ancestor transforms changed')
    matches = re.findall(r'^\[node name="(interact_dialog[0-9]*)" parent="Objects" instance=ExtResource\( 14 \)\]', house, re.M)
    require(len(matches) == 8 and len(set(matches)) == 8, 'Inspection instance set changed')
    objects = []; paths = []
    for name in matches:
        path = 'Objects/' + name; n = node(house, path)
        require(set(n) <= {'position','scale','dialog','_all_dialog','player_turn','button_offset','appear_flag','disappear_flag'}, 'Unreviewed inspection override')
        require(not n.get('appear_flag') and not n.get('disappear_flag'), 'Inspection flag lifetime requires new capability review')
        pos = n['position']; scale = n.get('scale', [1, 1]); turn = n.get('player_turn', root_props['player_turn'])
        require(set(turn) == {'x','y'} and all(type(v) is bool for v in turn.values()), 'Invalid inspection turn flags')
        center_local = shape_node.get('position', [0, 0])
        default = 'Data/Dialogue/' + n['dialog'] + '.yaml'; paths.append(default)
        overrides = []
        for row in n.get('_all_dialog', []):
            require(len(row) == 2 and all(isinstance(v,str) and v for v in row), 'Unknown inspection override form')
            dialogue = 'Data/Dialogue/' + row[1] + '.yaml'; paths.append(dialogue)
            overrides.append(dict(flag=row[0], dialogue_path=dialogue, supported=dialogue != DRAWER))
        objects.append(dict(id=len(objects)+1, source_path=path, position=[f32(v) for v in pos],
            interact_center=[f32(pos[i]+scale[i]*center_local[i]) for i in range(2)],
            interact_extents=[f32(abs(scale[i])*shape[i]) for i in range(2)],
            prompt_offset=[f32(v) for v in n.get('button_offset', [0,0])],
            player_turn=int(turn['x']) | (int(turn['y']) << 1), collision_mask=root_props.get('collision_layer',1),
            appear_flag=n.get('appear_flag',''), disappear_flag=n.get('disappear_flag',''),
            seen_key='', default_dialogue=default, default_supported=default != DRAWER, overrides=overrides))
    tables = {}
    for path in (TABLE, REUSABLE):
        for row in csv.DictReader(io.StringIO(ex.text(path))):
            require(row['key'] not in tables, 'Duplicate inspection translation key'); tables[row['key']] = row['en']
    texts = []; unsupported = []
    for path in dict.fromkeys(paths):
        doc = ex.yaml(path)
        if path == DRAWER:
            require(set(doc) == {'0','1','2','3','4'} and doc['0']['if'] == [{'flags':{'got_asthma_spray':True},'goto':'4'}, {'invspace':False,'goto':'3'}] and doc['0']['goto'] == '2' and doc['2']['item'] == 'AsthmaSpray' and doc['2']['setflags'] == 'got_asthma_spray', 'Drawer capability boundary source changed')
            unsupported.append(dict(source_path=path, source_program=doc, reason='Requires inventory-space branching, item grant, receiver token, item sound and persisted got_asthma_spray flag'))
            continue
        require(set(doc) == {'0'} and set(doc['0']) == {'text'}, 'Unknown inspection dialogue command: ' + path)
        raw = tables[doc['0']['text']]
        # Strict token lowering; singleton initial party proves PartyLead=Ninten.
        from tools.link_house_inspections import text_segments
        text_segments(raw)
        texts.append(dict(source_path=path, label='0', translation_key=doc['0']['text'], raw=raw))
    return dict(schema=1, kind='encore.house-inspections.source-ir', commit=PIN,
        scope='Eight original InteractDialog objects; seven complete literal inspections; Drawer item path explicitly unsupported',
        sources=ex.sources, objects=objects, texts=texts, unsupported=unsupported)

def load(root=ROOT):
    root = Path(root); ir = read_json(root / IR)
    require(ir == build(root), 'Stale/unreviewed inspection IR')
    review = read_json(root / REPORT)
    require(review['commit'] == PIN and review['ir_sha256'] == digest(root / IR) and review['sources'] == ir['sources'], 'Inspection source review mismatch')
    return ir

def lower(ir, house):
    fields(ir, ('schema','kind','commit','scope','sources','objects','texts','unsupported'), 'inspection IR')
    require(ir['schema'] == 1 and ir['commit'] == PIN and ir['kind'] == 'encore.house-inspections.source-ir', 'Inspection schema/pin')
    require(house['commit'] == PIN, 'Inspection House pin')
    dialogue_map = {}
    for index, row in enumerate(house['dialogues']):
        # Programme phrases legitimately share a YAML path. Inspections admit
        # only a single literal phrase and require uniqueness for their own path.
        dialogue_map.setdefault(row['source_path'], []).append(index)
    pool = bytearray(b'\0'); offsets = {'':0}
    def string(value):
        require(isinstance(value,str) and '\0' not in value and len(value.encode()) <= 4096, 'Invalid inspection string')
        if value not in offsets: offsets[value] = len(pool); pool.extend(value.encode()+b'\0')
        return offsets[value]
    def dialogue(path, supported):
        require(type(supported) is bool and safe_path(path), 'Invalid inspection dialogue binding')
        if not supported:
            require(path not in dialogue_map, 'Unsupported inspection path accidentally flattened'); return NONE
        require(path in dialogue_map, 'Missing linked House inspection dialogue: ' + path)
        require(len(dialogue_map[path])==1, 'Ambiguous House inspection dialogue path')
        return dialogue_map[path][0]
    rows = []; overrides = []
    for index, obj in enumerate(ir['objects']):
        fields(obj, ('id','source_path','position','interact_center','interact_extents','prompt_offset','player_turn','collision_mask','appear_flag','disappear_flag','seen_key','default_dialogue','default_supported','overrides'), 'inspection object')
        require(obj['seen_key'] == '' and not obj['appear_flag'] and not obj['disappear_flag'], 'Unknown inspection seen/lifetime capability')
        first = len(overrides)
        for override in obj['overrides']:
            fields(override, ('flag','dialogue_path','supported'), 'inspection override')
            overrides.append([index,string(override['flag']),string(override['dialogue_path']),dialogue(override['dialogue_path'],override['supported'])])
        coordinates = []
        for name in ('position','interact_center','interact_extents','prompt_offset'):
            v = obj[name]; require(isinstance(v,list) and len(v)==2 and all(type(x) in (int,float) and math.isfinite(x) for x in v), 'Invalid inspection coordinates'); coordinates.extend(v)
        rows.append([obj['id'],string(obj['source_path']),string(obj['default_dialogue']),string(obj['appear_flag']),string(obj['disappear_flag']),obj['player_turn'],first,len(overrides)-first,obj['collision_mask'],string(obj['seen_key']),dialogue(obj['default_dialogue'],obj['default_supported']),*coordinates])
    return dict(Strings=bytes(pool), Objects=rows, Overrides=overrides)

def encode(tables):
    data = bytearray(HEADER)
    for i,name in enumerate(NAMES):
        block = tables[name] if i==0 else b''.join(struct.pack(FORMATS[i],*row) for row in tables[name])
        if block:
            while len(data)%4: data.append(0)
        offset=len(data) if block else 0
        struct.pack_into('<HHIII',data,64+16*i,i+1,STRIDES[i],offset,len(block)//STRIDES[i],len(block)); data.extend(block)
    struct.pack_into('<8s6I20s12x',data,0,b'ENCHIN01',1,len(data),0,3,1,1,bytes.fromhex(PIN))
    struct.pack_into('<I',data,16,zlib.crc32(data)); return bytes(data)

def parse_pack(blob):
    require(HEADER <= len(blob) <= 1024*1024, 'Inspection size')
    magic,version,size,crc,count,caps,rules,pin = struct.unpack_from('<8s6I20s',blob)
    require(magic==b'ENCHIN01' and version==caps==rules==1 and count==3 and size==len(blob) and pin==bytes.fromhex(PIN) and not any(blob[52:64]), 'Inspection header')
    check=bytearray(blob); struct.pack_into('<I',check,16,0); require(zlib.crc32(check)==crc,'Inspection CRC')
    tables={}; end=HEADER
    for i,name in enumerate(NAMES):
        kind,stride,offset,number,amount=struct.unpack_from('<HHIII',blob,64+16*i)
        require(kind==i+1 and stride==STRIDES[i] and amount==number*stride, 'Inspection directory')
        if number:
            require(offset%4==0 and offset>=end and offset+amount<=len(blob) and not any(blob[end:offset]),'Inspection span')
            block=blob[offset:offset+amount]; end=offset+amount
        else: require(offset==amount==0,'Inspection empty section'); block=b''
        tables[name]=block if i==0 else list(struct.iter_unpack(FORMATS[i],block))
    require(end==len(blob),'Inspection trailing data')
    pool=tables['Strings']; require(pool and pool[0]==pool[-1]==0, 'Inspection string pool')
    starts={0}; starts.update(i+1 for i,v in enumerate(pool[:-1]) if v==0)
    def string(offset):
        require(offset in starts,'Inspection string start'); return pool[offset:pool.index(0,offset)].decode('utf-8')
    ids=set(); paths=set(); owned=set()
    require(0<len(tables['Objects'])<=256 and len(tables['Overrides'])<=1024,'Inspection object/override count')
    for index,row in enumerate(tables['Objects']):
        require(row[0]>0 and row[0] not in ids and safe_path(string(row[1])) and string(row[1]) not in paths, 'Inspection identity'); ids.add(row[0]); paths.add(string(row[1]))
        require(safe_path(string(row[2])) and not string(row[3]) and not string(row[4]) and row[5]&~3==0 and row[8]==1 and row[9]==0 and (row[10]==NONE or row[10]<1024),'Inspection policy')
        require(row[6]+row[7]<=len(tables['Overrides']) and all(math.isfinite(x) and abs(x)<=10000 for x in row[11:]) and all(x>0 for x in row[15:17]),'Inspection geometry/span')
        flags=set()
        for j in range(row[6],row[6]+row[7]):
            override=tables['Overrides'][j]
            require(j not in owned and override[0]==index and string(override[1]) and string(override[1]) not in flags and safe_path(string(override[2])) and (override[3]==NONE or override[3]<1024),'Inspection override ownership'); owned.add(j); flags.add(string(override[1]))
    require(len(owned)==len(tables['Overrides']),'Inspection orphan override')
    return tables

def stage_files(source):
    source=Path(source)
    ir=load(ROOT)
    expected=encode(lower(ir,read_json(ROOT/'content/native-house.json')))
    blob=(source/'data/opening.encinspect').read_bytes()
    parse_pack(blob)
    require(blob==expected,'Staged inspection binary differs from reviewed source/House bindings')
    return {Path('data/opening.encinspect'):blob}

def main():
    parser=argparse.ArgumentParser(); parser.add_argument('action',choices=('extract','compile','verify')); args=parser.parse_args()
    try:
        if args.action=='extract':
            ir=build(); write_json(ROOT/IR,ir)
            write_json(ROOT/REPORT,dict(schema=1,commit=PIN,ir_sha256=digest(ROOT/IR),sources=ir['sources'],
                mechanisms=['Inherited Area2D 4x4 shape scaled into world coordinates','Last satisfied override wins; no source seen writes','ButtonPrompt inverse-scale keeps authored world offset','Player cardinal turn override respects each source axis constraint; both false retains direction','Drawer item program retained as explicit capability boundary']))
        else:
            ir=load(); tables=lower(ir,read_json(ROOT/'content/native-house.json')); blob=encode(tables); parse_pack(blob)
            if args.action=='compile':
                (ROOT/PACK).write_bytes(blob)
                write_json(ROOT/'reports/house-inspections/compile.json',dict(schema=1,commit=PIN,bytes=len(blob),sha256=hashlib.sha256(blob).hexdigest(),ir_sha256=digest(ROOT/IR),house_ir_sha256=digest(ROOT/'content/native-house.json'),objects=len(tables['Objects']),overrides=len(tables['Overrides'])))
            else: require((ROOT/PACK).read_bytes()==blob,'Inspection binary stale')
            print('House inspections:',len(blob),'bytes; checked ENCHIN01')
    except (OSError,ValueError,KeyError,TypeError,struct.error,OverflowError) as error:
        print('HOUSE INSPECTION ERROR:',error,file=sys.stderr); return 1
    return 0
if __name__=='__main__': raise SystemExit(main())
