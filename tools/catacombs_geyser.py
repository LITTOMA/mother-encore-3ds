"""Strict original Geyser timeline candidate; no full scene/audio admission."""
import argparse,hashlib,json,math,re,struct,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import podunk_field as f
from tools.extract_battle_entry import PIN,require
from tools.reference_movement import extract
from tools.godot_text import Document,Vec2,NodePathValue,SubRef
from tools.dungeon_keys import fields,path
from tools.dungeon_key_indicator import unique
RECIPE=ROOT/'content/catacombs-geyser.json';MAGIC=b'ENCGYS01';STRIDES=(1,192,32,16,32,32);LIMIT=65536
def integer(v,lo=0,hi=0xffffffff):require(type(v)is int and lo<=v<=hi,'Geyser integer domain');return v
def real(v,lo=-1e6,hi=1e6):require(type(v)in(int,float)and math.isfinite(v)and lo<=v<=hi and f.f32(v)==v,'Geyser binary32 finite domain');return v
def safe(v):require(type(v)is str and 0<len(v)<=512 and all(32<=ord(c)<127 for c in v),'Geyser ASCII text');path(v);return v
def vec(v):require(type(v)is Vec2,'Geyser source vector');return[f.f32(v.x),f.f32(v.y)]
def read(file=RECIPE):
 def invalid(_):raise ValueError('Nonfinite Geyser JSON')
 p=json.loads(Path(file).read_text('utf8'),object_pairs_hook=unique,parse_constant=invalid)
 fields(p,'schema kind commit scope paths sources symbols poison_recipe poison_recipe_sha256 clips sounds atlas_columns outputs engine_commit engine_sources engine_license_sha256 license_review native_lengths','geyser recipe');require(type(p['schema'])is int and p['schema']==1 and p['kind']=='encore.catacombs-geyser.reviewed-source'and p['commit']==PIN,'Geyser reviewed identity')
 fields(p['paths'],'scene script manager bus texture texture_import project license','geyser source selectors');s=f.Sources();expected=set(p['paths'].values())
 for row in p['sounds']:
  fields(row,'id role source import_source pcm','geyser sound selector');integer(row['id'],1,0xfffffffe);safe(row['source']);safe(row['pcm']);require(row['import_source']==row['source']+'.import','Geyser import source');expected.update([row['source'],row['import_source']])
 require(len(p['sounds'])==3 and [v['role']for v in p['sounds']]==['splash','blast','rumble']and len({v['id']for v in p['sounds']})==3,'Geyser sound identity/role coverage');require(type(p['sources'])is dict and set(p['sources'])==expected,'Geyser exact source coverage')
 for name,digest in p['sources'].items():safe(name);require(type(digest)is str and re.fullmatch('[0-9a-f]{64}',digest)and hashlib.sha256(s.data(name)).hexdigest()==digest,'Geyser source changed: '+name)
 names={'script':['_ready','_process','pause','unpause','_on_AnimationPlayer_animation_finished','_on_Poison_Geyser_body_entered','_on_Poison_Geyser_body_exited','play_rumble'],'manager':['add_sfx','play_sfx','get_sfx']};require(set(p['symbols'])==set(names),'Geyser method coverage')
 for key,methods in names.items():raw=s.data(p['paths'][key]);_,digest=extract(raw,hashlib.sha256(raw).hexdigest(),methods);require(p['symbols'][key]==digest,'Geyser selected method review changed')
 require(p['poison_recipe']=='content/catacombs-poison.json'and hashlib.sha256((ROOT/p['poison_recipe']).read_bytes()).hexdigest()==p['poison_recipe_sha256'],'Geyser poison binding changed')
 require(p['engine_commit']=='3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8'and type(p['engine_sources'])is dict and len(p['engine_sources'])==12 and all(type(k)is str and re.fullmatch('[0-9a-f]{64}',v)for k,v in p['engine_sources'].items())and re.fullmatch('[0-9a-f]{64}',p['engine_license_sha256']),'Geyser engine source provenance')
 require(type(p['native_lengths'])is dict and set(p['native_lengths'])=={v['source']for v in p['sounds']},'Geyser native stream length coverage')
 for v in p['native_lengths'].values():integer(v,1,0x7f7fffff)
 require(type(p['clips'])is list and len(p['clips'])==2 and len({v['id']for v in p['clips']})==2 and {v['name']for v in p['clips']}=={'RESET','Spewing'},'Geyser source clip coverage')
 for c in p['clips']:fields(c,'id name','geyser clip binding');integer(c['id'],1,65535)
 require(type(p['atlas_columns'])is int and 1<=p['atlas_columns']<=32 and all(type(p[k])is str and 0<len(p[k])<=4096 for k in['scope','license_review']),'Geyser atlas/scope/license');fields(p['outputs'],'metadata texture audio','geyser outputs')
 for v in p['outputs'].values():safe(v)
 return p,s
