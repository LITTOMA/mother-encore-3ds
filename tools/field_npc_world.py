#!/usr/bin/env python3
"""Complete source-native Podunk kinematic/ray properties; never Ready."""
from pathlib import Path
import argparse, json, struct, sys, zlib, re
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import decode,stable,SCENE,PIN,read,write,sha,require
IR=ROOT/'content/podunk-npc-world.json'
REVIEW=ROOT/'reports/field-npc-world/source-review.json'
PACK=ROOT/'romfs/data/podunk.encnpcworld'
FAMILY=0x454e006a

def extract(native,engine):
 d=read(native);tree=read(ROOT/'content/podunk-node-tree.json');context=read(ROOT/'content/podunk-scene.json');npc=read(ROOT/'content/native-field-npc.json')
 require(sha(native)==tree['native_sha256']==context['export_sha256'] and d['source']=='res://'+SCENE,'NPC native snapshot identity')
 root=ROOT/'upstream/MOTHER-Encore';proof={}
 for name in ('ray_cast_2d.cpp','physics_body_2d.cpp'):
  file=Path(engine)/('godot-3.6.2-'+name);s=file.read_text(encoding='utf8');proof[name]=dict(sha256=sha(file),source='https://github.com/godotengine/godot/blob/3.6.2-stable/scene/2d/'+name)
  if name=='ray_cast_2d.cpp':require('to = Vector2(0, 0.01)'in s and 'NOTIFICATION_INTERNAL_PHYSICS_PROCESS'in s and 'collided = false'in s,'Ray native cache semantics changed')
  else:require('KinematicBody2D::move_and_slide'in s,'Kinematic engine scope')
 sources={SCENE:sha(root/SCENE)};bodies=[];rays=[];links=[];nodes={n['path']:n for n in d['nodes']};known={n['node']:n for n in npc['npcs']}
 for n in d['nodes']:
  if n['class']not in ('KinematicBody2D','RayCast2D'):continue
  p=decode(n['properties']);path=n['path'];sid=stable(path)
  if n['class']=='KinematicBody2D':
   require(not p['motion/sync_to_physics'] and p['physics_interpolation_mode']==0,'Kinematic sync/interpolation unsupported')
   shapes=[]
   for owner in n['physics_shape_owners']:
    o=decode(owner);require(not o['one_way'],'Kinematic one-way unsupported');shapes.append(dict(id=stable(o['owner']['path']),disabled=o['disabled']))
   bodies.append(dict(id=sid,path=path,layer=p['collision_layer'],mask=p['collision_mask'],safe_margin=p['collision/safe_margin'],platform_leave=p['moving_platform_apply_velocity_on_leave'],shapes=shapes))
  else:
   require(p['script'] is None and p['physics_interpolation_mode']==0,'Ray source script/interpolation unsupported')
   rays.append(dict(id=sid,path=path,parent=stable(path.rsplit('/',1)[0]),mask=p['collision_mask'],enabled=p['enabled'],exclude=p['exclude_parent'],bodies=p['collide_with_bodies'],areas=p['collide_with_areas'],cast=p['cast_to']))
 for path,n in known.items():
  parts=[('collider','CollisionShape2D'),('interact','interact/CollisionShape2D'),('near','NearPlayerArea/CollisionShape2D'),('view','ViewArea/CollisionShape2D2'),('wander','WanderRadius/CollisionShape2D2'),('ray','RayCast2D'),('timer','WanderRadius/Timer'),('shadow','Shadow'),('sprite','CharacterSprite')]
  row=dict(id=n['id']);row.update({role:stable(path+'/'+part)for role,part in parts});require(all(path+'/'+part in nodes for _,part in parts),'NPC full native child closure');links.append(row)
 for p,h in npc['sources'].items():
  if p.endswith('.gd'):require(sha(root/p)==h,'NPC source changed');sources[p]=h
 script=(root/'Scripts/Main/npc.gd').read_text(encoding='utf8');require('$RayCast2D.set_cast_to(travelPos - global_position)'in script and '$RayCast2D.get_collider() == null'in script,'NPC cached ray call changed')
 require(len(bodies)==215 and len(rays)==81 and len(links)==67,'Complete NPC world native closure')
 callbacks=[]
 for op,name,arity in [(1,'_on_Timer_timeout',0),(2,'return_to_init_dir',0),(3,'_on_ViewArea_body_entered',1),(4,'_on_ViewArea_body_exited',1),(5,'_on_NearPlayerArea_body_entered',1),(6,'_on_NearPlayerArea_body_exited',1),(7,'_on_VisibilityNotifier2D_screen_entered',0),(8,'_on_VisibilityNotifier2D_screen_exited',0),(9,'stop_interaction',0),(10,'interact',0),(11,'telepathy',0),(12,'update_visibility_changed',0),(13,'_on_npc_tree_exiting',0)]:
  require(re.search(r'func '+name+r'\(',script),'NPC callback missing');callbacks.append(dict(op=op,method=name,arity=arity))
 visibility=re.search(r'connect\("([^"]+)", self, "update_visibility_changed"\)',script);require(visibility,'NPC visibility connection changed')
 result=dict(schema=1,format=1,family=FAMILY,capability=1,rules=1,commit=PIN,scene=SCENE,scene_id=context['scene_id'],source_sha256=context['source_sha256'],native_sha256=sha(native),tree_ir_sha256=sha(ROOT/'content/podunk-node-tree.json'),npc_ir_sha256=sha(ROOT/'content/native-field-npc.json'),sources=sources,engine=proof,zero_cast=[0,0.01],callbacks=callbacks,visibility_signal=visibility.group(1),bodies=bodies,rays=rays,npcs=links,admission_ready=False)
 write(IR,result);write(REVIEW,dict(schema=1,ir_sha256=sha(IR),sources=sources,engine=proof,native_sha256=sha(native),admission_ready=False));return result
