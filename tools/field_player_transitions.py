#!/usr/bin/env python3
"""Actual 14 Podunk JumpArea and two Stairs; typed source transitions only."""
from __future__ import annotations
import argparse,json,re,struct,sys,zlib,math
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import PIN,SCENE,stable,read,sha,require,decode
from tools.extract_battle_entry import Extractor
IR=ROOT/'content/native-field-player-transitions.json';REVIEW=ROOT/'reports/field-player-transitions/source-review.json';PACK=ROOT/'romfs/data/podunk-player-transitions.encplayer'
JUMP='Scripts/Main/Jump Area.gd';STAIRS='Scripts/Main/Stairs.gd'
def write(p,d):
 p.parent.mkdir(parents=True,exist_ok=True)
 with p.open('w',encoding='utf-8',newline='\n')as f:json.dump(d,f,ensure_ascii=False,indent=2);f.write('\n')
def extract(native,receipt):
 d=read(native);rc=read(receipt);g=read(ROOT/'content/podunk-scene.json');require(d['source']=='res://'+SCENE and rc['commit']==PIN and rc['scene']==SCENE and g['export_sha256']==sha(native),'Changed actual Podunk source export')
 nm={n['path']:n for n in d['nodes']};rs={r['id']:r for r in d['resources']};ss={s['source'][6:]:s for s in d['scene_states']};require(len(nm)==8686,'Incomplete transition source scene');roots=[]
 def visit(root,f):
  roots.append((root,f))
  for n in ss[f]['nodes']:
   if n['instance']:
    p=n['path'].removeprefix('./');visit(p if root=='.' else root+'/'+p,rs[n['instance']['id']]['path'][6:])
 visit('.',SCENE);ov={}
 for root,f in sorted(roots,key=lambda x:x[0].count('/')if x[0]!='.'else-1,reverse=True):
  for n in ss[f]['nodes']:
   p=n['path'].removeprefix('./');p=root if p=='.' else p if root=='.' else root+'/'+p
   if p in nm:ov.setdefault(p,{}).update(decode(n['properties']))
 ex=Extractor(ROOT)
 for p,r in rc['files'].items():ex.data(p);require(ex.sources[p]==r['sha256'],'Changed transition source closure '+p)
 jt=ex.text(JUMP);st=ex.text(STAIRS);pt=ex.text('Scripts/Main/party/Player.gd');ft=ex.text('Scripts/Main/party/party_follower.gd');po=ex.text('Scripts/Main/party/party_object.gd');ct=ex.text('Scripts/Main/Switches/ControlledTwoStatesObject.gd');ex.text('Scripts/Main/Switches/ControlledObject.gd');ex.text('Scripts/Main/Camera2D.gd');ex.text('Scripts/global/global.gd');ex.text('Scripts/global/uiManager.gd');ex.text('Nodes/Overworld/jump area.tscn');ex.text('Nodes/Reusables/stairs.tscn');ex.text('LICENSE')
 require(re.findall(r'^func (\w+)\(',jt,re.M)==['_ready','_input','_process','_start_jump_process','_jump','_on_Jump_Area_body_entered','_on_Jump_Area_body_exited','_on_Close_body_entered','_on_Close_body_exited','_has_skill','_are_prompts_enabled','_can_jump','update_state','_get_ray_to_player','_update'],'Unknown JumpArea method')
 require(re.findall(r'^func (\w+)\(',st,re.M)==['_ready','_physics_process','_on_Stairs_area_entered','_on_Stairs_area_exited'],'Unknown Stairs method')
 require('Tween.TRANS_QUART' in pt and 'length*0.75' in pt and 'set_delay(0.02)' in pt and 'set_delay(length)' in ft and 'Tween.TRANS_QUAD' in ft,'Unreviewed party jump curves')
 require('jumper.jump(jump_height, 0.3, true)'in jt and '.set_ease(Tween.EASE_IN).set_delay(0.1)'in jt and '_step_distance += _input_vector.x'in st and 'global.partySpace[0].x += _input_vector.y * _direction'in st,'Changed source jump/stair execution order')
 def num(pattern,text):return float(re.search(pattern,text)[1])
 # Numeric parameters retain the actual source rather than content constants in C++.
 params=[num(r'TWEEN_TIME := ([\d.]+)',jt),*[-float(v)for v in re.search(r'ARROW_FURTHER_OFFSET := - Vector2\(([^)]+)\)',jt)[1].split(',')],num(r'jumper.jump\(jump_height, ([\d.]+)',jt),num(r'position, ([\d.]+)\)',jt),num(r'set_delay\(([\d.]+)\)',jt),num(r'create_timer\(([\d.]+)\), "timeout"',jt),num(r'Vector2\(0, -7\), ([\d.]+)\)',jt),num(r'return_camera\(([\d.]+)\)',jt),0,num(r'Vector2\(0, (-[\d.]+)\)',jt),num(r'Vector2\(([\d.]+), 1.5\)',jt),num(r'create_timer\(([\d.]+)\),"timeout"',jt),num(r'start_crouch_time := ([\d.]+)',pt),num(r'set_delay\(([\d.]+)\)',pt),num(r'length\*([\d.]+)',pt),*map(float,re.search(r'from\(Vector2\(([^)]+)\)',pt)[1].split(',')),*map(float,re.search(r'start_joy_vibration\(([^)]+)\)',jt)[1].split(','))]
 require(len(params)==22 and params[4]>0 and params[6]>0,'Invalid transition timing source')
 area=[];records=[]
 def geometry(root,child,shape):
  path=root+('/'+child if child else '');v=decode(nm[path]['properties']);sp=path+'/'+shape;q=decode(nm[sp]['properties']);world=decode(nm[sp]['world_transform']);require(world[:2]==[[1,0],[0,1]],'Transition shape requires nonidentity checked transform')
  values=[];kind=0
  if 'shape'in q and q['shape']:
   res=rs[q['shape']['id']];vshape=decode(res['properties'])
   if res['class']=='RectangleShape2D':kind=1;values=vshape['extents']
   elif res['class']=='CircleShape2D':kind=2;values=[vshape['radius']]
   else:raise ValueError('Unreviewed transition shape '+res['class'])
  elif 'polygon'in q:
   kind=3;values=q['polygon']['values'] if isinstance(q['polygon'],dict)else q['polygon'];values=[v for pair in values for v in pair]if values and isinstance(values[0],list)else values
  require(kind and not q.get('disabled',False) and v['monitoring'] and v['monitorable'],'Unreviewed transition shape monitoring')
  a=dict(id=stable(path),shape_id=stable(sp),layer=v['collision_layer'],mask=v['collision_mask'],kind=kind,position=world[2],values=values);area.append(a);return len(area)-1
 for b in sorted((b for b in g['pending']if b['script']in(JUMP,STAIRS)),key=lambda x:x['ready_ordinal']):
  p=b['node'];v=ov[p];world=decode(nm[p]['world_transform']);require(world[:2]==[[1,0],[0,1]],'Transition root basis unsupported');isjump=b['script']==JUMP
  record=dict(id=b['stable_id'],ready=b['ready_ordinal'],kind=1 if isjump else 2,node=p,position=world[2],camera_id=stable(p+'/Camera2D')if isjump else 0,areas=[],height=0,flags=0,step_length=0,points=[],sprite=[],ray=[],tracks=[])
  if isjump:
   require(not v.get('_is_synced_with_room',False)and v.get('_initially_on',True)and not v.get('_state_anim_player',''),'Unreviewed jump controlled switches');turn=v.get('player_turn',{'x':True,'y':True});turn=dict(turn['pairs'])if 'pairs'in turn else turn;require(set(turn)=={'x','y'}and all(type(x)is bool for x in turn.values()),'Unknown jump direction mask');record['flags']=int(turn['x'])|int(turn['y'])<<1|4;record['height']=v.get('jump_height',20)
   record['areas']=[geometry(p,'Inside','CollisionShape2D'),geometry(p,'Close','CollisionShape2D')]
   pointroot=p+'/Jump Points/';points=[n for n in nm if n.startswith(pointroot)and n.count('/')==pointroot.count('/')];require(points and all(nm[n]['class']=='Position2D'for n in points),'Unreviewed jump-point child');record['points']=[decode(nm[n]['world_transform'])[2]for n in points]
   sp=decode(nm[p+'/Sprite']['properties']);ray=decode(nm[p+'/RayCast2D']['properties']);texture=rs[sp['texture']['id']]['path'][6:];ex.data(texture);ex.text(texture+'.import');require(sp['centered'] and not sp['flip_h']and not sp['flip_v']and ray['enabled'],'Unknown Arrow/RayCast layout');record['sprite']=[*sp['position'],*sp['offset'],sp['rotation'],*sp['scale'],stable(p+'/Sprite')];record['ray']=[*decode(nm[p+'/RayCast2D']['world_transform'])[2],*ray['cast_to'],ray['collision_mask'],int(ray['collide_with_bodies']),int(ray['collide_with_areas']),int(ray['exclude_parent']),stable(p+'/RayCast2D')]
   a=decode(nm[p+'/AnimationPlayer']['properties']);clip=decode(rs[a['anims/Arrow']['id']]['properties']);require(clip['loop'],'Unreviewed Arrow nonloop');record['arrow_length']=clip['length'];record['texture']=texture
   for i,name in enumerate(('Sprite:offset','Sprite:rotation_degrees')):
    pref='tracks/'+str(i)+'/';keys=dict(clip[pref+'keys']['pairs']);require(clip[pref+'type']=='value'and clip[pref+'path']['value']==name and clip[pref+'interp']==1 and clip[pref+'enabled']and clip[pref+'loop_wrap']and keys['update']==0 and all(x==1 for x in keys['transitions']),'Unknown Arrow animation');record['tracks'].append(dict(times=keys['times'],values=keys['values']))
  else:
   record['flags']=int(v.get('_horizontal',True))|int(v.get('_northeast_southwest',True))<<1;record['step_length']=v.get('_step_length',1);record['areas']=[geometry(p,'','CollisionPolygon2D')];record['arrow_length']=0;record['texture']=''
  records.append(record)
 require(sum(r['kind']==1 for r in records)==14 and sum(r['kind']==2 for r in records)==2,'Incomplete source transition instances')
 result=dict(schema=1,kind='encore.field-player-transitions.source-ir',commit=PIN,scene=SCENE,scene_id=stable('.'),params=params,skill_flag=re.search(r'flags.get\("([^"]+)"\)',jt)[1],prompt_modes=re.search(r'button_prompts in \[([^]]+)\]',jt)[1].replace('"','').replace(' ','').split(','),animations=['Idle',re.search(r'_anim_state.travel\("([^"]+)"\)',pt[pt.index('func jump('):])[1],re.search(r'anim := "([^"]+)"',pt[pt.index('func jump('):])[1]],records=records,areas=area,sources=dict(sorted(ex.sources.items())),native_sha256=sha(native),receipt_sha256=sha(receipt),semantics=['Source Shape overlap, bilateral OR masks, actual player identity; no distance fallback','RayCast lookup uses last physics collider after look_at, never synchronous invented raycast','JumpArea source multi-point jump signal waits, party stagger and QUART/QUAD animation curves; no invented gravity','Stairs use frame-count step accumulator, actual party overlap order and running move_and_slide; source dangling body_exited connection has no callable handler'],unverified=['Manual tests not executed','Main/SceneHost/camera/player/renderer integration pending','Emulator and hardware unverified'])
 signal=re.search(r'^signal (\w+) ?\(([^)]+)\)',ct,re.M);ground=re.search(r'func (get_ground_position)\(\) -> Vector2:\s*return global_position \+ get_node\("([^"]+)"\).position \* ([\d.]+)',po);shadow=re.search(r'get_node\("([^"]+)"\).position',jt);prompt=re.search(r'globaldata\.(\w+) in \[',jt)
 require(signal and len(signal[2].split(','))==2 and ground and shadow and prompt,'Changed Transition source bindings')
 result['schema']=2;result['bindings']=dict(state_signal=signal[1],state_arguments=2,ground_method=ground[1],ground_path=ground[2],ground_multiplier=float(ground[3]),shadow_path=shadow[1],prompt_member=prompt[1])
 result['semantics'].append('Constructor binding does not query Player; live callbacks read same source Player body, native ground CollisionShape position multiplier, shadow and settings; state_changed emits even with silent=true after original tween')
 result['semantics'].append('Godot 3.6.2 Node._ready/GDScript.call_multilevel_reversed invoke ControlledTwoStatesObject before JumpArea: hide tween captures native Sprite scaleONE/position toward ZERO before leaf Ready stores arrow position and sets scaleZERO')
 result['required_sources']=[JUMP,STAIRS,'Scripts/Main/party/Player.gd','Scripts/Main/party/party_follower.gd','Scripts/Main/party/party_object.gd','Scripts/Main/Switches/ControlledTwoStatesObject.gd','Scripts/Main/Camera2D.gd','Scripts/global/global.gd'];validate(result);write(IR,result);write(REVIEW,dict(schema=1,commit=PIN,ir_sha256=sha(IR),sources=result['sources'],semantics=result['semantics'],unverified=result['unverified']));return result
