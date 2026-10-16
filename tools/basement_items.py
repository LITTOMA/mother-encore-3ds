#!/usr/bin/env python3
"""Reviewed basement item metadata and original art; genuine tex3ds producer.

This adapter admits two source items, not arbitrary item YAML or scripts.
The checked resources are consumed by the ordinary inventory and room core.
"""
from __future__ import annotations
import argparse, copy, re, subprocess, sys, tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from tools.extract_battle_entry import Extractor, PIN, require
from tools.house_presents import digest, canonical, read_json, write_json, table
IR='content/basement-items.json'
REVIEW='reports/basement-items/source-review.json'
RECEIPT='content/asset-receipts/graphics/basement/source.json'
ASSETS=(('bat','Graphics/Objects/Items/BatPlastic.png','graphics/basement/plastic-bat.t3x',[1,1]),
        ('special','Graphics/Character Sprites/Ninten/cutscene.png','graphics/basement/ninten-cutscene.t3x',[20,10]))

def build(root=ROOT):
    ex=Extractor(root); ex.data('LICENSE')
    bat=ex.yaml('Data/Items/BatPlastic.yaml')
    require(bat==dict(name='BATPLASTIC_NAME',sorting_name='BATPLASTIC_SORT',description='BATPLASTIC_DESC',article='BATPLASTIC_ART',
        keyitem=False,cost=100,value=50,can_use=['ninten','pippi'],actions=[dict(function='equip')],basic_skill='attack_bat',
        transform='',slot='weapon',HPrecover=0,PPrecover=0,boost=dict(maxhp=0,maxpp=0,offense=3,defense=0,speed=0,iq=0,guts=0)),
        'Unreviewed plastic bat properties')
    diary=ex.yaml('Data/Items/GGFDiary.yaml')
    require(diary['keyitem'] is True and diary['actions']==[] and diary['transform']=='' and diary['slot']=='', 'Unreviewed diary item')
    rows=table(ex,'Translations/TranslatedText/items - sheet.csv')
    translations={lang:{field:rows[bat[field]][column] for field in ('name','sorting_name','article','description')}
                  for lang,column in [('en','en'),('zh_Hans_CN','zh_CN')]}
    assets=[]
    for role,source,output,grid in ASSETS:
        size=ex.png_size(source);ex.data(source+'.import')
        require(all(s%g==0 for s,g in zip(size,grid)),'Basement sprite grid mismatch')
        if role=='special':
            require(size==[1280,640] and grid==[20,10],'Special sprite source geometry changed')
            assets.append(dict(role=role,source=source,output=output,source_size=size,source_grid=grid,
                size=[512,448],grid=[8,7],frames=list(range(15,68))))
        else:assets.append(dict(role=role,source=source,output=output,size=size,grid=grid))
    require(assets[0]['size']==[20,20],'Bat icon geometry changed')
    return dict(schema=1,kind='encore.basement-items.source-ir',commit=PIN,sources=ex.sources,
        bat=bat,diary=diary,translations=translations,assets=assets,
        permission='Pinned upstream LICENSE permits game-related modifications; original game art remains under upstream terms.',
        definition_id=3,scope='Original plastic bat metadata/icon and Ninten special-animation sheet; no whole-game behavior approval')

def load(root=ROOT):
    root=Path(root);ir=read_json(root/IR)
    require(canonical(ir)==canonical(build(root)),'Stale/unreviewed basement item IR')
    review=read_json(root/REVIEW)
    require(review==dict(schema=1,commit=PIN,ir_sha256=digest(root/IR),sources=ir['sources'],semantics=ir['scope'],permission=ir['permission']),
            'Basement item source review mismatch')
    return ir

