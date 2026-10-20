"""Source-owned dungeon camera rooms, independently checked and not staged.

ENCDRM01 format1/capability1 binds original sibling indices and flag strings to
stable port identities. Hidden CanvasItem parents do not disable physics areas.
Only this reviewed subtree and method dependency are admitted; other scene
nodes and the complete gameplay scene remain unadmitted.
"""
import argparse,hashlib,json,math,re,struct,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import podunk_field as f
from tools.extract_battle_entry import require,PIN
from tools.godot_text import instantiate,Vec2,owner_of
from tools.run_battle_victory_reference import function
from tools.dungeon_keys import fields,path
RECIPE=ROOT/'content/catacombs-rooms.json';MAGIC=b'ENCDRM01';STRIDES=(1,64,80);LIMIT=65536

def real(v):
 require(type(v)in(int,float)and math.isfinite(v)and abs(v)<=1000000,'Dungeon room finite real')
 return f.f32(v)
def vector(v):
 require(type(v)is Vec2,'Dungeon room vector type');return[real(v.x),real(v.y)]
def integer(v,lo=0,hi=0xffffffff):require(type(v)is int and lo<=v<=hi,'Dungeon room integer')
def unique(rows):
 out={}
 for k,v in rows:require(k not in out,'Dungeon room duplicate review field');out[k]=v
 return out
def read(file=RECIPE):
 p=json.loads(Path(file).read_text(encoding='utf8'),object_pairs_hook=unique)
 fields(p,'schema kind commit scope scene_id container paths sources symbols rooms','review')
 require(type(p['schema'])is int and p['schema']==1 and p['kind']=='encore.dungeon-rooms.reviewed-source'and p['commit']==PIN,'Dungeon room review schema/pin')
 integer(p['scene_id'],1,65535);path(p['container']);require(type(p['scope'])is str and 0<len(p['scope'])<=2048,'Dungeon room scope')
 fields(p['paths'],'scene scene_script area base_script room_script dungeon_script camera_script data_script','selectors')
 require(type(p['sources'])is dict and set(p['sources'])==set(p['paths'].values()),'Dungeon room source coverage')
 s=f.Sources()
 for value in p['paths'].values():
  path(value);digest=p['sources'][value];require(type(digest)is str and re.fullmatch('[0-9a-f]{64}',digest)and hashlib.sha256(s.data(value)).hexdigest()==digest,'Dungeon room reviewed source changed: '+value)
 require(type(p['symbols'])is dict and set(p['symbols'])==set(p['paths'])-{'scene','area'},'Dungeon room method coverage')
 for key,methods in p['symbols'].items():
  require(type(methods)is dict and methods,'Dungeon room empty method review')
  for name,digest in methods.items():require(type(name)is str and re.fullmatch('[A-Za-z_][A-Za-z_0-9]*',name)and type(digest)is str and hashlib.sha256(function(s.text(p['paths'][key]),name).encode()).hexdigest()==digest,'Dungeon room reviewed method changed')
 require(type(p['rooms'])is list and 1<=len(p['rooms'])<=128,'Dungeon room owner count');ids=set();paths=set()
 for row in p['rooms']:
  fields(row,'id path','owner');integer(row['id'],1);path(row['path']);require(row['id']not in ids and row['path']not in paths,'Dungeon room duplicate owner');ids.add(row['id']);paths.add(row['path'])
 return p,s

