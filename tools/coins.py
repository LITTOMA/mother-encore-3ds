"""Checked Coin/Sparkles/Spawner source model; no scene or save admission."""
import hashlib,json,math,re,struct,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import podunk_field as f
from tools.godot_text import instantiate,owner_of,SubRef,NodePathValue,Vec2,Rect2,Color
from tools.extract_battle_entry import PIN,require
from tools.reference_movement import extract
from tools.dungeon_keys import fields
from tools.dungeon_key_indicator import unique
from tools.coin_reference_protocol import validate as validate_reference
RECIPE=ROOT/'content/catacombs-coins.json'
MAGIC=b'ENCCOIN1';STRIDES=(112,96,24,24,8,24,48,24,1);LIMIT=128*1024
METHODS={'script':['_ready','randomize_movement','start_jump','_set_collisions_disabled','_physics_process','_collect','_on_Area2D_body_entered','_on_Area2D_area_entered'],'spawner_script':['_physics_process','spawn_coins'],'spawner_base':['create_object'],'sparkle_script':['_ready'],'ui_script':['add_cash','get_cash_box']}
PATH_KEYS='scene script spawner spawner_script spawner_base sparkle_scene sparkle_script ui_script coin_texture coin_texture_import sparkle_texture sparkle_texture_import shader drop_audio drop_audio_import collect_audio collect_audio_import project license'.split()

def number(v):require(type(v)in[int,float]and math.isfinite(v)and abs(v)<=1e6,'Coin finite scalar');return float(v)
def integer(v,lo=0,hi=0xffffffff):require(type(v)is int and lo<=v<=hi,'Coin integer domain');return v
def text(v):require(type(v)is str and 0<len(v)<=512 and all(32<=ord(c)<127 for c in v)and '\\'not in v,'Coin ASCII identity');return v
def vector(v):require(type(v)is Vec2,'Coin Vector2');return [f.f32(number(v.x)),f.f32(number(v.y))]
def one(pattern,s,label):
 found=re.findall(pattern,s,re.M);require(len(found)==1,'Coin ambiguous source '+label);return found[0]

def read(file=RECIPE):
 p=json.loads(Path(file).read_bytes(),object_pairs_hook=unique);fields(p,'schema kind commit paths sources symbols clips physics_hz safe_margin direction_reference scope license_review','coin recipe')
 require(type(p['schema'])is int and p['schema']==1 and p['kind']=='encore.coins.reviewed-source'and p['commit']==PIN,'Coin source identity')
 require(type(p['paths'])is dict and set(p['paths'])==set(PATH_KEYS)and type(p['sources'])is dict and set(p['sources'])==set(p['paths'].values()),'Coin exact dependency coverage')
 s=f.Sources()
 for path,digest in p['sources'].items():
  text(path);require(not path.startswith('/')and all(v not in['','.','..']for v in path.split('/'))and type(digest)is str and re.fullmatch('[0-9a-f]{64}',digest)and hashlib.sha256(s.data(path)).hexdigest()==digest,'Coin original source changed: '+path)
 require(type(p['symbols'])is dict and set(p['symbols'])==set(METHODS),'Coin method family coverage')
 for key,names in METHODS.items():
  raw=s.data(p['paths'][key]);_,digests=extract(raw,hashlib.sha256(raw).hexdigest(),names);require(p['symbols'][key]==digests,'Coin reviewed source method changed')
  if key!='ui_script':require(re.findall(r'^func ([A-Za-z_][A-Za-z_0-9]*)\(',raw.decode(),re.M)==names,'Coin unknown/unmapped source method')
 require(type(p['clips'])is list and len(p['clips'])==4 and {v['role']for v in p['clips']}=={'flipping','grounded','collect','reset'},'Coin animation roles')
 require(type(p['physics_hz'])is int and 1<=p['physics_hz']<=240 and 0<number(p['safe_margin'])<=1,'Coin checked engine cadence/margin')
 require(all(type(p[k])is str and 0<len(p[k])<=4096 for k in ['scope','license_review']),'Coin source scope/permission review')
 return p,s

