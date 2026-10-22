"""Source-reviewed ENCFMN01 v3 consumption extension, not a generic item VM."""
from __future__ import annotations
import argparse
import csv
import io
import json
import re
import struct
import sys
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from tools import catacombs_item_catalog as catalog
from tools import run_item_consume_reference as original
from tools.extract_battle_entry import Extractor,PIN,require

IR=Path('content/native-item-consumption.json')
REVIEW=Path('compatibility/reviews/item-consumption-v0410.json')
REFERENCES=(Path('reports/item-consumption/original-1'),Path('reports/item-consumption/original-2'))


def fields(value,names,label):
    require(type(value)is dict and set(value)==set(names),'Unknown/missing consume '+label)


def identity(value):
    return type(value)is str and re.fullmatch('[A-Za-z_][A-Za-z_0-9-]{0,127}',value)is not None


def validate(model):
    fields(model,'actor refresh_hp sound bus lane statuses effects'.split(),'model fields')
    require(identity(model['actor'])and type(model['refresh_hp'])is int and 0<model['refresh_hp']<=65535
            and identity(model['bus'])and type(model['lane'])is int and model['lane']==1,
            'Consume actor/refresh/bus/lane rejected')
    sound=model['sound']
    require(type(sound)is str and sound and not sound.startswith('/')and not sound.endswith('/')
            and '..'not in sound and '//'not in sound and ':'not in sound and '\\'not in sound
            and len(sound.encode())<=4096 and all(ord(c)>=32 and ord(c)!=127 for c in sound),
            'Consume source sound path rejected')
    statuses=model['statuses']
    require(type(statuses)is list and 1<=len(statuses)<=16 and all(identity(v)for v in statuses)
            and len(set(statuses))==len(statuses),'Consume known statuses rejected')
    require(type(model['effects'])is list and 1<=len(model['effects'])<=128,'Consume effect coverage rejected')
    seen=set()
    for row in model['effects']:
        fields(row,'id hp cure message_keys messages'.split(),'effect fields')
        require(identity(row['id'])and row['id']not in seen,'Consume effect identity rejected')
        seen.add(row['id'])
        require(type(row['hp'])is int and 0<=row['hp']<=65535 and type(row['cure'])is str
                and bool(row['hp'])!=bool(row['cure'])and(not row['cure']or row['cure']in statuses),
                'Consume effect domain rejected')
        require(type(row['message_keys'])is list and type(row['messages'])is list
                and len(row['message_keys'])==len(row['messages'])==4,'Consume message coverage rejected')
        for index,(key,label)in enumerate(zip(row['message_keys'],row['messages'])):
            fields(label,('en','zh'),'message locales')
            used=index<2 if row['hp']else index>=2
            if used:
                require(identity(key),'Consume message identity rejected')
                for text in label.values():
                    require(type(text)is str and text and len(text.encode())<=4096
                            and '\0'not in text and '['not in text and ']'not in text,
                            'Consume message text rejected')
                    remainder=text.replace('{owner}','').replace('{value}','')
                    require('{'not in remainder and '}'not in remainder,'Unknown consume message token')
            else:
                require(key==''and label==dict(en='',zh=''),'Consume unowned message rejected')
    return model


