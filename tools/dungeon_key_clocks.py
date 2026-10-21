"""Checked source key/door/child-lock clocks; not a live map admission.

ENCKLC01 carries the original key metadata, typed animation tracks and source
audio identities. Continuous Vector2 tracks remain explicit; unknown tracks,
root changes, methods, signals or source versions are never discarded.
"""
import argparse, hashlib, json, math, struct, sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.extract_battle_entry import Extractor,PIN,require
from tools.godot_text import Document,NodePathValue,SubRef,Vec2
from tools.podunk_field import pack_container,unpack_container
from tools import dungeon_keys,prop_clips
from tools.audio_asset import import_settings
from tools.run_battle_victory_reference import function

RECIPE=ROOT/'content/catacombs-key-clocks.json'
MAGIC=b'ENCKLC01';STRIDES=(1,1,64,32,24,32,80);LIMIT=128*1024
KINDS={'key':1,'door':2,'child':3};ROLES={'RESET':1,'Use Key':2,'Open':3,'Opened':4}
TRACKS={'Sprite:frame':1,'Sprite:visible':2,'Sprite:position':3,'AudioStreamPlayer:playing':4,'Lock:visible':5,'AnimationPlayer:root_node':6}

def fields(v,n):require(type(v)is dict and set(v)==set(n.split()),'Key clock fields: '+n)
def integer(v,lo=0,hi=0xffffffff):require(type(v)is int and lo<=v<=hi,'Key clock integer');return v
def real(v,lo=-1000000,hi=1000000):
 require(type(v)in(int,float)and math.isfinite(v)and lo<=v<=hi,'Key clock finite real');return prop_clips.f32(v)
def path(v):return dungeon_keys.path(v)
def unique(rows):
 out={}
 for k,v in rows:require(k not in out,'Duplicate key clock review field');out[k]=v
 return out
def read(file=RECIPE):
 p=json.loads(Path(file).read_text('utf8'),object_pairs_hook=unique)
 fields(p,'schema kind commit scope key_recipe key_review_sha256 profiles sources symbols license warning_profile')
 require(type(p['schema'])is int and p['schema']==1 and p['kind']=='encore.dungeon-key-clocks.reviewed-source'and p['commit']==PIN,'Key clock review schema/pin')
 require(type(p['scope'])is str and 0<len(p['scope'])<=2048,'Key clock review scope')
 require(p['key_recipe']=='content/catacombs-keys.json'and hashlib.sha256((ROOT/p['key_recipe']).read_bytes()).hexdigest()==p['key_review_sha256'],'Key clock key review changed')
 ex=Extractor(ROOT);require(type(p['sources'])is dict and p['sources'],'Key clock source coverage')
 for s,h in p['sources'].items():path(s);require(type(h)is str and len(h)==64 and hashlib.sha256(ex.data(s)).hexdigest()==h,'Key clock reviewed source changed: '+s)
 require(p['warning_profile']=={'unused_argument':False,'unused_signal':False}and all(type(v)is bool for v in p['warning_profile'].values())and'project.godot'in p['sources'],'Key clock source diagnostic profile')
 for warning in p['warning_profile']:require('gdscript/warnings/'+warning+'=false'in ex.text('project.godot'),'Key clock original warning setting changed')
 require(type(p['profiles'])is list and len(p['profiles'])==3,'Key clock reviewed profiles')
 ids=set();kinds=set()
 for row in p['profiles']:
  fields(row,'id kind scene script audio texture child diagnostics clips');integer(row['id'],1,65535);integer(row['child'],0,65535);integer(row['diagnostics'],0,7)
  require(row['id']not in ids and row['kind']in KINDS and row['kind']not in kinds,'Key clock duplicate profile');ids.add(row['id']);kinds.add(row['kind'])
  for n in ('scene','script','texture'):require(row[n]in p['sources'],'Key clock profile source coverage')
  if row['audio']is not None:require(row['audio']in p['sources']and row['audio']+'.import'in p['sources'],'Key clock audio source coverage')
  require(type(row['clips'])is list and row['clips'],'Key clock role review')
  for c in row['clips']:fields(c,'id name');integer(c['id'],1,65535);require(c['name']in ROLES,'Key clock unknown role')
 require(set(p['symbols'])=={row['script']for row in p['profiles']},'Key clock method source coverage')
 for s,methods in p['symbols'].items():
  require(type(methods)is dict and methods,'Key clock method review')
  for n,h in methods.items():require(hashlib.sha256(function(ex.text(s),n).encode()).hexdigest()==h,'Key clock reviewed method changed')
 require(p['license']in p['sources']and "purpose you're using them is related to the game"in ex.text(p['license']),'Key clock source permission changed')
 return p,ex

