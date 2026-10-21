"""Checked original hole/factory binding and independent existing metadata."""
import argparse,hashlib,json,re,struct,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import catacombs_poison as p,catacombs_geyser as g
from tools.extract_battle_entry import PIN,require
from tools.reference_movement import extract
from tools.dungeon_keys import fields
from tools.dungeon_key_indicator import unique
RECIPE=ROOT/'content/catacombs-geyser-lifecycle.json'
MAGIC=b'ENCGLF01';STRIDES=(1,32,1,1);LIMIT=524288

def source(file=RECIPE):
 recipe=json.loads(Path(file).read_bytes(),object_pairs_hook=unique)
 fields(recipe,'schema kind commit scope poison_recipe poison_recipe_sha256 geyser_recipe geyser_recipe_sha256 hole_source hole_sha256 timeout_sha256 license_review','geyser lifecycle recipe')
 require(type(recipe['schema'])is int and recipe['schema']==1 and recipe['kind']=='encore.catacombs-geyser-lifecycle.reviewed-source'and recipe['commit']==PIN,'Lifecycle reviewed identity')
 paths=[]
 for key in ['poison_recipe','geyser_recipe']:
  g.safe(recipe[key]);path=(ROOT/recipe[key]).resolve();require(path.is_relative_to(ROOT/'content')and not path.is_symlink()and hashlib.sha256(path.read_bytes()).hexdigest()==recipe[key+'_sha256'],'Lifecycle dependency recipe changed');paths.append(path)
 pr,pm,pp=p.source(paths[0]);gr,gm,gp=g.source(paths[1]);s=p.f.Sources();g.safe(recipe['hole_source']);raw=s.data(recipe['hole_source']);require(recipe['hole_source']==pr['paths']['hole_script']and hashlib.sha256(raw).hexdigest()==recipe['hole_sha256'],'Lifecycle original hole source changed');methods,digests=extract(raw,recipe['hole_sha256'],['_on_Timer_timeout']);require(digests['_on_Timer_timeout']==recipe['timeout_sha256'],'Lifecycle timeout method changed')
 factories=re.findall(r'^onready var poison = preload\("res://([^"\n]+)"\)$',raw.decode(),re.M);parents=re.findall(r'get_node\("([^"\n]+)"\)\.call_deferred\("add_child", newPoison\)',methods)
 require(factories==[pr['paths']['geyser']]==[gr['paths']['scene']]==[gm['source']]and len(parents)==1,'Lifecycle actual factory/parent binding changed');g.safe(parents[0])
 model=dict(scene=pm['scene'],source=pm['source'],prefab=factories[0],parent=parents[0],poison=p.encode(pm),geyser=g.encode(gm));validate(model)
 proof=dict(commit=PIN,recipe_sha256=hashlib.sha256(Path(file).read_bytes()).hexdigest(),poison=pp,geyser=gp,hole_sha256=recipe['hole_sha256'],timeout_sha256=recipe['timeout_sha256'],scope=recipe['scope'])
 return recipe,model,proof

def validate(model):
 fields(model,'scene source prefab parent poison geyser','lifecycle model');g.integer(model['scene'],1,65535)
 for key in ['source','prefab','parent']:g.safe(model[key])
 require(type(model['poison'])is bytes and type(model['geyser'])is bytes,'Lifecycle nested metadata types');poison=p.parse(model['poison']);geyser=g.parse(model['geyser']);require(poison['scene']==model['scene']and poison['source']==model['source']and geyser['source']==model['prefab'],'Lifecycle source/scene mismatch')

def encode(model):
 validate(model);pool=bytearray();spans=[]
 for key in ['source','prefab','parent']:
  raw=model[key].encode('ascii');spans.extend([len(pool),len(raw)]);pool.extend(raw)
 policy=struct.pack('<8I',*spans,0,0)
 return p.f.pack_container(MAGIC,model['scene'],[bytes(pool),policy,model['poison'],model['geyser']],STRIDES,LIMIT)

def parse(blob):
 scene,sections=p.f.unpack_container(blob,MAGIC,STRIDES,LIMIT);pool,policy,poison,geyser=sections
 require(1<=scene<=65535 and 0<len(pool)<=1536 and len(policy)==32 and 0<len(poison)<=262144 and 0<len(geyser)<=65536,'Lifecycle section domain');values=struct.unpack('<8I',policy);require(not any(values[6:]),'Lifecycle reserved policy');used=bytearray(len(pool));strings=[]
 for first,length in zip(values[:6:2],values[1:6:2]):
  require(0<length<=512 and first<=len(pool)and length<=len(pool)-first and not any(used[first:first+length]),'Lifecycle text span/alias');value=pool[first:first+length].decode('ascii');g.safe(value);used[first:first+length]=b'\1'*length;strings.append(value)
 require(all(used),'Lifecycle unowned text');model=dict(scene=scene,source=strings[0],prefab=strings[1],parent=strings[2],poison=poison,geyser=geyser);validate(model);return model

if __name__=='__main__':
 a=argparse.ArgumentParser(description=__doc__);a.add_argument('--out',type=Path,required=True);o=a.parse_args();_,model,_=source();blob=encode(model);require(parse(blob)==model,'Lifecycle binary roundtrip');dest=o.out.resolve();require(dest.is_relative_to(ROOT/'build')and dest!=ROOT/'build','Lifecycle candidate output must remain under build');dest.parent.mkdir(parents=True,exist_ok=True);require(not dest.exists(),'Fresh lifecycle candidate required');dest.write_bytes(blob);print('Checked lifecycle binding candidate',len(blob),'bytes;not scene admission')