def derive():
    files,copied,proof=original.source()
    source=json.loads(files['input.json'])
    ex=Extractor(ROOT)
    native=ex.yaml('Data/save_new_game.yaml')
    require(type(native['party'])is list and len(native['party'])==1 and identity(native['party'][0]),
            'Consume singleton source binding changed')
    party=original.source_methods(ex.data('Scripts/global/PartyMember.gd'))
    found=re.findall(r'set_hp\((\d+)\)',party['_refresh_hp_from_status'])
    require(len(found)==2 and found[0]=='0','Consume source status HP refresh changed')
    refresh=int(found[1])
    target=original.source_methods(ex.data('Nodes/Ui/Inventory/TargetCharaSelect.gd'))['consume_item']
    sounds=re.findall(r'audioManager.play_sfx\(load\("res://([^"\n]+)"\), "([a-z_]+)"\)',target)
    require(len(sounds)==1 and sounds[0][0]in copied,'Consume source audio selector changed')
    table={}
    for row in csv.DictReader(io.StringIO(ex.text('Translations/TranslatedText/menus - sheet.csv'))):
        require(row['key']not in table,'Duplicate consume translation')
        table[row['key']]=row
    def message(key):
        require(key in table,'Consume translation missing')
        return {locale:table[key][column].replace('{target}','{owner}')
                for locale,column in [('en','en'),('zh','zh_CN')]}
    names=[v['source']for v in catalog.load()['entries']]+['AsthmaSpray']
    effects=[]
    statuses=sorted({v for name in names for v in source['items'][name].get('status_heals',[])})
    for name in statuses:
        document=ex.yaml('Data/StatusAilments/'+name+'.yaml')
        for key in ('any','party_member'):
            effect=document.get('effects_by_char',{}).get(key,{})
            require(not(set(effect)&{'cant_receive_item','stat_mods','incapacitated'}),
                    'Consume status has unsupported item/stat restriction')
    for name in names:
        item=source['items'][name]
        require(item['keyitem']is False and item.get('PPrecover',0)==0
                and not any(item['boost'].values())and not item.get('reusable',False)
                and not item.get('transform')and not item.get('slot')
                and len(item['actions'])==1 and item['actions'][0]['function']=='consume'
                and ('can_consume'not in item or native['party'][0]in item['can_consume']),
                'Consume item source mechanism unsupported')
        hp=item.get('HPrecover',0)
        cure=item.get('status_heals',[])
        require(type(hp)is int and ((hp>0 and not cure)or(hp==0 and len(cure)==1)),
                'Consume source healing combinations unsupported')
        keys=['']*4
        if hp:keys[:2]=['ACTION_RESULT_HP_UP','ACTION_RESULT_HP_MAX']
        else:
            status=source['statuses'][cure[0]]
            keys[2:]=[status['messages']['heal_overworld'],status['messages']['heal_overworld_fail']]
        effects.append(dict(id=name,hp=hp,cure=cure[0]if cure else'',message_keys=keys,
                            messages=[message(key)if key else dict(en='',zh='')for key in keys]))
    model=validate(dict(actor=native['party'][0],refresh_hp=refresh,
                        sound=sounds[0][0],bus=sounds[0][1],lane=1,statuses=statuses,effects=effects))
    return dict(schema=1,kind='encore.native-item-consumption',commit=PIN,model=model,
                source=dict(original=proof,sources=ex.sources),
                dependencies={catalog.IR.as_posix():catalog.digest(ROOT/catalog.IR)},
                scope='Original HP-only food and one-status antidote/spray for the admitted singleton and declared status domain. No PP/boost/reusable/transform item, UI timer, PCM or full-scene admission.')


def load(path=None):
    path=ROOT/IR if path is None else Path(path)
    recipe=catalog.read(path)
    require(catalog.canonical(recipe)==catalog.canonical(derive()),'Unreviewed consumption recipe/source')
    review=catalog.read(ROOT/REVIEW)
    require(catalog.canonical(review)==catalog.canonical(review_document(recipe,path)),
            'Consumption source review mismatch')
    return recipe['model']


def reference_records(recipe):
    records=[]
    for relative in REFERENCES:
        folder=ROOT/relative
        receipt=catalog.read(folder/'receipt.json')
        digest=catalog.digest(folder/'reference.json')
        require(receipt['reference_sha256']==digest
                and receipt['producer_sha256']==catalog.digest(original.__file__)
                and receipt['engine_sha256']=='e748acc92ee5205dd3cc1754d4422870f6a68ae6e00a16ff88bd3438c957adfd'
                and receipt['source']['generated_sha256']==recipe['source']['original']['generated_sha256'],
                'Frozen actual consumption source/receipt changed')
        records.append(dict(path=(relative/'reference.json').as_posix(),sha256=digest))
    require(len({r['sha256']for r in records})==1,'Actual consumption runs are not idempotent')
    return records


def review_document(recipe,path):
    return dict(schema=1,commit=PIN,ir_sha256=catalog.digest(path),producer_sha256=catalog.digest(__file__),
                original_producer_sha256=catalog.digest(original.__file__),scope=recipe['scope'],
                references=reference_records(recipe),
                implementation={'Inventory.consume_item':'runtime/field_menus.cpp:consume_field_menu_item',
                    'PartyMember.remove_status/_refresh_hp_from_status':'runtime/field_menus.cpp:consume_field_menu_item',
                    'TargetCharaSelect.consume_item selectors':'ENCFMN01 v3 effect message/audio bindings',
                    'independent resource version':'runtime/field_menus_data.cpp:FieldMenusData::load'})


def encode_extension(model):
    validate(model)
    data=bytearray()
    def number(value):data.extend(struct.pack('<I',value))
    def text(value):
        raw=value.encode('utf-8');require(len(raw)<=4096 and '\0'not in value,'Consume string bound')
        number(len(raw));data.extend(raw)
    text(model['actor']);number(model['refresh_hp']);text(model['sound']);text(model['bus']);number(model['lane'])
    number(len(model['statuses']))
    for status in model['statuses']:text(status)
    number(len(model['effects']))
    for row in model['effects']:
        text(row['id']);number(row['hp']);text(row['cure'])
        for key,label in zip(row['message_keys'],row['messages']):text(key);text(label['en']);text(label['zh'])
    return bytes(data)


