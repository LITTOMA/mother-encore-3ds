#!/usr/bin/env python3
"""All54 source Podunk birds, exact native animations and genuine atlas assets."""
from __future__ import annotations
import argparse,hashlib,re,struct,subprocess,sys,zlib
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import PIN,SCENE,read,write,decode,stable,sha,require
SCRIPT='Scripts/misc/birds.gd';PROTOTYPE='Nodes/Overworld/birds.tscn';IR=ROOT/'content/podunk-birds.json';REVIEW=ROOT/'compatibility/reviews/podunk-birds-v0410.json';ASSETS=ROOT/'content/podunk-birds-assets.json';OUT=ROOT/'romfs/data/podunk.encbirds'

def extract(native):
 d=read(native);g=read(ROOT/'content/podunk-scene.json');inv=read(ROOT/'compatibility/upstream-inventory.json')['files'];nm={n['path']:n for n in d['nodes']};rs={r['id']:r for r in d['resources']};sources=dict(g['sources']);require(d['source']=='res://'+SCENE and len(nm)==8686 and sha(native)==g['export_sha256'],'Birds requires complete official native export')
 text=(ROOT/'upstream/MOTHER-Encore'/SCRIPT).read_text(encoding='utf8');methods=re.findall(r'^func ([A-Za-z_][A-Za-z0-9_]*)\(',text,re.M)
 require(methods==['_ready','_prepare','_process','_on_Area_body_entered','_movement','_on_AnimationPlayer_animation_finished','_on_VisibilityNotifier2D_screen_exited','_on_VisibilityNotifier2D_screen_entered'],'Unknown Birds source method')
 for fact in ['set_process(false)','if !Engine.editor_hint:','_movement()','!$VisibilityNotifier2D.is_on_screen() and $Timer.time_left == 0','velocity = move_and_slide(velocity)','if anim_name == "hop" or "hop right":','position = start_pos','_ready()','if $Timer.time_left == 0:','body.position.x <= position.x','body.position.x > position.x']:require(fact in text,'Unknown Birds source '+fact)
 prepare=re.search(r'func _prepare\(\):([\s\S]*?)(?=\nfunc )',text)[1];require(prepare.index('randi()')<prepare.index('randf()')<prepare.index('randi()',prepare.index('randf()'))<prepare.index('speed ='),'Birds RNG order changed')
 skin=int(re.search(r'var2str\(randi\(\)%(\d+)\+0\)',text)[1]);facing=int(re.search(r'\(randi\(\)%(\d+)\+0\) == (\d+)',text)[1]);facing_value=int(re.search(r'\(randi\(\)%(\d+)\+0\) == (\d+)',text)[2]);base,mod=re.search(r'speed = (\d+) \+ \(randi\(\)%(\d+)\)',text).groups();seek=float(re.search(r'seek\(\(randf\(\)\*([0-9.]+)\)',text)[1]);flight_z=int(re.search(r'self.z_index = (-?\d+)',text)[1]);fy=float(re.search(r'inputVector.y = (-?\d+)',text)[1]);fx=[float(v) for v in re.findall(r'inputVector.x = (-?\d+)',text)];require(len(fx)==2,'Bird flight source branches changed')
 raw=(ROOT/'upstream/MOTHER-Encore'/PROTOTYPE).read_text(encoding='utf8');signals=re.findall(r'^\[connection ([^\n]+)\]',raw,re.M);expected=['signal="body_entered" from="Area" to="." method="_on_Area_body_entered"','signal="animation_finished" from="AnimationPlayer" to="." method="_on_AnimationPlayer_animation_finished"','signal="screen_entered" from="VisibilityNotifier2D" to="." method="_on_VisibilityNotifier2D_screen_entered"','signal="screen_exited" from="VisibilityNotifier2D" to="." method="_on_VisibilityNotifier2D_screen_exited"'];require(signals==expected,'Bird signal connections differ')
 skins=[]
 for i in range(skin):
  path='Graphics/Character Sprites/Npcs/misc/birds/'+str(i)+'.png';png=(ROOT/'upstream/MOTHER-Encore'/path).read_bytes();require(png[:8]==b'\x89PNG\r\n\x1a\n','Birds source texture requires PNG');w,h=struct.unpack('>II',png[16:24]);skins.append(dict(index=i,source=path,path='graphics/objects/birds/'+str(i)+'.t3x',width=w,height=h));sources[path]=inv[path]['sha256'];sources[path+'.import']=inv[path+'.import']['sha256'];imp=(ROOT/'upstream/MOTHER-Encore'/(path+'.import')).read_text();require(all(x in imp for x in ['flags/filter=false','flags/mipmaps=false','process/premult_alpha=false']),'Birds source import differs')
 profiles=[];records=[];paths=['Sprite','Area','Area/CollisionShape2D','CollisionShape2D','AnimationPlayer','VisibilityNotifier2D','Timer'];ordinal={n['node']:n['ready_ordinal'] for n in g['pending']};children={p:[] for p in nm}
 for p in nm:
  if p!='.':children[p.rsplit('/',1)[0] if '/' in p else '.'].append(p)
 order=[]
 def visit(p):
  for c in children[p]:visit(c)
  order.append(p)
 visit('.');native_ordinal={p:i for i,p in enumerate(order)};roles=['Idle','Fly','hop','hop right','RESET','prepare'];property_role={'Sprite:frame':1,'Sprite:offset':2,'.:z_index':3}
 for b in g['pending']:
  if b['script']!=SCRIPT:continue
  p=b['node'];v=decode(nm[p]['properties']);parent=p.rsplit('/',1)[0];pt=decode(nm[parent]['world_transform']);root_world=decode(nm[p]['world_transform']);props={q:decode(nm[p+'/'+q]['properties']) for q in paths};s=props['Sprite'];a=props['AnimationPlayer'];area=props['Area'];shape=props['Area/CollisionShape2D'];body=props['CollisionShape2D'];timer=props['Timer'];notifier=props['VisibilityNotifier2D']
  require(v['rotation']==0 and v['scale']==[1,1] and pt[0][1]==pt[1][0]==0 and pt[0][0]==pt[1][1]>0 and body['disabled'] and not v['motion/sync_to_physics'],'Birds colliding/rotated body needs new capability')
  require(s['hframes']==s['vframes']==3 and s['centered'] and not s['region_enabled'] and not s['flip_v'] and s['normal_map'] is None and s['rotation']==0 and s['scale']==[1,1] and s['material'] is None and not s['use_parent_material'],'Bird source Sprite geometry differs')
  require(timer['one_shot'] and not timer['autostart'] and timer['process_mode']==1 and not timer.get('paused',False) and a['playback_process_mode']==1 and a['autoplay']=='' and a['playback_default_blend_time']==0 and a['blend_times']==[],'Bird source native leaves differ')
  clips=[]
  for role,name in enumerate(roles,1):
   q=decode(rs[a['anims/'+name]['id']]['properties']);tracks=[];i=0
   while 'tracks/'+str(i)+'/type' in q:
    prefix='tracks/'+str(i)+'/';k=dict(q[prefix+'keys']['pairs']);target=q[prefix+'path']['value'];require(q[prefix+'type']=='value' and target in property_role and q[prefix+'enabled'] and q[prefix+'interp']==1 and q[prefix+'loop_wrap'] and k['update'] in [0,1] and len(k['times'])==len(k['transitions'])==len(k['values']),'Bird unknown native track')
    tracks.append(dict(property=property_role[target],update=k['update'],keys=[dict(time=t,transition=c,value=value if property_role[target]==2 else [value,0]) for t,c,value in zip(k['times'],k['transitions'],k['values'])]));i+=1
   clips.append(dict(role=role,name=name,length=q['length'],loop=q['loop'],tracks=tracks))
  if clips not in profiles:profiles.append(clips)
  circle=rs[shape['shape']['id']];body_circle=rs[body['shape']['id']];require(circle['class']==body_circle['class']=='CircleShape2D' and shape['rotation']==body['rotation']==0 and shape['scale'][0]==shape['scale'][1]>0,'Bird circle source geometry differs');require(area['position']==[0,0] and area['rotation']==0 and area['scale']==[1,1],'Bird Area parent transform differs')
  for image in skins:require(image['width']%s['hframes']==0 and image['height']%s['vframes']==0,'Bird source sheet frames differ')
  z=decode(nm[p+'/VisibilityNotifier2D']['world_transform']);rect=notifier['rect'];require(z[0][1]==z[1][0]==0 and z[0][0]>0 and z[1][1]>0,'Bird notifier transform unsupported')
  color=lambda x:[x[k] for k in ['r','g','b','a']]
  records.append(dict(id=b['stable_id'],ready=b['ready_ordinal'],node=p,parent_id=stable(parent),profile=profiles.index(clips),children=[stable(p+'/'+q) for q in paths],animation_ready=native_ordinal[p+'/AnimationPlayer'],timer_ready=native_ordinal[p+'/Timer'],position=v['position'],parent=pt,sprite_position=s['position'],sprite_offset=s['offset'],sprite_frame=s['frame'],flags=sum(int(x)<<i for i,x in enumerate([s['flip_h'],s['centered'],s['visible'],v['visible'],v['z_as_relative'],body['disabled'],area['monitoring'],area['monitorable'],shape['disabled']])),initial_z=v['z_index'],body_layer=v['collision_layer'],body_mask=v['collision_mask'],body_radius=decode(body_circle['properties'])['radius'],safe_margin=v['collision/safe_margin'],area_layer=area['collision_layer'],area_mask=area['collision_mask'],area_center=shape['position'],area_radius=decode(circle['properties'])['radius']*shape['scale'][0],notifier=rect[0]+rect[1],notifier_position=notifier['position'],notifier_scale=notifier['scale'],timer_wait=timer['wait_time'],animation_speed=a['playback_speed'],root_pause=v['pause_mode'],animation_pause=a['pause_mode'],timer_pause=timer['pause_mode'],root_priority=v['process_priority'],animation_priority=a['process_priority'],timer_priority=timer['process_priority'],modulate=color(v['modulate']),self_modulate=color(v['self_modulate']),sprite_modulate=color(s['modulate']),sprite_self_modulate=color(s['self_modulate'])))
 require(len(records)==54 and profiles,'Bird full54 source roster differs');records.sort(key=lambda n:n['ready'])
 require('2d/snapping/use_gpu_pixel_snap=true' in (ROOT/'upstream/MOTHER-Encore/project.godot').read_text(),'Birds unreviewed pixel snapping');
 for p in [SCRIPT,PROTOTYPE,'LICENSE','project.godot']:sources[p]=inv[p]['sha256']
 for p,h in sources.items():require(h==inv[p]['sha256']==sha(ROOT/'upstream/MOTHER-Encore'/p),'Changed Bird source '+p)
 write(IR,dict(schema=1,kind='encore.field-birds.source-ir',commit=PIN,scene=SCENE,scene_id=stable('.'),source_sha256=sources[SCENE],scene_admitted=False,native_sha256=sha(native),sources=sources,rules=dict(skin_mod=skin,facing_mod=facing,facing_value=facing_value,speed_base=float(base),speed_mod=int(mod),seek_factor=seek,flight_z=flight_z,right=[fx[0],fy],left=[fx[1],fy],finished_first_always=True),profiles=profiles,skins=skins,records=records,columns=s['hframes'],rows=s['vframes'],pixel_snap=True,source_signals=signals,pending=['Actual live geometry, source4 signals, borrowed global player identity/local position, notifier visibility and scene lifetime required','Source Area layer4096/mask0/monitorablefalse retained; no invented distance/proximity trigger','Unknown native animation listeners/queues/blends and active Kinematic shape are rejected','Full Podunk activation remains pending other source consumers']))

