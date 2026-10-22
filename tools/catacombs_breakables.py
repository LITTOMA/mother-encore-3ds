"""Checked source model for the actual Catacombs Breakable script owners.

Content/tween/factory/voice execution is a separate prepared-host boundary.
This converter does not admit a complete scene or alter released resources.
"""
import argparse,hashlib,json,math,posixpath,re,struct,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import podunk_field as f,prop_clips
from tools.godot_text import Vec2,NodePathValue,SubRef,instantiate,owner_of
from tools.extract_battle_entry import PIN,require
from tools.reference_movement import extract
from tools.dungeon_keys import fields
from tools.dungeon_key_indicator import unique
RECIPE=ROOT/'content/catacombs-breakables.json'
MAGIC=b'ENCBRK01';STRIDES=(104,136,88,24,20,1,1);LIMIT=256*1024

def integer(value,lo=0,hi=0xffffffff):require(type(value)is int and lo<=value<=hi,'Breakable integer domain');return value
def number(value):require(type(value)in(int,float)and math.isfinite(value)and abs(value)<=1e6,'Breakable finite scalar');return float(value)
def vector(value):require(type(value)is Vec2,'Breakable source vector');return [f.f32(number(value.x)),f.f32(number(value.y))]
def text(value,empty=False):
 require(type(value)is str and (bool(value)or empty)and len(value)<=512 and all(32<=ord(c)<127 for c in value)and '\\'not in value and ':'not in value,'Breakable source ASCII text');return value
def boolean(value):require(type(value)is bool,'Breakable source boolean');return value
def stable(value):
 result=2166136261
 for byte in value.encode('ascii'):result=((result^byte)*16777619)&0xffffffff
 require(0<result<0xffffffff,'Breakable stable identity');return result

def read(file=RECIPE):
 p=json.loads(Path(file).read_bytes(),object_pairs_hook=unique);fields(p,'schema kind commit scene_id paths sources symbols families owners audio_recipe audio_recipe_sha256 scope license_review','breakable recipe')
 require(type(p['schema'])is int and p['schema']==1 and p['kind']=='encore.catacombs-breakables.reviewed-source'and p['commit']==PIN,'Breakable reviewed source identity');integer(p['scene_id'],1,65535)
 require(type(p['paths'])is dict and set(p['paths'])==set('scene script flag_script global_data global coffin barrel hurtbox coin_spawner coin_spawner_script coin_spawner_base coffin_texture barrel_texture sound sound_import license'.split()),'Breakable source path coverage')
 s=f.Sources();require(type(p['sources'])is dict and set(p['paths'].values())<=set(p['sources']),'Breakable source digest coverage')
 for name,digest in p['sources'].items():
  text(name);require(not name.startswith('/')and all(v not in ('','.','..')for v in name.split('/'))and type(digest)is str and re.fullmatch('[0-9a-f]{64}',digest)and hashlib.sha256(s.data(name)).hexdigest()==digest,'Breakable original source changed: '+name)
 expected={'script':['_ready','_on_Hurtbox_area_entered','set_item','_check_flags','_drop','interact'],'flag_script':['_ready','_get_flag_status','_set_flag_status','_on_leave_area'],'global_data':['check_appear_disappear_flags','set_flag','set_object_flag'],'global':['start_joy_vibration'],'coin_spawner_script':['spawn_coins']}
 require(type(p['symbols'])is dict and set(p['symbols'])==set(expected),'Breakable method family coverage')
 for key,names in expected.items():
  raw=s.data(p['paths'][key]);_,digests=extract(raw,hashlib.sha256(raw).hexdigest(),names);require(p['symbols'][key]==digests,'Breakable reviewed method changed')
  if key in ['script','flag_script']:require(re.findall(r'^func ([A-Za-z_][A-Za-z_0-9]*)\(',raw.decode(),re.M)==names,'Breakable unmapped source method')
 require(type(p['families'])is list and len(p['families'])==2 and type(p['owners'])is list and 1<=len(p['owners'])<=128,'Breakable source family/owner count')
 require(p['audio_recipe']=='content/native-audio.json'and hashlib.sha256((ROOT/p['audio_recipe']).read_bytes()).hexdigest()==p['audio_recipe_sha256'],'Breakable shared audio source changed')
 require(all(type(p[k])is str and 0<len(p[k])<=4096 for k in ['scope','license_review']),'Breakable source scope/license');return p,s

