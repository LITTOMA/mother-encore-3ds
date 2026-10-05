#!/usr/bin/env python3
"""Pinned Basic Enemy factories and state-machine facts. Offline JSON -> ENCFEN01."""
from __future__ import annotations
import argparse,hashlib,json,math,re,struct,sys,zlib
from pathlib import Path
import yaml
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from tools.podunk_scene import decode,stable,SCENE,PIN,read,write,sha,require
from tools.extract_battle_entry import Extractor,node,properties
IR=ROOT/'content/native-field-enemy.json'
OUTPUT=ROOT/'romfs/data/podunk-enemies.encenemy'
SPAWNER='Scripts/Main/Enemy Spawner.gd'
BASIC='Scripts/Main/Basic Enemy.gd'
FACTORY='Nodes/Overworld/Enemies/Basic Enemy.tscn'
PARAMETERS=('AppearanceModulus','WanderAxisModulus','ReadyDirectionMin','ReadyDirectionMax','ReadyTimerMin','WanderTimerDeviation','MinMovementLength','MinDisinterestTime','Knockback','KnockbackDeceleration','UnderlevelGap','ExplosionBase','ExplosionModulus','IncapacitatedDivisor','UnderlevelFlashLength','UnderlevelFlashInterval','UnderlevelFlashDelay','DamageFlashLength','StunLength','BashPower','BashVariance','AdvantageEnemy','AdvantageNeutral','AdvantagePlayer','SafeMargin')
FORMATS={1:None,2:'If',3:'5I8i12f',4:'6I9f',5:'II32s',6:'6I5f',7:'6If'}
HEADER=224

def source_instances(native,receipt,upstream):
 d=read(native);s=read(receipt);require(d['source']=='res://'+SCENE and d['schema']==1 and d['native_compatible'] is False,'Incomplete enemy native scene')
 require(s['commit']==PIN and s['scene']==SCENE,'Unreviewed enemy native receipt')
 inventory=read(ROOT/'compatibility/upstream-inventory.json')['files']
 for name,record in s['files'].items():require(sha(upstream/name)==record['sha256']==inventory[name]['sha256'],'Changed enemy closure '+name)
 names={n['path']:n for n in d['nodes']};require(len(names)==8686,'Incomplete Podunk enemy scene');require(decode(names['Objects']['world_transform'])==[[1,0],[0,1],[0,0]],'Basic Enemy local/global factory coordinates need new adapter')
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
   require(path in names,'Unresolved enemy binding '+path);bindings[path]=a['script']
 children={p:[] for p in names}
 for path in names:
  if path!='.':children[path.rsplit('/',1)[0] if '/' in path else '.'].append(path)
 ready=[]
 def visit(path):
  for child in children[path]:visit(child)
  ready.append(path)
 visit('.');ordinal={p:i for i,p in enumerate(ready)};out=[]
 for path in sorted(bindings,key=lambda p:ordinal[p]):
  if bindings[path]!=SPAWNER:continue
  out.append(dict(stable_id=stable(path),node=path,ready_ordinal=ordinal[path],overrides=overrides[path],native=names[path],children=[names[p] for p in children[path]]))
 require(len(out)==68,'Podunk spawner binding loss');return out,dict(native_export_sha256=sha(native),source_receipt_sha256=sha(receipt),source_files={name:r['sha256'] for name,r in s['files'].items()})

