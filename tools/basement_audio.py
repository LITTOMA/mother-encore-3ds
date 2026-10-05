"""Source-owned Mick/diary/Present sounds linked into the existing AudioBank."""
from pathlib import Path
import sys
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.drawer_program import read_json,fields,require,safe_path,canonical,write_json
from tools.basement_progression import load as progression_load
from tools.basement_actor_assets import load as actor_load
def bindings(root):
    root=Path(root);ir=progression_load(root);actors=actor_load(root);config=read_json(root/'content/basement-audio-binding.json')
    fields(config,('schema','kind','commit','license_review','shared_assets','assets','music_assets'),'Basement audio binding')
    require(type(config['schema'])is int and config['schema']==1 and config['kind']=='encore.basement-audio.source-binding' and config['commit']==ir['commit']==actors['commit'] and type(config['license_review'])is str and config['license_review'].strip() and actors['licence_review'],'Basement audio source identity/license')
    expected={c['sound_source'] for p in ir['programs'] for c in p['commands'] if c['kind']=='PlaySound'}|{t['voice'] for t in ir['texts'] if t['voice']}|{actors['present_sound']['source']}
    expected_music={row['track']['source_path'][6:]for row in [dict(track=ir['music']['track'])]+ir['music']['additional_regions']}
    phone_rows=read_json(root/'content/phone-linker-bindings.json')['audio'];expected_music-={row['source']for row in phone_rows}
    expected|=expected_music
    fingerprints={**ir['sources'],**actors['sources']}
    require(type(config['assets'])is list and config['assets'] and type(config['shared_assets'])is list,'Basement audio coverage')
    ids=set();paths=set();outputs=set()
    for row in config['assets']+config['shared_assets']+config['music_assets']:
        fields(row,('source','identity','pcm','gain_db','conversion'),'Basement audio row');fields(row['identity'],('kind','value'),'Basement audio identity')
        require(row['identity']['kind']=='stable' and type(row['identity']['value'])is int and 0<row['identity']['value']<2**32 and row['identity']['value']not in ids,'Basement audio stable ID')
        require(safe_path(row['source']) and row['source']in expected and row['source']not in paths and row['source']in fingerprints and row['source']+'.import'in fingerprints,'Basement audio source/import coverage')
        require(row['conversion']is None and type(row['gain_db'])in(int,float) and row['gain_db']==0 and safe_path(row['pcm']) and row['pcm'].startswith('sound/music/'if row['source']in expected_music else'sound/effects/') and row['pcm'].endswith('.pcm') and row['pcm']not in outputs,'Basement audio conversion/gain/path')
        ids.add(row['identity']['value']);paths.add(row['source']);outputs.add(row['pcm'])
    require(paths==expected,'Basement audio source sound omitted')
    from tools.drawer_audio import bindings as drawer_bindings
    existing=drawer_bindings(root)
    require(all(any(canonical(row)==canonical(previous) for previous in existing) for row in config['shared_assets']),'Basement shared audio identity differs from original author binding')
    return config['assets']+config['music_assets']
if __name__=='__main__':
    from tools.phone_linker_bindings import audio
    root=Path(__file__).resolve().parents[1]
    write_json(root/'content/native-audio.json',audio(root,read_json(root/'content/native-opening.json')))
    print('Extracted linked AudioIR with original Mick, diary, Female and Present source sounds')
