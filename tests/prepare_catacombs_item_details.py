"""Owned v1/v2 parser fixtures from historical IR and actual converted assets."""
import copy,hashlib,json,struct,sys,zlib
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import item_details as details,catacombs_item_catalog as catalog,native_items
from tools.extract_battle_entry import Extractor,require,PIN


def build(output,texture_base=ROOT):
    output=Path(output).resolve();texture_base=Path(texture_base).resolve()
    require(ROOT/'build'in output.parents and not output.is_symlink(),'Details fixture must stay under build')
    require(texture_base==ROOT or ROOT/'build'in texture_base.parents,'Details texture candidate ownership rejected')
    legacy=ROOT/'tests/fixtures/item-admission-legacy'
    proof=json.loads((legacy/'source.json').read_text())
    for source in ('content/native-items.json','content/native-item-details.json','content/asset-receipts/graphics/ui/item-details/source.json'):
        row=proof['files'][source]
        require(details.digest(ROOT/row['fixture'])==row['sha256'],'Historical details fixture changed')
    old_items=catalog.read(legacy/'native-items.json');old_ir=catalog.read(legacy/'native-item-details.json')
    items=catalog.append_item(Extractor(ROOT),old_items);ir=details.build(ROOT,items)
    require(catalog.read(texture_base/details.IR)==ir,'Candidate details textures refer to other IR')
    receipt=catalog.read(texture_base/details.RECEIPT)
    require(set(receipt)==set('schema commit ir_sha256 producer_sha256 tex3ds_sha256 outputs'.split())
            and receipt['schema']==1 and receipt['commit']==PIN and receipt['ir_sha256']==details.digest(texture_base/details.IR)
            and receipt['producer_sha256']==details.digest(ROOT/'tools/item_details.py')
            and receipt['tex3ds_sha256']=='58cecde429b2c4d1b4e09ae85b1f3b6851bf78914117d0e7bf8f1ba1a0a47c9e',
            'Candidate details conversion provenance rejected')
    require(set(receipt['outputs'])=={r['path']for r in ir['resources']},'Candidate details texture coverage rejected')
    files={}
    for resource in ir['resources']:
        raw=(texture_base/'romfs'/resource['path']).read_bytes()
        require(receipt['outputs'][resource['path']]==dict(bytes=len(raw),sha256=hashlib.sha256(raw).hexdigest(),crc32=zlib.crc32(raw)),
                'Candidate details converted bytes changed')
        files[resource['path']]=raw
        files['corrupt-textures/'+resource['path']]=raw if resource['id']==1 else raw[:-1]+bytes([raw[-1]^1])
        if resource['id']==1:files['missing-textures/'+resource['path']]=raw
    tables=details.lower(ir,receipt);blob=details.encode(tables)
    require(struct.unpack_from('<I',blob,8)[0]==2,'Expected v2 details candidate')
    files['candidate.encdetails']=blob;files['candidate.encitems']=native_items.encode(native_items.lower(items))
    old_receipt=catalog.read(legacy/'details-texture-receipt.json')
    files['legacy.encdetails']=details.encode(details.lower(old_ir,old_receipt),version=1)
    files['legacy.encitems']=native_items.encode(native_items.lower(old_items))
    for kind in ('items','details'):
        require(hashlib.sha256(files['legacy.enc'+kind]).hexdigest()==proof['files']['romfs/ui/opening.enc'+kind]['sha256'],
                'Legacy resource is not byte-identical to its actual historical pack')
    for presentation in ir['presentations']:
        if presentation['definition']<4:continue
        d=next(v for v in ir['definitions']if v['id']==presentation['definition'])
        expected=presentation['raw'].replace('\\n','').replace('[BR]','').replace('[ItemValue]',str(d['item_value'])).replace('[Poisoned]','<image1>')
        require(not any(c in expected for c in '\n\r\t[]'),'Unreviewed expected description controls')
        files[f"expected-{d['id']}-{presentation['locale']}.txt"]=expected.encode('utf-8')
    bad=[]
    def corrupt(label,offset,value,original=blob):
        raw=bytearray(original);struct.pack_into('<I',raw,offset,value);struct.pack_into('<I',raw,16,0)
        struct.pack_into('<I',raw,16,zlib.crc32(raw));bad.append(label);files[f'bad-{len(bad)-1}.encdetails']=bytes(raw)
    corrupt('unknown version',8,3);corrupt('v2 cannot claim old capability',24,1)
    raw=bytearray(blob);struct.pack_into('<I',raw,8,1);corrupt('v1 cannot claim v2 contents',24,1,raw)
    tokens=struct.unpack_from('<I',blob,64+4*16+4)[0]
    percents=[i for i,t in enumerate(tables['Tokens'])if t[0]==7]
    require(len(percents)==2,'Original bilingual percentage coverage changed')
    for i in percents:
        corrupt('unknown opcode',tokens+i*16,8);corrupt('percent unused operand',tokens+i*16+4,1)
    for i,t in enumerate(tables['Tokens']):
        if t[0]==4:corrupt('image outside resources',tokens+i*16+4,len(tables['Resources']))
    dose_presentation=next(v for v in tables['Presentations']if v[0]==1)
    dose_base=bytearray(blob);struct.pack_into('<I',dose_base,tokens+dose_presentation[2]*16+4,0)
    corrupt('multidose literal percent',tokens+dose_presentation[2]*16,7,dose_base)
    resources=struct.unpack_from('<I',blob,64+5*16+4)[0]
    corrupt('duplicate resource identity',resources+68,tables['Resources'][0][0])
    corrupt('duplicate resource path',resources+68+4,tables['Resources'][0][1])
    files['negative-count.txt']=str(len(bad)).encode()
    manifest=dict(schema=1,scope='Owned parser/composition fixture; no game save or scene state injected',
                  negative_cases=bad,legacy_source=proof['source'],
                  producers={name:details.digest(ROOT/name)for name in ['tools/item_details.py','tests/prepare_catacombs_item_details.py','include/encore/item_details.hpp','runtime/item_details.cpp']},
                  files={name:hashlib.sha256(raw).hexdigest()for name,raw in files.items()})
    files['manifest.json']=(json.dumps(manifest,indent=2)+'\n').encode()
    marker=output/'.owned-item-details-fixture'
    if output.exists():
        require(marker.is_file()and marker.read_text()=='1\n','Unowned details fixture')
        existing={str(p.relative_to(output)).replace('\\','/')for p in output.rglob('*')if p.is_file()}
        require(existing==set(files)|{marker.name},'Unexpected details fixture files')
        require(all(not(output/name).is_symlink()and(output/name).read_bytes()==raw for name,raw in files.items()),'Changed details fixture')
    else:
        output.mkdir(parents=True);marker.write_text('1\n')
        for name,raw in files.items():
            path=output/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(raw)
    return manifest


if __name__=='__main__':
    result=build(sys.argv[1],sys.argv[2]if len(sys.argv)>2 else ROOT)
    print(json.dumps(dict(files=len(result['files']),negative_cases=len(result['negative_cases']))))