def validate_reference(document,inputs):
    fields(document,('schema','engine','rows'),'reference fields')
    require(type(document['schema'])is int and document['schema']==1
            and type(document['engine'])is dict
            and document['engine'].get('string')=='3.6.2-stable (official)',
            'Consume reference version/engine rejected')
    fields(document['engine'],'major minor patch hex status build year hash string'.split(),'engine fields')
    require(all(type(document['engine'][k])is int for k in ('major','minor','patch','hex','year'))
            and (document['engine']['major'],document['engine']['minor'],document['engine']['patch'])==(3,6,2)
            and document['engine']['hash']=='3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8'
            and document['engine']['status']=='stable'and document['engine']['build']=='official',
            'Consume engine provenance rejected')
    require(type(document['rows'])is list and len(document['rows'])==len(inputs['cases']),
            'Consume reference case coverage rejected')
    def integer(value,maximum=0xffffffff):
        return type(value)is int and 0<=value<=maximum
    def snapshot(value):
        fields(value,'hp pp status inventory selected_doses selected_equipped'.split(),'snapshot fields')
        require(integer(value['hp'])and integer(value['pp'])and integer(value['selected_doses'],65535)
                and type(value['selected_equipped'])is bool,'Consume snapshot scalars rejected')
        require(type(value['status'])is list and len(value['status'])<=len(inputs['statuses']),
                'Consume snapshot status count rejected')
        seen=set()
        for status in value['status']:
            require(type(status)is dict and 'status'in status and set(status)<= {'status','passive_healing_turns'}
                    and type(status['status'])is str and status['status']in inputs['statuses']and status['status']not in seen
                    and integer(status.get('passive_healing_turns',0),65535),
                    'Consume snapshot status rejected')
            seen.add(status['status'])
        require(type(value['inventory'])is list and 1<=len(value['inventory'])<=16,
                'Consume snapshot inventory count rejected')
        uids=set()
        for item in value['inventory']:
            fields(item,('item_name','uid','doses','equipped'),'reference item fields')
            require(type(item['item_name'])is str and item['item_name']in inputs['items']and integer(item['uid'])and item['uid']not in uids
                    and integer(item['doses'],65535)and item['doses']>0 and type(item['equipped'])is bool,
                    'Consume snapshot item rejected')
            uids.add(item['uid'])
    for index,row in enumerate(document['rows']):
        fields(row,'index before actions after signals requests queued_messages'.split(),'reference row fields')
        require(integer(row['index'])and row['index']==index,'Consume reference index rejected')
        snapshot(row['before']);snapshot(row['after'])
        expected=inputs['cases'][index]
        require(row['before']['hp']==expected['hp']and row['before']['pp']==expected['pp']
                and row['before']['selected_doses']==expected['doses'],
                'Consume controlled initial state changed')
        require(type(row['actions'])is dict and len(row['actions'])==1,'Consume action coverage rejected')
        key,value=next(iter(row['actions'].items()))
        require(key in ('0','2','5','6'),'Unsupported original consume action')
        if key=='0':require(integer(value,65535)and value>0,'Consume HP action value rejected')
        elif key=='2':require(value is True,'Consume HP maximum action rejected')
        else:require(type(value)is list and len(value)==1 and type(value[0])is str and value[0]in inputs['statuses'],'Consume heal action rejected')
        require(type(row['signals'])is list and len(row['signals'])<=2,'Consume signal count rejected')
        for signal in row['signals']:
            require(type(signal)is list and (signal==['status']or
                    (len(signal)==4 and signal[:2]==['stat','hp']and integer(signal[2])and integer(signal[3]))),
                    'Unknown consume signal rejected')
        require(type(row['requests'])is list and len(row['requests'])<=2,'Consume request count rejected')
        for request in row['requests']:
            require(type(request)is list and request,'Consume request shape rejected')
            if request[0]=='audio':
                require(len(request)==3 and request[1]=='res://Audio/Sound effects/EB/eat.wav'
                        and request[2]=='menu','Consume audio request rejected')
            else:
                require(len(request)==6 and request[0]=='dialog'and type(request[1])is str
                        and request[1].startswith('ACTION_RESULT_HEAL_')and request[2]=='ninten'
                        and type(request[3])is int and request[3]==0 and request[4]==''and request[5]is False,
                        'Consume dialog request rejected')
        require(type(row['queued_messages'])is list and len(row['queued_messages'])<=1,
                'Consume queued message count rejected')
        for queued in row['queued_messages']:
            require(type(queued)is list and len(queued)==3 and queued[0]in('ACTION_RESULT_HP_UP','ACTION_RESULT_HP_MAX')
                    and queued[1]=='ninten'and(queued[2]is True if queued[0]=='ACTION_RESULT_HP_MAX'else integer(queued[2],65535)and queued[2]>0),
                    'Unknown consume queued message rejected')
    return document


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action',choices=('extract','verify'))
    args=parser.parse_args()
    if args.action=='extract':
        recipe=derive();catalog.write(ROOT/IR,recipe)
        catalog.write(ROOT/REVIEW,review_document(recipe,ROOT/IR))
    else:load()


if __name__=='__main__':main()
