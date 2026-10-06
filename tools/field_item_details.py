#!/usr/bin/env python3
"""Independent stable-ID field item descriptions and source icon assets."""
from __future__ import annotations
import argparse, csv, io, math, re, struct, subprocess, sys, zlib
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.extract_battle_entry import Extractor,PIN,require
from tools.drawer_program import canonical,digest,fields,read_json,safe_path
from tools import item_details as house
from tools.field_item_definitions import load as item_load
IR='content/native-field-item-details.json';RECIPE='content/field-item-details-assets.json';REVIEW='reports/field-item-details/source-review.json';RECEIPT='content/asset-receipts/graphics/ui/field-item-details/source.json';PACK='romfs/data/podunk.encfielddetails'
NAMES=house.NAMES;FORMATS=(None,'<6I',*house.FORMATS[2:]);STRIDES=(1,24,*house.STRIDES[2:]);HEADER=house.HEADER;TOKEN_KINDS=house.TOKEN_KINDS;PARAMETERS=house.PARAMETERS

def write_json(p,d):
 p=Path(p);p.parent.mkdir(parents=True,exist_ok=True)
 with p.open('w',encoding='utf8',newline='\n')as f:
  import json
  json.dump(d,f,ensure_ascii=False,indent=2);f.write('\n')

def tokenize(raw):
 raw=raw.replace('\\n','\n').replace('[BR]','\n');result=[]
 for part in re.split(r'(\[Ninten\]|\[Asthma\]|\[Blinded\]|\[ItemValue\]|%s|\n)',raw):
  if not part:continue
  kind={'[Ninten]':'Nickname','[Asthma]':'InlineImage','[Blinded]':'InlineImage','[ItemValue]':'ItemValue','%s':'Doses','\n':'Newline'}.get(part,'Text')
  require(kind!='Text'or not any(c in part for c in '[]{}'),'Unreviewed field description text control')
  result.append(dict(kind=kind,value=part if kind=='Text'else 1 if part=='[Blinded]'else 0,color=0))
 require(result and len(result)<=128,'Field details token capacity');return result

def build(root=ROOT):
 root=Path(root);ex=Extractor(root);base=house.build(root);defs=item_load();text=ex.text('Scripts/global/text_tools.gd')
 require('for stat in boosts:'in text and 'for recover in ["PPrecover", "HPrecover"]:'in text,'Changed source ItemValue order')
 for p,h in base['sources'].items():ex.data(p);require(ex.sources[p]==h,'Changed House description semantics '+p)
 rows=house.table(ex,'Translations/TranslatedText/items - sheet.csv');definitions=[];presentations=[];resources=[]
 for name in ('Asthma','Blinded'):
  path='Graphics/UI/Ailments/'+name+'.png';matching=[p for p in ex.inventory['files']if p.casefold()==path.casefold()];require(matching==[path],'Ambiguous inline status path casing');w,h=ex.png_size(path);ex.data(path+'.import');resources.append(dict(id=len(resources)+1,source=path,path='graphics/ui/field-item-details/'+name.lower()+'-status.t3x',kind=1,width=w,height=h,columns=1,rows=1))
 for d in defs['definitions']:
  raw=ex.yaml(d['source']);value=next((v for v in raw['boost'].values()if v>0),next((raw[k]for k in ('PPrecover','HPrecover')if raw.get(k,0)>0),0));icon=0xffffffff;path='Graphics/Objects/Items/'+d['item_name']+'.png'
  if path in ex.inventory['files']:
   w,h=ex.png_size(path);ex.data(path+'.import');icon=len(resources);resources.append(dict(id=icon+1,source=path,path='graphics/ui/field-item-details/'+d['item_name'].lower()+'.t3x',kind=1,width=w,height=h,columns=1,rows=1))
  definitions.append(dict(id=d['id'],source=d['source'],raw_description=d['description_key'],max_doses=d['doses'],item_value=value,item_icon=icon))
  for lang,column in (('en','en'),('zh_Hans_CN','zh_CN')):
   rawtext=rows[d['description_key']][column];presentations.append(dict(definition=d['id'],locale=lang,raw=rawtext,tokens=tokenize(rawtext)))
 require(len(definitions)==19 and len(presentations)==38,'Incomplete reviewed field description scope')
 # Both checked inline images have identical source image-font baseline.
 require(resources[0]['height']==resources[1]['height'],'Different inline status baseline needs new capability')
 return dict(schema=1,kind='encore.native-field-item-details.source-ir',commit=PIN,scope='All 19 source item descriptions and existing icon PNGs, stable field definition identities; use actions remain independently pending',sources=dict(sorted(ex.sources.items())),dependencies={'content/native-field-item-definitions.json':digest(root/'content/native-field-item-definitions.json')},definitions=definitions,locales=base['locales'],presentations=presentations,resources=resources,parameters=base['parameters'])