def build(records,provenance):
 ex=Extractor(ROOT);script=ex.text(BASIC);spawn=ex.text(SPAWNER);scene=ex.text(FACTORY);ex.text('Scripts/global/Enemy.gd');ex.text('Scripts/global/Character.gd');ex.text('Scripts/global/globalData.gd');ex.text('Scripts/UI/Battle/BattleSystem.gd');ex.text('Scripts/UI/cursor.gd')
 require('randi()%100 > _appearance_rate or _appearance_rate == 0 or _attached_enemy' in spawn and 'or uiManager.is_in_cutscene() or $Timer.time_left > 0' in spawn,'Spawner source gate order changed')
 require('_choose_movement_direction()\n\t\t\tstart_wander()' in script and 'if abs(global_position.x - new_pos.x) > difference or abs(global_position.y - new_pos.y) > difference:' in script,'Basic Enemy source state algorithm changed')
 def constant(name):return float(re.search(r'^const '+name+r' := ([\d.]+)',script,re.M)[1])
 defaults={}
 for name,value in re.findall(r'^export var ([A-Za-z_]+)(?:: int)? = (.*)$',script,re.M):
  if name in ('spriteOffset','shadow','returning','maxDistance','maxSpeed','acceleration','friction','walk_frequency'):defaults[name]=json.loads(value.lower())
 require(defaults.keys()=={'spriteOffset','shadow','returning','maxDistance','maxSpeed','acceleration','friction','walk_frequency'},'Missing Basic Enemy defaults')
 radius=float(re.search(r'^\[sub_resource type="CircleShape2D" id=157\]\nradius = ([\d.]+)',scene,re.M)[1])
 def duration(resource):
  body=re.search(r'^\[sub_resource type="Animation" id='+str(resource)+r'\]\n(.*?)(?=^\[)',scene,re.M|re.S)[1];m=re.search(r'^length = ([\d.]+)',body,re.M);return float(m[1]) if m else 1.0
 animations=[]
 for role,name,resource in ((1,'Flash',153),(2,'Stun',155),(3,'RESET',154)):
  require('anims/'+name+' = SubResource( '+str(resource)+' )' in scene,'Changed DamageAnimation binding')
  animations.append(dict(role=role,name=name,source=FACTORY,node='DamageAnimation',signal='animation_finished',handler='_on_DamageAnimation_animation_finished',duration=duration(resource)))
 require('yield($DamageAnimation,"animation_finished")' in script and 'if anim_name == "Stun"' in script and 'signal="animation_finished" from="DamageAnimation" to="." method="_on_DamageAnimation_animation_finished"' in scene,'Changed DamageAnimation coroutine/connection')
 bash=yaml.safe_load(ex.text('Data/BattleSkills/bash.yaml'))
 parameters=dict(AppearanceModulus=100,WanderAxisModulus=2,ReadyDirectionMin=-1,ReadyDirectionMax=1,ReadyTimerMin=.1,WanderTimerDeviation=.5,MinMovementLength=constant('MIN_MOVEMENT_LENGTH'),MinDisinterestTime=constant('MIN_DISINTEREST_TIME'),Knockback=constant('KNOCKBACK'),KnockbackDeceleration=constant('KNOCKBACK_DECELERATION'),UnderlevelGap=float(re.search(r'get_level\(\) \+ (\d+)',script)[1]),ExplosionBase=float(re.search(r'_do_damage\((\d+) \+ randi',script)[1]),ExplosionModulus=float(re.search(r'randi\(\) % (\d+), false',script)[1]),IncapacitatedDivisor=float(re.search(r'val/(\d+)\)',script)[1]),UnderlevelFlashLength=1,UnderlevelFlashInterval=.08,UnderlevelFlashDelay=.6,DamageFlashLength=duration(153),StunLength=duration(155),BashPower=bash['damage_or_heal'],BashVariance=bash['variance'],AdvantageEnemy=-1,AdvantageNeutral=0,AdvantagePlayer=1,SafeMargin=node(scene,'.')['collision/safe_margin'])
 require('round(rand_range(-1, 1))' in script and 'rand_range(0.1, walk_frequency)' in script and 'rand_range(walk_frequency - 0.5,walk_frequency + 0.5)' in script and 'flash(1, 0.08, 0.6, true)' in script,'Changed enemy RNG/flash lowering')
 require('var _direction = Vector2.ZERO' in script and not re.search(r'^var direction\b',script,re.M),'Enemy direction property guard requires semantic review')
 has_direction_property=False
 profiles=[];by_name={};spawners=[]
 for record in records:
  prop=decode(record['overrides']);enemy=prop.get('enemy','').replace(' ','');require(enemy and re.fullmatch('[a-zA-Z0-9_]+',enemy),'Unknown enemy identity')
  if enemy not in by_name:
   path='Data/Battlers/'+enemy+'.yaml';data=yaml.safe_load(ex.text(path));ov=data['ov'];require(ov.get('type','Basic Enemy')=='Basic Enemy','Unknown enemy factory type')
   allowed={'sprite','anim','spriteOffset','shadow','returning','maxDistance','maxSpeed','acceleration','friction','walk_frequency','walkFrequency','type','connections'};require(set(ov)<=allowed,'Unknown field enemy override')
   values={**defaults,**{k:v for k,v in ov.items() if k!='walkFrequency'}}
   sprite='Graphics/Character Sprites/Enemies/'+ov['sprite']+'.png';anim='Data/Animations/'+ov.get('anim','BasicEnemy')+'.yaml';ex.data(sprite);ex.text(anim)
   require(not ov.get('connections',[]),'Unknown enemy animation connection graph')
   profile=dict(id=len(profiles)+1,enemy=enemy,source=path,sprite=sprite,animation=anim,stats=[data[k] for k in ('level','hp','maxhp','pp','maxpp','defense','exp','cash')],sprite_offset=values['spriteOffset'],shadow=int(values['shadow']),returning=int(values['returning']),returning_override=('returning' in ov),max_distance=values['maxDistance'],max_speed=values['maxSpeed'],acceleration=values['acceleration'],friction=values['friction'],walk_frequency=values['walk_frequency'],wander_radius=radius,chase_delay=node(scene,'ChaseTimer')['wait_time'],return_delay=node(scene,'Timer')['wait_time'],ignored_source_spelling={'walkFrequency':ov['walkFrequency']} if 'walkFrequency' in ov else {})
   by_name[enemy]=len(profiles);profiles.append(profile)
  t=decode(record['native']['world_transform']);require(t[:2]==[[1,0],[0,1]],'Unsupported spawner transform')
  child={n['name']:n for n in record['children']};timer=decode(child['Timer']['properties']);require(timer['one_shot'] and not timer['autostart'],'Changed spawner Timer semantics')
  notifier=child['VisibilityNotifier2D'];nt=decode(notifier['world_transform']);np=decode(notifier['properties']);r,size=np['rect'];require(nt[0][1]==nt[1][0]==0,'Unsupported spawner notifier transform')
  profile=profiles[by_name[enemy]]
  effective_return=profile['returning'] if profile['returning_override'] else prop.get('_return',True)
  flags=int(effective_return)|(int(prop.get('_perma_death',False))<<1)|(int(has_direction_property)<<2)
  spawners.append(dict(id=record['stable_id'],node=record['node'],ready_ordinal=record['ready_ordinal'],profile=by_name[enemy],flags=flags,appearance_rate=prop.get('_appearance_rate',100),position=t[2],initial_direction=prop.get('_inital_direction',[0,0]),cooldown=timer['wait_time'],visibility_rect=[nt[2][0]+r[0]*nt[0][0],nt[2][1]+r[1]*nt[1][1],size[0]*nt[0][0],size[1]*nt[1][1]]))
 geometry=[]
 def add_geometry(role,mask,offset,rotation,vertices,layer=0,flags=0):
  for ordinal,v in enumerate(vertices):geometry.append(dict(id=len(geometry)+1,role=role,ordinal=ordinal,mask=mask,layer=layer,flags=flags,offset=offset,rotation=rotation,value=v))
 def polygon(path):
  body=re.search(r'^\[node name="'+path.rsplit('/',1)[-1]+r'"[^\n]*parent="'+path.rsplit('/',1)[0]+r'"\]\n(.*?)(?=^\[)',scene,re.M|re.S)[1]
  nums=[float(x) for x in re.search(r'polygon = PoolVector2Array\( ([^)]*)',body)[1].split(',')];return [[nums[i],nums[i+1]] for i in range(0,len(nums),2)]
 # Numeric native geometry, not source script evaluation at runtime.
 body=re.search(r'^\[node name="CollisionShape2D" type="CollisionPolygon2D" parent="\."\]\n(.*?)(?=^\[)',scene,re.M|re.S)[1];nums=[float(x) for x in re.search(r'polygon = PoolVector2Array\( ([^)]*)',body)[1].split(',')]
 add_geometry(1,node(scene,'.')['collision_mask'],[float(x) for x in re.search(r'position = Vector2\( ([^)]*)',body)[1].split(',')],0,[[nums[i],nums[i+1]] for i in range(0,len(nums),2)],node(scene,'.')['collision_layer'])
 for role,part in ((2,'ViewArea'),(3,'BlindSpot')):
  body=re.search(r'^\[node name="CollisionPolygon2D"[^\n]*parent="RayCast2D/'+part+r'"\]\n(.*?)(?=^\[)',scene,re.M|re.S)[1];nums=[float(x) for x in re.search(r'polygon = PoolVector2Array\( ([^)]*)',body)[1].split(',')];offset=[float(x) for x in re.search(r'position = Vector2\( ([^)]*)',body)[1].split(',')];parent=node(scene,'RayCast2D/'+part);angle=parent['rotation'];offset=[offset[0]*math.cos(angle)-offset[1]*math.sin(angle),offset[0]*math.sin(angle)+offset[1]*math.cos(angle)];rotation=float(re.search(r'rotation = ([^\n]*)',body)[1])+angle
  add_geometry(role,0,offset,rotation,[[nums[i],nums[i+1]] for i in range(0,len(nums),2)],parent['collision_layer'],1)
 def extent(resource):return [float(x) for x in re.search(r'^\[sub_resource type="RectangleShape2D" id='+str(resource)+r'\]\nextents = Vector2\( ([^)]*)',scene,re.M)[1].split(',')]
 add_geometry(4,1,[0,3],0,[extent(151)],node(scene,'interact')['collision_layer'],1);add_geometry(5,62,[0,.5],0,[extent(156)]);add_geometry(6,0,[0,-2],0,[[radius,0]])
 add_geometry(7,node(scene,'EventDetector')['collision_mask'],node(scene,'EventDetector')['position'],0,[node(scene,'EventDetector')['cast_to']]);add_geometry(8,node(scene,'RayCast2D')['collision_mask'],[0,0],0,[node(scene,'RayCast2D')['cast_to']]);add_geometry(9,0,[0,0],0,[[-40,-50],[80,80]])
 ex.text('Nodes/Overworld/Enemies/Enemy Spawner.tscn');ex.text(SCENE)
 return dict(schema=1,capabilities=2,kind='encore.field-enemy.source-ir',commit=PIN,scene=SCENE,scene_id=stable('.'),sources={**provenance['source_files'],**ex.sources},provenance=provenance,parameters=parameters,profiles=profiles,spawners=spawners,geometry=geometry,animations=animations,host_requirements=['Godot move_and_slide with two sequential velocity/knockback passes','EventDetector player hit and Wander raycast collider identity','PartyObject contact identity, current player flags and actual roster midpoint','OnScreenEnemy battle ordering, HP/PP preservation and source advantage','Source sprite animation/tween/flash feedback','Typed DamageAnimation completion with playback generation and source idle clock','Deferred tree exit cancels coroutine subscriptions after same-frame signal continuations'],semantics=['Spawner randi precedes every appearance/attached/cutscene/cooldown gate','Dynamic factory Ready: direction X then Y rand_range; optional initial timer draw; source initial direction applied after Ready','Wander timeout calls choose then start_wander (which chooses a second time)','RETURN source comparator intentionally preserved (greater distance starts wander)','camel walkFrequency is an exact reviewed nonexistent-property set; walk_frequency retains source default','Concurrent Hurt awaits any next completed DamageAnimation, including Stun; connected Stun handler precedes all coroutine subscriptions','queue_free does not cancel same-frame continuations; actual tree exit cancels outstanding awaits without executing them','Source typed int damage assignment truncates variance before round; final incapacitated val/4 uses integer division','Basic Enemy exposes _direction but not direction; the factory initial-direction property guard is false','Enemy ov.returning overrides factory _return during child Ready'],unsupported=[],unverified=['Manual tests retained but not run','Native full Podunk scene activation remains fail-closed until all script capabilities and real collision/battle hosts are admitted'])

