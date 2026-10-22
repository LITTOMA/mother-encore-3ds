"""Execute complete original Inventory/Item sorting with explicit translated input.

This is a method observation, not a game save/scene or original UID/RNG service.
No source file or earlier consumption observer is modified.
"""
from __future__ import annotations
import argparse,csv,hashlib,io,json,random,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import run_item_consume_reference as original,catacombs_item_catalog as catalog
from tools.extract_battle_entry import Extractor,require

PROBE=r'''extends Node
func _ready():call_deferred("run")
func run():
    var fixture=globaldata.fixture
    for locale in fixture.translations:
        var translation=Translation.new()
        translation.locale=locale
        for key in fixture.translations[locale]:translation.add_message(key,fixture.translations[locale][key])
        TranslationServer.add_translation(translation)
    var pairs=[]
    var rows=[]
    for locale in fixture.locales:
        TranslationServer.set_locale(locale)
        print("SORT_LOCALE "+JSON.print({"requested":locale,"actual":TranslationServer.get_locale()}))
        assert(TranslationServer.get_locale()==locale)
        var comparator=Inventory.new(Inventory.InvType.STORAGE)
        for key in fixture.translations[locale]:assert(comparator.tr(key)==fixture.translations[locale][key])
        for a in fixture.names:
            for b in fixture.names:
                var first=Item.new(a,false,int(fixture.items[a].get("doses",1)),1000)
                var second=Item.new(b,false,int(fixture.items[b].get("doses",1)),1001)
                pairs.append({"locale":locale,"a":a,"b":b,"less":comparator._sort_auto(first,second)})
        for row in fixture.cases:
            var inventory=Inventory.new(Inventory.InvType.STORAGE)
            for index in range(row.names.size()):
                var name=row.names[index]
                var doses=int(row.spray_doses) if name==fixture.spray else int(fixture.items[name].get("doses",1))
                assert(inventory.add_item(Item.new(name,false,doses,1000+index)))
            var before=inventory.to_array()
            inventory.sort_auto()
            var once=inventory.to_array()
            inventory.sort_auto()
            var twice=inventory.to_array()
            rows.append({"locale":locale,"index":row.index,"before":before,"once":once,"twice":twice})
    var file=File.new()
    assert(file.open("res://../reference.json",File.WRITE)==OK)
    file.store_string(JSON.print({"schema":1,"engine":Engine.get_version_info(),"pairs":pairs,"rows":rows},"  "))
    file.close()
    print("INVENTORY_SORT_REFERENCE_COMPLETE "+str(rows.size()))
    get_tree().quit()
'''


def source():
    files,unused,inherit=original.source()
    files.pop('Target.gd')
    ex=Extractor(ROOT)
    items_ir=catalog.read(ROOT/'tests/fixtures/item-admission-legacy/native-items.json')
    names=[row['source']for row in items_ir['definitions']]+[row['source']for row in catalog.load()['entries']]
    items={name:ex.yaml('Data/Items/'+name+'.yaml')for name in names}
    locales=(('en','en'),('zh_Hans_CN','zh_CN'))
    translations={locale:{}for locale,column in locales}
    source_rows=list(csv.DictReader(io.StringIO(ex.text('Translations/TranslatedText/items - sheet.csv'))))
    require(len({row['key']for row in source_rows})==len(source_rows),'Duplicate inventory sort translation key')
    table={row['key']:row for row in source_rows}
    for document in items.values():
        key=document['sorting_name']
        for locale,column in locales:
            require(key in table and table[key][column],'Missing inventory sorting translation')
            translations[locale][key]=table[key][column]
    cases=[]
    def add(sequence,doses=3):cases.append(dict(index=len(cases),names=list(sequence),spray_doses=doses))
    for sequence in (names,list(reversed(names))):
        for offset in range(len(sequence)):add(sequence[offset:]+sequence[:offset])
    grants={row['source']:len(row['grants'])for row in catalog.load()['entries']}
    copies=[name for name in names for _ in range(grants.get(name,1))]
    require(len(copies)==11 and names[1]=='AsthmaSpray','Inventory sort reviewed source scope changed')
    for sequence in (copies,list(reversed(copies))):
        for offset in range(len(sequence)):add(sequence[offset:]+sequence[:offset])
    generator=random.Random(0)
    for index in range(256):
        sequence=list(copies);generator.shuffle(sequence);add(sequence,index%items[names[1]]['doses']+1)
    payload=json.loads(files['input.json'])
    payload.update(items=items,translations=translations,locales=list(translations),names=names,cases=cases,spray=names[1])
    files['input.json']=json.dumps(payload,ensure_ascii=False)
    files['probe.gd']=PROBE
    files['project.godot']=files['project.godot'].replace('config/name="Original item consumption"','config/name="Original inventory sorting"')
    proof=dict(commit=inherit['commit'],sources=dict(sorted({**inherit['sources'],**ex.sources}.items())),
               inherited_observer_producer_sha256=catalog.digest(original.__file__),
               inherited_analyzer_adaptations=inherit['generated_sha256'],
               generated_sha256={name:hashlib.sha256(raw.encode()).hexdigest()for name,raw in files.items()},
               pair_cases=len(names)**2*len(translations),inventory_cases=len(cases)*len(translations),
               scope=__doc__,input_shuffle='Python Random(0) constructs method inputs only; no original gameplay RNG claim')
    return files,proof


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--godot',type=Path,required=True);parser.add_argument('--out',type=Path,required=True)
    args=parser.parse_args();out=args.out.resolve()
    require(catalog.digest(args.godot)=='e748acc92ee5205dd3cc1754d4422870f6a68ae6e00a16ff88bd3438c957adfd','Pinned original engine changed')
    require(ROOT/'build'in out.parents and not out.exists(),'Fresh owned inventory sort output required')
    files,proof=source();project=out/'project';project.mkdir(parents=True)
    for name,raw in files.items():(project/name).write_text(raw,encoding='utf-8',newline='')
    catalog.write(out/'inputs.json',proof)
    original.invoke(args.godot,project,['--video-driver','GLES2','--debug','--fixed-fps','60','--disable-render-loop'],out/'godot.log')
    require(all((project/name).read_bytes()==raw.encode()for name,raw in files.items()),'Executed inventory source changed')
    reference=catalog.read(out/'reference.json')
    require(reference['schema']==1 and reference['engine']['hash']=='3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8'
            and len(reference['pairs'])==proof['pair_cases']and len(reference['rows'])==proof['inventory_cases'],
            'Incomplete original sorting reference')
    receipt=dict(source=proof,engine_sha256=catalog.digest(args.godot),producer_sha256=catalog.digest(__file__),reference_sha256=catalog.digest(out/'reference.json'))
    catalog.write(out/'receipt.json',receipt)
    print(json.dumps(dict(pairs=len(reference['pairs']),rows=len(reference['rows']),idempotent=sum(row['once']==row['twice']for row in reference['rows']),reference_sha256=receipt['reference_sha256'])))


if __name__=='__main__':main()