def recipe(ir):return dict(schema=1,kind='encore.field-item-details.asset-recipe',commit=PIN,sources=ir['sources'],resources=ir['resources'],license_review='Pinned upstream game-related derivative permission; original art retains upstream terms, not MIT.',format='rgba8',compression='none')
def extract(root=ROOT):
 root=Path(root);ir=build(root);write_json(root/IR,ir);write_json(root/RECIPE,recipe(ir));write_json(root/REVIEW,dict(schema=1,commit=PIN,ir_sha256=digest(root/IR),sources=ir['sources'],semantics=['Independent ENCFID01 stable field definition IDs, never legacy House definition indices','TextTools value chooses first positive source boost in YAML dictionary order, then PP, then HP','All19 en/zh source descriptions only contain Nickname/Doses/BR/ItemValue/InlineAsthma/InlineBlinded','Real casefold-unique status PNGs and existing item icon PNGs, source default inline image baseline and measured word wrapping','Dose phrase actual current/max doses and source plural rules; read-only description composition'],unverified=['Manual tests not executed','Emulator/hardware display not validated']));return ir

def load(root=ROOT):
 root=Path(root);ir=read_json(root/IR);require(canonical(ir)==canonical(build(root)),'Stale field description IR');review=read_json(root/REVIEW);require(review['commit']==PIN and review['ir_sha256']==digest(root/IR)and review['sources']==ir['sources'],'Unreviewed field descriptions');require(read_json(root/RECIPE)==recipe(ir),'Field description recipe changed');return ir

def compile_assets(tex3ds,root=ROOT):
 root=Path(root);ir=load(root);tex3ds=Path(tex3ds);require(tex3ds.is_file(),'Need genuine tex3ds')
 def convert(r):
  target=root/'romfs'/r['path'];target.parent.mkdir(parents=True,exist_ok=True);subprocess.run([str(tex3ds),'-f','rgba8','-z','none','-o',str(target),str(root/'upstream/MOTHER-Encore'/r['source'])],check=True);return r['path'],dict(bytes=target.stat().st_size,sha256=digest(target),crc32=zlib.crc32(target.read_bytes()))
 with ThreadPoolExecutor(max_workers=4)as pool:outputs=dict(pool.map(convert,ir['resources']))
 receipt=dict(schema=1,commit=PIN,ir_sha256=digest(root/IR),producer_sha256=digest(root/'tools/field_item_details.py'),tex3ds_sha256=digest(tex3ds),outputs=outputs);write_json(root/RECEIPT,receipt);return receipt

def verify_receipt(ir,root=ROOT,source=None):
 root=Path(root);r=read_json(root/RECEIPT);require(r['schema']==1 and r['commit']==PIN and r['ir_sha256']==digest(root/IR)and r['producer_sha256']==digest(root/'tools/field_item_details.py')and re.fullmatch('[0-9a-f]{64}',r['tex3ds_sha256']),'Field texture receipt provenance');require(set(r['outputs'])=={a['path']for a in ir['resources']},'Field texture receipt coverage')
 for path,out in r['outputs'].items():
  raw=((root/'romfs'if source is None else Path(source))/path).read_bytes();require(len(raw)==out['bytes']and zlib.crc32(raw)==out['crc32']and digest((root/'romfs'if source is None else Path(source))/path)==out['sha256'],'Changed field texture '+path)
 return r
