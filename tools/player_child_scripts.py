#!/usr/bin/env python3
"""Four actual Player scripts, reusing checked camera/arrow execution schemas.
This is production source conversion, never a scene execution or probe.
"""
from pathlib import Path
import argparse,copy,hashlib,json,re,struct,sys,zlib
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import PIN,read,write,sha,require,decode
from tools.extract_battle_entry import Extractor
from tools.player_initialization import load as player_load,IR as PLAYER_IR
from tools.player_ready import IR as READY_IR
from tools import field_game_camera as gc,field_camera_arrows as ar
from tools.scene_reference import quarantine
IR=ROOT/'content/native-player-child-scripts.json'
REVIEW=ROOT/'reports/player-child-scripts/source-review.json'
PACK=ROOT/'romfs/data/player.encchildren'
FAMILY=0x454e0061
EMOTE='Nodes/Ui/emotes.tscn::6';TINT='Scripts/misc/character_tint.gd'

def camera_connections(ex):
 code=ex.text(gc.SCRIPT)
 calls=re.findall(r'^\s*(uiManager|global\.get_player\(\))\.connect\("([^"\n]+)", self, "([^"\n]+)"\)',code,re.M)
 require(len(calls)==2 and [x[0]for x in calls]==['uiManager','global.get_player()'],'Unknown Camera source connection owner/order')
 return[dict(role=i+1,signal=x[1],method=x[2])for i,x in enumerate(calls)]

def refresh_connections():
 d=load(check_connections=False);ex=Extractor(ROOT)
 for f,h in d['sources'].items():ex.data(f);require(ex.sources[f]==h,'Player child source closure changed '+f)
 d.update(schema=2,format=2,capability=2,camera_connections=camera_connections(ex))
 write(IR,d);r=read(REVIEW);r.update(ir_sha256=sha(IR),camera_connection_review='Both exact original Camera Ready connections are retained with actual owner role and method; no new Ready capability')
 write(REVIEW,r)

