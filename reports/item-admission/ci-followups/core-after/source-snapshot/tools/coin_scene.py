"""Checked concrete original CoinSpawner bindings; no full-room admission."""
import hashlib,json,math,struct,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import coins,catacombs_breakables as b,podunk_field as f
from tools.godot_text import instantiate,owner_of,Vec2
from tools.extract_battle_entry import PIN,require
from tools.dungeon_keys import fields
from tools.dungeon_key_indicator import unique
RECIPE=ROOT/'content/catacombs-coin-scene.json'
MAGIC=b'ENCCF001';STRIDES=(32,64,1);LIMIT=128*1024

def point(v):
 require(type(v)is Vec2,'Coin scene position');values=[v.x,v.y];require(all(type(n)in[int,float]and math.isfinite(n)and abs(n)<=1e6 for n in values),'Coin scene finite position');return [f.f32(n)for n in values]

def derive():
 bp,bm,break_proof=b.source();cp,cm,coin_proof=coins.source();s=f.Sources();nodes=instantiate(s.doc,bp['paths']['scene']);by={n.path:n for n in nodes};orders={n.path:i for i,n in enumerate(nodes)};owners={o['spawner']:o for o in bm['owners']if o['spawner']};factories=[n for n in nodes if n.script==cp['paths']['spawner_script']];require(len(owners)==len(factories)and {n.path for n in factories}==set(owners),'Coin scene complete unique owner/factory coverage')
 def chain(path):
  result=[]
  while path:
   require(path in by,'Coin scene missing ancestor');result.append(path);path=f.parent_path(path)
  return list(reversed(result))
 def position(path):
  result=[0.,0.]
  for ancestor in chain(path):
   n=by[ancestor];require(not(set(n.props)&{'transform','rotation','rotation_degrees','scale','skew'}),'Coin scene unreviewed transform');p=point(n.props.get('position',Vec2(0,0)));result=[f.f32(x+y)for x,y in zip(result,p)]
  return result
 require(cm['factory_parent']in by and by[cm['factory_parent']].type=='TileMap'and position('.')==position(cm['factory_parent'])==[0.,0.],'Coin scene original factory sink identity transform');bindings=[]
 for node in factories:
  owner=owners[node.path];require(node.type=='Node2D'and node.scene==cp['paths']['spawner']and set(node.props)=={'script','object','position'}and owner_of(node,'object').ext_path(node.props['object'],'PackedScene')==cp['paths']['scene'],'Coin scene original prefab/overrides');parent=f.parent_path(node.path);require(parent==owner['path']and by[parent].script==bp['paths']['script']and not owner['content'],'Coin scene checked direct breakable parent');require(owner['new_parent']in by and position(owner['new_parent'])==[0.,0.]and all(position(a)==[0.,0.]for a in chain(f.parent_path(parent))),'Coin scene preserved breakable reparent coordinates')
  bindings.append(dict(id=b.stable('coin-factory:'+node.path),owner=owner['id'],order=orders[node.path],owner_order=orders[parent],path=node.path,parent=parent,source=cp['paths']['spawner'],owner_path=owner['path'],sink=cm['factory_parent'],reparent=owner['new_parent'],default_count=cm['meta']['default_coins'],local=point(node.props['position']),position=position(node.path)))
 model=dict(scene=bm['scene'],source=bm['source'],sink=cm['factory_parent'],root_position=position('.'),sink_position=position(cm['factory_parent']),bindings=bindings)
 proof=dict(commit=PIN,breakables=break_proof,coins=coin_proof,sources=s.digests(),coordinate_scope='Initial untranslated source scene and identity Objects sink; no dynamic tree or complete physics-order admission')
 return model,proof

def source(file=RECIPE):
 p=json.loads(Path(file).read_bytes(),object_pairs_hook=unique);fields(p,'schema kind commit scene source_sha256 breakable_recipe_sha256 coin_recipe_sha256 instances scope','Coin scene recipe');require(type(p['schema'])is int and p['schema']==1 and p['kind']=='encore.catacombs-coin-scene.reviewed-source'and p['commit']==PIN,'Coin scene reviewed identity');model,proof=derive();require(type(p['scene'])is int and p['scene']==model['scene']and p['source_sha256']==proof['sources'][model['source']]and p['breakable_recipe_sha256']==proof['breakables']['recipe_sha256']and p['coin_recipe_sha256']==proof['coins']['recipe_sha256'],'Coin scene reviewed dependencies');expected=[{k:v[k]for k in ['id','owner','path']}for v in model['bindings']];require(p['instances']==expected and all(type(v['id'])is int and type(v['owner'])is int for v in p['instances'])and type(p['scope'])is str and bool(p['scope']),'Coin scene reviewed binding coverage');proof.update(recipe_sha256=hashlib.sha256(Path(file).read_bytes()).hexdigest(),producer_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),scope=p['scope']);return p,model,proof