def validate(d):
 require(set(d)==set('schema kind commit scene scene_id params skill_flag prompt_modes animations records areas sources native_sha256 receipt_sha256 semantics unverified required_sources bindings'.split())and d['schema']==2 and d['kind']=='encore.field-player-transitions.source-ir'and d['commit']==PIN and d['scene']==SCENE and d['scene_id']==stable('.')and len(d['params'])==22 and all(type(v)in(int,float)and math.isfinite(v)for v in d['params']),'Unknown transition policy')
 b=d['bindings'];require(set(b)==set('state_signal state_arguments ground_method ground_path ground_multiplier shadow_path prompt_member'.split()) and b['state_arguments']==2 and b['ground_multiplier']>0 and all(type(b[k])is str and b[k] for k in ('state_signal','ground_method','ground_path','shadow_path','prompt_member')),'Unknown Transition native bindings')
 require(all(d['params'][i]>0 for i in (0,3,4,6,7,8,11,12,13,15))and d['required_sources']and all(s in d['sources']for s in d['required_sources']),'Unreviewed transition timing/proof')
 require(len(d['records'])==16 and len(d['areas'])==30 and len(d['animations'])==3 and len(d['prompt_modes'])==2 and d['skill_flag'],'Incomplete transition scope');ids=set();ready=set()
 for r in d['records']:
  require(r['id']==stable(r['node'])and r['id']not in ids and r['ready']not in ready and r['kind']in(1,2)and len(r['areas'])==(2 if r['kind']==1 else 1)and all(0<=i<len(d['areas'])for i in r['areas']),'Unknown transition record');ids.add(r['id']);ready.add(r['ready'])
  require((r['kind']==1 and 0<r['height']<=10000 and r['flags']<=7 and r['flags']&4 and r['points']and r['arrow_length']>0 and len(r['tracks'])==2 and len(r['sprite'])==8 and len(r['ray'])==9)or(r['kind']==2 and r['flags']<=3 and r['step_length']>0 and not r['points']and not r['tracks']and not r['sprite']and not r['ray']),'Unsupported transition behavior')
 for a in d['areas']:require(a['id']and a['shape_id']and a['kind']in(1,2,3)and len(a['position'])==2 and (len(a['values'])==(2 if a['kind']==1 else 1)if a['kind']!=3 else len(a['values'])>=6 and len(a['values'])%2==0),'Unknown actual transition shape')
 return d
