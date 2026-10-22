"""Strict offline observation reader; source results are never game inputs."""
import json
from pathlib import Path
from tools import run_inventory_sort_reference as original,catacombs_item_catalog as catalog
from tools.extract_battle_entry import require


def fields(value,names,label):
    require(type(value)is dict and set(value)==set(names),'Unknown/missing sort '+label)


def validate(document,inputs):
    fields(document,('schema','engine','pairs','rows'),'reference fields')
    require(type(document['schema'])is int and document['schema']==1,'Sort reference version rejected')
    fields(document['engine'],'major minor patch hex status build year hash string'.split(),'engine fields')
    engine=document['engine']
    require(all(type(engine[k])is int for k in ('major','minor','patch','hex','year'))
            and (engine['major'],engine['minor'],engine['patch'],engine['hex'],engine['status'],engine['build'],engine['year'],engine['hash'],engine['string'])
            ==(3,6,2,198146,'stable','official',2025,'3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8','3.6.2-stable (official)'),
            'Sort reference engine rejected')
    names=inputs['names'];locales=inputs['locales'];cases=inputs['cases']
    pairs=[(locale,a,b)for locale in locales for a in names for b in names]
    require(type(document['pairs'])is list and len(document['pairs'])==len(pairs),'Sort comparison coverage rejected')
    for row,(locale,a,b)in zip(document['pairs'],pairs):
        fields(row,('locale','a','b','less'),'comparison fields')
        require((row['locale'],row['a'],row['b'])==(locale,a,b)and type(row['less'])is bool,'Sort comparison identity/type rejected')
    require(type(document['rows'])is list and len(document['rows'])==len(cases)*len(locales),'Sort inventory coverage rejected')
    def item(value):
        fields(value,('item_name','uid','equipped','doses'),'item fields')
        require(type(value['item_name'])is str and value['item_name']in names and type(value['uid'])is int
                and 0<=value['uid']<=0xffffffff and type(value['equipped'])is bool and not value['equipped']
                and type(value['doses'])is int and 1<=value['doses']<=inputs['items'][value['item_name']].get('doses',1),
                'Sort observed item rejected')
    for row,(locale,case)in zip(document['rows'],((locale,case)for locale in locales for case in cases)):
        fields(row,('locale','index','before','once','twice'),'inventory fields')
        require(row['locale']==locale and type(row['index'])is int and row['index']==case['index'],'Sort inventory identity rejected')
        expected=[dict(item_name=name,uid=1000+i,equipped=False,
                       doses=case['spray_doses']if name==inputs['spray']else inputs['items'][name].get('doses',1))
                  for i,name in enumerate(case['names'])]
        for name in ('before','once','twice'):
            require(type(row[name])is list and len(row[name])==len(expected),'Sort observed inventory count rejected')
            for value in row[name]:item(value)
            require(len({value['uid']for value in row[name]})==len(expected)
                    and sorted(row[name],key=lambda v:v['uid'])==expected,'Sort observed inventory ownership changed')
        require(row['before']==expected,'Sort controlled input changed')
    return document


def read(folder):
    files,proof=original.source();folder=Path(folder)
    receipt=catalog.read(folder/'receipt.json')
    fields(receipt,('source','engine_sha256','producer_sha256','reference_sha256'),'receipt fields')
    require(receipt['source']==proof and receipt['producer_sha256']==catalog.digest(original.__file__)
            and receipt['engine_sha256']=='e748acc92ee5205dd3cc1754d4422870f6a68ae6e00a16ff88bd3438c957adfd'
            and receipt['reference_sha256']==catalog.digest(folder/'reference.json'),'Sort original receipt/source changed')
    inputs=json.loads(files['input.json'])
    return validate(catalog.read(folder/'reference.json'),inputs),inputs
