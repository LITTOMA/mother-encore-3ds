"""Source-admitted sound bindings for the implemented world and battle slice."""
import json, re, struct, sys, zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from tools.extract_battle_entry import Extractor, require
from tools.drawer_program import read_json, write_json, fields, safe_path
ROLES=["MenuOpen","MenuClose","Confirm","Cursor","Back","Error","EnemyTurn","EnemyDefeat","PlayerDefeat","MortalDamage","Smash","Hurt","HurtCritical","Dodge","Miss","Swing","Attack","Hit","LearnedPsi","BossVictory","VictoryLoop","LevelUp","LevelUpLayer","Victory"]
OUT=Path('sound/banks/gameplay.enccues')

def world_sources(root=ROOT):
    root=Path(root);house=read_json(root/'content/native-house.json');mick=read_json(root/'content/mick-treats.json')
    expected={v for d in house['doors']+house['openable_doors'] for k,v in d.items() if k in ('start_sound','end_sound','ram_sound') and v}
    expected.update(s['voice'] for s in house['segments'] if s['voice'])
    expected.update(c['a'].removeprefix('res://') for c in mick['commands'] if c['op']=='PlaySound')
    field=read_json(root/'content/native-field-equipment.json')
    expected.update(s['sound'] for s in field.get('skills',[]) if s['sound'])
    return expected

def extract_world_coverage(root=ROOT):
    root=Path(root);target=root/'content/gameplay-audio-bindings.json';config=read_json(target)
    expected=world_sources(root);candidate=dict(config,extra_sources=sorted(expected))
    # This updates only the coverage join. Selectors, conversion policy, source
    # pin and licence admission remain checked by the existing complete loader.
    previous=target.read_bytes()
    try:
        write_json(target,candidate);load(root)
    except Exception:
        target.write_bytes(previous);raise
    return candidate

def load(root=ROOT):
    root=Path(root); ex=Extractor(root)
    config=read_json(root/'content/gameplay-audio-bindings.json')
    fields(config,('schema','kind','commit','license_review','cues','extra_sources','conversions'),'gameplay audio')
    require(type(config['schema']) is int and config['schema']==1 and config['kind']=='encore.gameplay-audio.bindings' and config['commit']==ex.lock['commit'] and bool(config['license_review']),'Gameplay audio schema/pin/license')
    require(type(config['cues']) is list and [r['role'] for r in config['cues']]==ROLES,'Gameplay audio role coverage/order')
    cues=[]
    for row in config['cues']:
        fields(row,('role','source','dictionary','key'),'gameplay cue')
        require(safe_path(row['source']) and row['dictionary'] in ('_sound_effects','_musical_effects') and re.fullmatch('[A-Za-z0-9_]+',row['key']),'Gameplay audio selector')
        text=ex.text(row['source'])
        match=re.search(r'^var '+re.escape(row['dictionary'])+r'\s*:?=\s*\{(.*?)^}',text,re.M|re.S)
        require(match,'Missing audio dictionary')
        rows=re.findall(r'^\s*"'+re.escape(row['key'])+r'": (?:load\()?"([^"\n]+)"',match[1],re.M)
        require(len(rows)==1,'Missing/ambiguous audio selector')
        source=rows[0].removeprefix('res://') if row['dictionary']=='_sound_effects' else 'Audio/Music/'+rows[0]
        ex.data(source);ex.data(source+'.import');cues.append(source)
    expected=world_sources(root)
    require(type(config['extra_sources']) is list and len(config['extra_sources'])==len(expected) and set(config['extra_sources'])==expected,'World audio coverage mismatch')
    for source in expected:ex.data(source);ex.data(source+'.import')
    import wave
    require(type(config['conversions']) is dict,'Audio conversions')
    for source,conversion in config['conversions'].items():
        fields(conversion,('source_sample_rate','output_sample_rate'),'audio conversion')
        require(source in set(cues)|expected and source.endswith('.wav'),'Unbound audio conversion')
        with wave.open(str(ex.upstream/source),'rb') as stream:require(stream.getframerate()==conversion['source_sample_rate'] and 48000<stream.getframerate()<=192000 and conversion['output_sample_rate']==48000,'Audio conversion rate mismatch')
    return cues, sorted(expected), ex

def bindings(root=ROOT):
    cues,extra,_=load(root)
    # Stable source-derived IDs avoid reassigning sounds when the list grows.
    existing=read_json(Path(root)/'content/native-audio.json')['assets']
    existing_paths={a['source_path'].removeprefix('res://') for a in existing if a['stable_id']<0x80000000}
    conversions=read_json(Path(root)/'content/gameplay-audio-bindings.json')['conversions']
    result=[]
    for source in sorted(set(cues+extra)-existing_paths):
        identity=zlib.crc32(source.encode())|0x80000000
        music=source.startswith('Audio/Music/')
        result.append(dict(source=source,identity=dict(kind='stable',value=identity),pcm='sound/'+('music/' if music else 'effects/')+'gameplay-'+str(identity)+'.pcm',gain_db=0,conversion=conversions.get(source)))
    return result

def encode(cues):
    require(type(cues) is list and len(cues)==len(ROLES),'Cue count')
    payload=bytearray()
    for cue in cues:
        require(safe_path(cue) and cue.startswith('Audio/') and len(cue)<=256,'Cue source path')
        raw=cue.encode('ascii');payload+=struct.pack('<I',len(raw))+raw
    return struct.pack('<8s4I',b'ENCCUE01',1,24+len(payload),zlib.crc32(payload),len(cues))+payload

def stage_files(source):
    blob=encode(load()[0]);require((Path(source)/OUT).read_bytes()==blob,'Stale gameplay audio cues');return {OUT:blob}

if __name__=='__main__':
    import argparse
    parser=argparse.ArgumentParser();parser.add_argument('action',choices=('extract','compile'),nargs='?',default='compile');args=parser.parse_args()
    if args.action=='extract':extract_world_coverage();print('Gameplay world audio coverage extracted and admitted')
    cues,_,_=load();out=ROOT/'romfs'/OUT;out.parent.mkdir(parents=True,exist_ok=True);out.write_bytes(encode(cues));print('Compiled gameplay sound cues:',len(cues))
