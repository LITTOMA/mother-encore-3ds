"""Source-admitted extra Pause sounds; shared equip/menu sounds stay singletons."""
from pathlib import Path
from tools.drawer_program import read_json, fields, require, safe_path
from tools.field_equipment import load

def bindings(root):
    root=Path(root);ir=load(root);config=read_json(root/'content/field-audio-binding.json')
    fields(config,('schema','kind','commit','license_review','assets'),'Field audio binding')
    require(type(config['schema']) is int and config['schema']==1 and config['kind']=='encore.field-audio.source-binding' and config['commit']==ir['commit'] and type(config['license_review']) is str and config['license_review'].strip(),'Field audio identity/license')
    expected={ir['bindings'][k]['en'] for k in ('PauseOpenSound','PauseCloseSound')}
    require(type(config['assets']) is list and len(config['assets'])==len(expected),'Field audio coverage')
    paths=set();ids=set();outputs=set()
    for row in config['assets']:
        fields(row,('source','identity','pcm','gain_db','conversion'),'Field audio row');fields(row['identity'],('kind','value'),'Field audio identity')
        require(row['identity']['kind']=='stable' and type(row['identity']['value']) is int and 0<row['identity']['value']<2**32 and row['identity']['value'] not in ids,'Field audio stable ID')
        require(safe_path(row['source']) and row['source'] in expected and row['source'] not in paths and row['source'] in ir['sources'] and row['source']+'.import' in ir['sources'],'Field audio source')
        require(row['conversion'] is None and type(row['gain_db']) in (int,float) and row['gain_db']==0 and safe_path(row['pcm']) and row['pcm'].startswith('sound/effects/') and row['pcm'].endswith('.pcm') and row['pcm'] not in outputs,'Field audio conversion/path/gain')
        paths.add(row['source']);ids.add(row['identity']['value']);outputs.add(row['pcm'])
    require(paths==expected,'Field audio incomplete coverage')
    from tools.extension_audio import bindings as extension_audio
    return config['assets']+extension_audio(root)

if __name__=='__main__':
    from tools.phone_linker_bindings import audio
    from tools.drawer_program import write_json
    root=Path(__file__).resolve().parents[1]
    write_json(root/'content/native-audio.json',audio(root,read_json(root/'content/native-opening.json')))
    print('Extracted checked audio IR with original Pause sound sources')