def source(file=RECIPE,*,directions=True):
 p,s=read(file);nodes=instantiate(s.doc,p['paths']['scene']);by={v.path:v for v in nodes};require(set(by)=={'.','Area2D','Area2D/CollisionShape2D','CollisionShape2D','Sprite','Sprite/Sparkles','AudioStreamPlayer','AudioStreamPlayer2D','AnimationPlayer'},'Coin exact prefab node coverage')
 root=by['.'];require(root.type=='KinematicBody2D'and root.script==p['paths']['script']and set(root.props)=={'collision_layer','collision_mask','script','_sprite','_animation_player','_drop_sfx','_collect_sfx'},'Coin root defaults/overrides')
 for key,path in [('_sprite','Sprite'),('_animation_player','AnimationPlayer'),('_drop_sfx','AudioStreamPlayer2D')]:require(type(root.props[key])is NodePathValue and root.props[key].path==path,'Coin checked node path')
 require(owner_of(root,'_collect_sfx').ext_path(root.props['_collect_sfx'],'AudioStream')==p['paths']['collect_audio'],'Coin collect stream')
 area=by['Area2D'];require(area.type=='Area2D'and set(area.props)=={'collision_layer','collision_mask','monitorable'}and area.props['monitorable']is False,'Coin pickup owner')
 radii=[]
 for path in ['CollisionShape2D','Area2D/CollisionShape2D']:
  node=by[path];require(node.type=='CollisionShape2D'and set(node.props)=={'shape'},'Coin untranslated enabled circle binding');shape=owner_of(node,'shape').sub_resource(node.props['shape'],'CircleShape2D');fields(shape,'radius','Coin circle');radius=f.f32(number(shape['radius']));require(radius>0,'Coin positive radius');radii.append(radius)
 sprite=by['Sprite'];require(sprite.type=='Sprite'and set(sprite.props)=={'material','texture','hframes'},'Coin sprite defaults');require(owner_of(sprite,'texture').ext_path(sprite.props['texture'],'Texture')==p['paths']['coin_texture'],'Coin texture identity');frames=integer(sprite.props['hframes'],1,65535)
 material=owner_of(sprite,'material').sub_resource(sprite.props['material'],'ShaderMaterial');fields(material,'resource_local_to_scene shader shader_param/flash_color shader_param/glow_color shader_param/flash_modifier shader_param/glow_modifier','Coin local shader');require(material['resource_local_to_scene']is True and owner_of(sprite,'material').ext_path(material['shader'],'Shader')==p['paths']['shader']and material['shader_param/flash_modifier']==material['shader_param/glow_modifier']==0,'Coin local initial shader')
 sparkle=by['Sprite/Sparkles'];require(sparkle.type=='AnimatedSprite'and sparkle.script==p['paths']['sparkle_script']and set(sparkle.props)=={'frames','animation','frame','playing','script','position'},'Coin sparkle defaults');d=owner_of(sparkle,'frames');animations=d.sub_resource(sparkle.props['frames'],'SpriteFrames')['animations'];require(len(animations)==1,'Coin sparkle animation coverage');animation=animations[0];fields(animation,'frames loop name speed','Coin sparkle animation');require(animation['name']==sparkle.props['animation']and animation['loop']is True and sparkle.props['playing']is True,'Coin looping sparkle');atlas=[]
 for ref in animation['frames']:
  require(type(ref)is SubRef,'Coin sparkle local frame');props=d.sub_resource(ref,'AtlasTexture');fields(props,'atlas region','Coin sparkle atlas');require(d.ext_path(props['atlas'],'Texture')==p['paths']['sparkle_texture']and type(props['region'])is Rect2,'Coin sparkle texture/region');v=props['region'];atlas.append([f.f32(number(x))for x in [v.x,v.y,v.w,v.h]])
 animator=by['AnimationPlayer'];require(animator.type=='AnimationPlayer'and set(animator.props)=={'anims/'+c['name']for c in p['clips']},'Coin animation/autoplay coverage');clips=[];ids=set()
 for binding in p['clips']:
  fields(binding,'id name role','Coin clip');identity=integer(binding['id'],1,65535);text(binding['name']);require(identity not in ids,'Coin duplicate clip identity');ids.add(identity);ref=animator.props['anims/'+binding['name']];require(type(ref)is SubRef,'Coin local animation');props=owner_of(animator,'anims/'+binding['name']).sub_resource(ref,'Animation');duration=f.f32(number(props.get('length',1)));require(0<duration<=60 and props.get('resource_name',binding['name'])==binding['name']and type(props.get('loop',False))is bool,'Coin clip duration/name/loop');groups={}
  for key,value in props.items():
   if key.startswith('tracks/'):
    parts=key.split('/');require(len(parts)==3 and parts[1].isdigit(),'Coin animation track field');groups.setdefault(int(parts[1]),{})[parts[2]]=value
   else:require(key in ['resource_name','length','step','loop'],'Coin unreviewed clip property')
  require(set(groups)==set(range(len(groups)))and groups,'Coin track index coverage');keys=[]
  for track in groups.values():
   fields(track,'type path interp loop_wrap imported enabled keys','Coin value track');require(track['type']=='value'and type(track['interp'])is int and track['interp']==1 and track['loop_wrap']is True and track['imported']is False and track['enabled']is True and type(track['path'])is NodePathValue,'Coin unsupported source track');target={'Sprite:frame':1,'Sprite:material:shader_param/glow_modifier':2}.get(track['path'].path);require(target is not None,'Coin unknown animation target');k=track['keys'];fields(k,'times transitions update values','Coin keys');require(type(k['update'])is int and k['update']==(1 if target==1 else 0)and all(type(k[n])is list for n in ['times','transitions','values'])and 0<len(k['times'])==len(k['values'])==len(k['transitions'])<=256 and all(number(v)==1 for v in k['transitions']),'Coin source key cardinality/mode/easing');previous=-1
   for time,value in zip(k['times'],k['values']):
    time=f.f32(number(time));require(previous<time<=duration,'Coin source key order/range');previous=time
    if target==1:integer(value,0,frames-1)
    else:require(0<=number(value)<=1,'Coin glow domain')
    keys.append(dict(time=time,target=target,value=float(value),update=k['update']))
  keys.sort(key=lambda k:(k['target'],k['time']));require(len({(k['target'],k['time'])for k in keys})==len(keys),'Coin duplicate property/time');clips.append(dict(**binding,duration=duration,loop=props.get('loop',False),keys=keys))
 raw=s.text(p['paths']['script']);constant=lambda name:number(float(one(r'^const '+name+r' := ([0-9.]+)$',raw,name)))
 policy={key:constant(name)for key,name in [('minimum_speed','MIN_SPEED'),('maximum_speed','MAX_SPEED'),('air_time_multiplier','AIR_TIME_MULTIPLIER'),('minimum_height','MIN_HEIGHT'),('maximum_height','MAX_HEIGHT'),('deceleration','DECELERATION')]};sparkle_range=one(r'frame = int\(rand_range\(([0-9.]+), ([0-9.]+)\)\)',s.text(p['paths']['sparkle_script']),'sparkle RNG');policy.update(sparkle_minimum=float(sparkle_range[0]),sparkle_maximum=float(sparkle_range[1]),spawn_interval=float(one(r'^const COIN_SPAWN_INTERVAL := ([0-9.]+)$',s.text(p['paths']['spawner_script']),'spawn interval')))
 meta=dict(cash_amount=int(one(r'^var _cash_amount := (\d+)$',raw,'cash amount')),body_layer=root.props['collision_layer'],body_mask=root.props['collision_mask'],pickup_layer=area.props['collision_layer'],pickup_mask=area.props['collision_mask'],frame_count=frames,initial_frame=0,direction_count=int(one(r'randi\(\) % (\d+)',raw,'heading modulus')),sparkle_count=len(atlas),sparkle_initial=integer(sparkle.props['frame'],0,len(atlas)-1),sparkle_playing=True,sparkle_loop=True,default_coins=int(one(r'^export var _coins_spawned := (\d+)$',s.text(p['paths']['spawner_script']),'spawner default')),pickup_area_bit=int(one(r'get_collision_layer_bit\((\d+)\)',raw,'pickup area bit')))
 spawner={v.path:v for v in instantiate(s.doc,p['paths']['spawner'])};require(set(spawner)=={'.'}and spawner['.'].script==p['paths']['spawner_script']and set(spawner['.'].props)=={'script','object'}and owner_of(spawner['.'],'object').ext_path(spawner['.'].props['object'],'PackedScene')==p['paths']['scene'],'Coin actual factory binding')
 for path,key in [('AudioStreamPlayer','collect_audio'),('AudioStreamPlayer2D','drop_audio')]:
  n=by[path];require(n.type==path and owner_of(n,'stream').ext_path(n.props['stream'],'AudioStream')==p['paths'][key]and n.props['bus']=='SFX'and set(n.props)==({'stream','bus'}if path=='AudioStreamPlayer'else{'stream','bus','panning_strength'}),'Coin source voice binding')
 colors=[]
 for key in ['shader_param/flash_color','shader_param/glow_color']:
  color=material[key];require(type(color)is Color,'Coin shader color');row=[f.f32(number(v))for v in [color.r,color.g,color.b,color.a]];require(all(0<=v<=1 for v in row),'Coin shader color domain');colors.append(row)
 model=dict(policy=policy,meta=meta,radii=radii,factory_parent=one(r'get_node\("([^"]+)"\)',s.text(p['paths']['spawner_base']),'factory parent'),colors=colors,initial_modifiers=[number(material['shader_param/flash_modifier']),number(material['shader_param/glow_modifier'])],panning=f.f32(number(by['AudioStreamPlayer2D'].props['panning_strength'])),margin=f.f32(p['safe_margin']),delta=f.f32(1/p['physics_hz']),sparkle_fps=f.f32(number(animation['speed'])),sparkle_position=vector(sparkle.props['position']),sprite_offset=[0.,0.],clips=clips,atlas=atlas,directions=[],paths=p['paths'],voice=one(r'audioManager.play_sfx\(_collect_sfx, "([^"]+)"\)',raw,'named collect voice'))
 if directions:
  definition=p['direction_reference'];fields(definition,'path sha256','Coin direction reference');path=(ROOT/definition['path']).resolve();require(path.is_relative_to(ROOT/'reports')and path.is_file()and hashlib.sha256(path.read_bytes()).hexdigest()==definition['sha256'],'Coin actual direction source changed');doc=json.loads(path.read_bytes(),object_pairs_hook=unique);require(type(doc['schema'])is int and doc['schema']==4 and doc['godot']['hash']=='3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8'and len(doc['directions'])==meta['direction_count'],'Coin direction engine/cardinality')
  validate_reference(doc,animations={c['name']for c in clips})
  for row in doc['directions']:
   require(type(row)is list and len(row)==2 and all(type(v)is int and 0<=v<=0xffffffff for v in row),'Coin direction binary32');point=list(struct.unpack('<2f',struct.pack('<2I',*row)));require(all(math.isfinite(v)for v in point)and abs(sum(v*v for v in point)-1)<1e-5,'Coin direction normalization');model['directions'].append(point)
 return p,model,dict(commit=PIN,recipe_sha256=hashlib.sha256(Path(file).read_bytes()).hexdigest(),producer_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),sources=s.digests(),symbols=p['symbols'],scope=p['scope'])

