#!/usr/bin/env python3
"""Complete pinned Podunk ambient butterflies; not a healing NPC substitute."""
from __future__ import annotations
import argparse,hashlib,json,math,re,struct,sys,zlib,subprocess
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import decode,stable,SCENE,PIN,read,write,sha,require
from tools.extract_battle_entry import Extractor,node,animation
from tools.asset_receipts import receipt_path
SCRIPT='Scripts/misc/butterfly.gd';SCENE_FLY='Nodes/Overworld/butterfly.tscn'
IR=ROOT/'content/native-field-butterfly.json';REVIEW=ROOT/'reports/field-butterfly/source-review.json';PACK=ROOT/'romfs/data/podunk-butterflies.encfly'
def source_instances(native,receipt,upstream):
 d=read(native);s=read(receipt);require(d['source']=='res://'+SCENE and d['schema']==1 and d['native_compatible'] is False,'Incomplete NPC native scene')
 require(s['commit']==PIN and s['scene']==SCENE,'Unreviewed NPC native receipt')
 inventory=read(ROOT/'compatibility/upstream-inventory.json')['files']
 for name,record in s['files'].items():require(sha(upstream/name)==record['sha256']==inventory[name]['sha256'],'Changed NPC closure '+name)
 names={n['path']:n for n in d['nodes']};require(len(names)==8686,'Incomplete Podunk enemy scene');require(decode(names['Objects']['world_transform'])==[[1,0],[0,1],[0,0]],'NPC Objects transform requires a source adapter')
 resources={r['id']:r for r in d['resources']};states={s['source'][6:]:s for s in d['scene_states']};roots=[]
 def visit_instances(path,filename):
  roots.append((path,filename))
  for n in states[filename]['nodes']:
   if n['instance'] is not None:
    local=n['path'][2:] if n['path'].startswith('./') else n['path'];target=local if path=='.' else path+'/'+local
    visit_instances(target,resources[n['instance']['id']]['path'][6:])
 visit_instances('.',SCENE);bindings={};overrides={}
 for root,filename in sorted(roots,key=lambda r:r[0].count('/') if r[0]!='.' else -1,reverse=True):
  for n in states[filename]['nodes']:
   local=n['path'][2:] if n['path'].startswith('./') else n['path'];path=root if local=='.' else local if root=='.' else root+'/'+local
   if path in names:overrides.setdefault(path,{}).update(decode(n['properties']))
  for a in s['script_attachments']:
   if a['source']!=filename:continue
   name=re.search(r'\bname="([^"]+)"',a['node_declaration'])[1];parent=re.search(r'\bparent="([^"]+)"',a['node_declaration'])
   local='.' if parent is None else name if parent[1]=='.' else parent[1]+'/'+name;path=root if local=='.' else local if root=='.' else root+'/'+local
   require(path in names,'Unresolved NPC binding '+path);bindings[path]=a['script']
 children={p:[] for p in names}
 for path in names:
  if path!='.':children[path.rsplit('/',1)[0] if '/' in path else '.'].append(path)
 ready=[]
 def visit(path):
  for child in children[path]:visit(child)
  ready.append(path)
 visit('.');ordinal={p:i for i,p in enumerate(ready)};out=[]
 for path in sorted(bindings,key=lambda p:ordinal[p]):
  if bindings[path]!=SCRIPT:continue
  out.append(dict(stable_id=stable(path),node=path,ready_ordinal=ordinal[path],script=bindings[path],overrides=overrides[path],native=names[path],children=[names[p] for p in children[path]]))
 require(len(out)==94,'Podunk butterfly binding loss');return out,names,resources,dict(native_export_sha256=sha(native),source_receipt_sha256=sha(receipt),source_files={name:r['sha256'] for name,r in s['files'].items()})