def load():
 d=validate(read(IR));r=read(REVIEW);require(r['ir_sha256']==sha(IR)and r['commit']==PIN and r['sources']==d['sources'],'Transition source review stale');ex=Extractor(ROOT)
 for p,h in d['sources'].items():ex.data(p);require(ex.sources[p]==h,'Changed transition source '+p)
 return d
def encode(d):
 validate(d);out=bytearray(64)
 def u(*v):out.extend(struct.pack('<'+'I'*len(v),*v))
 def f(*v):out.extend(struct.pack('<'+'f'*len(v),*v))
 def s(v):b=v.encode();u(len(b));out.extend(b)
 f(*d['params']);s(d['scene']);s(d['skill_flag'])
 for v in d['prompt_modes']+d['animations']:s(v)
 u(len(d['required_sources']))
 for v in d['required_sources']:s(v)
 for a in d['areas']:u(a['id'],a['shape_id'],a['layer'],a['mask'],a['kind'],len(a['values']));f(*a['position'],*a['values'])
 for r in d['records']:
  u(r['id'],r['ready'],r['kind'],r['camera_id'],r['flags'],len(r['areas']),len(r['points']));f(*r['position'],r['height'],r['step_length'],r['arrow_length']);s(r['node']);s(r['texture']);u(*r['areas'])
  for p in r['points']:f(*p)
  if r['kind']==1:
   f(*r['sprite'][:7]);u(r['sprite'][7]);f(*r['ray'][:4]);u(*r['ray'][4:]);
   for track in r['tracks']:
    u(len(track['times']))
    for t,v in zip(track['times'],track['values']):f(t,*(v if isinstance(v,list)else[v]))
 u(len(d['sources']))
 for p,h in d['sources'].items():s(p);out.extend(bytes.fromhex(h))
 b=d['bindings'];s(b['state_signal']);u(b['state_arguments']);s(b['ground_method']);s(b['ground_path']);f(b['ground_multiplier']);s(b['shadow_path']);s(b['prompt_member'])
 struct.pack_into('<8s6I20sIII',out,0,b'ENCFPT01',2,len(out),0,2,1,d['scene_id'],bytes.fromhex(PIN),len(d['records']),len(d['areas']),0);struct.pack_into('<I',out,16,zlib.crc32(out)&0xffffffff);return bytes(out)
def compile_pack():raw=encode(load());PACK.parent.mkdir(parents=True,exist_ok=True);PACK.write_bytes(raw);return raw
def stage_files(source):
 raw=encode(load());require((Path(source)/'data/podunk-player-transitions.encplayer').read_bytes()==raw,'Stale transition binary');return{Path('data/podunk-player-transitions.encplayer'):raw}
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile']);p.add_argument('--native',type=Path);p.add_argument('--source',type=Path);a=p.parse_args()
 try:
  if a.action=='extract':print(len(extract(a.native,a.source)['records']),'actual transition instances')
  else:print(len(compile_pack()),'checked transition bytes')
 except(ValueError,TypeError,KeyError,OSError,struct.error)as e:sys.exit('FIELD TRANSITIONS ERROR: '+str(e))