def extract(file=RECIPE):
 p,ex=read(file);metadata,meta_proof=dungeon_keys.extract(ROOT/p['key_recipe']);profiles=[];clips=[];audios=[]
 for row in p['profiles']:
  doc=Document(row['scene'],ex.text(row['scene']));nodes={'.'if n.attrs.get('parent')is None else n.attrs['name']if n.attrs['parent']=='.'else n.attrs['parent']+'/'+n.attrs['name']:n for n in doc.nodes}
  kind=KINDS[row['kind']]
  expected={'.','Sprite','AnimationPlayer','AudioStreamPlayer'}if kind==3 else{'.','Sprite','AnimationPlayer','AudioStreamPlayer','Area2D','Area2D/CollisionShape2D'}if kind==1 else{'.','Sprite','Lock','StaticBody2D','StaticBody2D/CollisionShape2D','interact','interact/CollisionShape2D2','interact/ButtonPrompt','AnimationPlayer'}
  require(set(nodes)==expected and len(nodes)==len(doc.nodes)and nodes['.'].attrs.get('type')=='Sprite','Key clock unknown prefab node/owner')
  root=nodes['.'];sprite=nodes['Sprite'];animator=nodes['AnimationPlayer']
  require(set(root.props)<=({'hframes','script','anim_name','anim_player'}if kind==3 else{'hframes','script','emit_flag_updated_signal'}),'Key clock unknown root property')
  require(doc.ext_path(root.props.get('script'),'Script')==row['script']and sprite.attrs.get('type')=='Sprite'and animator.attrs.get('type')=='AnimationPlayer','Key clock source owners')
  require(doc.ext_path(sprite.props.get('texture'),'Texture')==row['texture']and set(sprite.props)<= {'texture','hframes','vframes','frame','visible','position'},'Key clock sprite source/properties')
  columns=integer(sprite.props.get('hframes',1),1,65535);rows=integer(sprite.props.get('vframes',1),1,65535);size=ex.png_size(row['texture'])
  require(size[0]%columns==size[1]%rows==0 and columns*rows<=65535,'Key clock source grid')
  pos=sprite.props.get('position',Vec2(0,0));require(type(pos)is Vec2 and type(sprite.props.get('visible',True))is bool,'Key clock initial pose')
  require(set(animator.props)=={'anims/'+r['name']for r in row['clips']}and len(animator.props)==len(row['clips']),'Key clock unsupported autoplay/animation')
  signals=[dict(c)for c in doc.connections]
  require(all(c.get('signal')in ('animation_finished','body_entered','finished')for c in signals),'Key clock unknown source signal')
  expected=[{'signal':'animation_finished','from':'AnimationPlayer','to':'.','method':'_on_AnimationPlayer_animation_finished'}]
  if kind==1:expected=[{'signal':'body_entered','from':'Area2D','to':'.','method':'_on_Area2D_body_entered'},{'signal':'finished','from':'AudioStreamPlayer','to':'.','method':'_on_AudioStreamPlayer_finished'}]+expected
  require(signals==expected,'Key clock source signal binding changed')
  handler='_on_AnimationPlayer_animation_finished';script=ex.text(row['script'])
  if 'func '+handler+'('in script:
   require(kind==1 and function(script,handler).split('\n',1)[1].strip()=='queue_free()','Key clock unmapped completion method');diagnostics=4
  else:diagnostics=1
  if kind==3:
   require(root.props.get('anim_name')=='Open'and root.props.get('anim_player')==NodePathValue('AnimationPlayer'),'Key clock child animation selector')
   require('connect("animation_finished", self, "emit_signal", ["animation_ended"])'in function(ex.text(row['script']),'_ready'),'Key clock child source signal bind')
   require('var anim = anim_name if animation == "" else animation'in function(ex.text(row['script']),'play_anim')and'anim_player_node.play(anim)'in function(ex.text(row['script']),'play_anim'),'Key clock child play semantics')
   diagnostics|=2
  require(row['diagnostics']==diagnostics,'Key clock completion review differs from actual source handlers')
  if row['audio']is not None:
   audio=nodes['AudioStreamPlayer'];require(audio.attrs.get('type')=='AudioStreamPlayer'and set(audio.props)<= {'stream','bus','volume_db','pitch_scale','pause_mode'}and doc.ext_path(audio.props.get('stream'),'AudioStream')==row['audio']and audio.props.get('bus')=='SFX','Key clock audio owner/source')
   loop,offset=import_settings(ex.data(row['audio']+'.import'),'res://'+row['audio']);require(not loop and offset==0,'Key clock looping audio unsupported')
   audios.append(dict(profile=row['id'],source='res://'+row['audio'],sha=p['sources'][row['audio']],gain=real(audio.props.get('volume_db',0),-80,24),pitch=real(audio.props.get('pitch_scale',1),.01,8),bus=1,loop=0,pause=audio.props.get('pause_mode',0)))
  profile=dict(id=row['id'],kind=kind,frames=columns*rows,frame=sprite.props.get('frame',0),mask=int(sprite.props.get('visible',True))|int(kind==2)<<2,position=[real(pos.x),real(pos.y)],roles=[0]*4,child=row['child'],source='res://'+row['scene'],diagnostics=row['diagnostics'])
  for c in row['clips']:
   role=ROLES[c['name']];profile['roles'][role-1]=c['id'];ref=animator.props['anims/'+c['name']]
   require(type(ref)is SubRef and doc.sub[ref.id][0]=='Animation','Key clock local source animation');a=doc.sub[ref.id][1]
   require(not(set(a)-{'resource_name','length','step','loop'}-{n for n in a if n.startswith('tracks/')})and a.get('resource_name',c['name'])==c['name']and a.get('loop',False)is False,'Key clock source animation fields/loop')
   duration=real(a.get('length',1),.000001,60);groups={}
   for n,value in a.items():
    if n.startswith('tracks/'):
     pieces=n.split('/');require(len(pieces)==3 and pieces[1].isdigit()and str(int(pieces[1]))==pieces[1],'Key clock track field');groups.setdefault(int(pieces[1]),{})[pieces[2]]=value
   require(groups and set(groups)==set(range(len(groups))),'Key clock source track indices');tracks=[]
   for index,t in groups.items():
    fields(t,'type path interp loop_wrap imported enabled keys');require(type(t['path'])is NodePathValue and type(t['interp'])is int and t['interp']==1 and t['loop_wrap']is True and t['imported']is False and t['enabled']is True,'Key clock source track attributes')
    target=t['path'].path;k=t['keys']
    if t['type']=='value':
     require(target in TRACKS,'Key clock unknown value target');track_kind=TRACKS[target];fields(k,'times transitions update values');mode=integer(k['update'],0,1)
    else:
     require(t['type']=='method'and target in ('.','Lock')and kind==2,'Key clock unknown method target');track_kind=7 if target=='.'else 8;mode=2;fields(k,'times transitions values')
    require(all(type(k[n])is list for n in ('times','transitions','values'))and 0<len(k['times'])==len(k['transitions'])==len(k['values'])<=256,'Key clock source key cardinality');keys=[]
    for time,ease,value in zip(k['times'],k['transitions'],k['values']):
     key=dict(time=real(time,0,duration),ease=real(ease,.000001,64),value=0,position=[0.,0.])
     if track_kind==1:key['value']=integer(value,0,profile['frames']-1)
     elif track_kind in(2,4,5):require(type(value)is bool,'Key clock boolean source');key['value']=int(value)
     elif track_kind==3:require(type(value)is Vec2,'Key clock Vector2 source');key['position']=[real(value.x),real(value.y)]
     elif track_kind==6:require(type(value)is NodePathValue and value.path=='..','Key clock dynamic root unsupported');key['value']=1
     else:fields(value,'method args');require(value['method']==('open'if track_kind==7 else'play_anim')and value['args']==([]if track_kind==7 else['']),'Key clock unknown method/arguments')
     keys.append(key)
    tracks.append(dict(kind=track_kind,mode=mode,index=index,keys=keys))
   clips.append(dict(id=c['id'],profile=row['id'],role=role,duration=duration,tracks=tracks))
  profiles.append(profile)
 data=dict(scene=metadata['scene'],metadata=metadata,profiles=profiles,clips=clips,audios=audios)
 blob=encode(data);require(parse(blob)==data,'Key clock source/codec disagreement')
 proof=dict(commit=PIN,scope=p['scope'],whole_scripts_approved=False,sources={**meta_proof['sources'],**ex.sources},review_sha256=hashlib.sha256(Path(file).read_bytes()).hexdigest(),source_key_review=p['key_review_sha256'],resource_sha256=hashlib.sha256(blob).hexdigest(),resource_bytes=len(blob))
 return data,blob,proof