def extract(file=RECIPE):
 p,s=read(file);n=p['paths'];nodes=instantiate(s.doc,n['scene']);by={v.path:v for v in nodes};doc=s.doc(n['scene']);root=by['.']
 require(root.script==n['scene_script']and re.search(r'^extends DungeonAreaRoom$',s.text(n['scene_script']),re.M)and set(root.props)=={'script','_region_name'}and type(root.props['_region_name'])is str and root.props['_region_name'],'Dungeon room root inheritance/overrides')
 require(p['container']in by and by[p['container']].type=='Node2D'and set(by[p['container']].props)<= {'visible'},'Dungeon room container transform/state')
 require(type(by[p['container']].props.get('visible',True))is bool,'Dungeon room container visibility')
 base=s.text(n['base_script']);room=s.text(n['room_script']);dungeon=s.text(n['dungeon_script']);camera=s.text(n['camera_script'])
 require(re.search(r'^extends Camarea$',room,re.M)and re.search(r'^extends AreaRoom$',dungeon,re.M),'Dungeon room inheritance')
 require('_flag = global.currentScene.name + "/room_" + str(get_index())'in function(room,'_ready')and'global.currentScene.set_current_room(self)'in function(room,'_on_enter'),'Dungeon room source identity/current owner')
 require('global.get_player().global_position - _current_room.get_area_global_position()'in function(dungeon,'get_player_position_relative_to_current_room')and'return _current_room.get_index()'in function(dungeon,'get_current_room_id'),'Dungeon room source map position')
 require('_camarea_offset = off\n\tposition = _camarea_offset'in function(camera,'set_camarea_offset'),'Dungeon room camera offset setter')
 require('if emit_signal: global.emit_signal("flags_updated")'in function(s.text(n['data_script']),'set_object_flag'),'Dungeon room object flag signal')
 reset=function(base,'_reset_camera_limits');limits=[]
 for key in ('left','top','right','bottom'):
  found=re.findall(r'global.currentCamera.limit_'+key+r' = (-?[0-9]+)',reset);require(len(found)==1,'Dungeon room reset camera limit');limits.append(int(found[0]))
 floor=re.findall(r'^export var current_floor := (-?[0-9]+)$',dungeon,re.M);require(len(floor)==1,'Dungeon room current floor export');floor=int(floor[0])
 actual=[v for v in nodes if v.script==n['room_script']];children=[v for v in nodes if f.parent_path(v.path)==p['container']]
 require([v.path for v in actual]==[v['path']for v in p['rooms']]and actual==children,'Dungeon room exact source sibling coverage/order')
 prefab=s.doc(n['area']);require(prefab.nodes[0].attrs.get('type')=='Area2D'and prefab.ext_path(prefab.nodes[0].props.get('script'),'Script')==n['base_script'],'Dungeon room prefab root binding')
 require(prefab.connections==[dict(signal='body_entered',**{'from':'.','to':'.'},method='_on_camarea_body_entered'),dict(signal='body_exited',**{'from':'.','to':'.'},method='_on_camarea_body_exited')],'Dungeon room source signals')
 rows=[];native=[]
 for index,(node,binding)in enumerate(zip(actual,p['rooms'])):
  require(node.type=='Area2D'and node.scene==n['area']and set(node.props)<=set('script position collision_layer collision_mask monitorable monitoring camera_offset'.split()),'Dungeon room area override')
  shape=by[node.path+'/CollisionShape2D'];require(shape.type=='CollisionShape2D'and set(shape.props)<= {'position','shape','disabled'}and not shape.props.get('disabled',False),'Dungeon room shape override')
  require([v.path for v in nodes if f.parent_path(v.path)==node.path]==[shape.path],'Dungeon room unreviewed native child')
  value=owner_of(shape,'shape').sub_resource(shape.props['shape'],'RectangleShape2D');fields(value,'extents','rectangle')
  position=vector(node.props.get('position',Vec2(0,0)));local=vector(shape.props.get('position',Vec2(0,0)));extents=vector(value['extents']);require(min(extents)>0,'Dungeon room positive shape')
  center=[real(a+b)for a,b in zip(position,local)];size=[real(math.floor(2*v+0.5))for v in extents]
  flags=[]
  for key,default in (('monitoring',True),('monitorable',True)):
   value=node.props.get(key,default);require(type(value)is bool,'Dungeon room monitoring type');flags.append(value)
  layer=node.props.get('collision_layer',1);mask=node.props.get('collision_mask',1);integer(layer,0,0xfffff);integer(mask,0,0xfffff)
  row=dict(id=binding['id'],index=index,path=node.path,flag=doc.nodes[0].attrs['name']+'/room_'+str(index),center=center,extents=extents,size=size,offset=vector(node.props.get('camera_offset',Vec2(0,0))),layer=layer,mask=mask,bits=int(flags[0])|int(flags[1])<<1)
  rows.append(row);native.append(dict(path=node.path,position=position,shape_position=local))
 model=dict(scene=p['scene_id'],floor=floor,source=n['scene'],name=doc.nodes[0].attrs['name'],region=root.props['_region_name'],reset=limits,rooms=rows)
 raw=encode(model);require(parse(raw)==model,'Dungeon room encoded disagreement')
 proof=dict(commit=PIN,scope=p['scope'],sources=s.digests(),review_sha256=hashlib.sha256(Path(file).read_bytes()).hexdigest(),producer_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),native_instances=native,container_visible=by[p['container']].props.get('visible',True),rooms=len(rows),bytes=len(raw),sha256=hashlib.sha256(raw).hexdigest())
 return model,raw,proof

