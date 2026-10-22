"""Actual source-sort observations and independently encoded storage fixtures."""
import copy,hashlib,json,struct,sys,zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import inventory_sort_reference as reference,catacombs_item_catalog as catalog,native_storage as storage,storage_assets,native_items
from tools.extract_battle_entry import Extractor,require


def build(output):
    output=Path(output).resolve();require(ROOT/'build'in output.parents and not output.is_symlink(),'Owned storage fixture required')
    legacy=ROOT/'tests/fixtures/item-admission-legacy';proof=catalog.read(legacy/'source.json')
    for name in ('content/native-items.json','content/native-storage.json','content/asset-receipts/graphics/ui/storage/source.json'):
        row=proof['files'][name];require(catalog.digest(ROOT/row['fixture'])==row['sha256'],'Historical storage fixture changed')
    old_items=catalog.read(legacy/'native-items.json');items=catalog.append_item(Extractor(ROOT),old_items)
    ir=storage_assets.build(ROOT,items);receipt=catalog.read(legacy/'storage-texture-receipt.json')
    old_ir=catalog.read(legacy/'native-storage.json')
    require(ir['resources']==old_ir['resources'],'Candidate storage must retain exact old visual resources')
    for path,row in receipt['outputs'].items():
        require((ROOT/'romfs'/path).stat().st_size==row['bytes']and catalog.digest(ROOT/'romfs'/path)==row['sha256'],
                'Historical storage texture changed')
    tables=storage.lower(ir,receipt);blob=storage.encode(tables)
    files={'candidate.encitems':native_items.encode(native_items.lower(items)),'candidate.encstorage':blob,
           'legacy.encitems':native_items.encode(native_items.lower(old_items)),
           'legacy.encstorage':storage.encode(storage.lower(old_ir,receipt),version=1)}
    for suffix in ('items','storage'):
        require(hashlib.sha256(files['legacy.enc'+suffix]).hexdigest()==proof['files']['romfs/ui/opening.enc'+suffix]['sha256'],
                'Historical storage/items binary bytes changed')
    document,inputs=reference.read(ROOT/'reports/inventory-sort/original-1')
    second,_=reference.read(ROOT/'reports/inventory-sort/original-2')
    require(document==second,'Actual original sorting outcomes differ')
    lines=[f"ENCSORT1 1 {len(document['rows'])} {len(document['pairs'])}"]
    for row in document['pairs']:lines.append(f"{row['locale']} {row['a']} {row['b']} {int(row['less'])}")
    for row in document['rows']:
        lines.append(f"{row['locale']} {row['index']} {len(row['before'])}")
        lines.extend(f"{item['item_name']} {item['uid']} {item['doses']}"for item in row['before'])
        for state in ('once','twice'):lines.append(' '.join(str(item['uid'])for item in row[state]))
    raw=('\n'.join(lines)+'\n').encode();files['original.txt']=raw
    files['bad-observation-0.txt']=raw.replace(b'ENCSORT1 1 ',b'ENCSORT1 2 ',1)
    files['bad-observation-1.txt']=raw.replace(b'ENCSORT1 1 584 ',b'ENCSORT1 1 1 ',1)
    files['bad-observation-2.txt']=raw[:-16];files['bad-observation-3.txt']=raw+b'unknown\n'
    files['bad-observation-4.txt']=raw.replace(b'BaseballCap',b'UnknownItem',1)
    files['negative-observation-count.txt']=b'5'
    bad=[]
    def corrupt(label,offset,value,original=blob):
        raw=bytearray(original);struct.pack_into('<I',raw,offset,value);struct.pack_into('<I',raw,16,0)
        struct.pack_into('<I',raw,16,zlib.crc32(raw));bad.append(label);files[f'bad-{len(bad)-1}.encstorage']=bytes(raw)
    corrupt('unknown resource version',8,3);corrupt('v2 cannot claim old capability',20,1)
    downgraded=bytearray(blob);struct.pack_into('<I',downgraded,8,1)
    corrupt('v1 cannot admit repeated source grants',20,1,downgraded)
    offset=struct.unpack_from('<I',blob,68)[0]
    policies=struct.unpack_from('<I',blob,64+16+4)[0]
    for value in (0,81):corrupt('invalid source quantity',policies+3*4,value)
    corrupt('duplicate English rank',policies+24+4*4,tables['Policies'][0][4])
    corrupt('duplicate Chinese rank',policies+24+5*4,tables['Policies'][0][5])
    corrupt('duplicate item definition',policies+24,tables['Policies'][0][0])
    files['negative-count.txt']=str(len(bad)).encode()
    manifest=dict(schema=1,scope='New model with exact existing visual resources; original sorting observations only, no played save or release authority',
                  original_sha256=catalog.digest(ROOT/'reports/inventory-sort/original-1/reference.json'),
                  legacy_source=proof['source'],negative_cases=bad,
                  producers={path:catalog.digest(ROOT/path)for path in ['tools/native_storage.py','tools/storage_assets.py','runtime/storage_data.cpp','tests/prepare_catacombs_storage.py','tools/inventory_sort_reference.py']},
                  files={path:hashlib.sha256(raw).hexdigest()for path,raw in files.items()})
    files['manifest.json']=(json.dumps(manifest,indent=2)+'\n').encode()
    marker=output/'.owned-catacombs-storage-fixture'
    if output.exists():
        require(marker.is_file()and marker.read_text()=='1\n'and {p.name for p in output.iterdir()}==set(files)|{marker.name},'Unexpected storage fixture files')
        require(all(not(output/name).is_symlink()and(output/name).read_bytes()==raw for name,raw in files.items()),'Changed storage fixture')
    else:
        output.mkdir(parents=True);marker.write_text('1\n')
        for name,raw in files.items():(output/name).write_bytes(raw)
    return manifest


if __name__=='__main__':
    result=build(sys.argv[1]);print(json.dumps(dict(files=len(result['files']),negative_cases=len(result['negative_cases']))))
