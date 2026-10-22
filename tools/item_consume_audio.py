"""Admit the original target-selector eat effect without retuning the old bank."""
import argparse,re,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools import item_consumption as consume,catacombs_item_catalog as catalog
from tools.extract_battle_entry import Extractor,PIN,require

IR=Path('content/item-consume-audio.json')


def validate(document,model,source_bus):
    consume.fields(document,'schema kind commit consumption_ir consumption_sha256 source_slot source_bus identity pcm gain_db conversion license_review'.split(),'audio binding')
    require(type(document['schema'])is int and document['schema']==1
            and document['kind']=='encore.item-consume-audio.source-binding'and document['commit']==PIN
            and document['consumption_ir']==consume.IR.as_posix()
            and type(document['consumption_sha256'])is str and re.fullmatch('[0-9a-f]{64}',document['consumption_sha256'])
            and int(document['consumption_sha256'],16)>0,'Consume audio schema/source binding rejected')
    require(document['source_slot']==model['slot']and document['source_bus']==source_bus
            and model['lane']==1,'Consume audio source slot/bus/lane differs')
    consume.fields(document['identity'],('kind','value'),'audio identity')
    require(document['identity']['kind']=='stable'and type(document['identity']['value'])is int
            and 0<document['identity']['value']<2**32,'Consume audio stable identity rejected')
    require(type(document['pcm'])is str and re.fullmatch(r'sound/effects/[a-z0-9-]+\.pcm',document['pcm'])
            and type(document['gain_db'])in(int,float)and document['gain_db']==0
            and document['conversion']is None and type(document['license_review'])is str
            and document['license_review'].strip(),'Consume audio PCM/gain/conversion/permission rejected')
    return document


def source(root):
    root=Path(root);model=consume.load();ex=Extractor(root)
    manager=consume.original.source_methods(ex.data('Scripts/global/audioManager.gd'))
    require('return $Sfx.get_node_or_null(node_name) as AudioStreamPlayer'in manager['get_sfx']
            and 'var sfx_node: AudioStreamPlayer = get_sfx(node_name)'in manager['add_sfx']
            and 'sfx_node.name = node_name'in manager['add_sfx']
            and '$Sfx.add_child(sfx_node)'in manager['add_sfx']
            and 'sfx_node.stream = stream'in manager['add_sfx']
            and 'var sfx_node: AudioStreamPlayer = add_sfx(stream, node_name)'in manager['play_sfx']
            and 'sfx_node.play()'in manager['play_sfx'],'Consume audio source playback ownership changed')
    buses=re.findall(r'sfx_node.bus = "([^"\n]+)"',manager['add_sfx'])
    require(buses==['SFX'],'Unreviewed consume effect bus')
    scene=ex.text('Nodes/Ui/audioManager.tscn')
    require('[node name="Sfx" type="Node" parent="."]'in scene
            and not re.search(r'\[node [^\n]* parent="Sfx"\]',scene),'Unreviewed preexisting source effect node')
    ex.data(model['sound']);ex.data(model['sound']+'.import')
    require("purpose you're using them is related to the game"in ex.text('LICENSE'),'Consume audio source permission changed')
    return model,buses[0]


def bindings(root,document=None):
    root=Path(root);model,bus=source(root)
    config=catalog.read(root/IR)if document is None else document
    validate(config,model,bus)
    require(config['consumption_sha256']==catalog.digest(root/consume.IR),'Consume audio recipe fingerprint differs')
    return [dict(source=model['sound'],identity=config['identity'],pcm=config['pcm'],gain_db=config['gain_db'],conversion=None)]


def extract(root,stable_id):
    root=Path(root);model,bus=source(root)
    existing=catalog.read(root/'content/native-audio.json')['assets']
    require(type(stable_id)is int and 0<stable_id<2**32
            and not any(row['stable_id']==stable_id or row['source_path']=='res://'+model['sound']for row in existing),
            'Consume audio requires a new stable identity and source')
    config=dict(schema=1,kind='encore.item-consume-audio.source-binding',commit=PIN,
                consumption_ir=consume.IR.as_posix(),consumption_sha256=catalog.digest(root/consume.IR),
                source_slot=model['slot'],source_bus=bus,identity=dict(kind='stable',value=stable_id),
                pcm='sound/effects/item-eat.pcm',gain_db=0,conversion=None,
                license_review='Fixed upstream LICENSE allows game-related modifications; retain Team Encore notices. Original eat.wav and its import settings remain source-owned, not MIT.')
    validate(config,model,bus);catalog.write(root/IR,config);return config


if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('action',choices=('extract','verify'));parser.add_argument('--stable-id',type=int)
    args=parser.parse_args()
    if args.action=='extract':extract(consume.ROOT,args.stable_id)
    else:bindings(consume.ROOT)
