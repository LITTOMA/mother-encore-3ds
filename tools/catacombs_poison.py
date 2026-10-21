"""Source-owned poison contacts and hole timers; never full scene admission."""
import argparse,hashlib,json,math,re,struct,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import podunk_field as f,catacombs_steps as steps
from tools.godot_text import instantiate,Vec2,NodePathValue
from tools.dungeon_keys import fields,path
from tools.dungeon_key_indicator import unique
from tools.extract_battle_entry import require,PIN
from tools.reference_movement import extract
RECIPE=ROOT/'content/catacombs-poison.json';MAGIC=b'ENCPSN01';STRIDES=(1,64,48,40,32,8);LIMIT=262144

def integer(v,lo=0,hi=0xffffffff):require(type(v)is int and lo<=v<=hi,'Poison integer domain');return v
def real(v,lo=-1e6,hi=1e6):require(type(v)in(int,float)and math.isfinite(v)and lo<=v<=hi,'Poison finite real');return float(v)
def vector(v):require(type(v)is Vec2,'Poison source vector');return[f.f32(real(v.x)),f.f32(real(v.y))]
def safe(v):require(type(v)is str and 0<len(v)<=512 and all(32<=ord(c)<127 for c in v),'Poison ASCII text');path(v);return v

def simple(points):
 def cross(a,b,c):return(b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0])
 for i,a in enumerate(points):
  b=points[(i+1)%len(points)]
  for j in range(i+1,len(points)):
   if j==i+1 or i==0 and j==len(points)-1:continue
   c,d=points[j],points[(j+1)%len(points)]
   boxes=all(max(min(a[k],b[k]),min(c[k],d[k]))<=min(max(a[k],b[k]),max(c[k],d[k]))for k in[0,1])
   require(not(boxes and cross(a,b,c)*cross(a,b,d)<=0 and cross(c,d,a)*cross(c,d,b)<=0),'Poison self-intersecting polygon')

def read(file=RECIPE):
 def invalid(v):raise ValueError('Nonfinite poison JSON')
 p=json.loads(Path(file).read_text('utf8'),object_pairs_hook=unique,parse_constant=invalid)
 fields(p,'schema kind commit scene_id scope paths sources symbols areas holes steps_recipe steps_recipe_sha256 license_review','poison review')
 require(type(p['schema'])is int and p['schema']==1 and p['kind']=='encore.catacombs-poison.reviewed-source'and p['commit']==PIN,'Poison reviewed identity');integer(p['scene_id'],1,65535)
 fields(p['paths'],'scene area area_script hole hole_script geyser geyser_script party_script status step_script license','poison selectors')
 require(type(p['sources'])is dict and set(p['sources'])==set(p['paths'].values()),'Poison source coverage');s=f.Sources()
 for name,digest in p['sources'].items():safe(name);require(type(digest)is str and re.fullmatch('[0-9a-f]{64}',digest)and hashlib.sha256(s.data(name)).hexdigest()==digest,'Poison original source changed: '+name)
 names={'area_script':['_on_Area2D_body_entered','_on_Poison_body_exited'],'hole_script':['_ready','pause','unpause','_on_Timer_timeout'],'geyser_script':['_on_Poison_Geyser_body_entered','_on_Poison_Geyser_body_exited'],'party_script':['start_continuous_damage','stop_continuous_damage']}
 require(type(p['symbols'])is dict and set(p['symbols'])==set(names),'Poison method coverage')
 for key,methods in names.items():
  raw=s.data(p['paths'][key]);_,hashes=extract(raw,hashlib.sha256(raw).hexdigest(),methods);require(p['symbols'][key]==hashes,'Poison method review changed')
 ids=set();paths=set()
 for key in['areas','holes']:
  require(type(p[key])is list and 1<=len(p[key])<=128,'Poison source owner count')
  for row in p[key]:
   fields(row,'id path','poison reviewed owner');integer(row['id'],1,0xfffffffe);safe(row['path']);require(row['id']not in ids and row['path']not in paths,'Poison duplicate reviewed owner');ids.add(row['id']);paths.add(row['path'])
 require(p['steps_recipe']=='content/catacombs-stepping.json'and type(p['steps_recipe_sha256'])is str and hashlib.sha256((ROOT/p['steps_recipe']).read_bytes()).hexdigest()==p['steps_recipe_sha256'],'Poison stepping dependency changed')
 require(all(type(p[k])is str and 0<len(p[k])<=4096 for k in['scope','license_review']),'Poison scope/license');return p,s

