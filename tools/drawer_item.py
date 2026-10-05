#!/usr/bin/env python3
"""Checked AsthmaSpray definition/icon extension with explicit action boundary.

Original rich descriptions and all item properties remain in reviewed IR. The
current bounded consumer displays name/icon and refuses use/rich-description
rendering; no nickname, status-icon or dose control is silently flattened.
"""
from __future__ import annotations
import argparse,copy,csv,hashlib,io,json,re,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from tools.extract_battle_entry import Extractor,PIN,require
from tools.drawer_program import fields,read_json,write_json,digest,canonical
IR='content/native-drawer-item.json'
RECIPE='content/drawer-item-assets.json'
REVIEW='reports/drawer-program/item-source-review.json'
RECEIPT='content/asset-receipts/graphics/items/source.json'
ICON='Graphics/Objects/Items/AsthmaSpray.png'
OUTPUT='graphics/items/asthma-spray.t3x'
ITEM='Data/Items/AsthmaSpray.yaml'
TABLE='Translations/TranslatedText/items - sheet.csv'
LICENSE_REVIEW='Pinned upstream LICENSE permits game-related forks/modifications; original item art is used by this Mother: Encore port under upstream terms, not relicensed as MIT.'

def validate_item(doc):
    expected=dict(name='ASTHMASPRAY_NAME',sorting_name='ASTHMASPRAY_SORT',description='ASTHMASPRAY_DESC',article='ASTHMASPRAY_ART',
                  keyitem=False,doses=3,cost=20,value=2,actions=[dict(name='INVENTORY_ACTION_USE',function='consume')],transform='',slot='',
                  status_heals=['asthma'],HPrecover=0,PPrecover=0,boost=dict(maxhp=0,maxpp=0,offense=0,defense=0,speed=0,iq=0,guts=0))
    require(canonical(doc)==canonical(expected),'Unreviewed AsthmaSpray source property/action/status schema')

def validate_description(raw):
    require(isinstance(raw,str) and raw.count('%s')==1 and re.findall(r'\[[^\]]*\]',raw)==['[Ninten]','[Asthma]'],'Unreviewed AsthmaSpray description controls')

def build(root=ROOT):
    ex=Extractor(root);doc=ex.yaml(ITEM);validate_item(doc)
    require(ex.yaml('Data/save_new_game.yaml')['party']==['ninten'],'Drawer item single-owner boundary changed')
    ex.data('LICENSE');size=ex.png_size(ICON);ex.data(ICON+'.import')
    require(size==[20,20],'AsthmaSpray icon dimensions changed')
    item_script=ex.text('Scripts/global/Item.gd');description=ex.text('Scripts/UI/Reusables/Description.gd')
    text=ex.text('Scripts/global/text_tools.gd');ex.text('Scripts/global/PartyMember.gd')
    require('self.doses = get_data().get("doses", 1)' in item_script and 'func is_healing_item() -> bool:' in item_script and '!item_data.get("status_heals", []).empty()' in item_script,'AsthmaSpray dose/healing admission changed')
    require('Graphics/Objects/Items/' in description and 'get_item_doses_phrase' in description,'AsthmaSpray source description/icon path changed')
    require('get_item_doses_phrase' in text and '"INVENTORY_ITEM_USES_TOTAL"' in text and 'item.doses in range(0, nb_uses)' in text,'AsthmaSpray dynamic dose description changed')
    require('res://Graphics/UI/Ailments/%s.png' in text,'AsthmaSpray status-icon replacement changed')
    table={}
    for row in csv.DictReader(io.StringIO(ex.text(TABLE))):
        require(row['key'] not in table,'Duplicate item translation key');table[row['key']]=row
    translations={}
    for native,column in (('en','en'),('zh_Hans_CN','zh_CN')):
        raw={field:table[doc[field]][column] for field in ('name','description','sorting_name','article')}
        validate_description(raw['description']);require('[' not in raw['name'] and raw['name'],'AsthmaSpray item name controls')
        translations[native]=raw
    return dict(schema=1,kind='encore.drawer-item.source-ir',commit=PIN,
                scope='Source item identity, acquisition template, persistent inventory name/icon; rich description and consume behavior explicitly unsupported',
                sources=dict(sorted(ex.sources.items())),source_item='AsthmaSpray',source_path=ITEM,document=doc,
                translations=translations,icon=dict(source=ICON,size=size,output=OUTPUT),
                native_boundary=dict(definition_id=2,flags=2,can_use=0,description_policy='Preserve raw source controls; checked RichDescription capability blocks plain renderer',
                                     unsupported=['Nickname control in description','Inline Asthma status image','Dynamic dose phrase','consume/status_heals action']))