def build(records,nodes,resources,provenance):
 ex=Extractor(ROOT);ex.sources.update(provenance['source_files']);script=ex.text(SCRIPT);scene=ex.text(SCENE_FLY);ex.text('LICENSE');project=ex.text('project.godot');pixel_snap=re.search(r'2d/snapping/use_gpu_pixel_snap=(true|false)',project);require(pixel_snap,'Source pixel snap missing')
 for fact in ['set_process(false)','hide()','_movement()','position = position.move_toward(start_pos, speed * _delta)','if global_position == start_pos:','body is PartyMemberPlayer and body.is_walking()','inputVector = -sprite.global_position.direction_to(body.global_position)','body == global.get_player() and global_position != start_pos','yield($Timer,"timeout")','inputVector = Vector2.ZERO','_return = true','velocity = move_and_slide(velocity)','animationPlayer.play("Fly")','animationPlayer2.play("Flying")','animationPlayer.stop()','animationPlayer2.stop()']:require(fact in script,'Unknown butterfly source '+fact)
 require(script.count('yield($Timer,"timeout")')==2 and script.count('$Timer.start()')==2,'Butterfly source wait topology')
 modulus=int(re.search(r'var2str\(randi\(\)%(\d+)\)',script)[1]);speed=float(re.search(r'var speed = ([\d.]+)',script)[1]);fly_seek=float(re.search(r'animationPlayer.seek\(\(randf\(\)\*([\d.]+)\)',script)[1]);orbit_seek=float(re.search(r'animationPlayer2.seek\(\(randf\(\)\*([\d.]+)\)',script)[1]);assets=[]
 for i in range(modulus):
  art='Graphics/Character Sprites/Npcs/misc/butterflies/'+str(i)+'.png';size=ex.png_size(art);ex.data(art+'.import');assets.append(dict(id=i,source=art,path='graphics/objects/butterfly/'+str(i)+'.t3x',width=size[0],height=size[1]))
 clips=[]
 for player,clip,role in [('AnimationPlayer','Fly',1),('AnimationPlayer2','Flying',2)]:
  a=animation(scene,node(scene,player)['anims/'+clip]['SubResource'],SCENE_FLY,clip);tracks=[]
  for tr in a['tracks']:
   name=tr['path'];kind={'Sprite:frame':1,'Sprite:flip_h':2,'Sprite:position':3,'Sprite:offset':4}.get(name,0);require(kind and tr['enabled']and tr['type']=='value'and tr['interp']==1 and tr['loop_wrap']and tr['keys']['update']==(1 if kind<=2 else 0),'Unsupported butterfly source track')
   tracks.append(dict(role=kind,discrete=kind<=2,keys=[dict(time=t,transition=c,value=[float(v),0]if kind<=2 else v)for t,c,v in zip(tr['keys']['times'],tr['keys']['transitions'],tr['keys']['values'])]))
  require(a['loop'],'Butterfly source looping animation required');clips.append(dict(role=role,name=clip,length=a['length'],tracks=tracks))
 children={p:[]for p in nodes}
 for p in nodes:
  if p!='.':children[p.rsplit('/',1)[0]if'/'in p else'.'].append(p)
 ready=[]
 def visit(p):
  for c in children[p]:visit(c)
  ready.append(p)
 visit('.');ordinal={p:i for i,p in enumerate(ready)};bindings=[]
 def rect_at(path,parent_wt):
  n=nodes[path];v=decode(n['properties']);wt=decode(n['world_transform']);require(wt[0][1]==wt[1][0]==0 and wt[0][0]>0 and wt[1][1]>0,'Butterfly notifier transform');rect=v['rect'];return[wt[2][0]-parent_wt[2][0]+rect[0][0]*wt[0][0],wt[2][1]-parent_wt[2][1]+rect[0][1]*wt[1][1],rect[1][0]*wt[0][0],rect[1][1]*wt[1][1]]
 for a in records:
  path=a['node'];root=decode(a['native']['properties']);wt=decode(a['native']['world_transform']);parent=nodes[path.rsplit('/',1)[0]];parent_wt=decode(parent['world_transform']);sprite=decode(nodes[path+'/Sprite']['properties']);timer=decode(nodes[path+'/Timer']['properties']);body=decode(nodes[path+'/CollisionShape2D']['properties']);area=decode(nodes[path+'/Sprite/MoveArea']['properties']);shape=decode(nodes[path+'/Sprite/MoveArea/CollisionShape2D']['properties']);circle=resources[shape['shape']['id']];enable=decode(nodes[path+'/VisibilityEnabler2D']['properties']);require(root['collision_layer']==root['collision_mask']==0 and body['disabled'],'Butterfly collision capability: only source uncollidable body');require(wt[0][1]==wt[1][0]==parent_wt[0][1]==parent_wt[1][0]==0 and wt[0][0]==wt[1][1]==parent_wt[0][0]==parent_wt[1][1]>0 and root['scale']==[1,1],'Butterfly unsupported rotated/skew/nonuniform parent');require(sprite['material']is None and not sprite['use_parent_material']and sprite['hframes']==2 and sprite['vframes']==1 and sprite['centered']and not sprite['region_enabled']and sprite['rotation']==0 and sprite['scale']==[1,1]and not sprite['flip_v'],'Butterfly sprite capability');require(circle['class']=='CircleShape2D'and shape['scale'][0]==shape['scale'][1]>0 and shape['rotation']==0,'Butterfly circle capability');require(timer['process_mode']==1 and timer['one_shot']and not timer['autostart'],'Butterfly Timer capability');require(enable['pause_animations']and enable['freeze_bodies']and enable['process_parent']and enable['physics_process_parent']and not enable['pause_particles'],'Butterfly VisibilityEnabler source capability')
  bindings.append(dict(id=a['stable_id'],node=path,ready_ordinal=a['ready_ordinal'],timer_id=stable(path+'/Timer'),timer_ordinal=ordinal[path+'/Timer'],fly_id=stable(path+'/AnimationPlayer'),fly_ordinal=ordinal[path+'/AnimationPlayer'],orbit_id=stable(path+'/AnimationPlayer2'),orbit_ordinal=ordinal[path+'/AnimationPlayer2'],area_id=stable(path+'/Sprite/MoveArea'),notifier_id=stable(path+'/VisibilityNotifier2D'),enabler_id=stable(path+'/VisibilityEnabler2D'),position=root['position'],parent_origin=parent_wt[2],parent_scale=parent_wt[0][0],sprite_position=sprite['position'],sprite_offset=sprite['offset'],sprite_color=[sprite['modulate'][k]for k in ['r','g','b','a']],sprite_frame=sprite['frame'],sprite_flip=sprite['flip_h'],timer_wait=timer['wait_time'],fly_speed=decode(nodes[path+'/AnimationPlayer']['properties'])['playback_speed'],orbit_speed=decode(nodes[path+'/AnimationPlayer2']['properties'])['playback_speed'],area_center=shape['position'],area_radius=decode(circle['properties'])['radius']*shape['scale'][0],area_layer=area['collision_layer'],area_mask=area['collision_mask'],area_flags=int(area['monitoring'])|int(area['monitorable'])<<1,notifier=rect_at(path+'/VisibilityNotifier2D',wt),enabler=rect_at(path+'/VisibilityEnabler2D',wt)))
 connections=[]
 for role,signal in enumerate(['body_entered','body_exited'],1):
  methods=re.findall(r'\[connection signal="'+signal+r'" from="Sprite/MoveArea" to="\." method="([^"]+)"\]',scene)
  require(len(methods)==1 and re.search(r'^func '+re.escape(methods[0])+r'\(body\):',script,re.M),'Butterfly actual source Area connection differs')
  connections.append(dict(role=role,signal=signal,method=methods[0]))
 return dict(schema=1,format=2,capability=2,connections=connections,kind='encore.field-butterfly.source-ir',commit=PIN,scene=SCENE,script=SCRIPT,modulus=modulus,pixel_snap=pixel_snap[1]=='true',frames=node(scene,'Sprite')['hframes'],speed=speed,fly_seek=fly_seek,orbit_seek=orbit_seek,assets=assets,clips=clips,bindings=bindings,sources=dict(sorted(ex.sources.items())),provenance=provenance,engine=[dict(url='https://raw.githubusercontent.com/godotengine/godot/3.6.2-stable/scene/2d/physics_body_2d.cpp',sha256='1a47c0292bd20f92b14262ef0957fd1bf14060e555d072f739242b9e81914ee4')],semantics=['All94 actual ambient butterflies, complete inner-to-outer instance overrides and true Ready order; no healing/PP/flags exists in this source','Ready draws global randi modulo5, captures local start position, disables normal process and hides; screen enter draws exactly two global randf for Fly and Flying seek after play','Source animations update independently in native internal idle node order; ordinary script _process follows internal nodes, move_and_slide selects idle delta outside physics frame','Source body collision layer/mask zero and disabled shape justify source free movement, not generic KinematicBody replacement; area uses true circle source geometry and layer4096','PartyMemberPlayer walking contact changes return/input/facing based on current animated Sprite world position; global-player leave uses literal global-position versus local start compare','Multiple exits share one one-shot Timer and FIFO two-wait coroutines; Timer continues offscreen and is not paused merely by Player pause; second timeout enables local move_toward return','Loop interpolation keeps source continuous negative-1.2 ease, texture frame and discrete facing key crossing; movement-facing changes persist between discrete animation keys','VisibilityNotifier and VisibilityEnabler rectangles and source child identities are admitted by live host; two true native clocks are retained','Original Sprite/MoveArea body_entered and body_exited symbols are extracted from the actual scene and source receiver functions, without inventing new signals'],unsupported=['No healing NPC behavior, world flag or new Disappear animation invented: method exists unconnected and no such source clip in actual scene','Live parent changes/rotation/skew or generic colliding butterfly body need next explicit capability, not silent approximation'],unverified=['Manual tests not run','3DS art and integrated scene admission pending'])
