"""Compile checked wall property/audio/deferred-shake timeline candidates.

Only the declared four source animations and typed tracks are admitted. No
script VM, live scene, PCM, texture, catalog or save activation is performed.
"""
import argparse,hashlib,json,math,re,struct,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.extract_battle_entry import Extractor,PIN,require
from tools.godot_text import Document,SubRef,NodePathValue
from tools.podunk_field import pack_container,unpack_container
from tools import dungeon_switches,prop_clips
from tools.audio_asset import import_settings
from tools.run_battle_victory_reference import function
RECIPE=ROOT/'content/catacombs-wall-timeline.json'
MAGIC=b'ENCDWT01';STRIDES=(1,128,1,16,16);LIMIT=65536
ROLES=('On','RESET','StayOff','StayOn')
def fields(p,names):require(type(p)is dict and set(p)==set(names.split()),'Wall timeline fields')
def integer(v,low,high):require(type(v)is int and low<=v<=high,'Wall timeline integer');return v
def real(v,low,high):require(type(v)in(int,float)and math.isfinite(v)and low<=v<=high,'Wall timeline finite real');return float(v)
def path(v,uri=False):
 require(type(v)is str,'Wall timeline path type')
 if uri:require(v.startswith('res://'),'Wall timeline source URI');v=v[6:]
 require(0<len(v)<=512 and all(32<=ord(c)<127 and c not in '\\:'for c in v)and all(p not in('','.','..')for p in v.split('/')),'Wall timeline unsafe path');return v
def read(file=RECIPE):
 def unique(rows):
  out={}
  for k,v in rows:require(k not in out,'Duplicate wall timeline JSON');out[k]=v
  return out
 def invalid(v):raise ValueError('Nonfinite wall timeline JSON')
 p=json.loads(Path(file).read_text(encoding='utf8'),object_pairs_hook=unique,parse_constant=invalid)
 fields(p,'schema kind commit scope graph_recipe graph_review_sha256 owner paths roles audio_identity sources license_review')
 require(type(p['schema'])is int and p['schema']==1 and p['kind']=='encore.dungeon-wall-timeline.reviewed-source'and p['commit']==PIN,'Wall timeline review identity')
 require(all(type(p[k])is str and 0<len(p[k])<=2048 for k in('scope','license_review')),'Wall timeline scope/permission')
 require(p['graph_recipe']=='content/catacombs-switches.json'and hashlib.sha256((ROOT/p['graph_recipe']).read_bytes()).hexdigest()==p['graph_review_sha256'],'Wall graph review dependency changed')
 fields(p['owner'],'scene_id id path');integer(p['owner']['scene_id'],1,65535);integer(p['owner']['id'],1,2**32-1);path(p['owner']['path'])
 fields(p['paths'],'scene script texture audio audio_import camera shaker license')
 for value in p['paths'].values():path(value)
 require(p['paths']['audio_import']==p['paths']['audio']+'.import','Wall audio import selector')
 require(type(p['sources'])is dict and set(p['sources'])==set(p['paths'].values()),'Wall source coverage');ex=Extractor(ROOT)
 for name,digest in p['sources'].items():path(name);require(type(digest)is str and len(digest)==64 and hashlib.sha256(ex.data(name)).hexdigest()==digest,'Wall reviewed source changed: '+name)
 require(type(p['roles'])is list and len(p['roles'])==4,'Wall animation roles');ids=set()
 for row,name in zip(p['roles'],ROLES):
  fields(row,'role id name');require(row['role']==row['name']==name,'Wall animation role/name');integer(row['id'],1,65535);require(row['id']not in ids,'Duplicate wall role ID');ids.add(row['id'])
 fields(p['audio_identity'],'kind value assignment');require(p['audio_identity']['kind']=='stable'and type(p['audio_identity']['assignment'])is str and p['audio_identity']['assignment'],'Wall audio port identity');integer(p['audio_identity']['value'],1,2**32-1)
 require("purpose you're using them is related to the game"in ex.text(p['paths']['license']),'Wall source permission changed')
 return p,ex