def one(pattern,value,label):
 found=re.findall(pattern,value,re.M);require(len(found)==1,'Breakable ambiguous source '+label);return found[0]

def policy(p,s):
 raw=s.text(p['paths']['script']);delay=number(float(one(r'var delay = ([0-9.]+) \* distance',raw,'distance delay')));cap=one(r'if delay > ([0-9.]+): delay = ([0-9.]+)',raw,'delay clamp');require(cap[0]==cap[1],'Breakable clamp comparison/assignment differ')
 rumble=one(r'global.start_joy_vibration\((\d+), ([0-9.]+), ([0-9.]+), ([0-9.]+)\)',raw,'rumble');lift=one(r'tween_property\(item, "position", Vector2\(item.position.x, item.position.y - ([0-9.]+)\), ([0-9.]+)\)',raw,'lift');fall=one(r'tween_property\(item, "position", Vector2\(item.position.x, item.position.y \+ ([0-9.]+)\), ([0-9.]+)\)',raw,'fall')
 require('set_trans(Tween.TRANS_CUBIC).set_ease(Tween.EASE_OUT)'in raw and 'set_trans(Tween.TRANS_BOUNCE).set_ease(Tween.EASE_OUT)'in raw,'Breakable source tween curve coverage')
 layer=[int(v)for v in re.findall(r'\$Hurtbox.set_collision_layer_bit\((\d+), false\)',raw)];mask=[int(v)for v in re.findall(r'\$Hurtbox.set_collision_mask_bit\((\d+), false\)',raw)];body=[int(v)for v in re.findall(r'\$StaticBody2D.set_collision_layer_bit\((\d+),false\)',raw)];require(layer and mask and body and max(layer+mask+body)<20,'Breakable bit clear source coverage')
 defaults={key:False for key in ['Drop_item_if_hidden','interact_content','pause_on_break']}
 require('export (bool) var Drop_item_if_hidden'in raw and 'export var interact_content = false'in raw and 'export var pause_on_break := false'in raw,'Breakable boolean default coverage')
 coins=int(one(r'^export var coins_spawned := (\d+)$',raw,'coin count default'))
 return dict(delay_multiplier=delay,maximum_delay=number(float(cap[0])),device=int(rumble[0]),rumble=list(map(float,rumble[1:])),lift=[-float(lift[0]),float(lift[1])],fall=list(map(float,fall)),lift_curve='cubic_out',fall_curve='bounce_out',clear_hit_layer=sum(1<<v for v in layer),clear_hit_mask=sum(1<<v for v in mask),clear_body_layer=sum(1<<v for v in body),default_coins=coins,defaults=defaults)