def validate(m):
 fields(m,'scene floor source name region reset rooms','model');integer(m['scene'],1,65535);integer(m['floor'],-128,127)
 for key in ('source','name','region'):path(m[key])
 require(m['source'].startswith('Maps/')and m['source'].endswith('.tscn')and len(m['name'])<=64 and len(m['region'])<=64,'Dungeon room scene identity')
 require(type(m['reset'])is list and len(m['reset'])==4,'Dungeon room reset shape')
 for v in m['reset']:integer(v,-100000000,100000000)
 l,t,r,b=m['reset'];require(l<r and t<b,'Dungeon room reset bounds')
 require(type(m['rooms'])is list and 1<=len(m['rooms'])<=128,'Dungeon room record count');ids=set();paths=set();flags=set()
 for i,row in enumerate(m['rooms']):
  fields(row,'id index path flag center extents size offset layer mask bits','record');integer(row['id'],1);integer(row['index'],0,127);path(row['path']);path(row['flag'])
  require(row['index']==i and row['id']not in ids and row['path']not in paths and row['flag']not in flags,'Dungeon room saved identity/order');ids.add(row['id']);paths.add(row['path']);flags.add(row['flag'])
  require(row['flag'].startswith(m['name']+'/')and len(row['path'])<=256 and len(row['flag'])<=256,'Dungeon room flag ownership')
  for key in ('center','extents','size','offset'):
   require(type(row[key])is list and len(row[key])==2,'Dungeon room vector fields')
   for v in row[key]:real(v)
  require(min(row['extents'])>0 and min(row['size'])>0 and all(v==math.floor(v)for v in row['size']),'Dungeon room dimensions')
  require(row['size']==[real(math.floor(2*v+0.5))for v in row['extents']],'Dungeon room shape/size disagreement')
  integer(row['layer'],0,0xfffff);integer(row['mask'],0,0xfffff);integer(row['bits'],0,3)
 return m

def encode(m):
 validate(m);pool=bytearray()
 def text(v):p=(len(pool),len(v.encode('ascii')));pool.extend(v.encode('ascii'));return p
 policy=struct.pack('<Ii6I4i4I',m['scene'],m['floor'],*text(m['source']),*text(m['name']),*text(m['region']),*m['reset'],0,0,0,0)
 rows=bytearray()
 for r in m['rooms']:rows.extend(struct.pack('<6I8f6I',r['id'],r['index'],*text(r['path']),*text(r['flag']),*r['center'],*r['extents'],*r['size'],*r['offset'],r['layer'],r['mask'],r['bits'],0,0,0))
 return f.pack_container(MAGIC,m['scene'],[bytes(pool),policy,bytes(rows)],STRIDES,LIMIT)

def parse(blob):
 scene,parts=f.unpack_container(blob,MAGIC,STRIDES,LIMIT);require(len(parts[1])==64 and 0<len(parts[2])<=128*80,'Dungeon room policy/count');used=bytearray(len(parts[0]))
 def text(off,size):
  require(0<size<=512 and off<=len(used)and size<=len(used)-off and not any(used[off:off+size]),'Dungeon room text span/alias');used[off:off+size]=b'\1'*size
  try:value=parts[0][off:off+size].decode('ascii')
  except UnicodeError:raise ValueError('Dungeon room ASCII text required')from None
  path(value);return value
 p=struct.unpack('<Ii6I4i4I',parts[1]);require(not any(p[12:]),'Dungeon room policy reserved');m=dict(scene=p[0],floor=p[1],source=text(*p[2:4]),name=text(*p[4:6]),region=text(*p[6:8]),reset=list(p[8:12]),rooms=[])
 for at in range(0,len(parts[2]),80):
  r=struct.unpack_from('<6I8f6I',parts[2],at);require(not any(r[17:]),'Dungeon room record reserved')
  m['rooms'].append(dict(id=r[0],index=r[1],path=text(*r[2:4]),flag=text(*r[4:6]),center=list(r[6:8]),extents=list(r[8:10]),size=list(r[10:12]),offset=list(r[12:14]),layer=r[14],mask=r[15],bits=r[16]))
 require(all(used)and m['scene']==scene,'Dungeon room unowned text/scene binding');return validate(m)

def build(out):
 out=Path(out).resolve();require(out.is_relative_to((ROOT/'build').resolve())and out!=ROOT/'build','Dungeon room candidate stays under build');m,raw,proof=extract();out.mkdir(parents=True,exist_ok=True)
 (out/'source.encdrooms').write_bytes(raw);(out/'source.json').write_text(json.dumps(dict(model=m,proof=proof),indent=2)+'\n',encoding='utf8');print('Dungeon rooms:',len(m['rooms']),'owners;',len(raw),'bytes; hidden parent retained; not scene admission');return m,proof
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',type=Path,required=True);a=p.parse_args();build(a.out)