def properties(blob):
 scene,parts=unpack_container(blob,prop_clips.MAGIC,prop_clips.STRIDES,prop_clips.LIMIT)
 require(scene==0 and len(parts[0])==16 and len(parts[1])==4*24 and 0<len(parts[2])<=4096*12,'Wall nested property sections')
 frames,initial,mask,reserved=struct.unpack('<4I',parts[0]);require(0<frames<=65535 and initial<frames and mask<=3 and reserved==0,'Wall nested initial properties')
 clips=[];ids=set();first=0
 for at in range(0,len(parts[1]),24):
  ident,duration,start,count,r1,r2=struct.unpack_from('<If4I',parts[1],at);integer(ident,1,65535);require(ident not in ids,'Wall nested duplicate clip');ids.add(ident);real(duration,0,60)
  require(duration>0 and start==first and 0<count<=(len(parts[2])//12)-first and r1==r2==0,'Wall nested clip ownership');keys=[];previous=(-1,0)
  for i in range(start,start+count):
   time,kind,value=struct.unpack_from('<f2I',parts[2],i*12);real(time,0,duration);require(kind in(1,3)and(value<frames if kind==1 else value<=1)and(time,kind)>previous,'Wall nested unsupported/order/value');previous=(time,kind);keys.append(dict(time=time,kind=kind,value=value))
  clips.append(dict(id=ident,duration=duration,keys=keys));first+=count
 require(first==len(parts[2])//12,'Wall nested unowned properties')
 return dict(frames=frames,initial_frame=initial,initial_mask=mask,clips=clips)

def parse(blob):
 scene,parts=unpack_container(blob,MAGIC,STRIDES,LIMIT);require(0<scene<65536 and len(parts[1])==128 and len(parts[3])==64 and 0<len(parts[4])<=1024*16,'Wall timeline section counts')
 strings,raw,prop,cliprows,cuerows=parts
 owner,audio,*roles=struct.unpack_from('<6I',raw);require(owner and audio and len(set(roles))==4 and all(0<v<=65535 for v in roles),'Wall timeline identity/roles')
 po,pl,ao,al=struct.unpack_from('<4I',raw,24);require(po==0 and pl>0 and ao==pl and al>6 and ao+al==len(strings),'Wall timeline string ownership')
 try:owner_path=strings[po:po+pl].decode('ascii');audio_path=strings[ao:ao+al].decode('ascii')
 except UnicodeDecodeError as e:raise ValueError('Wall timeline string encoding')from e
 path(owner_path);path(audio_path,True);source_sha=raw[40:72];require(any(source_sha),'Wall timeline empty audio source')
 gain,flags=struct.unpack_from('<fI',raw,72);real(gain,-120,24);require(flags in(0,1),'Wall audio flags')
 magnitude=struct.unpack_from('<d',raw,80)[0];direction=list(struct.unpack_from('<2f',raw,88));interval,weight=struct.unpack_from('<2d',raw,96);diminish,bus,pitch,reserved=struct.unpack_from('<2IfI',raw,112)
 real(magnitude,0,1000000);real(interval,0,60);require(interval>0,'Wall shake interval');real(weight,0,1)
 for value in direction:real(value,-1000000,1000000)
 require(diminish in(0,1)and bus==1 and pitch==1 and reserved==0,'Wall timeline unsupported node policy')
 model=properties(prop);require([c['id']for c in model['clips']]==roles,'Wall nested role coverage/order');clips=[];first=0
 for i,c in enumerate(model['clips']):
  ident,start,count,rsv=struct.unpack_from('<4I',cliprows,i*16);require(ident==c['id']and start==first and count<=(len(cuerows)//16)-first and not rsv,'Wall cue clip ownership');rows=[];previous=(0,-1)
  for j in range(start,start+count):
   time,kind,value=struct.unpack_from('<fId',cuerows,j*16);real(time,0,c['duration']);require(kind in(1,2)and(kind,time)>previous,'Wall unsupported/unsorted cue');previous=(kind,time)
   if kind==1:require(value in(0,1),'Wall audio cue boolean')
   else:real(value,interval,60);require(value/interval<=1000000,'Wall shake step bound')
   rows.append(dict(time=time,kind=kind,value=value))
  clips.append(dict(id=ident,cues=rows));first+=count
 require(first==len(cuerows)//16,'Wall unowned cues')
 return dict(scene=scene,owner=owner,path=owner_path,roles=roles,audio=dict(id=audio,path=audio_path,sha=source_sha.hex(),gain=gain,loop=bool(flags),bus=bus,pitch=pitch),camera=dict(magnitude=magnitude,direction=direction,interval=interval,weight=weight,diminish=bool(diminish)),properties=model,cue_clips=clips)

def encode(data):
 fields(data,'scene owner path roles audio camera properties cue_clips');integer(data['scene'],1,65535);integer(data['owner'],1,2**32-1);path(data['path']);a=data['audio'];c=data['camera']
 fields(a,'id path sha gain loop bus pitch');integer(a['id'],1,2**32-1);path(a['path'],True);require(type(a['sha'])is str and re.fullmatch('[0-9a-f]{64}',a['sha'])and int(a['sha'],16),'Wall audio digest');real(a['gain'],-120,24);require(type(a['loop'])is bool and type(a['bus'])is int and a['bus']==1 and type(a['pitch'])in(int,float)and a['pitch']==1,'Wall audio policy')
 fields(c,'magnitude direction interval weight diminish');real(c['magnitude'],0,1000000);real(c['interval'],0,60);real(c['weight'],0,1);require(type(c['diminish'])is bool and type(c['direction'])is list and len(c['direction'])==2,'Wall camera fields')
 for v in c['direction']:real(v,-1000000,1000000)
 roles=data['roles'];require(type(roles)is list and len(roles)==4,'Wall role cardinality')
 for v in roles:integer(v,1,65535)
 require(len(set(roles))==4,'Wall duplicate role identity')
 model=data['properties'];fields(model,'frames initial_frame initial_mask clips');integer(model['frames'],1,65535);integer(model['initial_frame'],0,model['frames']-1);integer(model['initial_mask'],0,3);require(type(model['clips'])is list and len(model['clips'])==4,'Wall property clips')
 for clip in model['clips']:
  fields(clip,'id duration keys');integer(clip['id'],1,65535);real(clip['duration'],0,60);require(type(clip['keys'])is list,'Wall property key type')
  for key in clip['keys']:fields(key,'time kind value');real(key['time'],0,clip['duration']);integer(key['kind'],1,3);integer(key['value'],0,2**32-1)
 prop=prop_clips.encode(model);strings=(data['path']+a['path']).encode('ascii');policy=bytearray(128)
 struct.pack_into('<6I4I',policy,0,data['owner'],a['id'],*roles,0,len(data['path']),len(data['path']),len(a['path']));policy[40:72]=bytes.fromhex(a['sha']);struct.pack_into('<fId2f2d2IfI',policy,72,a['gain'],int(a['loop']),c['magnitude'],*c['direction'],c['interval'],c['weight'],int(c['diminish']),a['bus'],a['pitch'],0)
 require(type(data['cue_clips'])is list and len(data['cue_clips'])==4,'Wall cue clip type');rows=bytearray();cues=bytearray()
 for clip in data['cue_clips']:
  fields(clip,'id cues');integer(clip['id'],1,65535);require(type(clip['cues'])is list,'Wall cue rows');rows.extend(struct.pack('<4I',clip['id'],len(cues)//16,len(clip['cues']),0))
  for key in clip['cues']:fields(key,'time kind value');real(key['time'],0,60);integer(key['kind'],1,2);real(key['value'],0,60);cues.extend(struct.pack('<fId',key['time'],key['kind'],key['value']))
 blob=pack_container(MAGIC,data['scene'],[strings,policy,prop,rows,cues],STRIDES,LIMIT);parse(blob);return blob

def extract(file=RECIPE):
 p,ex=read(file);graph,graph_proof=dungeon_switches.extract(ROOT/p['graph_recipe']);matches=[o for o in graph['objects']if o['id']==p['owner']['id']]
 require(graph['scene']==p['owner']['scene_id']and len(matches)==1 and matches[0]['kind']==2 and matches[0]['path']==p['owner']['path'],'Wall graph owner differs')
 s=p['paths'];doc=Document(s['scene'],ex.text(s['scene']));nodes={'.'if n.attrs.get('parent')is None else n.attrs['name']if n.attrs['parent']=='.'else n.attrs['parent']+'/'+n.attrs['name']:n for n in doc.nodes}
 root_audio=json.loads((ROOT/'content/native-audio.json').read_bytes());require(type(root_audio)is dict and root_audio.get('schema')==1 and root_audio.get('upstream_commit')==PIN and type(root_audio.get('assets'))is list,'Wall audio identity table schema')
 for asset in root_audio['assets']:
  require(type(asset)is dict and type(asset.get('stable_id'))is int and type(asset.get('source_path'))is str,'Wall audio identity table row')
  if asset['stable_id']==p['audio_identity']['value']or asset['source_path']=='res://'+s['audio']:
   require(asset['stable_id']==p['audio_identity']['value']and asset['source_path']=='res://'+s['audio']and asset['source_sha256']==p['sources'][s['audio']],'Wall audio port identity collision')
 require(set(nodes)=={'.','main','collision','collision/CollisionShape2D','AnimationPlayer','AudioStreamPlayer'}and len(nodes)==len(doc.nodes)and not doc.connections,'Wall unknown scene nodes/signals')
 require(doc.ext_path(nodes['.'].props.get('script'),'Script')==s['script']and set(nodes['.'].props)=={'script','_move_cam_when_changed'}and nodes['.'].props['_move_cam_when_changed']is True,'Wall root source binding')
 sprite=nodes['main'];require(sprite.attrs.get('type')=='Sprite'and set(sprite.props)<=set('position texture hframes vframes frame visible'.split())and doc.ext_path(sprite.props.get('texture'),'Texture')==s['texture'],'Wall sprite source')
 columns=sprite.props.get('hframes',1);lines=sprite.props.get('vframes',1);integer(columns,1,65535);integer(lines,1,65535);size=ex.png_size(s['texture']);require(size[0]%columns==size[1]%lines==0 and columns*lines<=65535,'Wall source frame grid')
 body=nodes['collision/CollisionShape2D'];require(body.attrs.get('type')=='CollisionShape2D'and nodes['collision'].attrs.get('type')=='KinematicBody2D','Wall body owner')
 visible=sprite.props.get('visible',True);disabled=body.props.get('disabled',False);require(type(visible)is bool and type(disabled)is bool,'Wall source initial booleans')
 audio=nodes['AudioStreamPlayer'];require(audio.attrs.get('type')=='AudioStreamPlayer'and set(audio.props)<=set('stream bus volume_db pitch_scale'.split())and doc.ext_path(audio.props.get('stream'),'AudioStream')==s['audio']and audio.props.get('bus','Master')=='SFX'and audio.props.get('pitch_scale',1)==1,'Wall source audio binding')
 loop,offset=import_settings(ex.data(s['audio_import']),'res://'+s['audio']);require(offset==0,'Wall source loop offset unsupported')
 text=ex.text(s['script']);shake=function(text,'_shake');m=re.fullmatch(r'func _shake\(duration: float\):\s+global.currentCamera.shake_camera\(([0-9.]+), duration, Vector2.ZERO, ([0-9.]+), ([0-9.]+), (true|false)\)\s*',shake);require(m is not None,'Wall unmapped shake method')
 camera=dict(magnitude=float(m[1]),direction=[0.,0.],interval=float(m[2]),weight=float(m[3]),diminish=m[4]=='true')
 require(re.sub(r'\s+',' ',function(text,'_init')).strip()=='func _init(): _state_anim_player = "AnimationPlayer"','Wall animation owner init')
 animator=nodes['AnimationPlayer'];require(animator.attrs.get('type')=='AnimationPlayer'and list(animator.props)==['anims/'+r['name']for r in p['roles']],'Wall source animation order/coverage')
 clips=[];cue_clips=[]
 for role in p['roles']:
  ref=animator.props['anims/'+role['name']];require(type(ref)is SubRef and doc.sub[ref.id][0]=='Animation','Wall local animation');props=doc.sub[ref.id][1]
  require(not(set(props)-{'resource_name','length','step','loop'}-{k for k in props if k.startswith('tracks/')})and props.get('resource_name',role['name'])==role['name']and props.get('loop',False)is False,'Wall unsupported animation fields/loop')
  duration=prop_clips.f32(props.get('length',1));real(duration,0,60);groups={}
  for k,v in props.items():
   if k.startswith('tracks/'):
    parts=k.split('/');require(len(parts)==3 and parts[1].isdigit()and str(int(parts[1]))==parts[1],'Wall source track field');groups.setdefault(int(parts[1]),{})[parts[2]]=v
  require(set(groups)==set(range(len(groups)))and len(groups)in(2,3,4),'Wall source track count/order');keys=[];cues=[];seen=[]
  for i,t in groups.items():
   require(set(t)==set('type path interp loop_wrap imported enabled keys'.split())and type(t['path'])is NodePathValue and t['interp']==1 and type(t['interp'])is int and t['loop_wrap']is True and t['imported']is False and t['enabled']is True,'Wall source track attributes')
   targets={'main:frame':(1,'value'),'collision/CollisionShape2D:disabled':(3,'value'),'AudioStreamPlayer:playing':(6,'value'),'.':(7,'method')}
   require(t['path'].path in targets,'Wall unknown track target');kind,tracktype=targets[t['path'].path];require(t['type']==tracktype and kind not in seen and(not seen or kind>seen[-1]),'Wall source track type/order');seen.append(kind);k=t['keys'];fields(k,'times transitions update values'if kind!=7 else'times transitions values')
   require(all(type(k[n])is list for n in('times','transitions','values'))and 0<len(k['times'])==len(k['transitions'])==len(k['values'])<=256 and all(type(v)in(int,float)and v==1 for v in k['transitions']),'Wall key lengths/curve')
   if kind!=7:require(type(k['update'])is int and(k['update']==1 or k['update']==0 and len(k['times'])==1 and k['times'][0]==0),'Wall continuous curve unsupported')
   previous=-1
   for time,value in zip(k['times'],k['values']):
    time=prop_clips.f32(time);require(0<=time<=duration and time>previous,'Wall key time/order');previous=time
    if kind in(1,3):require(type(value)is(int if kind==1 else bool)and(0<=value<columns*lines if kind==1 else True),'Wall property value');keys.append(dict(time=time,kind=kind,value=int(value)))
    elif kind==6:require(type(value)is bool,'Wall audio property type');cues.append(dict(time=time,kind=1,value=float(value)))
    else:
     fields(value,'args method');require(value['method']=='_shake'and type(value['args'])is list and len(value['args'])==1,'Wall unknown method/arity');length=real(value['args'][0],camera['interval'],60);cues.append(dict(time=time,kind=2,value=length))
  require(seen[:2]==[1,3],'Wall required frame/body tracks');keys.sort(key=lambda k:(k['time'],k['kind']));clips.append(dict(id=role['id'],duration=duration,keys=keys));cue_clips.append(dict(id=role['id'],cues=cues))
 data=dict(scene=p['owner']['scene_id'],owner=p['owner']['id'],path=p['owner']['path'],roles=[r['id']for r in p['roles']],audio=dict(id=p['audio_identity']['value'],path='res://'+s['audio'],sha=p['sources'][s['audio']],gain=float(audio.props.get('volume_db',0)),loop=loop,bus=1,pitch=float(audio.props.get('pitch_scale',1))),camera=camera,properties=dict(frames=columns*lines,initial_frame=sprite.props.get('frame',0),initial_mask=int(visible)|int(not disabled)<<1,clips=clips),cue_clips=cue_clips)
 blob=encode(data);require(parse(blob)==data,'Wall source/codec disagreement')
 proof=dict(commit=PIN,whole_scripts_approved=False,scope=p['scope'],review_sha256=hashlib.sha256(Path(file).read_bytes()).hexdigest(),sources=dict(sorted({**ex.sources,**graph_proof['sources']}.items())),graph_review_sha256=p['graph_review_sha256'],mapped_implementation={'properties':'runtime/prop_clips.cpp','camera_request':'runtime/cutscene_camera.cpp; explicit SourceRandom owner required','timeline_codec':'runtime/dungeon_wall_timeline.cpp','typed_command_state':'runtime/dungeon_wall_state.cpp; no live scene or prepared media'},resource=dict(bytes=len(blob),sha256=hashlib.sha256(blob).hexdigest()))
 return data,blob,proof
def build(out,file=RECIPE):
 out=Path(out).resolve();require(out.is_relative_to((ROOT/'build').resolve())and out!=ROOT/'build','Wall candidates stay under build');data,blob,proof=extract(file);out.mkdir(parents=True,exist_ok=True);(out/'source.encwall').write_bytes(blob);(out/'source.json').write_text(json.dumps(dict(source=proof,model=data,producer_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest()),indent=2)+'\n',encoding='utf8');print('Wall timeline candidate:',len(blob),'bytes,4clips,',sum(len(c['cues'])for c in data['cue_clips']),'typed cues; no scene/PCM/save admission')
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--recipe',type=Path,default=RECIPE);p.add_argument('--out',type=Path,required=True);a=p.parse_args();build(a.out,a.recipe)