def encode(m):
 fields(m,'scene source sink root_position sink_position bindings','Coin scene model');b.integer(m['scene'],1,65535)
 for name in ['root_position','sink_position']:require(type(m[name])is list and len(m[name])==2 and all(type(v)in[int,float]and math.isfinite(v)and v==0 for v in m[name]),'Coin scene supported origin')
 require(type(m['bindings'])is list and 0<len(m['bindings'])<=4096,'Coin scene binding count');pool=bytearray(b'\0');strings={'':0}
 def string(v):
  b.text(v);require(v and not v.startswith('/')and all(p not in['','.','..']for p in v.split('/')),'Coin scene source path')
  if v not in strings:strings[v]=len(pool);pool.extend(v.encode()+b'\0')
  return strings[v]
 meta=struct.pack('<4I4f',m['scene'],string(m['source']),string(m['sink']),1,*m['root_position'],*m['sink_position']);records=bytearray()
 for v in m['bindings']:
  fields(v,'id owner order owner_order path parent source owner_path sink reparent default_count local position','Coin scene instance');[b.integer(v[k])for k in ['id','owner','order','owner_order','default_count']]
  for key in ['local','position']:require(type(v[key])is list and len(v[key])==2,'Coin scene vector');[b.number(n)for n in v[key]]
  records.extend(struct.pack('<12I4f',v['id'],v['owner'],v['order'],v['owner_order'],*[string(v[k])for k in ['path','parent','source','owner_path','sink']],v['default_count'],string(v['reparent']),0,*v['local'],*v['position']))
 blob=f.pack_container(MAGIC,m['scene'],[meta,bytes(records),bytes(pool)],STRIDES,LIMIT);decode(blob);return blob

def decode(blob):
 scene,(meta,records,pool)=f.unpack_container(blob,MAGIC,STRIDES,LIMIT);require(0<scene<=65535 and len(meta)==32 and 0<len(records)<=4096*64 and 1<len(pool)<=65536,'Coin scene section counts');info=struct.unpack('<4I4f',meta);require(info[0]==scene and info[3]==1 and all(math.isfinite(v)and v==0 for v in info[4:]),'Coin scene coordinate contract');require(pool[0]==pool[-1]==0,'Coin scene pool boundary');strings={};cursor=0;distinct=set()
 while cursor<len(pool):
  end=pool.find(b'\0',cursor);require(end>=cursor,'Coin scene unterminated string');v=pool[cursor:end].decode('ascii');b.text(v,True);require((cursor==0 and not v)or(cursor>0 and v and not v.startswith('/')and all(p not in['','.','..']for p in v.split('/'))),'Coin scene path');require(v not in distinct,'Coin scene duplicate string');strings[cursor]=v;distinct.add(v);cursor=end+1
 used={0}
 def string(offset):require(offset in strings and bool(strings[offset]),'Coin scene string reference');used.add(offset);return strings[offset]
 string(info[1]);sink=string(info[2]);ids=set();owners=set();paths=set();previous=0;bindings=[]
 for offset in range(0,len(records),64):
  v=struct.unpack_from('<12I4f',records,offset);identity,owner,order,owner_order=v[:4];require(0<identity<0xffffffff and identity not in ids and 0<owner<0xffffffff and owner not in owners and previous<order<=65535 and 0<owner_order<order and 0<v[9]<=1024 and not v[11]and all(math.isfinite(n)and abs(n)<=1e6 for n in v[12:]),'Coin scene instance domain');path,parent,source,owner_path,target=[string(n)for n in v[4:9]];string(v[10]);require(path not in paths and f.parent_path(path)==parent==owner_path and target==sink and identity==b.stable('coin-factory:'+path),'Coin scene ownership/identity');ids.add(identity);owners.add(owner);paths.add(path);previous=order;bindings.append(v)
 require(used==set(strings),'Coin scene unowned string data');return dict(scene=scene,metadata=info,bindings=bindings,strings=strings)