def family(binding,p,s):
 fields(binding,'id scene sprite animation audio body body_shape hitbox hit_shape drop_point timer clips','breakable family');integer(binding['id'],1,0xfffffffe);require(binding['scene']in [p['paths']['coffin'],p['paths']['barrel']]and binding['id']==stable('breakable-family:'+binding['scene']),'Breakable source family identity')
 nodes=instantiate(s.doc,binding['scene']);by={n.path:n for n in nodes};root=by['.'];require(root.type=='Sprite'and root.script==p['paths']['script'],'Breakable prefab script/type')
 for key in ['sprite','animation','audio','body','body_shape','hitbox','hit_shape','drop_point','timer']:text(binding[key]);require(binding[key]in by,'Breakable missing family target')
 sprite=by[binding['sprite']];require(sprite.type=='Sprite','Breakable sprite target');doc=owner_of(sprite,'texture');texture=doc.ext_path(sprite.props['texture'],'Texture');require(texture in [p['paths']['coffin_texture'],p['paths']['barrel_texture']],'Breakable unreviewed texture')
 columns=integer(sprite.props.get('hframes',1),1,1024);rows=integer(sprite.props.get('vframes',1),1,1024);width,height=s.extractor.png_size(texture);require(width%columns==0 and height%rows==0,'Breakable nonintegral source sprite grid')
 animator=by[binding['animation']];require(animator.type=='AnimationPlayer'and set(animator.props)=={'anims/'+c['name']for c in binding['clips']},'Breakable animation/autoplay coverage');require(type(binding['clips'])is list and {v['role']for v in binding['clips']}=={'break','ready','reset'},'Breakable clip role coverage')
 definitions=[];audio=[];mapping={binding['sprite']+':frame':1,binding['sprite']+':visible':2,binding['body_shape']+':disabled':3,binding['audio']+':playing':6}
 for clip in binding['clips']:
  fields(clip,'id name role','breakable clip');integer(clip['id'],1,65535);text(clip['name']);reference=animator.props['anims/'+clip['name']];require(type(reference)is SubRef,'Breakable local source clip');d=owner_of(animator,'anims/'+clip['name']);props=d.sub_resource(reference,'Animation');require(not props.get('loop',False),'Breakable source looping clip');duration=f.f32(number(props.get('length',1)));group={}
  for key,value in props.items():
   if key.startswith('tracks/'):
    parts=key.split('/');require(len(parts)==3 and parts[1].isdigit(),'Breakable source track field');group.setdefault(int(parts[1]),{})[parts[2]]=value
   else:require(key in ['resource_name','length','step','loop'],'Breakable unknown animation property')
  require(props.get('resource_name',clip['name'])==clip['name']and 0<duration<=60,'Breakable source clip name/duration')
  require(set(group)==set(range(len(group)))and group,'Breakable track index coverage');keys=[];audio_first=len(audio)
  for index,track in group.items():
   fields(track,'type path interp loop_wrap imported enabled keys','breakable source track');require(track['type']=='value'and track['interp']==1 and type(track['interp'])is int and track['loop_wrap']is True and track['imported']is False and track['enabled']is True and type(track['path'])is NodePathValue and track['path'].path in mapping,'Breakable unknown property track');kind=mapping[track['path'].path];k=track['keys'];fields(k,'times transitions update values','breakable keys');require(type(k['update'])is int and k['update']in [0,1]and all(type(k[v])is list for v in ['times','transitions','values'])and 0<len(k['times'])==len(k['transitions'])==len(k['values'])<=256 and all(number(v)==1 for v in k['transitions']),'Breakable source key modes/counts')
   if k['update']==0:require(k['times']==[0.0],'Breakable unsupported continuous curve')
   previous=-1
   for time,value in zip(k['times'],k['values']):
    time=f.f32(number(time));require(previous<time<=duration,'Breakable key time order/range');previous=time;require(type(value)is(int if kind==1 else bool),'Breakable source key type')
    if kind==1:integer(value,0,columns*rows-1)
    if kind==6:audio.append(dict(clip=clip['id'],time=time,value=int(value),update=k['update'],source_track=index))
    else:keys.append(dict(time=time,kind=kind,value=int(value)))
  keys.sort(key=lambda v:(v['time'],v['kind']));require(len({(v['time'],v['kind'])for v in keys})==len(keys)and keys,'Breakable duplicate/unowned property keys');definitions.append(dict(id=clip['id'],name=clip['name'],role=clip['role'],duration=duration,keys=keys,audio_first=audio_first,audio_count=len(audio)-audio_first))
 inner=dict(frames=columns*rows,initial_frame=integer(sprite.props.get('frame',0),0,columns*rows-1),initial_mask=int(boolean(sprite.props.get('visible',True)))|int(not boolean(by[binding['body_shape']].props.get('disabled',False)))<<1,clips=definitions)
 sound=by[binding['audio']];require(sound.type in ['AudioStreamPlayer','AudioStreamPlayer2D'],'Breakable audio target type');sound_doc=owner_of(sound,'stream');require(sound_doc.ext_path(sound.props['stream'],'AudioStream')==p['paths']['sound']and sound.props.get('bus','Master')=='SFX','Breakable source audio binding')
 timer=by[binding['timer']];require(timer.type=='Timer'and set(timer.props)=={'wait_time','one_shot'}and timer.props['one_shot']is True,'Breakable source drop timer')
 body=by[binding['body']];hit=by[binding['hitbox']];require(body.type=='StaticBody2D'and hit.type=='Area2D','Breakable collision node binding');shapes=[]
 for key in ['body_shape','hit_shape']:
  n=by[binding[key]];d=owner_of(n,'shape');shape=d.sub_resource(n.props['shape'],'RectangleShape2D');require(set(shape)=={'extents'}and not n.props.get('disabled',False),'Breakable source shape metadata');shapes.append(dict(position=vector(n.props.get('position',Vec2(0,0))),extents=vector(shape['extents'])))
 return dict(id=binding['id'],source=binding['scene'],texture=texture,texture_sha256=hashlib.sha256(s.data(texture)).hexdigest(),grid=[columns,rows,width,height],sprite_offset=vector(sprite.props.get('position',Vec2(0,0))),drop_offset=vector(by[binding['drop_point']].props.get('position',Vec2(0,0))),drop_seconds=f.f32(number(timer.props['wait_time'])),body_layer=body.props.get('collision_layer',1),body_mask=body.props.get('collision_mask',1),hit_layer=hit.props.get('collision_layer',1),hit_mask=hit.props.get('collision_mask',1),shapes=shapes,clip_data=prop_clips.encode(inner),clips=definitions,audio=audio,audio_spatial=sound.type=='AudioStreamPlayer2D',sound=p['paths']['sound'])

