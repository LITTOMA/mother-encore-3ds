"""Checked shop sounds; reused menu sources keep their existing bank identities."""
from pathlib import Path
from tools.drawer_program import read_json,fields,require,safe_path
from tools.field_shop import load

def bindings(root):
 root=Path(root);ir=load();config=read_json(root/'content/shop-audio-binding.json')
 fields(config,('schema','kind','commit','license_review','shared_assets','assets'),'Shop audio binding')
 require(config['schema']==1 and config['kind']=='encore.shop-audio.source-binding' and config['commit']==ir['commit'] and config['license_review'],'Shop audio source identity/license')
 expected=set(ir['sounds']);shared=config['shared_assets'];assets=config['assets'];seen=set();ids=set();outputs=set()
 previous=read_json(root/'content/phone-linker-bindings.json')['audio']
 for row in shared:
  fields(row,('source','identity'),'Shop shared audio')
  require(row['source']in expected and row['source']not in seen and any(p['source']==row['source']and p['identity']==row['identity']for p in previous),'Shop shared source identity differs')
  seen.add(row['source'])
 for row in assets:
  fields(row,('source','identity','pcm','gain_db','conversion'),'Shop audio row');fields(row['identity'],('kind','value'),'Shop audio identity')
  require(row['identity']['kind']=='stable'and type(row['identity']['value'])is int and 0<row['identity']['value']<2**32 and row['identity']['value']not in ids,'Shop audio stable identity rejected')
  p=row['source'];require(safe_path(p)and p in expected and p not in seen and p in ir['sources']and p+'.import'in ir['sources'],'Shop source/import coverage rejected')
  require(row['conversion']is None and row['gain_db']==0 and safe_path(row['pcm'])and row['pcm'].startswith('sound/effects/')and row['pcm'].endswith('.pcm')and row['pcm']not in outputs,'Shop audio gain/conversion/path rejected')
  seen.add(p);ids.add(row['identity']['value']);outputs.add(row['pcm'])
 require(seen==expected,'Shop source sound omitted')
 return assets