def load():
 d=read(IR);r=read(REVIEW);inv=read(ROOT/'compatibility/upstream-inventory.json')['files'];require(d['schema']==1 and d['commit']==PIN and d['scene']==SCENE and not d['scene_admitted'] and r['commit']==PIN and r['ir_sha256']==sha(IR),'Bird semantic review absent/stale')
 for p,h in d['sources'].items():require(h==inv[p]['sha256']==sha(ROOT/'upstream/MOTHER-Encore'/p),'Changed bird source '+p)
 return d

def assets(d,tex3ds=None):
 if tex3ds:
  def convert(a):
   target=ROOT/'romfs'/a['path'];target.parent.mkdir(parents=True,exist_ok=True);subprocess.run([str(tex3ds),'-f','rgba8','-z','none','-o',str(target),str(ROOT/'upstream/MOTHER-Encore'/a['source'])],check=True)
  with ThreadPoolExecutor(max_workers=4)as pool:list(pool.map(convert,d['skins']))
 result=[dict(path=a['path'],bytes=(ROOT/'romfs'/a['path']).stat().st_size,source_sha256=d['sources'][a['source']],output_sha256=sha(ROOT/'romfs'/a['path']),ir_sha256=sha(IR),producer_sha256=sha(Path(__file__)),format='rgba8',compression='none',tex3ds_sha256=sha(tex3ds) if tex3ds else None)for a in d['skins']]
 if not tex3ds:
  previous=read(ASSETS);require(len(previous)==len(result),'Bird conversion receipt roster differs')
  for a,p in zip(result,previous):a['tex3ds_sha256']=p.get('tex3ds_sha256');require(re.fullmatch('[0-9a-f]{64}',a['tex3ds_sha256'] or '') and a==p,'Bird genuine conversion receipt differs')
 return result