def source(file=RECIPE):
 p,s=read(file);nodes=instantiate(s.doc,p['paths']['scene']);by={n.path:n for n in nodes};definitions=[family(v,p,s)for v in p['families']];families={v['source']:v['id']for v in definitions};require(len(families)==2 and len({v['id']for v in definitions})==2,'Breakable duplicate source families');rules=policy(p,s);actual=[n for n in nodes if n.script==p['paths']['script']];require([n.path for n in actual]==[v['path']for v in p['owners']],'Breakable exact source owner order');owners=[];ids=set();targets=[];target_ids=set();parents=set()
 def resolve(node,name,default=''):
  value=node.props.get(name,NodePathValue(default));require(type(value)is NodePathValue,'Breakable node reference type');text(value.path,True)
  if not value.path:return ''
  path=posixpath.normpath(node.path+'/'+value.path);require(path in by,'Breakable node reference unresolved');return path
 allowed=set('script position contents Drop_item_if_hidden Drop_where New_parent appear_flag disappear_flag interact_content pause_on_break coins_spawned coin_spawner flag is_object_flag emit_flag_updated_signal reset_when_leaving_region reset_when_leaving_area'.split())
 for binding,node in zip(p['owners'],actual):
  fields(binding,'id path family','breakable owner');integer(binding['id'],1,0xfffffffe);require(binding['id']==stable('catacombs-breakable:'+node.path)and binding['id']not in ids and binding['family']==families[node.scene]and set(node.props)<=allowed,'Breakable unknown/duplicate owner override');ids.add(binding['id']);parent=f.parent_path(node.path);new_parent=resolve(node,'New_parent');drop=resolve(node,'Drop_where')or'Objects';require(all(path in by for path in [parent,drop])and (not new_parent or new_parent in by),'Breakable parent binding');parents.update(v for v in [parent,new_parent,drop]if v)
  for path in [parent,new_parent,drop]:
   current=path
   while current:
    ancestor=by[current];require(not(set(ancestor.props)&{'transform','rotation','rotation_degrees','scale','skew'})and vector(ancestor.props.get('position',Vec2(0,0)))==[0.,0.],'Breakable unreviewed parent transform');current=f.parent_path(current)
  target=resolve(node,'contents');spawner=resolve(node,'coin_spawner');require(not spawner or by[spawner].script==p['paths']['coin_spawner_script'],'Breakable factory class binding')
  if target and target not in target_ids:target_ids.add(target);t=by[target];targets.append(dict(path=target,type=t.type,script=t.script,source=t.scene,parent=f.parent_path(target)))
  flag=text(node.props.get('flag',''),True);is_object=boolean(node.props.get('is_object_flag',False));flag_kind='object'if not flag or is_object else'story';flag=flag or by['.'].name+'/'+node.name
  owners.append(dict(**binding,parent=parent,new_parent=new_parent,drop_parent=drop,drop_literal=node.props.get('Drop_where',NodePathValue('')).path,content=target,spawner=spawner,position=vector(node.props.get('position',Vec2(0,0))),coins=integer(node.props.get('coins_spawned',rules['default_coins']),0,1024),flag=flag,flag_kind=flag_kind,emit_flag=boolean(node.props.get('emit_flag_updated_signal',False)),reset_area=boolean(node.props.get('reset_when_leaving_area',False)),reset_region=boolean(node.props.get('reset_when_leaving_region',False)),appear=text(node.props.get('appear_flag',''),True),disappear=text(node.props.get('disappear_flag',''),True),**{key:boolean(node.props.get(key,rules['defaults'][key]))for key in rules['defaults']}))
 audio=json.loads((ROOT/p['audio_recipe']).read_bytes(),object_pairs_hook=unique);entries=[v for v in audio['assets']if v['source_path']=='res://'+p['paths']['sound']];require(len(entries)==1 and entries[0]['source_sha256']==p['sources'][p['paths']['sound']],'Breakable shared sound exact source')
 model=dict(scene=p['scene_id'],source=p['paths']['scene'],name=by['.'].name,policy=rules,families=definitions,owners=owners,contents=targets,parents=[dict(path=path,source_order=nodes.index(by[path]))for path in sorted(parents)],sound=entries[0]);proof=dict(commit=PIN,recipe_sha256=hashlib.sha256(Path(file).read_bytes()).hexdigest(),producer_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),sources=s.digests(),symbols=p['symbols'],scope=p['scope']);return p,model,proof