def source(file=RECIPE):
 p,s=read(file);n=p['paths'];nodes=instantiate(s.doc,n['scene']);by={v.path:v for v in nodes};_,step,_=steps.source();step_by={v['path']:v['id']for v in step['areas']}
 def methods(key):
  raw=s.data(n[key]);names=list(p['symbols'][key]);return extract(raw,hashlib.sha256(raw).hexdigest(),names)[0]
 def damage(key):
  found=re.findall(r'body.start_continuous_damage\((\d+), (\d+), Vector2.ZERO, Status.AILMENT_POISONED\)',methods(key));require(len(found)==1,'Poison original damage binding');return list(map(int,found[0]))
 status=re.findall(r'^const AILMENT_POISONED := "([a-z_]+)"$',s.text(n['status']),re.M);require(len(status)==1,'Poison source ailment identity')
 party=methods('party_script');require('_is_continuous_damage = true\n\t_attack_damage = damage\n\t_damage_variance = damage_variance'in party and '_party_member.add_status(status)'in party and '_is_continuous_damage = false'in party,'Poison party assignment semantics')
 hole=methods('hole_script');require('timer.wait_time = delay if delay != 0.0 else interval'in hole and 'get_node("Objects").call_deferred("add_child", newPoison)'in hole and 'newPoison.position = position - Vector2(0, 8)'in hole,'Poison hole source scheduling/spawn')
 defaults={}
 for key in['interval','delay']:
  found=re.findall(r'^export var '+key+r' = ([0-9.]+)$',s.text(n['hole_script']),re.M);require(len(found)==1,'Poison hole default');defaults[key]=float(found[0])
 proto=s.doc(n['area']);require(proto.nodes[0].attrs.get('type')=='Area2D'and proto.ext_path(proto.nodes[0].props['script'],'Script')==n['area_script']and proto.connections==[dict(signal='body_entered',**{'from':'.','to':'.'},method='_on_Area2D_body_entered'),dict(signal='body_exited',**{'from':'.','to':'.'},method='_on_Poison_body_exited')],'Poison prefab/source signals')
 timer=s.doc(n['hole']);require(len(timer.nodes)==2 and timer.nodes[1].attrs==dict(name='Timer',type='Timer',parent='.')and timer.nodes[1].props=={'wait_time':8.0}and timer.connections==[dict(signal='timeout',**{'from':'Timer','to':'.'},method='_on_Timer_timeout')],'Poison original hole Timer profile')
 def parent_origin(node):
  current=f.parent_path(node.path);origin=[0.,0.]
  while current is not None:
   ancestor=by[current];require(not(set(ancestor.props)&{'transform','rotation','rotation_degrees','scale','skew'}),'Poison unreviewed ancestor transform');v=vector(ancestor.props.get('position',Vec2(0,0)));origin=[f.f32(a+b)for a,b in zip(origin,v)];current=f.parent_path(current)
  require(origin==[0.,0.],'Poison selected parent transform changed');return origin
 model=dict(scene=p['scene_id'],source=n['scene'],name=by['.'].name,status=status[0],damage=damage('area_script')+damage('geyser_script'),hit=[0.,0.],spawn=[0.,-8.],holes=[],areas=[],shapes=[],points=[])
 for key,script in[('holes','hole_script'),('areas','area_script')]:
  actual=[v for v in nodes if v.script==n[script]];require([v.path for v in actual]==[v['path']for v in p[key]],'Poison exact source owner/order coverage')
  for node,owner in zip(actual,p[key]):
   parent_origin(node);require(not(set(node.props)&{'transform','rotation','scale','skew'}),'Poison unreviewed owner transform')
   if key=='holes':
    require(node.type=='Sprite'and node.scene==n['hole']and set(node.props)<=set('texture script position interval delay'.split())and by[node.path+'/Timer'].props=={'wait_time':8.0},'Poison hole overrides')
    model['holes'].append(dict(**owner,position=vector(node.props.get('position',Vec2(0,0))),interval=real(node.props.get('interval',defaults['interval']),.001,3600),delay=real(node.props.get('delay',defaults['delay']),0,3600),timer_bits=0,process=1));continue
   require(node.type=='Area2D'and node.scene==n['area']and set(node.props)<=set('collision_layer collision_mask monitorable monitoring script position stepping_sound'.split()),'Poison area overrides')
   binding=node.props.get('stepping_sound');require(type(binding)is NodePathValue,'Poison source step path');resolved=list(Path(node.path).parts)
   for part in binding.path.split('/'):
    if part=='..':require(resolved,'Poison source step path escaped');resolved.pop()
    else:require(part not in['','.'],'Poison source step path segment');resolved.append(part)
   target='/'.join(resolved);require(target in step_by,'Poison stepping owner is unavailable');first=len(model['shapes']);position=vector(node.props.get('position',Vec2(0,0)))
   expected=[node.path+'/CollisionPolygon2D',node.path+'/StaticBody2D',node.path+'/StaticBody2D/CollisionPolygon2D'];require([v.path for v in nodes if v.path.startswith(node.path+'/')]==expected,'Poison unreviewed child tree')
   area_layer=node.props.get('collision_layer',1);area_mask=node.props.get('collision_mask',1);monitoring=node.props.get('monitoring',True);monitorable=node.props.get('monitorable',True)
   require(type(monitoring)is bool and type(monitorable)is bool,'Poison monitor flags')
   for role,child in enumerate([expected[0],expected[2]],1):
    poly=by[child];require(poly.type=='CollisionPolygon2D'and set(poly.props)<= {'position','polygon'}and type(poly.props.get('polygon'))is list,'Poison polygon source');local=vector(poly.props.get('position',Vec2(0,0)));origin=[f.f32(a+b)for a,b in zip(position,local)];points=[vector(v)for v in poly.props['polygon']];body=by[expected[1]]
    require(body.type=='StaticBody2D'and set(body.props)<= {'collision_layer','collision_mask'},'Poison blocker body profile');layer,mask=(area_layer,area_mask)if role==1 else(body.props.get('collision_layer',1),body.props.get('collision_mask',1))
    model['shapes'].append(dict(owner=len(model['areas']),role=role,first=len(model['points']),count=len(points),position=origin,layer=layer,mask=mask));model['points']+=points
   model['areas'].append(dict(**owner,step=step_by[target],layer=area_layer,mask=area_mask,bits=int(monitoring)|int(monitorable)<<1,first=first,count=2))
 validate(model);blob=encode(model);require(parse(blob)==model,'Poison binary roundtrip')
 proof=dict(commit=PIN,sources={name:hashlib.sha256(s.data(name)).hexdigest()for name in n.values()},review_sha256=hashlib.sha256(Path(file).read_bytes()).hexdigest(),producer_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),scope=p['scope'],bytes=len(blob),sha256=hashlib.sha256(blob).hexdigest());return p,model,proof

