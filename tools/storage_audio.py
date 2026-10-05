"""Source-admitted menu sounds required by ordinary Storage."""
from pathlib import Path
from tools.drawer_program import read_json, fields, require, safe_path
from tools.storage_assets import load

def bindings(root):
    root=Path(root);ir=load(root);config=read_json(root/'content/storage-audio-binding.json')
    fields(config,('schema','kind','commit','license_review','assets'),'Storage audio binding')
    require(type(config['schema'])is int and config['schema']==1 and config['kind']=='encore.storage-audio.source-binding' and config['commit']==ir['commit'] and type(config['license_review'])is str and config['license_review'].strip(),'Storage audio identity/license')
    expected={ir['bindings'][k]for k in ('ClearSound','EquipSound')}
    require(type(config['assets'])is list and len(config['assets'])==len(expected),'Storage audio coverage')
    paths=set();ids=set();outputs=set()
    for row in config['assets']:
        fields(row,('source','identity','pcm','gain_db','conversion'),'Storage audio row');fields(row['identity'],('kind','value'),'Storage audio identity')
        require(row['identity']['kind']=='stable' and type(row['identity']['value'])is int and 0<row['identity']['value']<2**32 and row['identity']['value'] not in ids,'Storage audio stable ID')
        require(safe_path(row['source']) and row['source']in expected and row['source']not in paths and row['source']in ir['sources'] and row['source']+'.import'in ir['sources'],'Storage audio source')
        require(row['conversion']is None and type(row['gain_db'])in(int,float) and row['gain_db']==0 and safe_path(row['pcm']) and row['pcm'].startswith('sound/effects/') and row['pcm'].endswith('.pcm') and row['pcm']not in outputs,'Storage audio conversion/path/gain')
        paths.add(row['source']);ids.add(row['identity']['value']);outputs.add(row['pcm'])
    require(paths==expected,'Storage audio incomplete coverage');return config['assets']