def lower(ir, receipt):
    fields(ir, ('schema','kind','commit','scope','sources','dependencies','definitions','locales','presentations','resources','parameters'), 'Details IR')
    require(type(ir['schema']) is int and ir['schema'] == 1 and ir['kind'] == 'encore.native-field-item-details.source-ir'
            and ir['commit'] == PIN, 'Details IR schema/pin')
    fields(ir['parameters'], PARAMETERS, 'Details parameters')
    pool = bytearray(b'\0'); offsets = {'': 0}
    def string(value):
        require(isinstance(value, str) and '\0' not in value and len(value.encode('utf-8')) <= 4096, 'Details string')
        if value not in offsets:
            offsets[value] = len(pool); pool.extend(value.encode('utf-8') + b'\0')
        return offsets[value]
    t = {n: [] for n in NAMES}
    for d in ir['definitions']:
        fields(d, ('id','source','raw_description','max_doses','item_value','item_icon'), 'Details definition IR')
        t['Definitions'].append([d['id'], string(d['source']), string(d['raw_description']), d['max_doses'], d['item_value'], d['item_icon']])
    for l in ir['locales']:
        fields(l, ('code','separator','total','left_singular','left_plural','image_measure','base_color','hint_color'), 'Details locale IR')
        t['Locales'].append([string(l['code']), string(l['separator']), string(l['total']), string(l['left_singular']),
                             string(l['left_plural']), string(l['image_measure']), l['base_color'], l['hint_color']])
    locale_indices = {l['code']: i for i, l in enumerate(ir['locales'])}
    for p in ir['presentations']:
        fields(p, ('definition','locale','raw','tokens'), 'Details presentation IR')
        require(p['locale'] in locale_indices and p['tokens'] == tokenize(p['raw']), 'Details raw/token binding')
        first = len(t['Tokens'])
        for token in p['tokens']:
            fields(token, ('kind','value','color'), 'Details token IR')
            require(token['kind'] in TOKEN_KINDS, 'Details token kind IR')
            value = string(token['value']) if token['kind'] == 'Text' else token['value']
            t['Tokens'].append([TOKEN_KINDS[token['kind']], value, token['color'], 0])
        t['Presentations'].append([p['definition'], locale_indices[p['locale']], first, len(t['Tokens']) - first])
    for r in ir['resources']:
        fields(r, ('id','source','path','kind','width','height','columns','rows'), 'Details resource IR')
        t['Resources'].append([r['id'], string(r['path']), r['kind'], r['width'], r['height'], r['columns'], r['rows'],
                               bytes.fromhex(receipt['outputs'][r['path']]['sha256']),
                               receipt['outputs'][r['path']]['bytes'], receipt['outputs'][r['path']]['crc32']])
    t['Parameters'] = [[i + 1, ir['parameters'][n]] for i, n in enumerate(PARAMETERS)]
    t['Strings'] = bytes(pool); validate(t)
    return t


def validate(t):
    require(set(t) == set(NAMES), 'Details sections')
    pool = t['Strings']
    require(isinstance(pool, bytes) and 1 <= len(pool) <= 65536 and pool[0] == pool[-1] == 0, 'Details strings')
    decoded = pool.decode('utf-8')
    require(all(ord(c) >= 32 or c in '\0\n' for c in decoded) and '\x7f' not in decoded, 'Details string controls')
    def string(off):
        require(type(off) is int and 0 <= off < len(pool) and (off == 0 or pool[off - 1] == 0), 'Details string offset')
        value = pool[off:pool.index(0, off)].decode('utf-8')
        require(len(value.encode('utf-8')) <= 4096, 'Details string capacity')
        return value
    limits = ((1,256), (2,2), (2,512), (1,32768), (1,258), (4,4))
    for n, f, (lo, hi) in zip(NAMES[1:], FORMATS[1:], limits):
        require(lo <= len(t[n]) <= hi, 'Details ' + n + ' capacity')
        for row in t[n]:
            integers = row[:7] + row[8:] if n == 'Resources' else row[:1] if n == 'Parameters' else row
            require(all(type(v) is int and 0 <= v <= 0xffffffff for v in integers), 'Details ' + n + ' integer')
            try: struct.pack(f, *row)
            except (struct.error, TypeError, OverflowError) as error: raise ValueError('Details ' + n + ' record') from error
    ids = set()
    for d in t['Definitions']:
        require(d[0] not in ids and string(d[1]) and string(d[2])
                and 1 <= d[3] <= 65535 and d[4] <= 65535, 'Details definition')
        require(d[5]==0xffffffff or d[5]<len(t['Resources']),'Field item icon index');ids.add(d[0])
    for i, l in enumerate(t['Locales']):
        require(string(l[0]) == ('en', 'zh_Hans_CN')[i] and string(l[1]) in ('', ' ')
                and string(l[5]) and (l[6] >> 24) == (l[7] >> 24) == 255, 'Details locale')
        for off in l[2:5]:
            value = string(off)
            require(value.count('%s') == 1 and '%' not in value.replace('%s','')
                    and not any(c in value for c in '[]{}\n'), 'Details dose pattern')
    for r in t['Resources']:
        require(r[0] > 0 and safe_path(string(r[1])) and string(r[1]).startswith('graphics/')
                and string(r[1]).endswith('.t3x') and r[2] == 1 and 0 < r[3] <= 4096
                and 0 < r[4] <= 4096 and r[5] == r[6] == 1 and len(r[7]) == 32 and any(r[7])
                and 0 < r[8] <= 1024*1024, 'Details resource')
    require(len({r[0] for r in t['Resources']}) == len(t['Resources']), 'Details resource identity')
    for token in t['Tokens']:
        kind, value, color, reserved = token
        require(kind in TOKEN_KINDS.values() and color <= 1 and reserved == 0, 'Details token')
        if kind == 1:
            s = string(value); require(s and not any(c in s for c in '[]{}\n'), 'Details literal controls')
        elif kind == 4: require(value < len(t['Resources']), 'Details inline image index')
        else: require(value == 0, 'Details dynamic token value')
    cursor = 0; seen = set()
    for d, locale, first, count in t['Presentations']:
        require(d in ids and locale < 2 and (d,locale) not in seen and first == cursor
                and 0 < count <= 128 and first + count <= len(t['Tokens']), 'Details presentation')
        cursor += count; seen.add((d, locale))
        kinds = [token[0] for token in t['Tokens'][first:first+count]]
        max_doses = next(row[3] for row in t['Definitions'] if row[0] == d)
        require(kinds.count(TOKEN_KINDS['Nickname']) <= 1 and kinds.count(TOKEN_KINDS['Doses']) == (1 if max_doses > 1 else 0),
                'Details duplicate/policy token binding')
    require(cursor == len(t['Tokens']) and seen == {(d,l) for d in ids for l in (0,1)}, 'Details presentation coverage')
    for i, (ident, value) in enumerate(t['Parameters']):
        require(type(value) in (int,float) and ident == i + 1 and math.isfinite(value)
                and (-128 <= value <= 128 if i == 1 else 0 < value <= 128), 'Details parameter')
        if i >= 2: require(value == int(value), 'Details nickname scalar limit')