def verify_receipt(root=ROOT,source=None):
    root=Path(root);ir=load(root);r=read_json(root/RECEIPT)
    require(set(r)=={'schema','commit','ir_sha256','producer_sha256','tex3ds_sha256','assets','outputs'} and r['schema']==1 and
        r['commit']==PIN and r['ir_sha256']==digest(root/IR) and r['producer_sha256']==digest(root/'tools/basement_items.py') and
        r['assets']==ir['assets'] and isinstance(r['tex3ds_sha256'],str) and re.fullmatch('[0-9a-f]{64}',r['tex3ds_sha256']) is not None,'Basement texture provenance mismatch')
    require(set(r['outputs'])=={a['output'] for a in ir['assets']},'Basement texture set mismatch')
    base=root/'romfs' if source is None else Path(source)
    for path,rec in r['outputs'].items():
        target=base/path
        require(rec==dict(bytes=target.stat().st_size,sha256=digest(target)),'Basement texture hash/size mismatch')
    return r

def append_item(ex,ir):
    data=load(ex.root);r=verify_receipt(ex.root)
    require([d['id'] for d in ir['definitions']]==[1,2],'Basement stable item prefix changed')
    out=copy.deepcopy(ir);asset=data['assets'][0];english=data['translations']['en']
    icon=len(out['resources'])
    out['resources'].append(dict(id=max(r['id'] for r in out['resources'])+1,path=asset['output'],kind=1,
        width=asset['size'][0],height=asset['size'][1],columns=1,rows=1,sha256=r['outputs'][asset['output']]['sha256']))
    out['definitions'].append(dict(id=data['definition_id'],source='BatPlastic',name=english['name'],description=english['description'],
        icon=icon,equipment_slot=0,heal_hp=0,heal_pp=0,max_hp_boost=0,max_pp_boost=0,flags=1,can_use=1))
    for path in (IR,RECEIPT):out['dependencies'][path]=digest(Path(ex.root)/path)
    out['scope']+='; original unequipped plastic bat acquisition, weapon slot and icon'
    return out

def stage_files(source):
    r=verify_receipt(ROOT,source)
    return {Path(p):(Path(source)/p).read_bytes() for p in r['outputs']}

def main():
    p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile','verify']);p.add_argument('--tex3ds',type=Path);a=p.parse_args()
    if a.action=='extract':
        ir=build();write_json(ROOT/IR,ir);write_json(ROOT/REVIEW,dict(schema=1,commit=PIN,ir_sha256=digest(ROOT/IR),
            sources=ir['sources'],semantics=ir['scope'],permission=ir['permission']))
    elif a.action=='compile':
        require(a.tex3ds is not None,'Genuine tex3ds is required');ir=load();outputs={}
        for asset in ir['assets']:
            dest=ROOT/'romfs'/asset['output'];dest.parent.mkdir(parents=True,exist_ok=True)
            source=ROOT/'upstream/MOTHER-Encore'/asset['source']
            if 'frames' in asset:
                from PIL import Image
                scratch=ROOT/'build/basement-assets';scratch.mkdir(parents=True,exist_ok=True)
                with tempfile.TemporaryDirectory(prefix='sprite-',dir=scratch) as td:
                    with Image.open(source) as image:
                        atlas=Image.new('RGBA',asset['size'])
                        for i,frame in enumerate(asset['frames']):
                            x,y=frame%20*64,frame//20*64
                            atlas.paste(image.crop((x,y,x+64,y+64)),(i%8*64,i//8*64))
                    png=Path(td)/'special.png';atlas.save(png)
                    subprocess.run([str(a.tex3ds),'-f','rgba8','-z','none','-o',str(dest),str(png)],check=True)
            else:subprocess.run([str(a.tex3ds),'-f','rgba8','-z','none','-o',str(dest),str(source)],check=True)
            outputs[asset['output']]=dict(bytes=dest.stat().st_size,sha256=digest(dest))
        write_json(ROOT/RECEIPT,dict(schema=1,commit=PIN,ir_sha256=digest(ROOT/IR),producer_sha256=digest(ROOT/'tools/basement_items.py'),
            tex3ds_sha256=digest(a.tex3ds),assets=ir['assets'],outputs=outputs))
        verify_receipt()
    else:verify_receipt()
    return 0
if __name__=='__main__':raise SystemExit(main())
