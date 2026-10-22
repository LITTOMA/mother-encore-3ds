"""Owned v3 menu and actual original-consumption fixtures; never game saves."""
import copy
import hashlib
import json
import struct
import sys
import zlib
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from tools import catacombs_item_catalog as catalog, item_consumption as consume
from tools import field_menus, native_session


def fixture_files(reference_root):
    model=consume.load()
    source_files,_,proof=consume.original.source()
    inputs=json.loads(source_files['input.json'])
    first=Path(reference_root)
    document=consume.validate_reference(catalog.read(first/'reference.json'),inputs)
    receipt=catalog.read(first/'receipt.json')
    if receipt['reference_sha256']!=catalog.digest(first/'reference.json') or receipt['producer_sha256']!=catalog.digest(consume.original.__file__) or receipt['source']['generated_sha256']!=proof['generated_sha256']:
        raise ValueError('Changed actual original consumption receipt')
    base=catalog.read(ROOT/'content/native-field-menus.json')
    menu=copy.deepcopy(base)
    menu['schema']=3
    menu['consumption']=model
    catalog_data=catalog.load()
    for row in catalog_data['entries']:
        menu['items'].append(dict(id=row['source'],key=0,drop=1,action=2,doses=1,
            score=0,equipped_score=0,slot=0xffffffff,definition=row['id']-1,
            name={key:v['name']for key,v in row['labels'].items()},
            sorting={key:v['sorting_name']for key,v in row['labels'].items()},
            description=dict(en='',zh=''),result=dict(en='',zh='')))
    files={'menu-v3.encmenus':field_menus.encode(menu),
           'menu-v2.encmenus':field_menus.encode(base)}
    if files['menu-v2.encmenus']!=(ROOT/'romfs/ui/field.encmenus').read_bytes():
        raise ValueError('Production legacy menu bytes changed')
    raw=files['menu-v3.encmenus'];extension=consume.encode_extension(model)
    at=len(raw)-len(extension);locations={};effect_spans=[]
    def word(name=None):
        nonlocal at
        if name:locations[name]=at
        at+=4
    def string(name=None):
        nonlocal at
        if name:locations[name]=at
        size=struct.unpack_from('<I',raw,at)[0];at+=4+size
    string('actor');word('refresh');string('sound');string('bus');word('lane');word('statuses')
    for i,_ in enumerate(model['statuses']):string('status-%d'%i)
    word('effects')
    for i,row in enumerate(model['effects']):
        start=at;string('item-%d'%i);word('hp-%d'%i);string('cure-%d'%i)
        for m in range(4):string('key-%d-%d'%(i,m));string('en-%d-%d'%(i,m));string('zh-%d-%d'%(i,m))
        effect_spans.append((start,at))
    if at!=len(raw):raise ValueError('Consume fixture layout drift')
    bad_menus=[]
    def bad_menu(label,mutate):
        blob=bytearray(raw);mutate(blob);struct.pack_into('<I',blob,12,len(blob));struct.pack_into('<I',blob,16,zlib.crc32(blob[32:]))
        bad_menus.append(label);files['bad-menu-%d.encmenus'%(len(bad_menus)-1)]=bytes(blob)
    for name,value in [('refresh',0),('refresh',65536),('lane',0),('lane',2),('statuses',0),('statuses',17),('effects',0),('effects',129),('hp-0',0),('hp-0',65536)]:
        bad_menu(name+'='+str(value),lambda b,n=name,v=value:struct.pack_into('<I',b,locations[n],v))
    for name,value in [('actor',ord('?')),('sound',ord('/')),('bus',ord('?')),('status-0',ord('?')),('item-0',ord('?')),('cure-3',ord('?')),('key-0-0',ord('?'))]:
        bad_menu('invalid '+name,lambda b,n=name,v=value:b.__setitem__(locations[n]+4,v))
    for header,value in [(8,4),(20,2),(24,2),(28,1)]:
        bad_menu('header '+str(header),lambda b,n=header,v=value:struct.pack_into('<I',b,n,v))
    bad_menu('unknown value token',lambda b:b.__setitem__(locations['en-0-0']+6,ord('x')))
    def duplicate(b):
        first,last=effect_spans[1];source_first,source_last=effect_spans[0]
        b[first:last]=b[source_first:source_last]
    bad_menu('duplicate effect identity',duplicate)
    def missing(b):
        first,last=effect_spans[-1];del b[first:last];struct.pack_into('<I',b,locations['effects'],len(effect_spans)-1)
    bad_menu('missing effect coverage',missing)
    def unused(b):
        pos=locations['key-0-2'];struct.pack_into('<I',b,pos,1);b[pos+4:pos+4]=b'X'
    bad_menu('unused message has data',unused)
    bad_menu('trailing data',lambda b:b.extend(b'\0'))
    def legacy_extension(b):struct.pack_into('<I',b,8,2);struct.pack_into('<I',b,20,2)
    bad_menu('legacy v2 cannot discard extension',legacy_extension)
    def unknown_version(b):struct.pack_into('<I',b,8,4);struct.pack_into('<I',b,20,4)
    bad_menu('unknown matching version/capability',unknown_version)
    files['negative-menu-count.txt']=str(len(bad_menus)).encode()
    payload=bytearray(b'ENCCNSR1'+struct.pack('<2I',1,len(document['rows'])))
    offsets={}
    def number(value):payload.extend(struct.pack('<I',value))
    def real_integer(value):payload.extend(struct.pack('<q',value))
    def text(value):raw=value.encode('utf-8');number(len(raw));payload.extend(raw)
    def snapshot(state):
        real_integer(state['hp']);real_integer(state['pp'])
        offsets.setdefault('status_count',len(payload));number(len(state['status']))
        for status in state['status']:text(status['status']);real_integer(status.get('passive_healing_turns',0))
        offsets.setdefault('inventory_count',len(payload));number(len(state['inventory']))
        for item in state['inventory']:
            text(item['item_name']);number(item['uid']);real_integer(item['doses']);number(int(item['equipped']))
    for case,row in zip(inputs['cases'],document['rows']):
        offsets.setdefault('index',len(payload));number(row['index'])
        snapshot(row['before']);snapshot(row['after'])
        offsets.setdefault('selection',len(payload));number(1)
        number(case['maxhp']);number(case['maxpp'])
        action,value=next(iter(row['actions'].items()))
        offsets.setdefault('kind',len(payload));number({'0':1,'2':2,'5':3,'6':4}[action])
        number(int(value)if action in('0','2')else 0)
        number(row['after']['selected_doses'])
        sounds=[r for r in row['requests']if r[0]=='audio']
        dialogs=[r for r in row['requests']if r[0]=='dialog']
        number(int(bool(sounds)))
        messages=row['queued_messages']
        if len(messages)+len(dialogs)!=1 or len(sounds)>1:
            raise ValueError('Ambiguous original consume message/audio delivery')
        text(messages[0][0]if messages else dialogs[0][1])
        text(sounds[0][1][6:]if sounds else'');text(sounds[0][2]if sounds else'')
        number(len(row['signals']))
        for signal in row['signals']:
            offsets.setdefault('signal_kind',len(payload));number(1 if signal[0]=='stat'else 2)
            real_integer(signal[2]if signal[0]=='stat'else 0)
            real_integer(signal[3]if signal[0]=='stat'else 0)
    files['original.bin']=bytes(payload)
    negatives=[payload[:8],b'UNKNOWN!'+payload[8:],payload+b'\0']
    for at,value in [(8,2),(12,1025),(offsets['index'],999),(offsets['selection'],99),(offsets['kind'],99),(offsets['status_count'],17),(offsets['inventory_count'],17),(offsets['signal_kind'],99)]:
        changed=bytearray(payload);struct.pack_into('<I',changed,at,value);negatives.append(changed)
    for i,raw in enumerate(negatives):files['bad-reference-%d.bin'%i]=bytes(raw)
    files['negative-reference-count.txt']=str(len(negatives)).encode()
    return files,dict(reference_sha256=receipt['reference_sha256'],cases=len(document['rows']),
                      native_offsets=offsets,bad_menu_cases=bad_menus,menu_offsets=locations,
                      scope='Only original method outcomes; no real gameplay save, UI timing, PCM, emulator or hardware claim.')


def build(output,reference_root):
    output=Path(output).resolve()
    if not output.is_relative_to(ROOT/'build')or output==ROOT/'build' or output.is_symlink():
        raise ValueError('Owned consumption fixture below build required')
    files,proof=fixture_files(reference_root)
    marker='.owned-item-consumption-fixture'
    proof['files']={name:hashlib.sha256(raw).hexdigest()for name,raw in files.items()}
    files['manifest.json']=(json.dumps(proof,indent=2)+'\n').encode()
    if output.exists():
        if not(output/marker).is_file()or(output/marker).read_bytes()!=b'1\n'or{p.name for p in output.iterdir()}!=set(files)|{marker}or any((output/name).is_symlink()or not(output/name).is_file()or(output/name).read_bytes()!=raw for name,raw in files.items()):
            raise ValueError('Unowned or changed consumption fixture')
    else:
        output.mkdir(parents=True)
        (output/marker).write_bytes(b'1\n')
        for name,raw in files.items():(output/name).write_bytes(raw)
    return proof


if __name__=='__main__':
    print(json.dumps(build(sys.argv[1],sys.argv[2]),indent=2))