def extract():
 p=player_load();ready=read(READY_IR);require(ready['initialization_ir_sha256']==sha(PLAYER_IR),'Player Ready source differs');oldcam=gc.load();oldarrow=ar.load();ex=Extractor(ROOT)
 ns={n['path']:decode(n['properties'])for n in p['native_snapshot']['nodes']};rs={r['id']:r for r in p['native_snapshot']['resources']};nodes={n['node']:n for n in p['records']}
 for f,h in p['sources'].items():ex.data(f);require(ex.sources[f]==h,'Player source closure differs '+f)
 for d in [oldcam,oldarrow]:
  for f,h in d['sources'].items():
   if f in [d['scene']]:continue
   ex.data(f);require(ex.sources[f]==h,'Shared original script/resources differ '+f)
 whole=ex.text(EMOTE.split('::')[0]);_,_,att,_=quarantine(ex.data(EMOTE.split('::')[0]),EMOTE.split('::')[0]);embedded=next(x['embedded_script']for x in att if x['script']==EMOTE)
 require(embedded['sha256']==nodes['Position/main/emotes']['script_sha'],'Actual embedded script identity differs')
 require(re.findall(r'^\[connection[^\n]*\]',whole,re.M)==['[connection signal="animation_started" from="AnimationPlayer" to="." method="_on_AnimationPlayer_animation_started"]'],'Unknown emote original source connection')
 quoted=re.search(r'script/source = ("(?:[^"\\]|\\.)*")',whole)[1];code=json.loads(quoted.replace("\n",r"\n").replace("\r",r"\r").replace("\t",r"\t"))
 require(re.findall(r'^func (\w+)\(',code,re.M)==['_on_AnimationPlayer_animation_started','set_bubble_offset'],'Unknown emotes method')
 for fact in ['onready var animaPlayer = $AnimationPlayer','onready var object = get_node_or_null(objectPath)','while !emotive_parent.has_method("get_direction")','if direction.x < 0:','scale.x = -1','scale.x = 1','object.texture.get_height() / object.vframes + 8']:require(fact in code,'Unknown emotes branch '+fact)
 clips=re.search(r'anim_name in (\[[^\n]+\])',code);clips=json.loads(clips[1]);padding=float(re.search(r'object.vframes \+ ([0-9.]+)',code)[1]);direction=re.search(r'get_direction\(\) -> Vector2:\n\treturn (\w+)',ex.text(p['player_script']))
 require(direction,'Original actual direction getter missing')
 tint=ex.text(TINT);require(re.findall(r'^func (\w+)\(',tint,re.M)==['_ready','_set_targets','connect_tint','set_tint'],'Unknown tint methods')
 for fact in ['var _tint := Color.white','if node: _targets.append(node)','if !_targets: _set_targets()','node.self_modulate = _tint','emit_signal("changed_tint", _tint)']:require(fact in tint,'Unknown tint branch '+fact)
 st=next(s for s in p['native_snapshot']['scene_states']if s['source']=='res://'+p['scene']);decl={n['path'].removeprefix('./'):decode(n['properties'])for n in st['nodes']};rows=[]
 for role,script in enumerate([EMOTE,TINT,gc.SCRIPT,ar.SCRIPT],1):
  r=next(x for x in p['records']if x['script']==script);rows.append(dict(role=role,id=r['id'],ready=r['ready'],path=r['node'],native_class=r['native_class'],script=script,script_sha256=r['script_sha']))
 camera=copy.deepcopy(oldcam);camera.update(scene=p['scene'],scene_id=p['scene_id'],source_sha256=p['source_sha256'],sources=ex.sources,records=[])
 c=nodes[next(r['path']for r in rows if r['role']==3)];path=c['node'];v=ns[path];a=path+'/Area2D';sp=a+'/CollisionShape2D';ap=path+'/ArrowsAnim';av=ns[a];sv=ns[sp];an=ns[ap]
 require(v['process_mode']==1 and v['current']and v['zoom']==[1,1]and not v['smoothing_enabled']and not v['limit_smoothed']and not v['drag_margin_h_enabled']and not v['drag_margin_v_enabled']and v['rotation']==0 and v['scale']==[1,1],'Unknown Player Camera native mode')
 lengths=[]
 for name in ['Come In','Come Out','RESET']:
  q=decode(rs[an['anims/'+name]['id']]['properties']);require(not q['loop']and not any(k.startswith('tracks/')for k in q),'Camera animation unknown tracks');lengths.append(q['length'])
 require(lengths==camera['animation_lengths'],'Camera prototype lengths differ')
 cr=dict(id=c['id'],ready=c['ready'],parent_id=c['parent'],arrows_id=nodes[path+'/ScopeArrows']['id'],animation_id=nodes[ap]['id'],animation_ready=nodes[ap]['ready'],area_id=nodes[a]['id'],shape_id=nodes[sp]['id'],flags=sum(int(x)<<i for i,x in enumerate([v['current'],v['rotating'],v['visible'],v['z_as_relative']])),pause=v['pause_mode'],physics_interpolation=v['physics_interpolation_mode'],z=v['z_index'],priority=v['process_priority'],limits=[v['limit_'+k]for k in ['top','left','right','bottom']],position=v['position'],offset=v['offset'],world=next(decode(x['world_transform'])for x in p['native_snapshot']['nodes']if x['path']==path),zoom=v['zoom'],rotation=v['rotation'],area_position=av['position'],area_scale=av['scale'],shape_position=sv['position'],shape_scale=sv['scale'],shape_extents=decode(rs[sv['shape']['id']]['properties'])['extents'],area_layer=av['collision_layer'],area_mask=av['collision_mask'],area_flags=sum(int(x)<<i for i,x in enumerate([av['monitoring'],av['monitorable'],sv['disabled'],sv['one_way_collision']])),node=path)
 camera['records']=[cr]
 arrows=copy.deepcopy(oldarrow);arrows.update(scene=p['scene'],scene_id=p['scene_id'],source_sha256=p['source_sha256'],sources=ex.sources,records=[],sprites=[],players=[])
 atlas={}
 for aid,body in re.findall(r'^\[sub_resource type="AtlasTexture" id=(\d+)\]\n(.*?)(?=^\[|\Z)',ex.text(ar.PROTOTYPE),re.M|re.S):
  region=re.search(r'region = Rect2\( ([^)]*) \)',body);require(region,'Original atlas region missing');atlas['res://'+ar.PROTOTYPE+'::'+aid]=[float(x)for x in region[1].split(',')]
 root=nodes[next(r['path']for r in rows if r['role']==4)];rp=root['node'];v=ns[rp];color=lambda q:[q[k]for k in ['r','g','b','a']];ids=[]
 for index,part in enumerate(['arrowU','arrowD','arrowL','arrowR']):
  n=nodes[rp+'/'+part];s=ns[n['node']];f=dict(decode(rs[s['frames']['id']]['properties'])['animations'][0]['pairs']);require(f['name']=='Idle'and f['loop']and len(f['frames'])==4,'Unknown Player arrow frames')
  regions=[atlas[rs[x['id']]['path']]for x in f['frames']];require(regions==arrows['frames'],'Player arrow source atlas differs')
  ids.append(n['id']);arrows['sprites'].append(dict(id=n['id'],root_id=root['id'],ready=n['ready'],direction=index,frame=0,flags=sum(int(x)<<i for i,x in enumerate([s['visible'],s['playing'],s['centered']])),pause=s['pause_mode'],priority=s['process_priority'],position=s['position'],offset=s['offset'],scale=s['scale'],rotation=s['rotation'],speed_scale=s['speed_scale'],animation_speed=f['speed'],modulate=color(s['modulate']),self_modulate=color(s['self_modulate'])))
 arrows['records']=[dict(id=root['id'],ready=root['ready'],parent_id=root['parent'],node=rp,arrows=ids,position=v['position'],visible=v['visible'],z=v['z_index'],z_relative=v['z_as_relative'],pause=v['pause_mode'],priority=v['process_priority'],modulate=color(v['modulate']),self_modulate=color(v['self_modulate']))]
 for target,kind,profile in [(root,0,1)]+[(nodes[rp+'/'+x],1,0)for x in ['arrowU','arrowD','arrowL','arrowR']]:
  n=nodes[target['node']+'/AnimationPlayer'];a=ns[n['node']];clips=arrows['profiles'][profile]
  require(a['playback_process_mode']==1 and a['autoplay']==''and a['blend_times']==[]and a['playback_default_blend_time']==0,'Unknown Player arrow animation native state')
  for clip in clips:
   q=decode(rs[a['anims/'+clip['name']]['id']]['properties']);require(q['length']==clip['length']and q['loop']==clip['loop'],'Player arrow clip source differs')
   tracks=clip['tracks'];require(len([k for k in q if re.fullmatch(r'tracks/\d+/type',k)])==len(tracks),'Player arrow track count differs')
   for i,tr in enumerate(tracks):
    pre='tracks/'+str(i)+'/';keys=dict(q[pre+'keys']['pairs']);require(q[pre+'type']=='value'and q[pre+'enabled']and q[pre+'interp']==1 and q[pre+'loop_wrap']and keys['update']==tr['update'],'Player arrow track semantics differ')
    prop={1:'frame',2:'offset',3:'playing',4:'position',5:'visible'}[tr['property']];parts=['.','arrowU','arrowD','arrowL','arrowR'];require(q[pre+'path']['value']==parts[tr['target']]+':'+prop,'Player arrow track target differs')
    values=[x if isinstance(x,list)else[float(x),0]for x in keys['values']];require([k['value']for k in tr['keys']]==values and[k['time']for k in tr['keys']]==keys['times']and[k['transition']for k in tr['keys']]==keys['transitions'],'Player arrow track keys differ')
  arrows['players'].append(dict(id=n['id'],ready=n['ready'],target_root=target['id'],kind=kind,profile=profile,pause=a['pause_mode'],priority=a['process_priority'],speed=a['playback_speed']))
 em=rows[0];ep=decl[em['path']];tpaths=[x['value']for x in decl[rows[1]['path']]['sprite_paths']]
 d=dict(schema=1,format=1,capability=1,rules=1,family=FAMILY,commit=PIN,scene=p['scene'],scene_id=p['scene_id'],source_sha256=p['source_sha256'],player_ir_sha256=sha(PLAYER_IR),ready_ir_sha256=sha(READY_IR),rows=rows,emote=dict(object_path=ep['objectPath']['value'],animation_path='AnimationPlayer',sensitive_clips=json.loads(re.search(r'anim_name in (\[[^\n]+\])',code)[1]),padding=padding,negative_scale=-1,other_scale=1,direction_method='get_direction',direction_member=direction[1],signal='animation_started',method='_on_AnimationPlayer_animation_started'),tint=dict(paths=tpaths,color=[1,1,1,1],signal='changed_tint',method='set_tint'),camera_process_mode=ns[path]['process_mode'],camera=camera,arrows=arrows,assets=read(ar.ASSETS),sources=ex.sources,ready_admitted=False,pending=['Native Camera viewport/SourceTween/Shaker must be provided by actual source owners; missing endpoints reject','Native sprite and AnimationPlayer clocks belong to the unique actual Player native service','No other Player or scene script is admitted'])
 require(d['camera_process_mode']==1,'Player native Camera process mode differs')
 d.update(schema=2,format=2,capability=2,camera_connections=camera_connections(ex))
 write(IR,d);write(REVIEW,dict(schema=1,commit=PIN,ir_sha256=sha(IR),sources=ex.sources,embedded_block_sha256=embedded['sha256'],scope='All four actual remaining Player script instances; exact source native camera/arrow data in existing checked schemas; no native lifecycle approval',ready_admitted=False))