def encode(d,receipts):
 b=bytearray(128)
 def u(*v):b.extend(struct.pack('<'+'I'*len(v),*v))
 def i(*v):b.extend(struct.pack('<'+'i'*len(v),*v))
 def f(*v):b.extend(struct.pack('<'+'f'*len(v),*v))
 def t(s):v=s.encode();u(len(v));b.extend(v)
 t(d['scene']);t(SCRIPT);r=d['rules'];u(r['skin_mod'],r['facing_mod'],r['facing_value'],r['speed_mod'],int(r['finished_first_always']));f(r['speed_base'],r['seek_factor'],*r['right'],*r['left']);i(r['flight_z']);u(d['columns'],d['rows'],int(d['pixel_snap']),len(d['profiles']))
 for profile in d['profiles']:
  u(len(profile))
  for a in profile:
   u(a['role']);t(a['name']);f(a['length']);u(int(a['loop']),len(a['tracks']))
   for tr in a['tracks']:
    u(tr['property'],tr['update'],len(tr['keys']))
    for k in tr['keys']:f(k['time'],k['transition'],*k['value'])
 u(len(d['skins']))
 for a,r in zip(d['skins'],receipts):u(a['index'],a['width'],a['height']);t(a['source']);t(a['path']);b.extend(bytes.fromhex(r['source_sha256']+r['output_sha256']))
 for n in d['records']:
  u(n['id'],n['ready'],n['parent_id'],n['profile'],*n['children'],n['animation_ready'],n['timer_ready'],n['sprite_frame'],n['flags'],n['body_layer'],n['body_mask'],n['area_layer'],n['area_mask'],n['root_pause'],n['animation_pause'],n['timer_pause']);i(n['initial_z'],n['root_priority'],n['animation_priority'],n['timer_priority']);f(n['body_radius'],n['safe_margin'],n['area_radius'],n['timer_wait'],n['animation_speed'],*n['position'],*[v for row in n['parent'] for v in row],*n['sprite_position'],*n['sprite_offset'],*n['area_center'],*n['notifier'],*n['notifier_position'],*n['notifier_scale'],*n['modulate'],*n['self_modulate'],*n['sprite_modulate'],*n['sprite_self_modulate']);t(n['node'])
 u(len(d['sources']))
 for p,h in d['sources'].items():t(p);b.extend(bytes.fromhex(h))
 struct.pack_into('<8s8I',b,0,b'ENCFBRD1',1,128,len(b),0,0x454e0022,3,len(d['records']),d['scene_id']);b[40:60]=bytes.fromhex(PIN);b[60:92]=bytes.fromhex(d['source_sha256']);b[92:124]=hashlib.sha256(IR.read_bytes()).digest();struct.pack_into('<I',b,20,zlib.crc32(b[128:]));return bytes(b)