def validate(d):
 require(d['schema']==1 and d['kind']=='encore.field-butterfly.source-ir'and d['commit']==PIN and d['scene']==SCENE and d['script']==SCRIPT and len(d['bindings'])==94 and len(d['assets'])==d['modulus']and len(d['clips'])==2 and 0<d['speed']<=65535,'Butterfly source schema/topology');ids=set()
 for b in d['bindings']:
  require(b['id']==stable(b['node'])and b['id']not in ids and b['timer_ordinal']<b['ready_ordinal']and b['fly_ordinal']<b['ready_ordinal']and b['orbit_ordinal']<b['ready_ordinal'],'Butterfly source identity/Ready');ids.add(b['id'])
  for key in ['position','parent_origin','sprite_position','sprite_offset','sprite_color','area_center','notifier','enabler']:require(all(type(v)in(int,float)and math.isfinite(v)and abs(v)<=1e6 for v in b[key]),'Butterfly finite source geometry')
 require(d['format']==2 and d['capability']==2 and len(d['connections'])==2 and [v['role']for v in d['connections']]==[1,2] and [v['signal']for v in d['connections']]==['body_entered','body_exited'] and all(re.fullmatch(r'[A-Za-z_][A-Za-z0-9_]*',v['method'])for v in d['connections']) and len({v['method']for v in d['connections']})==2,'Butterfly source connection symbols')
 return d
