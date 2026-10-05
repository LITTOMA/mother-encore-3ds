"""Source-admitted Drawer audio conversion binding; no runtime JSON."""
from pathlib import Path
from tools.drawer_program import load,read_json,fields,require

def bindings(root):
    root=Path(root);ir=load(root);config=read_json(root/'content/drawer-audio-binding.json')
    fields(config,('schema','kind','commit','license_review','assets'),'Drawer audio binding')
    require(type(config['schema']) is int and config['schema']==1 and config['kind']=='encore.drawer-audio.source-binding' and config['commit']==ir['commit'] and config['license_review'],'Drawer audio binding identity/license')
    expected={row['a'] for row in ir['commands'] if row['opcode']=='PlaySound'}
    paths=set();ids=set()
    for row in config['assets']:
        fields(row,('source','identity','pcm','gain_db','conversion'),'Drawer audio row')
        fields(row['identity'],('kind','value'),'Drawer audio identity')
        require(row['identity']['kind']=='stable' and type(row['identity']['value']) is int and 0<row['identity']['value']<2**32 and row['identity']['value'] not in ids,'Drawer audio stable ID')
        require(row['source'] in expected and row['source'] not in paths and row['source'] in ir['sources'] and row['source']+'.import' in ir['sources'],'Drawer audio source')
        require(row['conversion'] is None and type(row['gain_db']) in (int,float) and row['gain_db']==0 and row['pcm'].startswith('sound/effects/') and row['pcm'].endswith('.pcm'),'Unreviewed Drawer conversion/path/gain')
        paths.add(row['source']);ids.add(row['identity']['value'])
    require(paths==expected,'Incomplete Drawer audio coverage')
    return config['assets']