def stage_files(source):
 d=load();r=assets(d);raw=encode(d,r);source=Path(source);require((source/'data/podunk.encbirds').read_bytes()==raw,'Bird stagedpack differs');out={Path('data/podunk.encbirds'):raw}
 for a,e in zip(d['skins'],r):p=source/a['path'];require(p.stat().st_size==e['bytes'] and sha(p)==e['output_sha256'],'Bird stagedatlas differs');out[Path(a['path'])]=p.read_bytes()
 return out

def main():
 p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile','verify']);p.add_argument('--native',type=Path);p.add_argument('--tex3ds',type=Path);a=p.parse_args()
 if a.action=='extract':require(a.native,'Explicit complete native export required');extract(a.native);return
 d=load();r=assets(d,a.tex3ds);raw=encode(d,r)
 if a.action=='verify':require(read(ASSETS)==r and OUT.read_bytes()==raw,'Bird assets/data differ')
 else:write(ASSETS,r);OUT.parent.mkdir(parents=True,exist_ok=True);OUT.write_bytes(raw)
 print('Bird complete source:',len(d['records']),'Ready;',len(d['profiles']),'native profiles;',len(d['skins']),'real skins;',len(raw),'bytes; scene admitted=False')
if __name__=='__main__':
 try:main()
 except(ValueError,KeyError,TypeError,OSError,struct.error,subprocess.SubprocessError)as e:sys.exit('FIELD BIRDS ERROR: '+str(e))