def load(check_connections=True):
 d=read(IR);r=read(REVIEW);require(d['commit']==PIN and d['family']==FAMILY and r['ir_sha256']==sha(IR)and d['player_ir_sha256']==sha(PLAYER_IR)and d['ready_ir_sha256']==sha(READY_IR),'Player child source/dependency proof differs');ex=Extractor(ROOT)
 for f,h in d['sources'].items():ex.data(f);require(ex.sources[f]==h,'Changed Player child source '+f)
 if check_connections:require(d['schema']==2 and d['format']==2 and d['capability']==2 and d['camera_connections']==camera_connections(ex),'Player Camera source connections differ')
 require(sha(ROOT/'romfs'/d['arrows']['asset']['path'])==d['assets']['output_sha256']and sha(ROOT/'romfs'/d['arrows']['program']['path'])==d['assets']['program']['output_sha256'],'Actual converted arrows asset differs');return d

def encode(d):
 b=bytearray(128)
 def u(*x):b.extend(struct.pack('<'+'I'*len(x),*x))
 def t(s):v=s.encode();u(len(v));b.extend(v)
 def h(s):b.extend(bytes.fromhex(s))
 def f(*v):b.extend(struct.pack('<'+'f'*len(v),*v))
 t(d['scene']);h(d['player_ir_sha256']);h(d['ready_ir_sha256']);u(len(d['rows']))
 for r in d['rows']:u(r['role'],r['id'],r['ready']);t(r['path']);t(r['native_class']);t(r['script']);h(r['script_sha256'])
 em=d['emote'];[t(em[k])for k in ['object_path','animation_path','direction_method','direction_member','signal','method']];u(len(em['sensitive_clips']));[t(s)for s in em['sensitive_clips']];f(em['padding'],em['negative_scale'],em['other_scale']);ti=d['tint'];f(*ti['color']);t(ti['signal']);t(ti['method']);u(len(ti['paths']));[t(s)for s in ti['paths']];u(d['camera_process_mode'])
 u(len(d['camera_connections']))
 for c in d['camera_connections']:u(c['role']);t(c['signal']);t(c['method'])
 for raw in [gc.encode(d['camera']),ar.encode(d['arrows'],d['assets'])]:
  raw=bytearray(raw);raw[92:124]=bytes.fromhex(sha(IR));u(len(raw));b.extend(raw)
 u(len(d['sources']))
 for s,v in d['sources'].items():t(s);h(v)
 struct.pack_into('<8s8I',b,0,b'ENCPSCR1',2,128,len(b),zlib.crc32(b[128:]),FAMILY,2,1,d['scene_id']);b[40:60]=bytes.fromhex(PIN);b[60:92]=bytes.fromhex(d['source_sha256']);b[92:124]=bytes.fromhex(sha(IR));return bytes(b)

def stage_files(root):
 raw=encode(load());p=Path('data/player.encchildren');require((Path(root)/p).read_bytes()==raw,'Staged Player child scripts differ');return{p:raw}
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile','verify','refresh-connections']);a=p.parse_args()
 try:
  if a.action=='extract':extract()
  elif a.action=='refresh-connections':refresh_connections()
  else:
   raw=encode(load())
   if a.action=='compile':PACK.parent.mkdir(parents=True,exist_ok=True);PACK.write_bytes(raw)
   else:require(PACK.read_bytes()==raw,'Stale Player child binary')
   print('Actual Player child scripts:',len(raw),'bytes; no native Ready approval')
 except(ValueError,KeyError,TypeError,OSError,struct.error,AttributeError,StopIteration)as e:sys.exit('PLAYER CHILD SCRIPTS ERROR: '+str(e))