def encode(model):
 """Independent checked resource; nested ENCPCL01 and saved identity unchanged."""
 validate_model(model)
 pool=bytearray(b'\0');strings={'':0}
 def string(value):
  text(value,True)
  if value not in strings:strings[value]=len(pool);pool.extend(value.encode('ascii')+b'\0')
  return strings[value]
 p=model['policy'];policy=struct.pack('<9d8I',p['delay_multiplier'],p['maximum_delay'],*p['rumble'],*p['lift'],*p['fall'],model['scene'],p['device'],p['clear_hit_layer'],p['clear_hit_mask'],p['clear_body_layer'],model['sound']['stable_id'],string(model['source']),string(model['name']))
 families=bytearray();owners=bytearray();targets=bytearray();audio=bytearray();clips=bytearray();target_index={v['path']:i for i,v in enumerate(model['contents'])}
 for v in model['families']:
  ids={c['role']:c['id']for c in v['clips']};first=len(audio)//20
  for key in v['audio']:audio.extend(struct.pack('<If3I',key['clip'],key['time'],key['value'],key['update'],key['source_track']))
  families.extend(struct.pack('<20I13fI',v['id'],string(v['source']),string(v['texture']),*v['grid'],len(clips),len(v['clip_data']),ids['break'],ids['ready'],ids['reset'],first,len(v['audio']),int(v['audio_spatial']),v['body_layer'],v['body_mask'],v['hit_layer'],v['hit_mask'],0,*v['sprite_offset'],*v['drop_offset'],v['drop_seconds'],*[n for shape in v['shapes']for n in [*shape['position'],*shape['extents']]],0));clips.extend(v['clip_data'])
 for v in model['owners']:
  bits=sum(int(v[k])<<i for i,k in enumerate(['emit_flag','reset_area','reset_region','Drop_item_if_hidden','interact_content','pause_on_break']))
  owners.extend(struct.pack('<20I2f',v['id'],v['family'],*[string(v[k])for k in ['path','parent','new_parent','drop_parent']],target_index[v['content']]if v['content']else 0xffffffff,string(v['spawner']),v['coins'],string(v['flag']),int(v['flag_kind']=='object'),bits,string(v['appear']),string(v['disappear']),int(v['drop_literal']in ['','Objects']),int(v['drop_parent'].split('/')[-1]=='Objects'),0,0,0,0,*v['position']))
 for v in model['contents']:targets.extend(struct.pack('<6I',*[string(v[k])for k in ['path','type','script','source','parent']],0))
 blob=f.pack_container(MAGIC,model['scene'],[policy,families,owners,targets,audio,clips,pool],STRIDES,LIMIT);parse(blob);return blob


