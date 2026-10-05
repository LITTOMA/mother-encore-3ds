#!/usr/bin/env python3
"""Pinned Bedside Drawer frontend and independent ENCDRP01 binary compiler.

JSON is an offline review/source IR. Item grant, conditional flow, sound and
flag writes remain executable binary instructions, never flattened into text.
"""
from __future__ import annotations
import argparse, csv, hashlib, io, json, re, struct, sys, zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from tools.extract_battle_entry import Extractor, PIN, require, node

IR='content/native-drawer-program.json'
REVIEW='reports/drawer-program/source-review.json'
PACK='romfs/data/opening.encdrawer'
PATH="Data/Dialogue/Podunk/non-person text/Ninten's house/Bedside Drawer.yaml"
TABLE="Translations/TranslatedText/dialogue_Podunk_non-person text_Ninten's house - sheet.csv"
SCENE='Maps/podunk/Nintens House.tscn'
SOUND='Audio/Sound effects/Item Received.mp3'
ITEM='Data/Items/AsthmaSpray.yaml'
HEADER=128
NAMES=('Strings','Templates','Commands','Binding')
STRIDES=(1,16,20,8)
OPCODES={'ShowText':1,'AwaitText':2,'BranchFlag':3,'BranchSpace':4,'GrantItem':5,
         'PlaySound':6,'SetFlag':7,'Jump':8,'End':9}
KEY='DIALOGUE_PODUNK_NON-PERSON_TEXT_NINTEN_S_HOUSE_BEDSIDE_DRAWER_'

def fields(value,keys,label):
    require(isinstance(value,dict) and set(value)==set(keys),'Unknown/missing '+label+' fields')