POLICY='minimum_speed maximum_speed air_time_multiplier minimum_height maximum_height deceleration sparkle_minimum sparkle_maximum spawn_interval'.split()
META='cash_amount body_layer body_mask pickup_layer pickup_mask frame_count initial_frame flipping grounded collect reset direction_count sparkle_count sparkle_initial sparkle_playing sparkle_loop default_coins pickup_area_bit'.split()
ASSETS=['coin_texture','sparkle_texture','shader','spawner','spawner_script']

def validate_model(m):
 fields(m,'policy meta radii factory_parent colors initial_modifiers panning margin delta sparkle_fps sparkle_position sprite_offset clips atlas directions paths voice','Coin model')
 fields(m['policy'],' '.join(POLICY),'Coin model policy')
 for value in m['policy'].values():number(value)
 fields(m['meta'],' '.join(k for k in META if k not in ['flipping','grounded','collect','reset']),'Coin model metadata')
 for key,value in m['meta'].items():
  if key in ['sparkle_playing','sparkle_loop']:require(type(value)is bool,'Coin model boolean')
  else:integer(value)
 fields(m['paths'],' '.join(PATH_KEYS),'Coin model source bindings')
 for value in [*m['paths'].values(),m['factory_parent'],m['voice']]:text(value)
 for key in ['radii','initial_modifiers','sparkle_position','sprite_offset']:
  require(type(m[key])is list and len(m[key])==2,'Coin model vector');[number(v)for v in m[key]]
 for key in ['panning','margin','delta','sparkle_fps']:number(m[key])
 for key,count,limit in [('colors',4,2),('directions',2,4096),('atlas',4,1024)]:
  require(type(m[key])is list and 0<len(m[key])<=limit,'Coin model array count')
  if key=='colors':require(len(m[key])==2,'Coin model colors')
  for row in m[key]:require(type(row)is list and len(row)==count,'Coin model array shape');[number(v)for v in row]
 require(type(m['clips'])is list and len(m['clips'])==4,'Coin model clip count');roles=set()
 for clip in m['clips']:
  fields(clip,'id name role duration loop keys','Coin model clip');integer(clip['id'],1,65535);text(clip['name']);require(clip['role']in ['flipping','grounded','collect','reset']and clip['role']not in roles and type(clip['loop'])is bool,'Coin model clip role/loop');roles.add(clip['role']);number(clip['duration']);require(type(clip['keys'])is list and 0<len(clip['keys'])<=2048,'Coin model key count')
  for key in clip['keys']:
   fields(key,'time target value update','Coin model key');integer(key['target'],1,2);integer(key['update'],0,1);number(key['time']);number(key['value'])
 return m