def source(file=RECIPE):
 p,s=read(file);n=p['paths'];doc=s.doc(n['scene']);nodes={'.'if 'parent'not in a.attrs else a.attrs['name']:a for a in doc.nodes};require(set(nodes)=={'.','CollisionShape2D','Sprite','AnimationPlayer','AudioStreamPlayer2D','VisibilityNotifier2D'},'Geyser exact source node coverage')
 root=nodes['.'];require(root.attrs==dict(name='Poison Geyser',type='Area2D')and set(root.props)==set('position collision_layer collision_mask monitoring monitorable script'.split())and doc.ext_path(root.props['script'],'Script')==n['script'],'Geyser source root/script')
 sprite=nodes['Sprite'];require(sprite.attrs==dict(name='Sprite',type='Sprite',parent='.')and set(sprite.props)=={'position','texture','offset','hframes'}and doc.ext_path(sprite.props['texture'],'Texture')==n['texture'],'Geyser original sprite profile')
 # Sources intentionally exposes checked raw bytes,not a Godot runtime/cache.
 from tools.extract_battle_entry import Extractor
 w,h=Extractor(ROOT).png_size(n['texture']);columns=integer(sprite.props['hframes'],1,256);require(w%columns==0,'Geyser frame geometry')
 shape=nodes['CollisionShape2D'];require(shape.attrs==dict(name='CollisionShape2D',type='CollisionShape2D',parent='.')and set(shape.props)=={'position','rotation','shape'}and type(shape.props['shape'])is SubRef,'Geyser original shape profile');kind,sp=doc.sub[shape.props['shape'].id];require(kind=='RectangleShape2D'and set(sp)=={'extents'},'Geyser unsupported collision shape')
 audio=nodes['AudioStreamPlayer2D'];require(audio.attrs==dict(name='AudioStreamPlayer2D',type='AudioStreamPlayer2D',parent='.')and set(audio.props)=={'max_distance','bus'},'Geyser Audio2D source profile');require(nodes['VisibilityNotifier2D'].attrs==dict(name='VisibilityNotifier2D',type='VisibilityNotifier2D',parent='.')and not nodes['VisibilityNotifier2D'].props,'Geyser visibility profile')
 script=s.text(n['script']);gain=re.findall(r'volume_db = (-?[0-9]+) if .*\n\s*or uiManager.is_game_over\(\) else (-?[0-9]+)',script);slot=re.findall(r'get_sfx\("([^"]+)"\)',script);require(len(gain)==1 and len(slot)==1 and 'play("Spewing")'in script and 'playback_active = false'in script and 'stream_paused = true'in script and 'queue_free()'in script,'Geyser source policy semantics');sounds=[]
 for row in p['sounds']:
  imp=s.text(row['import_source']);require('importer="mp3"'in imp and 'loop=false'in imp and 'loop_offset=0'in imp,'Geyser original MP3 import profile');length=struct.unpack('<f',struct.pack('<I',p['native_lengths'][row['source']]))[0];sounds.append(dict(id=row['id'],source=row['source'],length=length,role=['splash','blast','rumble'].index(row['role'])+1))
 m=dict(source=n['scene'],script=n['script'],slot=slot[0],bus=audio.props['bus'],spewing=next(c['id']for c in p['clips']if c['name']=='Spewing'),reset=next(c['id']for c in p['clips']if c['name']=='RESET'),frames=columns,initial_frame=0,initial_monitoring=root.props['monitoring'],initial_disabled=False,monitorable=root.props['monitorable'],layer=root.props['collision_layer'],mask=root.props['collision_mask'],hframes=columns,vframes=1,centered=True,root=vec(root.props['position']),sprite=vec(sprite.props['position']),offset=vec(sprite.props['offset']),size=[float(w//columns),float(h)],shape=vec(shape.props['position']),extents=vec(sp['extents']),rotation=f.f32(shape.props['rotation']),max_distance=f.f32(audio.props['max_distance']),attenuation=1.,pitch=1.,normal_gain=float(gain[0][1]),muted_gain=float(gain[0][0]),notify=[-10.,-10.,20.,20.],panning=1.,clips=[],keys=[],cues=[],sounds=sounds)
 animator=nodes['AnimationPlayer'];require(animator.attrs==dict(name='AnimationPlayer',type='AnimationPlayer',parent='.')and set(animator.props)=={'anims/'+c['name']for c in p['clips']},'Geyser clip/autoplay profile')
 target_kinds={'Sprite:frame':1,'.:monitoring':2,'CollisionShape2D:disabled':3}
 for c in p['clips']:
  sub=animator.props['anims/'+c['name']];require(type(sub)is SubRef,'Geyser local clip required');kind,props=doc.sub[sub.id];require(kind=='Animation'and not props.get('loop',False),'Geyser nonloop animation required');require(all(k in['resource_name','length','step','loop']or k.startswith('tracks/')for k in props),'Geyser unreviewed animation field');duration=f.f32(props.get('length',1));group={}
  for k,v in props.items():
   if k.startswith('tracks/'):parts=k.split('/');require(len(parts)==3 and parts[1].isdigit(),'Geyser track key');group.setdefault(int(parts[1]),{})[parts[2]]=v
  require(set(group)==set(range(len(group))),'Geyser track order');keys=[];cues=[]
  for track,t in group.items():
   fields(t,'type path interp loop_wrap imported enabled keys','geyser source track');require(type(t['path'])is NodePathValue and t['interp']==1 and type(t['interp'])is int and t['loop_wrap']is True and t['imported']is False and t['enabled']is True,'Geyser unsupported track flags');k=t['keys'];times=k['times'];require(type(times)is list and 0<len(times)<=256,'Geyser track times');times=[f.f32(v)for v in times];require(times==sorted(set(times))and 0<=times[0]<=times[-1]<=duration,'Geyser sorted clip key times')
   if t['type']=='value':
    fields(k,'times transitions update values','geyser property keys');require(t['path'].path in target_kinds and k['transitions']==[1]*len(times)and type(k['update'])is int and k['update']in[0,1]and len(k['values'])==len(times),'Geyser property key coverage');require(k['update']!=0 or times==[0.],'Geyser continuous curve unsupported');kind=target_kinds[t['path'].path]
    for time,value in zip(times,k['values']):require(type(value)is(int if kind==1 else bool),'Geyser property Variant type');keys.append(dict(time=time,kind=kind,value=int(value),constant=int(k['update']==0)))
   elif t['type']=='method':
    fields(k,'times transitions values','geyser method keys');require(t['path'].path=='.'and k['transitions']==[1]*len(times)and k['values']==[dict(args=[],method='play_rumble')for _ in times],'Geyser unknown method/opcode')
    for time in times:cues.append(dict(time=time,kind=1,sound=sounds[2]['id'],start=0.,end=0.,track=track))
   elif t['type']=='audio':
    fields(k,'clips times','geyser audio keys');require(t['path'].path=='AudioStreamPlayer2D'and len(k['clips'])==len(times),'Geyser audio target/keys')
    for time,a in zip(times,k['clips']):fields(a,'end_offset start_offset stream','geyser audio cue');source=doc.ext_path(a['stream'],'AudioStream');found=[v for v in sounds if v['source']==source];require(len(found)==1,'Geyser unknown cue stream');cues.append(dict(time=time,kind=2,sound=found[0]['id'],start=f.f32(a['start_offset']),end=f.f32(a['end_offset']),track=track))
   else:raise ValueError('Geyser unsupported source track')
  keys.sort(key=lambda k:(k['time'],k['kind']));cues.sort(key=lambda k:(k['time'],k['track']));m['clips'].append(dict(id=c['id'],duration=duration,first=len(m['keys']),count=len(keys),cue_first=len(m['cues']),cue_count=len(cues)));m['keys']+=keys;m['cues']+=cues
 require(doc.connections==[dict(signal='body_entered',**{'from':'.','to':'.'},method='_on_Poison_Geyser_body_entered'),dict(signal='body_exited',**{'from':'.','to':'.'},method='_on_Poison_Geyser_body_exited'),dict(signal='animation_finished',**{'from':'AnimationPlayer','to':'.'},method='_on_AnimationPlayer_animation_finished')],'Geyser source signal coverage');validate(m);return p,m,dict(commit=PIN,recipe_sha256=hashlib.sha256(Path(file).read_bytes()).hexdigest(),sources=p['sources'],symbols=p['symbols'],scope=p['scope'],engine_commit=p['engine_commit'],engine_sources=p['engine_sources'])
def validate(m):
 fields(m,'source script slot bus spewing reset frames initial_frame initial_monitoring initial_disabled monitorable layer mask hframes vframes centered root sprite offset size shape extents rotation max_distance attenuation pitch normal_gain muted_gain notify panning clips keys cues sounds','geyser model')
 for k in['source','script','slot','bus']:safe(m[k])
 for k in['spewing','reset']:integer(m[k],1,65535)
 require(m['spewing']!=m['reset'],'Geyser duplicate lifecycle clips');integer(m['frames'],1,256);integer(m['hframes'],1,256);integer(m['vframes'],1,256);require(m['frames']==m['hframes']*m['vframes'],'Geyser frame grid');integer(m['initial_frame'],0,m['frames']-1)
 for k in['initial_monitoring','initial_disabled','monitorable','centered']:require(type(m[k])is bool,'Geyser property Boolean')
 for k in['layer','mask']:integer(m[k],0,0xfffff)
 for k in['root','sprite','offset','size','shape','extents','notify']:
  require(type(m[k])is list and len(m[k])==(4 if k=='notify'else 2),'Geyser vector shape')
  for v in m[k]:real(v)
 require(all(v>0 for v in m['size']+m['extents']+m['notify'][2:]),'Geyser positive geometry')
 for k in['rotation','max_distance','attenuation','pitch','normal_gain','muted_gain','panning']:real(m[k])
 require(m['max_distance']>0 and m['attenuation']>0 and m['pitch']>0 and 0<=m['panning']<=4 and -100<=m['normal_gain']<=24 and -100<=m['muted_gain']<=24,'Geyser audio defaults')
 require(type(m['sounds'])is list and len(m['sounds'])==3 and {v['role']for v in m['sounds']}=={1,2,3}and len({v['id']for v in m['sounds']})==3,'Geyser media roles');sound_ids=set()
 for a in m['sounds']:fields(a,'id source length role','geyser sound');integer(a['id'],1,0xfffffffe);safe(a['source']);real(a['length'],.001,3600);integer(a['role'],1,3);sound_ids.add(a['id'])
 require(type(m['clips'])is list and len(m['clips'])==2 and {v['id']for v in m['clips']}=={m['spewing'],m['reset']}and 0<len(m['keys'])<=1024 and 0<len(m['cues'])<=256,'Geyser clip/key/cue capacities');first=cue_first=0
 for c in m['clips']:
  fields(c,'id duration first count cue_first cue_count','geyser clip');real(c['duration'],.000001,60);require(type(c['first'])is int and c['first']==first and type(c['cue_first'])is int and c['cue_first']==cue_first,'Geyser key/cue ownership');integer(c['count'],1,256);integer(c['cue_count'],0,64);require(c['count']<=len(m['keys'])-first and c['cue_count']<=len(m['cues'])-cue_first,'Geyser key span');seen=set();previous=(-1,0);properties=set()
  for k in m['keys'][first:first+c['count']]:
   fields(k,'time kind value constant','geyser key');real(k['time'],0,c['duration']);integer(k['kind'],1,3);integer(k['constant'],0,1);integer(k['value'],0,m['frames']-1 if k['kind']==1 else 1);order=(k['time'],k['kind']);require(order>previous,'Geyser sorted unique property key');previous=order;properties.add(k['kind']);require(not k['constant']or k['time']==0,'Geyser constant curve profile');require(k['kind']not in seen,'Geyser constant/discrete track mix');
   if k['constant']:seen.add(k['kind'])
  require(1 in properties,'Geyser frame track missing');previous=(-1,-1);tracks={}
  for k in m['cues'][cue_first:cue_first+c['cue_count']]:
   fields(k,'time kind sound start end track','geyser cue');real(k['time'],0,c['duration']);integer(k['kind'],1,2);integer(k['track'],0,31);require(k['sound']in sound_ids and (k['time'],k['track'])>previous,'Geyser cue identity/order');previous=(k['time'],k['track']);real(k['start'],0,3600);real(k['end'],0,3600);role=next(a['role']for a in m['sounds']if a['id']==k['sound']);require((k['kind']==1 and role==3 and k['start']==k['end']==0)or(k['kind']==2 and role in[1,2]),'Geyser opcode/media role')
   require(k['kind']not in tracks or tracks[k['kind']]==k['track'],'Geyser multiple same-target cue tracks unsupported');tracks[k['kind']]=k['track'];require(len(set(tracks.values()))==len(tracks),'Geyser mixed method/audio track')
  first+=c['count'];cue_first+=c['cue_count']
 require(first==len(m['keys'])and cue_first==len(m['cues']),'Geyser unowned key/cue');return m
def encode(m):
 validate(m);pool=bytearray()
 def S(v):raw=v.encode('ascii');span=(len(pool),len(raw));pool.extend(raw);return span
 ints=[v for k in['source','script','slot','bus']for v in S(m[k])]+[m['spewing'],m['reset'],m['frames'],m['initial_frame'],int(m['initial_monitoring']),int(m['initial_disabled']),int(m['monitorable']),m['layer'],m['mask'],m['hframes'],m['vframes'],int(m['centered'])]
 floats=[v for k in['root','sprite','offset','size','shape','extents']for v in m[k]]+[m[k]for k in['rotation','max_distance','attenuation','pitch','normal_gain','muted_gain']]+m['notify']+[m['panning']]+[0.]*5;policy=struct.pack('<20I28f',*ints,*floats)
 clips=b''.join(struct.pack('<If6I',c['id'],c['duration'],c['first'],c['count'],c['cue_first'],c['cue_count'],0,0)for c in m['clips']);keys=b''.join(struct.pack('<f3I',k['time'],k['kind'],k['value'],k['constant'])for k in m['keys']);cues=b''.join(struct.pack('<f2I2f3I',k['time'],k['kind'],k['sound'],k['start'],k['end'],k['track'],0,0)for k in m['cues']);sounds=b''.join(struct.pack('<3If4I',a['id'],*S(a['source']),a['length'],a['role'],0,0,0)for a in m['sounds']);return f.pack_container(MAGIC,0,[pool,policy,clips,keys,cues,sounds],STRIDES,LIMIT)
def parse(blob):
 scene,sections=f.unpack_container(blob,MAGIC,STRIDES,LIMIT,capability=1);pool,policy,clips,keys,cues,sounds=sections;require(scene==0 and 0<len(pool)<=8192 and len(policy)==192 and len(clips)==64 and 0<len(keys)//16<=1024 and 0<len(cues)//32<=256 and len(sounds)==96,'Geyser section bounds');used=bytearray(len(pool))
 def S(a,b):require(0<b<=512 and a<=len(pool)and b<=len(pool)-a and not any(used[a:a+b]),'Geyser string span/alias');used[a:a+b]=b'\1'*b;return pool[a:a+b].decode('ascii')
 v=struct.unpack('<20I28f',policy);m={k:S(*v[2*i:2*i+2])for i,k in enumerate(['source','script','slot','bus'])};m.update(zip(['spewing','reset','frames','initial_frame','initial_monitoring','initial_disabled','monitorable','layer','mask','hframes','vframes','centered'],v[8:20]));require(all(m[k]in[0,1]for k in['initial_monitoring','initial_disabled','monitorable','centered'])and not any(v[43:]),'Geyser policy flags/reserved')
 for k in['initial_monitoring','initial_disabled','monitorable','centered']:m[k]=bool(m[k])
 for i,k in enumerate(['root','sprite','offset','size','shape','extents']):m[k]=list(v[20+2*i:22+2*i])
 m.update(zip(['rotation','max_distance','attenuation','pitch','normal_gain','muted_gain'],v[32:38]));m['notify']=list(v[38:42]);m['panning']=v[42];m.update(clips=[],keys=[],cues=[],sounds=[])
 for at in range(0,len(clips),32):v=struct.unpack_from('<If6I',clips,at);require(not any(v[6:]),'Geyser clip reserved');m['clips'].append(dict(zip(['id','duration','first','count','cue_first','cue_count'],v[:6])))
 for at in range(0,len(keys),16):m['keys'].append(dict(zip(['time','kind','value','constant'],struct.unpack_from('<f3I',keys,at))))
 for at in range(0,len(cues),32):v=struct.unpack_from('<f2I2f3I',cues,at);require(not any(v[6:]),'Geyser cue reserved');m['cues'].append(dict(zip(['time','kind','sound','start','end','track'],v[:6])))
 for at in range(0,len(sounds),32):v=struct.unpack_from('<3If4I',sounds,at);require(not any(v[5:]),'Geyser sound reserved');m['sounds'].append(dict(id=v[0],source=S(v[1],v[2]),length=v[3],role=v[4]))
 require(all(used),'Geyser unused string bytes');return validate(m)
def main():
 a=argparse.ArgumentParser(description=__doc__);a.add_argument('--out',type=Path,required=True);o=a.parse_args();out=o.out.resolve();require(out.is_relative_to(ROOT/'build')and out!=ROOT/'build'and not out.exists(),'Fresh Geyser candidate under build required');p,m,proof=source();blob=encode(m);require(parse(blob)==m,'Geyser generated roundtrip');out.mkdir(parents=True);(out/'geyser.encgeyser').write_bytes(blob);(out/'model.json').write_text(json.dumps(m,indent=2)+'\n');proof.update(bytes=len(blob),sha256=hashlib.sha256(blob).hexdigest(),producer_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest());(out/'receipt.json').write_text(json.dumps(proof,indent=2)+'\n');print('Geyser candidate:',len(blob),'bytes;',len(m['keys']),'property keys,',len(m['cues']),'typed cues;not scene/audio admission')
if __name__=='__main__':main()