def encode(data):
 strings=bytearray();spans={}
 def text(s):
  if s not in spans:spans[s]=(len(strings),len(s.encode('ascii')));strings.extend(s.encode('ascii'))
  return spans[s]
 profiles=bytearray();clips=bytearray();tracks=bytearray();keys=bytearray();audios=bytearray()
 for p in data['profiles']:profiles.extend(struct.pack('<5I2f9I',p['id'],p['kind'],p['frames'],p['frame'],p['mask'],*p['position'],*p['roles'],p['child'],*text(p['source']),p['diagnostics'],0))
 for c in data['clips']:
  clips.extend(struct.pack('<3If4I',c['id'],c['profile'],c['role'],c['duration'],len(tracks)//24,len(c['tracks']),0,0))
  for t in c['tracks']:
   tracks.extend(struct.pack('<6I',t['kind'],t['mode'],t['index'],len(keys)//32,len(t['keys']),0))
   for k in t['keys']:keys.extend(struct.pack('<2fI2f3I',k['time'],k['ease'],k['value'],*k['position'],0,0,0))
 for a in data['audios']:
  audios.extend(struct.pack('<3I',a['profile'],*text(a['source']))+bytes.fromhex(a['sha'])+struct.pack('<2f7I',a['gain'],a['pitch'],a['bus'],a['loop'],a['pause'],0,0,0,0))
 blob=pack_container(MAGIC,data['scene'],[strings,dungeon_keys.encode(data['metadata']),profiles,clips,tracks,keys,audios],STRIDES,LIMIT);parse(blob);return blob

def parse(blob):
 scene,s=unpack_container(blob,MAGIC,STRIDES,LIMIT);metadata=dungeon_keys.parse(s[1]);require(scene==metadata['scene'],'Key clock nested scene mismatch')
 cursor_bytes=64+len(STRIDES)*16
 for index,part in enumerate(s):
  offset=struct.unpack_from('<I',blob,64+index*16+4)[0];require(offset==(cursor_bytes+3)//4*4,'Key clock canonical section offset');cursor_bytes=offset+len(part)
 owned={};cursor=0
 def text(at,n):
  nonlocal cursor
  require(0<n<=512 and at<=len(s[0])and n<=len(s[0])-at,'Key clock text bounds')
  value=s[0][at:at+n].decode('ascii');require(value.startswith('res://'),'Key clock source URI');path(value[6:])
  if value in owned:require(owned[value]==(at,n),'Key clock duplicate text ownership')
  else:require(at==cursor,'Key clock canonical text ownership');owned[value]=(at,n);cursor+=n
  return value
 require(0<len(s[2])//64<=16 and 0<len(s[3])//32<=64 and 0<len(s[4])//24<=256 and 0<len(s[5])//32<=4096 and 0<len(s[6])//80<=16,'Key clock section counts')
 profiles=[];pids=set();kinds=set()
 for at in range(0,len(s[2]),64):
  row=struct.unpack_from('<5I2f9I',s[2],at);ident,kind,frames,frame,mask=row[:5];position=list(row[5:7]);roles=list(row[7:11]);child,a,n,diag,res=row[11:]
  require(0<ident<=65535 and ident not in pids and kind in(1,2,3)and kind not in kinds and 0<frames<=65535 and frame<frames and mask<=7 and not(mask&2 and kind==2)and not(mask&4 and kind!=2)and not res,'Key clock profile identity/pose')
  require(all(math.isfinite(v)and abs(v)<=1000000 for v in position)and diag<=7 and not(diag&4 and(kind!=1 or diag&3)),'Key clock profile position/completion')
  require((kind!=2 or roles[2]and roles[3])and(kind!=3 or roles[2]),'Key clock required execution role coverage')
  require((0<child<=65535)if kind==2 else child==0,'Key clock child binding');pids.add(ident);kinds.add(kind)
  profiles.append(dict(id=ident,kind=kind,frames=frames,frame=frame,mask=mask,position=position,roles=roles,child=child,source=text(a,n),diagnostics=diag))
 require({n['kind']for n in metadata['nodes']}<=kinds,'Key clock missing method profile kind');by={p['id']:p for p in profiles};require(all(not p['child']or p['child']in by and by[p['child']]['kind']==3 for p in profiles),'Key clock child profile reference')
 clips=[];ids=set();used_tracks=0;used_keys=0
 for at in range(0,len(s[3]),32):
  ident,profile,role,duration,first,count,res,res2=struct.unpack_from('<3If4I',s[3],at)
  require(0<ident<=65535 and ident not in ids and profile in by and 1<=role<=4 and by[profile]['roles'][role-1]==ident and math.isfinite(duration)and 0<duration<=60 and first==used_tracks and 0<count<=16 and count<=len(s[4])//24-first and not(res or res2),'Key clock animation identity/span')
  ids.add(ident);ts=[];seen=set()
  for index in range(count):
   kind,mode,source_index,kfirst,kcount,res=struct.unpack_from('<6I',s[4],(first+index)*24)
   require(1<=kind<=8 and kind not in seen and source_index==index and mode in(0,1,2)and kfirst==used_keys and 0<kcount<=256 and kcount<=len(s[5])//32-kfirst and not res,'Key clock track identity/span');seen.add(kind)
   require((mode==2)==(kind in(7,8))and(mode!=0 or kind==3 or kcount==1)and(kind!=3 or mode==0),'Key clock unsupported track interpolation')
   require(kind not in(5,7,8)or by[profile]['kind']==2,'Key clock track owner kind');require(kind!=4 or by[profile]['kind']in(1,3),'Key clock audio track owner')
   ks=[];previous=-1
   for ki in range(kcount):
    time,ease,value,x,y,r1,r2,r3=struct.unpack_from('<2fI2f3I',s[5],(kfirst+ki)*32)
    require(all(math.isfinite(v)for v in(time,ease,x,y))and 0<=time<=duration and time>previous and 0<ease<=64 and abs(x)<=1000000 and abs(y)<=1000000 and not(r1 or r2 or r3),'Key clock key time/value');previous=time
    require(kind==3 or x==y==0,'Key clock stray position payload');require(kind==3 or ease==1,'Key clock unsupported scalar easing')
    require((kind!=1 or value<by[profile]['frames'])and(kind not in(2,4,5)or value<=1)and(kind!=6 or value==1)and(kind not in(3,7,8)or value==0),'Key clock key value domain')
    require(mode!=0 or kind==3 or time==0,'Key clock nonconstant scalar track')
    ks.append(dict(time=time,ease=ease,value=value,position=[x,y]))
   used_keys+=kcount;ts.append(dict(kind=kind,mode=mode,index=index,keys=ks))
  used_tracks+=count;clips.append(dict(id=ident,profile=profile,role=role,duration=duration,tracks=ts))
 require(used_tracks==len(s[4])//24 and used_keys==len(s[5])//32 and ids=={r for p in profiles for r in p['roles']if r},'Key clock unowned/missing animation records')
 audios=[];audio_profiles=set()
 for at in range(0,len(s[6]),80):
  profile,a,n=struct.unpack_from('<3I',s[6],at);digest=s[6][at+12:at+44];gain,pitch,bus,loop,pause,r1,r2,r3,r4=struct.unpack_from('<2f7I',s[6],at+44)
  require(profile in by and by[profile]['kind']in(1,3)and profile not in audio_profiles and any(digest)and math.isfinite(gain)and -80<=gain<=24 and math.isfinite(pitch)and real(.01)<=pitch<=8 and bus==1 and loop==0 and pause==0 and not(r1 or r2 or r3 or r4),'Key clock source audio metadata')
  audio_profiles.add(profile);audios.append(dict(profile=profile,source=text(a,n),sha=digest.hex(),gain=gain,pitch=pitch,bus=bus,loop=loop,pause=pause))
 require(audio_profiles=={p['id']for p in profiles if p['kind']in(1,3)}and cursor==len(s[0]),'Key clock unowned text/audio')
 return dict(scene=scene,metadata=metadata,profiles=profiles,clips=clips,audios=audios)

def build(out,file=RECIPE):
 out=Path(out).resolve();require(out.is_relative_to(ROOT/'build')and not out.exists(),'Fresh key clock candidate under build required');data,blob,proof=extract(file);out.mkdir(parents=True)
 (out/'source.encklc').write_bytes(blob);(out/'source.json').write_text(json.dumps(dict(model=data,source=proof),ensure_ascii=False,indent=2)+'\n',encoding='utf8');print('Key clock candidate:',len(blob),'bytes,',len(data['clips']),'source animations; no live scene/PCM/save admission')
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--recipe',type=Path,default=RECIPE);p.add_argument('--out',type=Path,required=True);a=p.parse_args();build(a.out,a.recipe)