def validate(ir):
 require(ir['schema']==1 and ir['capabilities']==2 and ir['commit']==PIN and ir['kind']=='encore.field-enemy.source-ir' and ir['scene_id']==stable('.'),'Enemy IR identity')
 require(set(ir['parameters'])==set(PARAMETERS) and len(ir['profiles'])==14 and len(ir['spawners'])==68 and len(ir['geometry'])==23,'Enemy IR topology')
 require(all(type(v) in (int,float) and math.isfinite(v) and abs(v)<1000000 for v in ir['parameters'].values()),'Enemy parameter values')
 for i,g in enumerate(ir['geometry']):
  require(g['id']==i+1 and 1<=g['role']<=9 and type(g['ordinal']) is int and g['ordinal']>=0 and 0<=g['mask']<=65535 and 0<=g['layer']<=65535 and g['flags'] in (0,1) and all(math.isfinite(v) and abs(v)<=1000000 for v in g['offset']+[g['rotation']]+g['value']),'Enemy geometry schema')
 require(len(ir['animations'])==3,'Enemy animation capability topology')
 for role,a in enumerate(ir['animations'],1):
  require(a['role']==role and a['name']==('Flash','Stun','RESET')[role-1] and a['source']==FACTORY and a['source'] in ir['sources'] and a['node']=='DamageAnimation' and a['signal']=='animation_finished' and a['handler']=='_on_DamageAnimation_animation_finished' and math.isfinite(a['duration']) and 0<a['duration']<=86400,'Enemy animation binding')
 require(ir['animations'][0]['duration']==ir['parameters']['DamageFlashLength'] and ir['animations'][1]['duration']==ir['parameters']['StunLength'],'Enemy animation parameter mismatch')
 ids=set();ordinals=set()
 for i,p in enumerate(ir['profiles']):
  require(p['id']==i+1 and len(p['stats'])==8 and all(type(v) is int and 0<=v<=1000000 for v in p['stats']) and 0<p['max_speed']<=1024 and 0<p['wander_radius']<=1024 and 0<=p['walk_frequency']<=100,'Enemy profile policy')
 for s in ir['spawners']:
  require(s['id']==stable(s['node']) and s['id'] not in ids and s['ready_ordinal'] not in ordinals and 0<=s['profile']<len(ir['profiles']) and 0<=s['flags']<=7 and not(s['flags']&4) and type(s['appearance_rate']) is int and 0<=s['appearance_rate']<=100 and 0<s['cooldown']<=86400,'Enemy spawner identity/policy');ids.add(s['id']);ordinals.add(s['ready_ordinal'])
  require(all(math.isfinite(x) and abs(x)<=1000000 for x in s['position']+s['initial_direction']+s['visibility_rect']),'Enemy spatial value')