def load():
 d=read(IR);require(d['commit']==PIN and d['family']==FAMILY and read(REVIEW)['ir_sha256']==sha(IR),'NPC world source review')
 for p,h in d['sources'].items():require(sha(ROOT/'upstream/MOTHER-Encore'/p)==h,'NPC world source changed '+p)
 require(sha(ROOT/'content/podunk-node-tree.json')==d['tree_ir_sha256'] and sha(ROOT/'content/native-field-npc.json')==d['npc_ir_sha256'],'NPC world cross-binding changed');return d
def encode(d):
 b=bytearray(128)
 def u(*v):b.extend(struct.pack('<'+'I'*len(v),*v))
 def f(*v):b.extend(struct.pack('<'+'f'*len(v),*v))
 def t(v):p=v.encode();u(len(p));b.extend(p)
 f(*d['zero_cast']);u(len(d['bodies']),len(d['rays']),len(d['npcs']))
 for x in d['bodies']:
  u(x['id'],x['layer'],x['mask'],x['platform_leave'],len(x['shapes']));f(x['safe_margin']);t(x['path'])
  for s in x['shapes']:u(s['id'],s['disabled'])
 for x in d['rays']:u(x['id'],x['parent'],x['mask'],x['enabled'],x['exclude'],x['bodies'],x['areas']);f(*x['cast']);t(x['path'])
 for x in d['npcs']:u(*[x[k]for k in ('id','collider','interact','near','view','wander','ray','timer','shadow','sprite')])
 u(len(d['callbacks']))
 for x in d['callbacks']:u(x['op'],x['arity']);t(x['method'])
 t(d['visibility_signal'])
 struct.pack_into('<8s8I',b,0,b'ENCNPCW1',1,128,len(b),zlib.crc32(b[128:]),FAMILY,1,1,d['scene_id']);b[40:60]=bytes.fromhex(PIN);b[60:92]=bytes.fromhex(d['source_sha256']);b[92:124]=bytes.fromhex(sha(IR));return bytes(b)
def stage_files(root):
 b=encode(load());require((Path(root)/'data/podunk.encnpcworld').read_bytes()==b,'NPC world pack changed');return {Path('data/podunk.encnpcworld'):b}
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile']);p.add_argument('--native',type=Path);p.add_argument('--engine',type=Path);a=p.parse_args()
 try:
  d=extract(a.native,a.engine)if a.action=='extract'else load();PACK.parent.mkdir(parents=True,exist_ok=True);PACK.write_bytes(encode(d));print('NPC native world:',len(d['bodies']),'bodies;',len(d['rays']),'rays;',len(d['npcs']),'NPCs; Ready not granted')
 except(ValueError,KeyError,OSError,TypeError,struct.error)as e:sys.exit('NPC WORLD ERROR: '+str(e))