def digest(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def canonical(value):return json.dumps(value,sort_keys=True,ensure_ascii=False,separators=(',',':'),allow_nan=False)
def read_json(path): return json.loads(Path(path).read_text(encoding='utf-8'))
def write_json(path,value):
    path=Path(path);path.parent.mkdir(parents=True,exist_ok=True)
    path.write_bytes((json.dumps(value,ensure_ascii=False,indent=2)+'\n').encode('utf-8'))
def safe_path(value):
    return isinstance(value,str) and bool(value) and not value.startswith('/') and ':' not in value and '\\' not in value and all(ord(c)>=32 and ord(c)!=127 for c in value) and all(p not in ('','.','..') for p in value.split('/'))

def validate_document(doc):
    expected={'0':dict(text=KEY+'0', **{'if':[dict(flags={'got_asthma_spray':True},goto='4'),dict(invspace=False,goto='3')]},goto='2'),
              '1':dict(text=KEY+'1',goto='2'),
              '2':dict(text=KEY+'2',item='AsthmaSpray',soundeffect='Item Received.mp3',setflags='got_asthma_spray'),
              '3':dict(text=KEY+'3'),'4':dict(text=KEY+'4')}
    require(canonical(doc)==canonical(expected),'Unreviewed Drawer phrase fields/ordered conditional graph')

def text_segments(raw):
    # Receiver mapping is admitted only by the singleton-party and ownership
    # assertions in build(). Other receiver article/case mechanisms fail closed.
    from tools.link_house_inspections import text_segments as parse_text
    require(isinstance(raw,str),'Drawer text type')
    return parse_text(raw.replace('[ItemReceiver]','[PartyLead]'))

def commands_for(doc,texts):
    validate_document(doc)
    ids={row['label']:row['id'] for row in texts}
    require(set(ids)==set(doc) and len(ids)==len(texts),'Drawer text label identity')
    commands=[];labels={};fixups=[]
    def emit(opcode,a=0,b=0,c=0,d=0):
        commands.append(dict(opcode=opcode,a=a,b=b,c=c,d=d));return len(commands)-1
    for label,phrase in doc.items():
        labels[label]=len(commands)
        if 'item' in phrase: emit('GrantItem',0)
        emit('ShowText',ids[label])
        if 'soundeffect' in phrase: emit('PlaySound',SOUND)
        if 'setflags' in phrase: emit('SetFlag',phrase['setflags'],1)
        emit('AwaitText')
        for condition in phrase.get('if',[]):
            if 'flags' in condition:
                flag,value=next(iter(condition['flags'].items()))
                fixups.append((emit('BranchFlag',flag,int(value)),'c',condition['goto']))
            else:
                fixups.append((emit('BranchSpace',int(condition['invspace'])),'b',condition['goto']))
        if 'goto' in phrase: fixups.append((emit('Jump'),'a',phrase['goto']))
        else: emit('End')
    for pc,field,label in fixups: commands[pc][field]=labels[label]
    return commands,labels

def build(root=ROOT):
    ex=Extractor(root);doc=ex.yaml(PATH);validate_document(doc)
    item=ex.yaml(ITEM)
    require(item['doses']==3 and item['keyitem'] is False,'Drawer grant template changed')
    require(ex.yaml('Data/save_new_game.yaml')['party']==['ninten'],'Drawer receiver mapping requires singleton Ninten party')
    inventory=ex.text('Scripts/global/Inventory.gd');item_script=ex.text('Scripts/global/Item.gd')
    dialogue=ex.text('Scripts/UI/DialogueBox.gd');text=ex.text('Scripts/global/text_tools.gd')
    ex.text('Scripts/global/PartyMember.gd');ex.text('Scripts/global/globalData.gd')
    require('const _MAX_INVENTORY_SIZE := 16' in inventory and 'for inv in _get_all_inventories(false, false):' in inventory and 'if !inv.is_full():' in inventory,'Drawer normal-party inventory space policy changed')
    require('for member in global.party:\n\t\t\tvar inv = member.inv' in inventory and 'return inv.add_item_by_name(item_name)' in inventory and 'func add_item_by_name(item_name: String) -> Item:' in inventory and 'var item := Item.new(item_name)' in inventory,'Drawer recipient grant path changed')
    require('static func get_item_owner(item: Item, default_as_leader := false)' in inventory and 'item in chara.inv.get_items()' in inventory,'Drawer receiver ownership policy changed')
    require('self.doses = get_data().get("doses", 1)' in item_script and 'equipped := false, doses := -1' in item_script,'Drawer granted item defaults changed')
    require('var receiver: PartyMember = Inventory.get_item_owner(global.item, true)' in text and '_cut_custom_name(receiver.get_nickname(), tag_content) if receiver else ""' in text and 'global.party[0].get_nickname()' in text,'Drawer dynamic receiver nickname mapping changed')
    item_pos=dialogue.index('if _curr_phrase.has("item"):');text_pos=dialogue.index('if _curr_phrase.has("text"):');sound_pos=dialogue.index('if _curr_phrase.has("soundeffect"):');flag_pos=dialogue.index('if _curr_phrase.has("setflags"):')
    require(item_pos<text_pos<sound_pos<flag_pos and 'global.item = Inventory.add_item_available(_curr_phrase["item"])' in dialogue and '_change_flags(_curr_phrase["setflags"], true)' in dialogue,'Drawer source effect ordering changed')
    next_body=dialogue[dialogue.index('func _next_phrase('):dialogue.index('func _handle_gotos(')]
    require('for curr_if in all_ifs:' in next_body and '_handle_gotos(curr_if, with_sound)\n\t\t\t\treturn' in next_body and 'Inventory.has_inventory_space() != curr_if["invspace"]' in next_body and next_body.index('if _curr_phrase.has("if"):')<next_body.index('if _curr_phrase.has("redirect") or _curr_phrase.has("goto"):'),'Drawer after-acknowledgement first-match branching changed')
    interact=ex.text('Scripts/Main/Interact Dialog.gd');ex.text('Nodes/Reusables/interact_dialog.tscn')
    obj=node(ex.text(SCENE).replace("\\'","'"),'Objects/interact_dialog6')
    # Binding is selected from the pinned scene, not an invented native address.
    require(obj['dialog']==PATH.removeprefix('Data/Dialogue/').removesuffix('.yaml') and obj['_all_dialog']==[['poltergeist','Reusable/nottime'],['doll_defeated',obj['dialog']]],'Drawer scene override binding changed')
    require('ret = flags[1]' in interact and 'for flags in _all_dialog:' in interact,'Drawer ordered inspection override changed')
    ex.data(SOUND);ex.data(SOUND+'.import')
    table={}
    for row in csv.DictReader(io.StringIO(ex.text(TABLE))):
        require(row['key'] not in table,'Duplicate Drawer translation key');table[row['key']]=row
    texts=[]
    for label,phrase in doc.items():
        row=table[phrase['text']]
        for language in ('en','zh_CN'):text_segments(row[language])
        texts.append(dict(id=62+len(texts),source_path=PATH,label=label,translation_key=phrase['text'],raw=row['en']))
    commands,labels=commands_for(doc,texts)
    return dict(schema=1,kind='encore.drawer-program.source-ir',commit=PIN,
                scope='Complete original Bedside Drawer phrases0..4; singleton Ninten grant/receiver only; no item-use mechanism',
                sources=dict(sorted(ex.sources.items())),source_path=PATH,inspection_source='Objects/interact_dialog6',
                document=doc,texts=texts,templates=[dict(id=1,source_item='AsthmaSpray',doses=item['doses'],key_item=item['keyitem'])],
                entry=0,phrase_targets=labels,commands=commands)

def load(root=ROOT):
    root=Path(root);ir=read_json(root/IR)
    require(canonical(ir)==canonical(build(root)),'Stale/unreviewed Drawer source IR')
    review=read_json(root/REVIEW)
    fields(review,('schema','commit','ir_sha256','sources','scope','semantics','unsupported','unverified'),'Drawer review')
    require(review['schema']==1 and review['commit']==PIN and review['ir_sha256']==digest(root/IR) and review['sources']==ir['sources'] and review['scope']==ir['scope'],'Drawer source review mismatch')
    return ir

def lower(ir,house):
    fields(ir,('schema','kind','commit','scope','sources','source_path','inspection_source','document','texts','templates','entry','phrase_targets','commands'),'Drawer IR')
    require(type(ir['schema']) is int and ir['schema']==1 and ir['kind']=='encore.drawer-program.source-ir' and ir['commit']==PIN and house['commit']==PIN and type(ir['entry']) is int and ir['entry']==0,'Drawer schema/source pin/entry')
    expected,labels=commands_for(ir['document'],ir['texts'])
    require(canonical(ir['commands'])==canonical(expected) and canonical(ir['phrase_targets'])==canonical(labels),'Drawer compiled source graph mismatch')
    require(ir['source_path']==PATH and ir['inspection_source']=='Objects/interact_dialog6','Drawer source object identity')
    require([r['id'] for r in ir['texts']]==list(range(62,67)) and [r['label'] for r in ir['texts']]==list(ir['document']),'Drawer stable text IDs/order')
    require(canonical(ir['templates'])==canonical([dict(id=1,source_item='AsthmaSpray',doses=3,key_item=False)]),'Drawer source grant template mismatch')
    pool=bytearray(b'\0');offsets={'':0}
    def string(value):
        require(isinstance(value,str) and '\0' not in value and len(value.encode())<=4096,'Invalid Drawer string')
        if value not in offsets:offsets[value]=len(pool);pool.extend(value.encode()+b'\0')
        return offsets[value]
    templates=[]
    for row in ir['templates']:
        fields(row,('id','source_item','doses','key_item'),'Drawer template')
        require(type(row['key_item']) is bool,'Drawer key-item boolean')
        templates.append([row['id'],string(row['source_item']),row['doses'],int(row['key_item'])])
    dialogue={row['id']:row for row in house['dialogues']}
    require(len(dialogue)==len(house['dialogues']),'Duplicate House text identity')
    for row in ir['texts']:
        fields(row,('id','source_path','label','translation_key','raw'),'Drawer text')
        require(row['id'] in dialogue and dialogue[row['id']]['source_path']==ir['source_path']==row['source_path'],'Missing/mismatched House Drawer text')
    commands=[]
    for row in ir['commands']:
        fields(row,('opcode','a','b','c','d'),'Drawer command')
        require(row['opcode'] in OPCODES,'Unknown Drawer opcode')
        operands=[row[k] for k in 'abcd']
        if row['opcode'] in ('BranchFlag','SetFlag','PlaySound'):operands[0]=string(operands[0])
        commands.append([OPCODES[row['opcode']],*operands])
    binding=[[string(ir['source_path']),string(ir['inspection_source'])]]
    result=dict(Strings=bytes(pool),Templates=templates,Commands=commands,Binding=binding)
    parse_pack(encode(result));return result

def encode(tables):
    fields(tables,NAMES,'Drawer binary tables');data=bytearray(HEADER)
    for i,name in enumerate(NAMES):
        block=tables[name] if i==0 else b''.join(struct.pack('<'+{1:'4I',2:'5I',3:'2I'}[i],*row) for row in tables[name])
        if block:
            while len(data)%4:data.append(0)
        struct.pack_into('<HHIII',data,64+16*i,i+1,STRIDES[i],len(data) if block else 0,len(block)//STRIDES[i],len(block));data.extend(block)
    struct.pack_into('<8s6I20s12x',data,0,b'ENCDRP01',1,len(data),0,4,1,1,bytes.fromhex(PIN))
    struct.pack_into('<I',data,16,zlib.crc32(data));return bytes(data)

def parse_pack(blob):
    require(HEADER<=len(blob)<=1024*1024,'Drawer size')
    magic,version,size,crc,count,caps,rules,pin=struct.unpack_from('<8s6I20s',blob)
    require(magic==b'ENCDRP01' and version==caps==rules==1 and size==len(blob) and count==4 and pin==bytes.fromhex(PIN) and not any(blob[52:64]),'Drawer header')
    check=bytearray(blob);struct.pack_into('<I',check,16,0);require(zlib.crc32(check)==crc,'Drawer CRC')
    tables={};end=HEADER
    for i,name in enumerate(NAMES):
        kind,stride,offset,number,amount=struct.unpack_from('<HHIII',blob,64+16*i)
        require(kind==i+1 and stride==STRIDES[i] and amount==number*stride,'Drawer directory')
        if number:
            require(offset%4==0 and offset>=end and offset+amount<=len(blob) and not any(blob[end:offset]),'Drawer span')
            block=blob[offset:offset+amount];end=offset+amount
        else:require(offset==amount==0,'Drawer empty span');block=b''
        tables[name]=block if i==0 else list(struct.iter_unpack('<'+{1:'4I',2:'5I',3:'2I'}[i],block))
    require(end==len(blob),'Drawer trailing data')
    pool=tables['Strings'];require(pool and len(pool)<=65536 and pool[0]==pool[-1]==0,'Drawer strings')
    starts={0};starts.update(i+1 for i,v in enumerate(pool[:-1]) if v==0)
    decoded={offset:pool[offset:pool.index(0,offset)].decode('utf-8') for offset in starts}
    require(all(len(value.encode('utf-8'))<=4096 for value in decoded.values()),'Drawer string length')
    def string(offset):require(offset in decoded and decoded[offset],'Drawer string reference');return decoded[offset]
    templates=tables['Templates'];commands=tables['Commands']
    require(len(tables['Binding'])==1,'Drawer singleton binding')
    source,inspection=tables['Binding'][0]
    require(safe_path(string(source)) and string(source).startswith('Data/Dialogue/') and string(source).endswith('.yaml') and safe_path(string(inspection)),'Drawer source/inspection binding')
    require(len(templates)<=64 and 0<len(commands)<=1024,'Drawer table counts')
    ids=set();identities=set()
    for identity,path,doses,key in templates:
        require(identity>0 and identity not in ids and doses>0 and doses<=65535 and key<=1,'Drawer template policy');ids.add(identity)
        source=string(path);require(safe_path(source) and '/' not in source and source not in identities,'Drawer item identity');identities.add(source)
    edges=[]
    for pc,(op,a,b,c,d) in enumerate(commands):
        require(op in OPCODES.values() and d==0,'Drawer opcode/reserved')
        edge=[]
        if op==1:require(a>0 and b==c==0,'Drawer text operands')
        elif op in (2,9):require(a==b==c==0,'Drawer gate/end operands')
        elif op==3:require(string(a) and b<=1 and c<len(commands),'Drawer flag branch');edge.append(c)
        elif op==4:require(a<=1 and b<len(commands) and c==0,'Drawer space branch');edge.append(b)
        elif op==5:require(a<len(templates) and b==c==0,'Drawer item template reference')
        elif op==6:require(safe_path(string(a)) and string(a).startswith('Audio/') and b==c==0,'Drawer sound reference')
        elif op==7:require(string(a) and b<=1 and c==0,'Drawer flag assignment')
        elif op==8:require(a<len(commands) and b==c==0,'Drawer jump');edge.append(a)
        if op not in (8,9):require(pc+1<len(commands),'Drawer fallthrough');edge.append(pc+1)
        edges.append(edge)
    # Every command, including dormant phrase1, must terminate. Validate all
    # components; unknown graphs cannot hide behind an unreachable branch.
    degree=[0]*len(commands)
    for edge in edges:
        for target in edge:degree[target]+=1
    ready=[pc for pc,d in enumerate(degree) if d==0];processed=0
    while ready:
        pc=ready.pop();processed+=1
        for target in edges[pc]:
            degree[target]-=1
            if degree[target]==0:ready.append(target)
    require(processed==len(commands),'Drawer cyclic control flow')
    require(any(row[0]==9 for row in commands),'Drawer missing termination')
    # Validate gate discipline from every component entry. Dormant phrase1 is
    # legal, but grant/sound/flag operations must not erase unacknowledged text.
    incoming={target for edge in edges for target in edge};seen=set()
    queue=[(pc,False) for pc in {0}|(set(range(len(commands)))-incoming)]
    while queue:
        pc,pending=queue.pop()
        if (pc,pending) in seen:continue
        seen.add((pc,pending));op=commands[pc][0]
        if op==1:require(not pending,'Drawer unacknowledged text replaced');pending=True
        elif op==2:require(pending,'Drawer gate without text');pending=False
        elif op==9:require(not pending,'Drawer unacknowledged termination')
        for target in edges[pc]:queue.append((target,pending))
    return tables

def stage_files(source):
    ir=load(ROOT);blob=(Path(source)/'data/opening.encdrawer').read_bytes()
    parse_pack(blob);require(blob==encode(lower(ir,read_json(ROOT/'content/native-house.json'))),'Staged Drawer binary differs from reviewed bindings')
    return {Path('data/opening.encdrawer'):blob}

def main():
    p=argparse.ArgumentParser();p.add_argument('action',choices=('extract','compile','verify'));args=p.parse_args()
    try:
        if args.action=='extract':
            ir=build();write_json(ROOT/IR,ir)
            write_json(ROOT/REVIEW,dict(schema=1,commit=PIN,ir_sha256=digest(ROOT/IR),sources=ir['sources'],scope=ir['scope'],semantics={
                'branching':'Phrase0 text acknowledgement precedes first-match got flag, then inventory-full, then goto2',
                'effects':'Phrase2 grant before text, sound then flag before acknowledgement',
                'receiver':'Granted item owner nickname equals party leader only for admitted singleton Ninten party',
                'dormant_phrase':'Source phrase1→2 retained; no invented reachability',
                'identity':'Existing text/flag/item identities retained; native UID execution belongs to the inventory consumer'},
                unsupported=['Other parties, receiver articles/cases','Item consume/use/storage mechanisms'],unverified=['Manual test suites','Emulator','Hardware','Audio audibility']))
        else:
            ir=load();tables=lower(ir,read_json(ROOT/'content/native-house.json'));blob=encode(tables)
            if args.action=='compile':
                (ROOT/PACK).write_bytes(blob);write_json(ROOT/'reports/drawer-program/compile.json',dict(schema=1,commit=PIN,bytes=len(blob),sha256=hashlib.sha256(blob).hexdigest(),ir_sha256=digest(ROOT/IR),house_ir_sha256=digest(ROOT/'content/native-house.json'),commands=len(tables['Commands'])))
            else:require((ROOT/PACK).read_bytes()==blob,'Stale Drawer binary')
            print('Drawer programme:',len(blob),'bytes; checked ENCDRP01')
    except (OSError,ValueError,KeyError,TypeError,struct.error,OverflowError,RecursionError) as error:
        print('DRAWER PROGRAM ERROR:',error,file=sys.stderr);return 1
    return 0
if __name__=='__main__':raise SystemExit(main())