def load():
 d=validate(read(IR));review=read(REVIEW);require(review['commit']==PIN and review['ir_sha256']==sha(IR)and review['sources']==d['sources'],'Butterfly source review');inventory=read(ROOT/'compatibility/upstream-inventory.json')['files']
 for p,h in d['sources'].items():require(sha(ROOT/'upstream/MOTHER-Encore'/p)==h==inventory[p]['sha256'],'Changed butterfly source '+p)
 return d
def encode(d):
 validate(d);b=bytearray(64)
 def u(*v):b.extend(struct.pack('<'+'I'*len(v),*v))
 def f(*v):b.extend(struct.pack('<'+'f'*len(v),*v))
 def t(s):v=s.encode();u(len(v));b.extend(v)
 t(d['scene']);t(d['script']);u(d['modulus'],d['frames'],int(d['pixel_snap']));f(d['speed']);b.extend(struct.pack('<2d',d['fly_seek'],d['orbit_seek']));u(len(d['clips']))
 for c in d['clips']:
  u(c['role']);t(c['name']);f(c['length']);u(len(c['tracks']))
  for tr in c['tracks']:
   u(tr['role'],int(tr['discrete']),len(tr['keys']))
   for k in tr['keys']:f(k['time'],k['transition'],*k['value'])
 u(len(d['assets']));rc=read(receipt_path(ROOT/'romfs/graphics/objects/butterfly',ROOT));require(rc['commit']==PIN and rc['recipe_sha256']==sha(IR),'Butterfly genuine texture receipt')
 for a in d['assets']:
  raw=(ROOT/'romfs'/a['path']).read_bytes();require(hashlib.sha256(raw).hexdigest()==rc['outputs'][a['path']]['sha256'],'Butterfly real texture fingerprint');u(a['id'],a['width'],a['height'],len(raw),zlib.crc32(raw)&0xffffffff);t(a['source']);t(a['path'])
 u(len(d['bindings']))
 for h in d['bindings']:
  u(*[h[k]for k in ['id','ready_ordinal','timer_id','timer_ordinal','fly_id','fly_ordinal','orbit_id','orbit_ordinal','area_id','notifier_id','enabler_id','sprite_frame']],int(h['sprite_flip']),h['area_layer'],h['area_mask'],h['area_flags']);t(h['node']);f(h['parent_scale'],h['timer_wait'],h['fly_speed'],h['orbit_speed'],h['area_radius'])
  for k in ['position','parent_origin','sprite_position','sprite_offset','sprite_color','area_center','notifier','enabler']:f(*h[k])
 u(len(d['sources']))
 for p,h in d['sources'].items():t(p);b.extend(bytes.fromhex(h))
 u(len(d['connections']))
 for c in d['connections']:u(c['role']);t(c['signal']);t(c['method'])
 struct.pack_into('<8s6I20s12x',b,0,b'ENCFLY01',2,len(b),0,2,1,len(d['bindings']),bytes.fromhex(PIN));struct.pack_into('<I',b,16,zlib.crc32(b)&0xffffffff);return bytes(b)