def encode(m):
 validate_model(m)
 strings=bytearray(b'\0');offsets={'':0}
 def string(value):
  text(value)
  if value not in offsets:offsets[value]=len(strings);strings.extend(value.encode('ascii')+b'\0')
  return offsets[value]
 roles={c['role']:c['id']for c in m['clips']};meta=dict(m['meta'],**roles)
 policy=struct.pack('<9d10f',*(m['policy'][v]for v in POLICY),*m['radii'],m['margin'],m['delta'],m['sparkle_fps'],m['panning'],*m['sprite_offset'],*m['sparkle_position'])
 metadata=struct.pack('<24I',*(int(meta[k])for k in META),string(m['paths']['drop_audio']),string(m['paths']['collect_audio']),string(m['voice']),string(m['paths']['scene']),string(m['paths']['script']),0)
 clips=bytearray();keys=bytearray()
 for c in m['clips']:
  clips.extend(struct.pack('<If4I',c['id'],c['duration'],int(c['loop']),len(keys)//24,len(c['keys']),0))
  for k in c['keys']:keys.extend(struct.pack('<Ifd2I',k['target'],k['time'],k['value'],k['update'],0))
 directions=b''.join(struct.pack('<2f',*p)for p in m['directions']);atlas=b''.join(struct.pack('<2I4f',i,string(m['paths']['sparkle_texture']),*v)for i,v in enumerate(m['atlas']));colors=struct.pack('<8f2d',*(v for row in m['colors']for v in row),*m['initial_modifiers']);assets=struct.pack('<6I',*(string(m['paths'][k])for k in ASSETS),string(m['factory_parent']))
 blob=f.pack_container(MAGIC,0,[policy,metadata,bytes(clips),bytes(keys),directions,atlas,colors,assets,bytes(strings)],STRIDES,LIMIT);decode(blob);return blob

def decode(blob):
 scene,sections=f.unpack_container(blob,MAGIC,STRIDES,LIMIT);require(scene==0,'Coin resource cannot admit a scene');policy,meta,clips,keys,directions,atlas,colors,assets,pool=sections
 require(len(policy)==112 and len(meta)==96 and len(colors)==48 and len(assets)==24 and 0<len(clips)<=64*24 and 0<len(keys)<=2048*24 and 0<len(directions)<=4096*8 and 0<len(atlas)<=1024*24 and 1<len(pool)<=8192,'Coin section counts')
 values=struct.unpack('<9d10f',policy);require(all(math.isfinite(v)and abs(v)<=1e6 for v in values),'Coin policy finite');require(0<values[0]<=values[1]and values[2]>0 and 0<values[3]<=values[4]and values[5]>0 and 0<=values[6]<=values[7]and values[8]>0 and values[9]>0 and values[10]>0 and 0<=values[11]<=100 and 0<values[12]<=1 and values[13]>0 and values[14]>=0,'Coin policy domain')
 words=struct.unpack('<24I',meta);require(0<words[0]<=1000000 and all(v<1<<20 for v in words[1:5])and 0<words[5]<=65535 and words[6]<words[5]and len(set(words[7:11]))==4 and words[11]==len(directions)//8 and words[12]==len(atlas)//24 and words[13]<words[12]and words[14]<=1 and words[15]<=1 and 0<words[16]<=1024 and words[17]<20 and not words[23],'Coin metadata domain')
 require(pool[0]==pool[-1]==0,'Coin string pool boundary');strings={};distinct=set();cursor=0
 while cursor<len(pool):
  end=pool.find(b'\0',cursor);require(end>=cursor,'Coin unterminated string');value=pool[cursor:end].decode('ascii');require((cursor==0 and value=='')or(cursor>0 and 0<len(value)<=512 and all(32<=ord(c)<127 for c in value)and '\\'not in value),'Coin pool text');require(value not in distinct,'Coin duplicate pool string');strings[cursor]=value;distinct.add(value);cursor=end+1
 used={0}
 def string(offset,path=True):
  require(offset in strings and bool(strings[offset]),'Coin string reference');value=strings[offset];used.add(offset)
  if path:require(not value.startswith('/')and ':'not in value and all(p not in['','.','..']for p in value.split('/')),'Coin source path')
  return value
 for n in [18,19,21,22]:string(words[n])
 string(words[20],False)
 asset_refs=struct.unpack('<6I',assets)
 for ref in asset_refs:string(ref)
 clip_ids=set();first=0
 for at in range(0,len(clips),24):
  identity,duration,loop,start,count,reserved=struct.unpack_from('<If4I',clips,at);require(0<identity<=65535 and identity not in clip_ids and math.isfinite(duration)and 0<duration<=60 and loop<=1 and start==first and 0<count<=len(keys)//24-first and not reserved,'Coin clip identity/span');clip_ids.add(identity);previous=(0,-1)
  for i in range(start,start+count):
   target,time,value,update,reserved=struct.unpack_from('<Ifd2I',keys,i*24);require(target in[1,2]and math.isfinite(time)and 0<=time<=duration and math.isfinite(value)and update==(1 if target==1 else 0)and not reserved and(target,time)>previous and(value==int(value)and 0<=value<words[5]if target==1 else 0<=value<=1),'Coin key target/mode/value/order');previous=(target,time)
  first+=count
 require(first==len(keys)//24 and clip_ids==set(words[7:11]),'Coin unowned keys/roles')
 for at in range(0,len(directions),8):
  x,y=struct.unpack_from('<2f',directions,at);require(math.isfinite(x)and math.isfinite(y)and abs(x*x+y*y-1)<1e-5,'Coin direction domain')
 for at in range(0,len(atlas),24):
  index,texture,x,y,w,h=struct.unpack_from('<2I4f',atlas,at);require(index==at//24 and texture==asset_refs[1]and all(math.isfinite(v)and 0<=v<=65535 for v in[x,y,w,h])and w>0 and h>0,'Coin atlas identity/rectangle');string(texture)
 cv=struct.unpack('<8f2d',colors);require(all(math.isfinite(v)and 0<=v<=1 for v in cv),'Coin material values');require(used==set(strings),'Coin unowned string data')
 return dict(policy=values,meta=words,clips=clips,keys=keys,directions=directions,atlas=atlas,colors=cv,assets=asset_refs,strings=strings)