def validate_model(model):
 def safe(value,empty=False):text(value,empty);require((empty and not value)or(not value.startswith('/')and not value.endswith('/')and all(p not in ['','.','..']for p in value.split('/'))),'Breakable model path escaped')
 fields(model,'scene source name policy families owners contents parents sound','Breakable model');integer(model['scene'],1,65535);text(model['source']);text(model['name']);p=model['policy'];fields(p,'delay_multiplier maximum_delay device rumble lift fall lift_curve fall_curve clear_hit_layer clear_hit_mask clear_body_layer default_coins defaults','Breakable policy model')
 require(p['lift_curve']=='cubic_out'and p['fall_curve']=='bounce_out','Breakable unreviewed tween curve')
 for key in ['delay_multiplier','maximum_delay']:number(p[key])
 for key,size in [('rumble',3),('lift',2),('fall',2)]:require(type(p[key])is list and len(p[key])==size,'Breakable policy vector');[number(v)for v in p[key]]
 for key in ['device','clear_hit_layer','clear_hit_mask','clear_body_layer','default_coins']:integer(p[key])
 fields(p['defaults'],'Drop_item_if_hidden interact_content pause_on_break','Breakable source defaults');[boolean(v)for v in p['defaults'].values()]
 require(all(type(model[k])is list for k in ['families','owners','contents','parents']),'Breakable model arrays')
 for v in model['families']:
  fields(v,'id source texture texture_sha256 grid sprite_offset drop_offset drop_seconds body_layer body_mask hit_layer hit_mask shapes clip_data clips audio audio_spatial sound','Breakable family model');integer(v['id'],1,0xfffffffe);[text(v[k])for k in ['source','texture','sound']];require(type(v['texture_sha256'])is str and re.fullmatch('[0-9a-f]{64}',v['texture_sha256']),'Breakable texture digest');require(type(v['grid'])is list and len(v['grid'])==4,'Breakable grid model');[integer(n,1,65535)for n in v['grid']]
  for key in ['sprite_offset','drop_offset']:require(type(v[key])is list and len(v[key])==2,'Breakable offset model');[number(n)for n in v[key]]
  number(v['drop_seconds']);[integer(v[k],0,(1<<20)-1)for k in ['body_layer','body_mask','hit_layer','hit_mask']];boolean(v['audio_spatial']);require(type(v['clip_data'])is bytes and type(v['clips'])is list and len(v['clips'])==3 and type(v['audio'])is list and type(v['shapes'])is list and len(v['shapes'])==2,'Breakable family array/blob model')
  for shape in v['shapes']:
   fields(shape,'position extents','Breakable shape model')
   for key in ['position','extents']:require(type(shape[key])is list and len(shape[key])==2,'Breakable shape vector');[number(n)for n in shape[key]]
  for key in v['audio']:fields(key,'clip time value update source_track','Breakable audio model');integer(key['clip'],1,65535);number(key['time']);integer(key['value'],0,1);integer(key['update'],0,1);integer(key['source_track'],0,63)
  require({c['role']for c in v['clips']}=={'break','ready','reset'},'Breakable clip role model')
  for clip in v['clips']:
   fields(clip,'id name role duration keys audio_first audio_count','Breakable clip model');integer(clip['id'],1,65535);text(clip['name']);number(clip['duration']);integer(clip['audio_first']);integer(clip['audio_count']);require(type(clip['keys'])is list,'Breakable property keys')
   for key in clip['keys']:fields(key,'time kind value','Breakable property key');number(key['time']);integer(key['kind'],1,3);integer(key['value'])
  _,inner=f.unpack_container(v['clip_data'],prop_clips.MAGIC,prop_clips.STRIDES,prop_clips.LIMIT);prop_clips_checked(inner);initial=struct.unpack('<4I',inner[0]);expected=prop_clips.encode(dict(frames=v['grid'][0]*v['grid'][1],initial_frame=initial[1],initial_mask=initial[2],clips=v['clips']));require(expected==v['clip_data'],'Breakable property clip model/blob differ')
 for v in model['owners']:
  fields(v,'id path family parent new_parent drop_parent drop_literal content spawner position coins flag flag_kind emit_flag reset_area reset_region appear disappear Drop_item_if_hidden interact_content pause_on_break','Breakable owner model');integer(v['id'],1,0xfffffffe);integer(v['family'],1,0xfffffffe);[text(v[k],True)for k in ['path','parent','new_parent','drop_parent','drop_literal','content','spawner','flag','appear','disappear']];require(v['flag_kind']in ['object','story']and type(v['position'])is list and len(v['position'])==2,'Breakable owner model flag/vector');[number(n)for n in v['position']];integer(v['coins'],0,1024);[boolean(v[k])for k in ['emit_flag','reset_area','reset_region','Drop_item_if_hidden','interact_content','pause_on_break']]
 for v in model['contents']:fields(v,'path type script source parent','Breakable content model');[text(n,True)for n in v.values()]
 for v in model['parents']:fields(v,'path source_order','Breakable parent model');text(v['path']);integer(v['source_order'])
 fields(model['sound'],'stable_id source_path source_sha256 import_sha256 pcm_path gain_db','Breakable sound source binding');integer(model['sound']['stable_id'],1,0xfffffffe)
 targets={v['path']for v in model['contents']};safe(model['source'])
 for v in model['owners']:
  require(not v['content']or v['content']in targets,'Breakable unresolved content reference')
  for key in ['path','parent','new_parent','drop_parent','content','spawner']:safe(v[key],key in ['new_parent','content','spawner'])
 for v in model['contents']:
  for key in ['path','script','source','parent']:safe(v[key],key=='script')