def recipe(ir):
    return dict(schema=1,kind='encore.drawer-item.asset-recipe',commit=PIN,licence_review=LICENSE_REVIEW,
                sources=ir['sources'],resource=dict(name='asthma_spray',source=ICON,size=ir['icon']['size'],grid=[1,1],output=OUTPUT,format='rgba8',compression='none'))

def load(root=ROOT):
    root=Path(root);ir=read_json(root/IR);require(canonical(ir)==canonical(build(root)),'Stale/unreviewed Drawer item IR')
    review=read_json(root/REVIEW)
    fields(review,('schema','commit','ir_sha256','sources','document','translations','semantics','unsupported','unverified'),'Drawer item review')
    require(review['schema']==1 and review['commit']==PIN and review['ir_sha256']==digest(root/IR) and review['sources']==ir['sources'] and review['document']==ir['document'] and review['translations']==ir['translations'],'Drawer item review mismatch')
    require(read_json(root/RECIPE)==recipe(ir),'Drawer item asset recipe differs from reviewed source')
    return ir

def verify_receipt(root=ROOT,source=None):
    root=Path(root);ir=load(root);r=read_json(root/RECEIPT)
    fields(r,('schema','commit','recipe','ir_sha256','producer_sha256','tex3ds_sha256','resource','outputs','limits'),'Drawer item receipt')
    require(r['schema']==1 and r['commit']==PIN and r['recipe']==recipe(ir) and r['ir_sha256']==digest(root/IR) and r['producer_sha256']==digest(root/'tools/drawer_item.py'),'Drawer item producer receipt mismatch')
    require(isinstance(r['tex3ds_sha256'],str) and re.fullmatch('[0-9a-f]{64}',r['tex3ds_sha256']) is not None,'Drawer item missing real texture compiler receipt')
    expected=root/'romfs'/OUTPUT if source is None else Path(source)/OUTPUT
    fields(r['outputs'],(OUTPUT,),'Drawer item output receipt');out=r['outputs'][OUTPUT]
    fields(out,('bytes','sha256'),'Drawer item output')
    require(expected.stat().st_size==out['bytes'] and digest(expected)==out['sha256'],'Drawer item texture differs from reviewed receipt')
    require(r['resource']==dict(id=1,path=OUTPUT,kind=1,width=ir['icon']['size'][0],height=ir['icon']['size'][1],columns=1,rows=1,sha256=out['sha256']),'Drawer item resource geometry/hash mismatch')
    return r