def validate(m):
 fields(m,'scene source name status damage hit spawn holes areas shapes points','poison model');integer(m['scene'],1,65535)
 for key in['source','name','status']:safe(m[key])
 require(m['source'].startswith('Maps/')and m['source'].endswith('.tscn')and re.fullmatch('[a-z][a-z_]{0,63}',m['status']),'Poison source/status domain')
 require(type(m['damage'])is list and len(m['damage'])==4,'Poison damage policy')
 for v in m['damage']:integer(v,0,1000000)
 for key in['hit','spawn']:
  require(type(m[key])is list and len(m[key])==2,'Poison policy vector')
  for v in m[key]:require(real(v)==f.f32(v),'Poison binary32 vector')
 require(type(m['holes'])is list and 1<=len(m['holes'])<=128 and type(m['areas'])is list and 1<=len(m['areas'])<=128 and type(m['shapes'])is list and len(m['shapes'])==2*len(m['areas'])and type(m['points'])is list and 6<=len(m['points'])<=16384,'Poison model capacities');ids=set();paths=set()
 for key in['holes','areas']:
  for row in m[key]:
   fields(row,'id path position interval delay timer_bits process'if key=='holes'else'id path step layer mask bits first count','poison owner');integer(row['id'],1,0xfffffffe);safe(row['path']);require(row['id']not in ids and row['path']not in paths,'Poison duplicate model owner');ids.add(row['id']);paths.add(row['path'])
   if key=='holes':
    require(type(row['position'])is list and len(row['position'])==2,'Poison hole position')
    for v in row['position']:require(real(v)==f.f32(v),'Poison hole binary32 position')
    real(row['interval'],.001,3600);real(row['delay'],0,3600);require(row['delay']==0 or f.f32(row['delay'])>0,'Poison Timer delay underflows binary32');require(type(row['timer_bits'])is int and row['timer_bits']==0 and type(row['process'])is int and row['process']==1,'Poison unsupported Timer mode')
   else:
    integer(row['step'],1,0xfffffffe);integer(row['layer'],0,0xfffff);integer(row['mask'],0,0xfffff);integer(row['bits'],0,3);require(row['first']==2*m['areas'].index(row)and type(row['first'])is int and type(row['count'])is int and row['count']==2,'Poison shape ownership')
 cursor=0
 for index,row in enumerate(m['shapes']):
  fields(row,'owner role first count position layer mask','poison shape');require(type(row['owner'])is int and row['owner']==index//2 and type(row['role'])is int and row['role']==index%2+1 and type(row['first'])is int and row['first']==cursor,'Poison canonical shape owner/order');integer(row['count'],3,128);require(row['count']<=len(m['points'])-cursor,'Poison polygon span');integer(row['layer'],0,0xfffff);integer(row['mask'],0,0xfffff)
  require(type(row['position'])is list and len(row['position'])==2,'Poison shape position')
  for v in row['position']:require(real(v)==f.f32(v),'Poison shape binary32 origin')
  if row['role']==1:require(row['layer']==m['areas'][row['owner']]['layer']and row['mask']==m['areas'][row['owner']]['mask'],'Poison trigger filter mismatch')
  points=m['points'][cursor:cursor+row['count']];area=0
  for j,point in enumerate(points):
   require(type(point)is list and len(point)==2,'Poison polygon point')
   for v in point:require(real(v)==f.f32(v),'Poison polygon binary32 point')
   following=points[(j+1)%len(points)];require(point!=following,'Poison degenerate polygon edge');area+=point[0]*following[1]-following[0]*point[1]
  require(area!=0 and len(set(map(tuple,points)))==len(points),'Poison degenerate polygon');simple(points);cursor+=row['count']
 require(cursor==len(m['points']),'Poison unowned points');return m

def encode(m):
 validate(m);pool=bytearray()
 def S(value):raw=value.encode('ascii');pair=(len(pool),len(raw));pool.extend(raw);return pair
 policy=struct.pack('<6I4i4f2I',*S(m['source']),*S(m['name']),*S(m['status']),*m['damage'],*m['hit'],*m['spawn'],0,0);holes=bytearray();areas=bytearray();shapes=bytearray();points=bytearray()
 for h in m['holes']:holes.extend(struct.pack('<3I2f2d3I',h['id'],*S(h['path']),*h['position'],h['interval'],h['delay'],h['timer_bits'],h['process'],0))
 for a in m['areas']:areas.extend(struct.pack('<10I',a['id'],*S(a['path']),a['step'],a['layer'],a['mask'],a['bits'],a['first'],a['count'],0))
 for s in m['shapes']:shapes.extend(struct.pack('<4I2f2I',s['owner'],s['role'],s['first'],s['count'],*s['position'],s['layer'],s['mask']))
 for p in m['points']:points.extend(struct.pack('<2f',*p))
 return f.pack_container(MAGIC,m['scene'],[pool,policy,holes,areas,shapes,points],STRIDES,LIMIT)

def parse(blob):
 scene,sections=f.unpack_container(blob,MAGIC,STRIDES,LIMIT);pool,policy,holes,areas,shapes,points=sections;require(0<len(pool)<=65536 and len(policy)==64 and 1<=len(holes)//48<=128 and 1<=len(areas)//40<=128 and len(shapes)//32==2*len(areas)//40 and 6<=len(points)//8<=16384,'Poison section capacities');used=bytearray(len(pool))
 def S(off,size):
  require(0<size<=512 and off<=len(pool)and size<=len(pool)-off and not any(used[off:off+size]),'Poison string span/alias');used[off:off+size]=b'\1'*size;return pool[off:off+size].decode('ascii')
 p=struct.unpack('<6I4i4f2I',policy);require(not any(p[-2:]),'Poison policy reserved');m=dict(scene=scene,source=S(*p[:2]),name=S(*p[2:4]),status=S(*p[4:6]),damage=list(p[6:10]),hit=list(p[10:12]),spawn=list(p[12:14]),holes=[],areas=[],shapes=[],points=[])
 for at in range(0,len(holes),48):
  h=struct.unpack_from('<3I2f2d3I',holes,at);require(h[-1]==0,'Poison hole reserved');m['holes'].append(dict(id=h[0],path=S(*h[1:3]),position=list(h[3:5]),interval=h[5],delay=h[6],timer_bits=h[7],process=h[8]))
 for at in range(0,len(areas),40):
  a=struct.unpack_from('<10I',areas,at);require(a[-1]==0,'Poison area reserved');m['areas'].append(dict(id=a[0],path=S(*a[1:3]),step=a[3],layer=a[4],mask=a[5],bits=a[6],first=a[7],count=a[8]))
 for at in range(0,len(shapes),32):
  s=struct.unpack_from('<4I2f2I',shapes,at);m['shapes'].append(dict(owner=s[0],role=s[1],first=s[2],count=s[3],position=list(s[4:6]),layer=s[6],mask=s[7]))
 for at in range(0,len(points),8):m['points'].append(list(struct.unpack_from('<2f',points,at)))
 require(all(used),'Poison unowned text');return validate(m)

def build(out):
 out=Path(out).resolve();require(out.is_relative_to(ROOT/'build')and out!=ROOT/'build'and not out.exists(),'Fresh poison candidate under build required');_,m,proof=source();out.mkdir();(out/'.owned-catacombs-poison').write_bytes(b'catacombs-poison-v1');(out/'catacombs.encpoison').write_bytes(encode(m));(out/'source.json').write_text(json.dumps(dict(model=m,proof=proof),indent=2)+'\n');print('Poison candidate:',len(m['areas']),'areas,',len(m['holes']),'hole timers,',len(m['points']),'raw trigger/blocker points;not full hazard/scene admission');return m,proof
if __name__=='__main__':
 a=argparse.ArgumentParser(description=__doc__);a.add_argument('--out',type=Path,required=True);build(a.parse_args().out)