def assets(tex):
 d=load()
 def convert(a):
  p=ROOT/'romfs'/a['path'];p.parent.mkdir(parents=True,exist_ok=True);subprocess.run([str(tex),'-f','rgba8','-z','none','-o',str(p),str(ROOT/'upstream/MOTHER-Encore'/a['source'])],check=True);return a['path'],dict(bytes=p.stat().st_size,sha256=sha(p))
 with ThreadPoolExecutor(max_workers=4)as pool:outputs=dict(pool.map(convert,d['assets']))
 write(receipt_path(ROOT/'romfs/graphics/objects/butterfly',ROOT),dict(schema=1,commit=PIN,recipe_sha256=sha(IR),tex3ds_sha256=sha(tex),outputs=outputs))
def stage_files(source):
 d=load();b=encode(d);require((Path(source)/'data/podunk-butterflies.encfly').read_bytes()==b,'Stale butterfly binary');out={Path('data/podunk-butterflies.encfly'):b}
 for a in d['assets']:
  p=Path(a['path']);v=(Path(source)/p).read_bytes();require(v==(ROOT/'romfs'/p).read_bytes(),'Stale butterfly texture');out[p]=v
 return out
def main():
 p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile','assets']);p.add_argument('--native',type=Path);p.add_argument('--source',type=Path);p.add_argument('--tex3ds',type=Path);a=p.parse_args()
 if a.action=='extract':
  require(a.native and a.source,'Actual complete Podunk source exports required');d=validate(build(*source_instances(a.native,a.source,ROOT/'upstream/MOTHER-Encore')));write(IR,d);write(REVIEW,dict(schema=1,commit=PIN,ir_sha256=sha(IR),sources=d['sources'],engine=d['engine'],semantics=d['semantics'],unsupported=d['unsupported']));print('Butterfly source:94 actual instances /5 source skins');return
 if a.action=='assets':require(a.tex3ds and a.tex3ds.is_file(),'Genuine tex3ds required');assets(a.tex3ds);return
 b=encode(load());PACK.parent.mkdir(parents=True,exist_ok=True);PACK.write_bytes(b);print('Butterfly checked resource:',len(b),'bytes')
if __name__=='__main__':
 try:main()
 except(ValueError,KeyError,OSError,TypeError,struct.error)as e:sys.exit('FIELD BUTTERFLY ERROR: '+str(e))
