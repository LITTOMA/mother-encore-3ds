"""Admit the source-node sound once; each live prop keeps its own playback."""
import json,re
from pathlib import Path
from tools import field_props as props
from tools.extract_battle_entry import require,Extractor

def bindings(root):
 root=Path(root);p=json.loads((root/'content/field-prop-audio.json').read_text(encoding='utf8'),object_pairs_hook=props.unique)
 require(type(p)is dict and set(p)==set('schema kind commit props_recipe identity pcm gain_db conversion license_review'.split()),'Prop audio binding fields')
 require(type(p['schema'])is int and p['schema']==1 and p['kind']=='encore.field-prop-audio.source-binding'and p['commit']==props.f.PIN and type(p['license_review'])is str and p['license_review'].strip(),'Prop audio binding identity/permission')
 require(type(p['props_recipe'])is str and re.fullmatch(r'content/[a-z0-9-]+\.json',p['props_recipe'])is not None,'Prop audio recipe path')
 recipe=props.read(root/p['props_recipe']);require(recipe['schema']==3,'Prop audio requires checked node/contact metadata')
 data,_,proof=props.source(root/p['props_recipe']);sound=data['policy']['sound'];contact=data['policy']['contact']
 require(contact['sound_sha256']==proof['sources'][sound]and contact['bus']==1 and contact['pause_mode']==0,'Prop audio source companion differs')
 identity=p['identity'];require(type(identity)is dict and set(identity)=={'kind','value'}and identity['kind']=='stable'and type(identity['value'])is int and 0<identity['value']<2**32,'Prop audio stable identity')
 require(type(p['pcm'])is str and re.fullmatch(r'sound/effects/[a-z0-9-]+\.pcm',p['pcm'])is not None and type(p['gain_db'])in(int,float)and p['gain_db']==0 and p['conversion']is None,'Prop audio PCM/gain/conversion')
 ex=Extractor(root);ex.data(sound);ex.data(sound+'.import');require("purpose you're using them is related to the game"in ex.text('LICENSE'),'Prop audio upstream permission changed')
 return [dict(source=sound,identity=identity,pcm=p['pcm'],gain_db=p['gain_db'],conversion=None)]