def pack(ir):
 validate(ir);strings=bytearray(b'\0');offsets={'':0}
 def string(s):
  require(type(s) is str and '\0' not in s and len(s.encode())<4096,'Enemy string')
  if s not in offsets:offsets[s]=len(strings);strings.extend(s.encode()+b'\0')
  return offsets[s]
 rows={2:[],3:[],4:[],5:[],6:[],7:[]}
 for i,name in enumerate(PARAMETERS):rows[2].append(struct.pack('<If',i+1,ir['parameters'][name]))
 for p in ir['profiles']:
  rows[3].append(struct.pack('<'+FORMATS[3],p['id'],string(p['enemy']),string(p['source']),string(p['sprite']),string(p['animation']),*p['stats'],*p['sprite_offset'],p['shadow'],p['returning'],p['max_distance'],p['max_speed'],p['acceleration'],p['friction'],p['walk_frequency'],p['wander_radius'],p['chase_delay'],p['return_delay']))
 for s in ir['spawners']:rows[4].append(struct.pack('<'+FORMATS[4],s['id'],string(s['node']),s['ready_ordinal'],s['profile'],s['flags'],s['appearance_rate'],*s['position'],*s['initial_direction'],s['cooldown'],*s['visibility_rect']))
 for i,(path,h) in enumerate(sorted(ir['sources'].items())):rows[5].append(struct.pack('<II32s',i+1,string(path),bytes.fromhex(h)))
 for g in ir['geometry']:rows[6].append(struct.pack('<6I5f',g['id'],g['role'],g['ordinal'],g['mask'],g['layer'],g['flags'],*g['offset'],g['rotation'],*g['value']))
 for a in ir['animations']:rows[7].append(struct.pack('<6If',a['role'],string(a['name']),string(a['source']),string(a['node']),string(a['signal']),string(a['handler']),a['duration']))
 sections={1:bytes(strings),**{k:b''.join(v) for k,v in rows.items()}};out=bytearray(HEADER)
 for k,payload in sections.items():
  offset=len(out);out.extend(payload);stride=1 if k==1 else struct.calcsize('<'+FORMATS[k]);struct.pack_into('<4I',out,80+(k-1)*16,k,offset,len(payload)//stride,stride)
 struct.pack_into('<8s6I20s',out,0,b'ENCFEN01',1,len(out),0,2,1,7,bytes.fromhex(PIN));struct.pack_into('<I',out,52,ir['scene_id']);struct.pack_into('<I',out,16,zlib.crc32(out));return bytes(out)

def main():
 p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile','verify']);p.add_argument('--native',type=Path);p.add_argument('--source',type=Path);p.add_argument('--ir',type=Path,default=IR);p.add_argument('--output',type=Path,default=OUTPUT);a=p.parse_args()
 if a.action=='extract':
  require(a.native and a.source,'Enemy extraction requires official full native/source exports');records,provenance=source_instances(a.native,a.source,ROOT/'upstream/MOTHER-Encore');ir=build(records,provenance);validate(ir);write(a.ir,ir)
 else:
  ir=read(a.ir);validate(ir);inventory=read(ROOT/'compatibility/upstream-inventory.json')['files']
  for source,h in ir['sources'].items():require(sha(ROOT/'upstream/MOTHER-Encore'/source)==h==inventory[source]['sha256'],'Changed enemy source '+source)
  payload=pack(ir)
  if a.action=='compile':a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_bytes(payload)
  else:require(a.output.read_bytes()==payload,'Stale enemy binary')
  print('Field Enemy:',len(payload),'bytes;',len(ir['spawners']),'dynamic spawners;',len(ir['profiles']),'source profiles')
if __name__=='__main__':
 try:main()
 except (ValueError,KeyError,OSError,struct.error) as e:sys.exit('FIELD ENEMY ERROR: '+str(e))