def append_item(ex,result):
    ir=load(ex.root);receipt=verify_receipt(ex.root)
    require(len(result['definitions'])==1 and result['definitions'][0]['id']==1 and result['definitions'][0]['source']=='BaseballCap','Drawer item stable definition prefix changed')
    require(all(r['path']!=OUTPUT for r in result['resources']),'Drawer item icon already appended')
    out=copy.deepcopy(result);resource=copy.deepcopy(receipt['resource']);resource['id']=max(r['id'] for r in out['resources'])+1
    icon_index=len(out['resources']);out['resources'].append(resource)
    doc=ir['document'];english=ir['translations']['en']
    out['definitions'].append(dict(id=ir['native_boundary']['definition_id'],source=ir['source_item'],name=english['name'],description=english['description'],
                                   icon=icon_index,equipment_slot=None,heal_hp=doc['HPrecover'],heal_pp=doc['PPrecover'],max_hp_boost=doc['boost']['maxhp'],
                                   max_pp_boost=doc['boost']['maxpp'],flags=ir['native_boundary']['flags'],can_use=ir['native_boundary']['can_use']))
    # Independent reviewed dependencies preserve the original UI receipt and
    # source prefix. The full extension source ledger is in IR and re-admitted.
    out['dependencies']=dict(out['dependencies'])
    # The review is re-admitted by load(); resource dependency paths remain in
    # content/romfs as required by the Items format, rather than weakening it.
    for path in (IR,RECIPE,RECEIPT):out['dependencies'][path]=digest(Path(ex.root)/path)
    out['scope']+='; checked AsthmaSpray acquired definition/icon; raw rich description and consume action explicitly gated'
    require(out['initial_inventory']==result['initial_inventory'],'Drawer extension changed new-game instances')
    return out

def stage_files(source):
    verify_receipt(ROOT,source);return {Path(OUTPUT):(Path(source)/OUTPUT).read_bytes()}

def main():
    p=argparse.ArgumentParser();p.add_argument('action',choices=('extract','compile','verify'));p.add_argument('--tex3ds',type=Path);args=p.parse_args()
    try:
        if args.action=='extract':
            ir=build();write_json(ROOT/IR,ir);write_json(ROOT/RECIPE,recipe(ir))
            write_json(ROOT/REVIEW,dict(schema=1,commit=PIN,ir_sha256=digest(ROOT/IR),sources=ir['sources'],document=ir['document'],translations=ir['translations'],
                semantics={'grant':'Non-key AsthmaSpray, unequipped, source-default three doses; identity binds the independent Drawer grant template',
                           'display':'Original item name/icon preserved; rich description keeps nickname/status image/%s dose controls verbatim',
                           'action_boundary':'Source consume heals asthma; current consumer explicitly disables action and does not claim source use parity',
                           'baseline':'Append-only definition/resource; initial instances and existing UI source receipt preserved'},
                unsupported=ir['native_boundary']['unsupported'],unverified=['Manual test suites','GPU/display equivalence','Hardware']))
        elif args.action=='verify':verify_receipt();print('Drawer item source/icon receipt admitted')
        else:
            require(args.tex3ds is not None,'Need official tex3ds for genuine item texture conversion')
            ir=load();target=ROOT/'romfs'/OUTPUT;target.parent.mkdir(parents=True,exist_ok=True)
            subprocess.run([str(args.tex3ds),'-f','rgba8','-z','none','-o',str(target),str(ROOT/'upstream/MOTHER-Encore'/ICON)],check=True)
            out=dict(bytes=target.stat().st_size,sha256=digest(target))
            write_json(ROOT/RECEIPT,dict(schema=1,commit=PIN,recipe=recipe(ir),ir_sha256=digest(ROOT/IR),producer_sha256=digest(ROOT/'tools/drawer_item.py'),tex3ds_sha256=digest(args.tex3ds),
                resource=dict(id=1,path=OUTPUT,kind=1,width=ir['icon']['size'][0],height=ir['icon']['size'][1],columns=1,rows=1,sha256=out['sha256']),outputs={OUTPUT:out},
                limits='Pinned original lossless icon converted by real tex3ds; rich description/use explicitly unsupported; no hardware claim'))
            verify_receipt();print('Drawer item original icon compiled')
    except (ValueError,OSError,KeyError,TypeError,subprocess.SubprocessError) as error:
        print('DRAWER ITEM ERROR:',error,file=sys.stderr);return 1
    return 0
if __name__=='__main__':raise SystemExit(main())