def encode(t):
    validate(t); data = bytearray(HEADER)
    for i, n in enumerate(NAMES):
        block = t[n] if i == 0 else b''.join(struct.pack(FORMATS[i], *r) for r in t[n])
        while len(data) % 4: data.append(0)
        struct.pack_into('<HHIII', data, 64 + i * 16, i+1, STRIDES[i], len(data), len(block)//STRIDES[i], len(block))
        data.extend(block)
    struct.pack_into('<8s6I20s12x', data, 0, b'ENCFID01', 1, len(data), 0, 7, 1, 1, bytes.fromhex(PIN))
    struct.pack_into('<I', data, 16, zlib.crc32(data)); return bytes(data)


def parse_pack(blob):
    require(HEADER <= len(blob) <= 1024*1024, 'Details size')
    magic, version, size, crc, n, caps, rules, pin = struct.unpack_from('<8s6I20s', blob)
    require((magic,version,size,n,caps,rules,pin.hex()) == (b'ENCFID01',1,len(blob),7,1,1,PIN)
            and not any(blob[52:64]), 'Details header/pin')
    copy = bytearray(blob); struct.pack_into('<I', copy, 16, 0)
    require(zlib.crc32(copy) == crc, 'Details CRC')
    t = {}; end = HEADER
    for i,n in enumerate(NAMES):
        kind,stride,off,count,amount = struct.unpack_from('<HHIII', blob, 64+i*16)
        require(kind == i+1 and stride == STRIDES[i] and count > 0 and amount == count*stride
                and off == (end+3)//4*4 and off+amount <= len(blob) and not any(blob[end:off]), 'Details directory/span')
        block = blob[off:off+amount]; end = off+amount
        t[n] = bytes(block) if i == 0 else list(struct.iter_unpack(FORMATS[i],block))
    require(end == len(blob), 'Details trailing bytes'); validate(t); return t



def glyph_requests():
 ir=load();rows=read_json(ROOT/'content/native-field-item-definitions.json');ex=Extractor(ROOT);text=house.table(ex,'Translations/TranslatedText/items - sheet.csv');parts=[]
 for row in rows['definitions']:
  for key in ('name_key','description_key','article_key'):
   for column in ('en','zh_CN'):parts.append(text[row[key]][column])
 for loc in ir['locales']:parts.extend(loc[k]for k in ('total','left_singular','left_plural','image_measure'))
 return {'Fonts/EBMain_la.tres':[''.join(parts)+'0123456789']}
def compile_pack(root=ROOT):
 root=Path(root);ir=load(root);receipt=verify_receipt(ir,root);raw=encode(lower(ir,receipt));parse_pack(raw);(root/PACK).parent.mkdir(parents=True,exist_ok=True);(root/PACK).write_bytes(raw);return raw
def stage_files(source):
 ir=load();receipt=verify_receipt(ir,source=source);raw=encode(lower(ir,receipt));require((Path(source)/'data/podunk.encfielddetails').read_bytes()==raw,'Changed field details pack');return {Path('data/podunk.encfielddetails'):raw,**{Path(r['path']):(Path(source)/r['path']).read_bytes()for r in ir['resources']}}
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','assets','compile']);p.add_argument('--tex3ds',type=Path);a=p.parse_args()
 if a.action=='extract':d=extract();print('Field descriptions',len(d['definitions']),len(d['resources']))
 elif a.action=='assets':compile_assets(a.tex3ds)
 else:print('Field details bytes',len(compile_pack()))