def parse(blob):
 scene,sections=f.unpack_container(blob,MAGIC,STRIDES,LIMIT,capability=1);policy,families,owners,targets,audio,clips,pool=sections
 require(0<scene<=65535 and len(policy)==104 and 1<=len(families)//136<=8 and 1<=len(owners)//88<=128 and len(targets)//24<=128 and len(audio)//20<=256 and len(clips)<=65536 and pool[:1]==b'\0'and pool[-1:]==b'\0'and len(pool)<=65536,'Breakable section counts')
 strings={};at=0
 while at<len(pool):
  end=pool.index(0,at);value=pool[at:end].decode('ascii');text(value,at==0);require(value not in strings.values(),'Breakable duplicate string');strings[at]=value;at=end+1
 def string(offset,empty=False):require(offset in strings and(empty or bool(strings[offset])),'Breakable string boundary');return strings[offset]
 def path(offset,empty=False):
  value=string(offset,empty);require(not value.startswith('/')and all(c not in ['.','..']for c in value.split('/')),'Breakable unsafe path');return value
 p=struct.unpack('<9d8I',policy);require(all(math.isfinite(v)and abs(v)<=1e6 for v in p[:9])and p[0]>0 and p[1]>=0 and all(0<=v<=1 for v in p[2:4])and p[4]>=0 and p[5]<=0 and p[6]>0 and p[7]>=0 and p[8]>0 and p[9]==scene and p[10]<16 and all(0<v<1<<20 for v in p[11:14])and 0<p[14]<0xffffffff,'Breakable policy domain');path(p[15]);string(p[16])
 target_rows=[];target_paths=set()
 for at in range(0,len(targets),24):
  row=struct.unpack_from('<6I',targets,at);require(not row[5],'Breakable target reserved');value=[path(row[0]),string(row[1]),path(row[2],True),path(row[3]),path(row[4])];require(value[0]not in target_paths,'Breakable duplicate content');target_paths.add(value[0]);target_rows.append(value)
 family_rows=[];family_ids=set();clip_first=0;audio_first=0
 for at in range(0,len(families),136):
  v=struct.unpack_from('<20I13fI',families,at);require(0<v[0]<0xffffffff and v[0]not in family_ids and not v[19]and not v[33]and all(0<n<=65535 for n in v[3:7])and v[5]%v[3]==0 and v[6]%v[4]==0 and v[7]==clip_first and 0<v[8]<=65536-clip_first and v[7]+v[8]<=len(clips)and v[12]==audio_first and v[13]<=len(audio)//20-audio_first and v[14]in[0,1]and all(n<1<<20 for n in v[15:19])and all(math.isfinite(n)and abs(n)<=1e6 for n in v[20:33])and 0<v[24]<=60 and all(v[n]>0 for n in [27,28,31,32]),'Breakable family domain/span');path(v[1]);path(v[2]);family_ids.add(v[0]);inner=clips[v[7]:v[7]+v[8]];_,inner_sections=f.unpack_container(inner,prop_clips.MAGIC,prop_clips.STRIDES,prop_clips.LIMIT);initial,inner_clips,keys=inner_sections;require(len(initial)==16 and struct.unpack_from('<I',initial)[0]==v[3]*v[4]and len(inner_clips)==72 and set(v[9:12])=={struct.unpack_from('<I',inner_clips,i)[0]for i in range(0,72,24)}and len(set(v[9:12]))==3,'Breakable nested clip roles/grid')
  prop_clips_checked(inner_sections);durations={struct.unpack_from('<I',inner_clips,i)[0]:struct.unpack_from('<f',inner_clips,i+4)[0]for i in range(0,72,24)};last={}
  for index in range(v[12],v[12]+v[13]):
   a=struct.unpack_from('<If3I',audio,index*20);require(a[0]in durations and math.isfinite(a[1])and 0<=a[1]<=durations[a[0]]and a[2]in[0,1]and a[3]in[0,1]and a[4]<64 and a[1]>last.get(a[0],-1)and(a[3]!=0 or a[1]==0),'Breakable audio key domain/order');last[a[0]]=a[1]
  clip_first+=v[8];audio_first+=v[13];family_rows.append(v)
 require(clip_first==len(clips)and audio_first==len(audio)//20,'Breakable unowned nested/audio rows')
 owner_ids=set();owner_paths=set();owner_rows=[];used_targets=set()
 for at in range(0,len(owners),88):
  v=struct.unpack_from('<20I2f',owners,at);require(0<v[0]<0xffffffff and v[0]not in owner_ids and v[1]in family_ids and(v[6]==0xffffffff or v[6]<len(target_rows))and v[8]<=1024 and v[10]<=1 and v[11]<64 and v[14]<=1 and v[15]<=1 and not any(v[16:20])and all(math.isfinite(n)and abs(n)<=1e6 for n in v[20:22]),'Breakable owner domain');name=path(v[2]);require(name not in owner_paths,'Breakable duplicate owner');path(v[3]);path(v[4],True);path(v[5]);path(v[7],True);string(v[9]);string(v[12],True);string(v[13],True);owner_ids.add(v[0]);owner_paths.add(name);owner_rows.append(v)
  if v[6]!=0xffffffff:used_targets.add(v[6])
 require(used_targets==set(range(len(target_rows))),'Breakable unowned content');return dict(scene=scene,policy=p,families=family_rows,owners=owner_rows,contents=target_rows,strings=strings)


def prop_clips_checked(sections):
 initial,clips,keys=sections;require(len(initial)==16 and len(clips)==72 and len(keys)%12==0,'Breakable nested section counts');frames,frame,mask,reserved=struct.unpack('<4I',initial);require(0<frames<=65535 and frame<frames and mask<4 and not reserved,'Breakable initial pose');first=0;ids=set()
 for at in range(0,len(clips),24):
  identity,duration,start,count,r1,r2=struct.unpack_from('<If4I',clips,at);require(0<identity<=65535 and identity not in ids and math.isfinite(duration)and 0<duration<=60 and start==first and 0<count<=len(keys)//12-first and not(r1 or r2),'Breakable nested clip domain');ids.add(identity);previous=(-1,0)
  for index in range(start,start+count):
   time,kind,value=struct.unpack_from('<f2I',keys,index*12);require(math.isfinite(time)and 0<=time<=duration and kind in[1,2,3]and(value<frames if kind==1 else value<=1)and(time,kind)>previous,'Breakable nested key domain/order');previous=(time,kind)
  first+=count
 require(first==len(keys)//12,'Breakable unowned nested keys')

if __name__=='__main__':
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--out',type=Path,required=True);out=ap.parse_args().out.resolve();require(out.is_relative_to(ROOT/'build')and out!=ROOT/'build'and not out.exists(),'Fresh Breakable source candidate under build required');_,model,proof=source();out.mkdir(parents=True)
 blob=encode(model);(out/'breakables.encbrk').write_bytes(blob)
 for family in model['families']:blob=family.pop('clip_data');name='family-'+str(family['id'])+'.encpcl';(out/name).write_bytes(blob);family['clip_data']=dict(path=name,bytes=len(blob),sha256=hashlib.sha256(blob).hexdigest())
 (out/'model.json').write_text(json.dumps(model,indent=2)+'\n');(out/'source.json').write_text(json.dumps(proof,indent=2)+'\n');print('Checked65 original Breakable owners in independent ENCBRK01;not formal scene admission')
