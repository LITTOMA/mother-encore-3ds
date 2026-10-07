#!/usr/bin/env python3
"""Pinned complete Podunk NPC source -> checked native NPC records, no runtime JSON."""
from __future__ import annotations
import argparse,hashlib,json,math,re,struct,sys,zlib,subprocess
from pathlib import Path
from dataclasses import dataclass
from typing import Callable
import yaml
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from tools.podunk_scene import decode,stable,SCENE,PIN,read,write,sha,require
from tools.extract_battle_entry import Extractor
from tools.asset_receipts import receipt_path
SCRIPT='Scripts/Main/npc.gd'
FACTORY='Nodes/Reusables/npc.tscn'
IR=ROOT/'content/native-field-npc.json'
OUT=ROOT/'romfs/data/podunk-npcs.encnpc'
PARAMETERS=('InitialDownX','InitialDownY','ReturnDelay','ReadyTimerMin','TimerDeviation','AxisModulus','MinimumMove','MotionThreshold','LookThreshold','ExtentVertical','ExtentHorizontal','MovementDifferenceFloor')
@dataclass(frozen=True)
class NpcScene:
 scene:str
 scene_id:int
 node_count:int
 npc_count:int
 node_id:Callable[[str],int]
 root_paths:tuple[str,...]=()
 source_guarded_transitions:bool=False

PODUNK=NpcScene(SCENE,stable('.'),8686,67,stable)

def source_instances(native,receipt,upstream,scene=PODUNK,resolved_scripts=None):
 d=read(native);s=read(receipt);require(d['source']=='res://'+scene.scene and d['schema']==1 and d['native_compatible'] is False,'Incomplete NPC native scene')
 require(s['commit']==PIN and s['scene']==scene.scene,'Unreviewed NPC native receipt')
 inventory=read(ROOT/'compatibility/upstream-inventory.json')['files']
 for name,record in s['files'].items():require(sha(upstream/name)==record['sha256']==inventory[name]['sha256'],'Changed NPC closure '+name)
 names={n['path']:n for n in d['nodes']};require(len(names)==scene.node_count,'Incomplete NPC native topology');require(decode(names['Objects']['world_transform'])==[[1,0],[0,1],[0,0]],'NPC Objects transform requires a source adapter')
 resources={r['id']:r for r in d['resources']};states={s['source'][6:]:s for s in d['scene_states']};roots=[]
 def visit_instances(path,filename):
  roots.append((path,filename))
  for n in states[filename]['nodes']:
   if n['instance'] is not None:
    local=n['path'][2:] if n['path'].startswith('./') else n['path'];target=path if local=='.' else local if path=='.' else path+'/'+local
    visit_instances(target,resources[n['instance']['id']]['path'][6:])
 visit_instances('.',scene.scene);bindings={};overrides={}
 for root,filename in sorted(roots,key=lambda r:r[0].count('/') if r[0]!='.' else -1,reverse=True):
  for n in states[filename]['nodes']:
   local=n['path'][2:] if n['path'].startswith('./') else n['path'];path=root if local=='.' else local if root=='.' else root+'/'+local
   if path in names:overrides.setdefault(path,{}).update(decode(n['properties']))
  for a in s['script_attachments']:
   if a['source']!=filename:continue
   name=re.search(r'\bname="([^"]+)"',a['node_declaration'])[1];parent=re.search(r'\bparent="([^"]+)"',a['node_declaration'])
   local='.' if parent is None else name if parent[1]=='.' else parent[1]+'/'+name;path=root if local=='.' else local if root=='.' else root+'/'+local
   require(path in names,'Unresolved NPC binding '+path);bindings[path]=a['script']
 if resolved_scripts is not None:
  require(all(p in names and type(v)is str and v for p,v in resolved_scripts.items()),'Unresolved explicit NPC script inventory')
  bindings=dict(resolved_scripts)
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
  out.append(dict(stable_id=scene.node_id(path),node=path,ready_ordinal=ordinal[path],overrides=overrides[path],native=names[path],children=[names[p] for p in children[path]]))
 require(len(out)==scene.npc_count and (not scene.root_paths or tuple(r['node']for r in out)==scene.root_paths),'NPC binding loss');return out,names,resources,dict(native_export_sha256=sha(native),source_receipt_sha256=sha(receipt),source_files={name:r['sha256'] for name,r in s['files'].items()})

def dictionary(v):
 if isinstance(v,dict) and v.get('type')=='Dictionary':return {dictionary(k):dictionary(x) for k,x in v['pairs']}
 if isinstance(v,list):return [dictionary(x) for x in v]
 if isinstance(v,dict):return {k:dictionary(x) for k,x in v.items()}
 return v

def png(ex,path):
 data=ex.data(path);require(data[:8]==b'\x89PNG\r\n\x1a\n','NPC source texture codec');return list(struct.unpack('>II',data[16:24]))

def extract(native,receipt,scene=PODUNK,resolved_scripts=None):
 records,nodes,resources,provenance=source_instances(native,receipt,ROOT/'upstream/MOTHER-Encore',scene,resolved_scripts)
 ex=Extractor(ROOT);script=ex.text(SCRIPT);ex.text(FACTORY);ex.text('LICENSE');ex.text('Scripts/Main/character_sprite.gd');ex.text('Scripts/global/globalData.gd')
 require('for i in dialog_array.size():' in script and 'if !globaldata.seen_dialogue_flags.get(dialog_hash, false) or j == dialog_array[i].size() - 1:' in script,'NPC dialogue traversal changed')
 require('wander_timer.wait_time = rand_range(walk_frequency - 0.5,walk_frequency + 0.5)' in script,'NPC Timer review changed')
 defaults={}
 for line in script.splitlines():
  if not line.startswith('export '):continue
  m=re.search(r'var ([A-Za-z_]+)(?::[^=]+)?\s*(?::?=)?\s*(.*)',line);require(m,'NPC export declaration');name=m[1];raw=re.split(r'\bsetget\b',m[2].split('#')[0])[0].strip()
  if raw.startswith('='):raw=raw[1:].strip()
  if name=='player_turn':defaults[name]={'x':True,'y':True};continue
  if not raw:defaults[name]=[] if name in ('_all_dialog','_all_thoughts','event_positions') else False if name in ('_automatic_shadow','no_shadow','no_collision') else ''
  elif raw=='Vector2.ZERO':defaults[name]=[0,0]
  elif raw in ('true','false'):defaults[name]=raw=='true'
  elif raw.startswith('[') or raw.startswith('"'):defaults[name]=json.loads(raw)
  else:
   require(re.fullmatch(r'-?[0-9.]+',raw),'Unsupported NPC export default '+name+':'+raw);defaults[name]=float(raw) if '.' in raw else int(raw)
 require(set(defaults)=={'_name','sprite','dialog','_thoughts','yaml','connections','appear_flag','disappear_flag','_all_dialog','_all_thoughts','event_positions','player_turn','turn_to_player_on_interact','turn_to_player_on_telepathy','no_problem_thoughts','_automatic_shadow','no_shadow','no_collision','extended_interact','sprite_offset','initial_dir','idle_animation','talk_idle_animation','staring','wander','speed','walk_frequency','debug_npc'},'Missing NPC exported defaults')
 def fact(pattern):
  m=re.search(pattern,script);require(m,'Missing NPC source fact '+pattern);return float(m[1])
 down=re.search(r'initial_dir = Vector2\(([0-9.-]+),([0-9.-]+)\)',script);require(down,'Missing source initial direction')
 params=dict(InitialDownX=float(down[1]),InitialDownY=float(down[2]),ReturnDelay=fact(r'create_timer\(([0-9.]+)\)'),ReadyTimerMin=fact(r'rand_range\(([0-9.]+),walk_frequency\)'),TimerDeviation=fact(r'rand_range\(walk_frequency - ([0-9.]+)'),AxisModulus=fact(r'randi\(\)%([0-9]+)'),MinimumMove=fact(r'ample_distance_x > ([0-9.]+)'),MotionThreshold=fact(r'abs\(old_pos.x - global_position.x\) > ([0-9.]+)'),LookThreshold=fact(r'abs\(old_pos.x - global_position.x\) < ([0-9.]+)'),ExtentVertical=fact(r'old_height \* \(1 \+ ([0-9.]+)'),ExtentHorizontal=fact(r'old_width \* \(1 \+ ([0-9.]+)'),MovementDifferenceFloor=fact(r'difference = max\(ceil\(abs\(speed \* delta\)\), ([0-9.]+)\)'))
 assets={};npcs=[];programs=set();omitted_transitions={}
 for record in records:
  native_node=record['native'];wt=decode(native_node['world_transform']);require(wt[:2]==[[1,0],[0,1]],'NPC root transform requires typed adapter')
  override=dictionary(record['overrides']);require(set(override)<=set(defaults)|{'position','collision_layer','collision_mask','script'},'Unknown NPC scene override')
  values={**defaults,**{k:v for k,v in override.items() if k in defaults}};sprite='Graphics/Character Sprites/'+values['sprite']+'.png';size=png(ex,sprite)
  party='Npcs' not in values['sprite'] and 'Enemies' not in values['sprite'];anim='Data/Animations/PartyMember.yaml' if party else values['yaml'].removeprefix('res://') if values['yaml'] else 'Data/Animations/4dir.yaml'
  animation=yaml.safe_load(ex.text(anim));require(set(animation)=={'animations','offset','size','type'} and animation['type']==0 and len(animation['size'])==2,'Unknown NPC AnimationTree schema');cols,rows=animation['size'];require(size[0]%cols==size[1]%rows==0,'NPC cell geometry')
  asset='graphics/npcs/'+hashlib.sha256(sprite.encode()).hexdigest()[:16]+'.t3x';assets[asset]=dict(source=sprite,size=size,grid=[cols,rows],source_sha256=ex.sources[sprite])
  motions=[]
  for name,motion in animation['animations'].items():
   require(motion.keys()=={'directions','type'} and motion['type'] in (0,1) and len(motion['directions']) in (1,2,4,8),'Unknown NPC motion type '+anim+':'+name+':'+str(motion))
   directions=[]
   for keys in motion['directions']:
    require(len(keys)==len(motion['directions'][0]),'NPC source first-direction frameCount mismatch')
    time=float(keys[0]);frames=[]
    for frame,duration in keys[1:]:frames.append([time,frame-1]);time+=duration
    basis=[[0,0]] if len(motion['directions'])==1 else [[-1,0],[1,0]] if len(motion['directions'])==2 else [[0,1],[-1,0],[1,0],[0,-1],[-2**-.5,2**-.5],[2**-.5,2**-.5],[-2**-.5,-2**-.5],[2**-.5,-2**-.5]]
    directions.append(dict(vector=basis[len(directions)],duration=time,keys=frames))
   motions.append(dict(name=name,loop=(motion['type']==0),directions=directions))
  connections=values['connections'];require(all(len(c)==3 and c[2] in (0,1,2) and all(type(x)is str for x in c[:2])for c in connections),'Unknown NPC transition schema')
  absent=[c for c in connections if any(x not in animation['animations']for x in c[:2])]
  if scene.source_guarded_transitions:
   require('if animationTree.tree_root.has_node(anim[0]) and animationTree.tree_root.has_node(anim[1]):'in ex.text('Scripts/Main/character_sprite.gd'),'NPC original guarded transition source changed')
   if absent:omitted_transitions[record['node']]=absent
   connections=[c for c in connections if c not in absent]
  else:require(not absent,'Unknown NPC transition')
  dialogues=[]
  for thoughts,base,array in ((False,values['dialog'],values['_all_dialog']),(True,values['_thoughts'],values['_all_thoughts'])):
   branches=[list(v) for v in array]
   if base and (not branches or branches[0][0]!=''):branches.insert(0,['',base])
   for group,branch in enumerate(branches):
    require(len(branch)>=2 and all(type(v) is str for v in branch),'NPC dialogue branch shape')
    for ordinal,program in enumerate(branch[1:],1):
     source='Data/Dialogue/'+program+'.yaml';ex.text(source);programs.add(program)
     dialogues.append(dict(thoughts=thoughts,group=group,ordinal=ordinal,last=ordinal==len(branch)-1,flag=branch[0],program=program,source=source))
  eventpositions=[]
  for event in values['event_positions']:
   require(len(event)==3 and type(event[0]) is str,'NPC event position');eventpositions.append(dict(flag=event[0],position=[float(event[1]),float(event[2])]))
  geometry=[]
  def spatial(role,part,shape=False):
   n=nodes[record['node']+'/'+part];p=decode(n['properties']);t=decode(n['world_transform']);require(t[0][1]==t[1][0]==0,'NPC child rotation needs new adapter')
   mask=layer=0;value=[0,0];kind=5
   if shape:
    resource=resources[p['shape']['id']];prop=decode(resource['properties']);kind={'RectangleShape2D':1,'CircleShape2D':2}.get(resource['class']);require(kind,'Unknown NPC collision shape');value=prop['extents'] if kind==1 else [prop['radius'],0]
    parent=record['node'] if '/' not in part else record['node']+'/'+part.rsplit('/',1)[0];pp=decode(nodes[parent]['properties']);mask=pp.get('collision_mask',0);layer=pp.get('collision_layer',0)
   elif role==6:kind=3;value=p['cast_to'];mask=p['collision_mask']
   elif role==7:kind=4;rect,extent=p['rect'];value=extent;t[2]=[t[2][0]+rect[0]*t[0][0],t[2][1]+rect[1]*t[1][1]]
   elif role==8:value=png(ex,resources[p['texture']['id']]['path'][6:])
   geometry.append(dict(role=role,kind=kind,layer=layer,mask=mask,offset=[t[2][0]-wt[2][0],t[2][1]-wt[2][1]],value=value,scale=[t[0][0],t[1][1]],rotation=0))
  for role,part in ((1,'CollisionShape2D'),(2,'interact/CollisionShape2D'),(3,'NearPlayerArea/CollisionShape2D'),(4,'ViewArea/CollisionShape2D2'),(5,'WanderRadius/CollisionShape2D2')):spatial(role,part,True)
  for role,part in ((6,'RayCast2D'),(7,'VisibilityNotifier2D'),(8,'Shadow'),(9,'CharacterSprite')):spatial(role,part)
  shadow=geometry[7];shadow_texture='Graphics/Character Sprites/Shadow.png';assets['graphics/npcs/shadow.t3x']=dict(source=shadow_texture,size=png(ex,shadow_texture),grid=[1,1],source_sha256=ex.sources[shadow_texture])
  if values['_automatic_shadow']:shadow['scale'][0]+=shadow['value'][0]/50
  flags=0
  for bit,key in enumerate(('turn_to_player_on_interact','turn_to_player_on_telepathy','no_problem_thoughts','_automatic_shadow','no_shadow','no_collision','staring','wander','debug_npc')):
   if values[key]:flags|=1<<bit
  flags|=(int(values['player_turn']['x'])<<9)|(int(values['player_turn']['y'])<<10)|(int(party)<<11)
  offset=[animation['offset'][0]+values['sprite_offset'][0],-int(size[1]/float(rows*2))+animation['offset'][1]+values['sprite_offset'][1]]
  np=decode(native_node['properties']);npcs.append(dict(id=record['stable_id'],node=record['node'],ready_ordinal=record['ready_ordinal'],sprite=sprite,animation=anim,texture=asset,appear=values['appear_flag'],disappear=values['disappear_flag'],idle=values['idle_animation'],talk_idle=values['talk_idle_animation'],walk='Walk',talk='Talk',flags=flags,extended_interact=values['extended_interact'],columns=cols,rows=rows,size=size,position=wt[2],direction=values['initial_dir'],sprite_offset=offset,speed=values['speed'],walk_frequency=values['walk_frequency'],wander_radius=geometry[4]['value'][0],timer_wait=decode(nodes[record['node']+'/WanderRadius/Timer']['properties'])['wait_time'],safe_margin=np['collision/safe_margin'],dialogues=dialogues,event_positions=eventpositions,connections=connections,motions=motions,geometry=geometry))
 result=dict(schema=1,kind='encore.field-npc.source-ir',commit=PIN,scene=scene.scene,scene_id=scene.scene_id,sources={**provenance['source_files'],**ex.sources},provenance=provenance,license_review='Pinned LICENSE permits repository graphics for game-related forks, modifications and translations. These NPC sprites are used only by this Mother: Encore 3DS port and retain upstream asset conditions; no unrelated redistribution or relicensing.',parameters=params,npcs=npcs,assets=assets,programs=sorted(programs),program_admission='Each selected source path must be admitted by the existing checked Room Programme Host; untranslated/uncompiled paths fail closed',semantics=[f'All {scene.npc_count} NPC roots, exact postorder Ready IDs and serialized inner-to-outer overrides','Last matching dialogue group wins; within group first unseen or final line, then mark only the final selected line','No collision export is followed by final visibility_changed which rewrites disabled from actual tree visibility','Wander Timer timeout always draws its next interval, even when no movement occurs','Ordinary interaction sets talker; telepathy does not set talker and invokes source telepathy effect'],unverified=['No tests run','Full field SceneHost, sprite fetch/tint/emotes and all Programme paths require separate capability admission'])

 if scene.source_guarded_transitions:result['source_guarded_absent_transitions']=omitted_transitions
 return result

def validate(ir,scene=PODUNK):
 require(ir['schema']==1 and ir['commit']==PIN and ir['scene']==scene.scene and ir['scene_id']==scene.scene_id and ir['kind']=='encore.field-npc.source-ir','NPC IR identity')
 require(set(ir['parameters'])==set(PARAMETERS) and len(ir['npcs'])==scene.npc_count and (not scene.root_paths or tuple(n['node']for n in ir['npcs'])==scene.root_paths),'NPC topology')
 ids=set();orders=set()
 for n in ir['npcs']:
  require(n['id']==scene.node_id(n['node']) and n['id'] not in ids and n['ready_ordinal'] not in orders and 0<=n['flags']<4096 and 0<=n['extended_interact']<=3 and n['texture'] in ir['assets'] and n['sprite'] in ir['sources'] and n['animation'] in ir['sources'],'NPC identity/assets');ids.add(n['id']);orders.add(n['ready_ordinal'])
  require(n['columns']>0 and n['rows']>0 and n['speed']>0 and n['walk_frequency']>0 and len(n['geometry'])==9,'NPC numeric schema')
  for g in n['geometry']:
   require(g['role'] in range(1,10) and g['kind'] in range(1,6) and 0<=g['mask']<=65535 and 0<=g['layer']<=65535 and all(type(v) in (int,float) and math.isfinite(v) and abs(v)<=1000000 for v in g['offset']+g['value']+g['scale']+[g['rotation']]) and all(v>0 for v in g['scale']),'NPC geometry schema')
  require([g['role'] for g in n['geometry']]==list(range(1,10)),'NPC geometry ordering')
  for d in n['dialogues']:require(d['ordinal']>0 and d['group']>=0 and d['program'] in ir['programs'] and type(d['last']) is bool,'NPC dialogue reference')
  for m in n['motions']:
   require(len(m['directions']) in (1,2,4,8),'NPC directional motion')
   for direction in m['directions']:
    require(0<direction['duration']<=86400 and direction['keys'],'NPC clip length')
    previous=-1
    for time,frame in direction['keys']:require(math.isfinite(time) and previous<=time<direction['duration'] and type(frame) is int and 0<=frame<n['columns']*n['rows'],'NPC key schema');previous=time

def pack(ir,scene=PODUNK):
 validate(ir,scene);out=bytearray(64)
 def u(*values):out.extend(struct.pack('<'+'I'*len(values),*values))
 def f(*values):require(all(math.isfinite(x) and abs(x)<=1000000 for x in values),'NPC nonfinite value');out.extend(struct.pack('<'+'f'*len(values),*values))
 def string(s):require(type(s) is str and '\0' not in s and len(s.encode())<=4096,'NPC string');b=s.encode();u(len(b));out.extend(b)
 u(len(PARAMETERS))
 for i,name in enumerate(PARAMETERS):u(i+1);f(ir['parameters'][name])
 for n in ir['npcs']:
  u(n['id'],n['ready_ordinal'],n['flags'],n['extended_interact'],n['columns'],n['rows'],*n['size'])
  for key in ('node','sprite','animation','texture','appear','disappear','idle','talk_idle','walk','talk'):string(n[key])
  f(*n['position'],*n['direction'],*n['sprite_offset'],n['speed'],n['walk_frequency'],n['wander_radius'],n['timer_wait'],n['safe_margin'])
  u(len(n['dialogues']))
  for d in n['dialogues']:u(int(d['thoughts']),d['group'],d['ordinal'],int(d['last']));string(d['flag']);string(d['program']);string(d['source'])
  u(len(n['event_positions']))
  for event in n['event_positions']:string(event['flag']);f(*event['position'])
  u(len(n['connections']))
  for a,b,mode in n['connections']:string(a);string(b);u(mode)
  u(len(n['motions']))
  for m in n['motions']:
   string(m['name']);u(int(m['loop']),len(m['directions']))
   for direction in m['directions']:
    f(*direction['vector'],direction['duration']);u(len(direction['keys']))
    for time,frame in direction['keys']:f(time);u(frame)
  u(len(n['geometry']))
  for g in n['geometry']:u(g['role'],g['kind'],g['layer'],g['mask']);f(*g['offset'],*g['value'],*g['scale'],g['rotation'])
 u(len(ir['sources']))
 for name,h in sorted(ir['sources'].items()):string(name);out.extend(bytes.fromhex(h))
 struct.pack_into('<8s6I20sI',out,0,b'ENCNPC01',1,len(out),0,1,1,len(ir['npcs']),bytes.fromhex(PIN),ir['scene_id']);struct.pack_into('<I',out,16,zlib.crc32(out));return bytes(out)

def assets(ir,tex3ds):
 validate(ir);ex=Extractor(ROOT);outputs={}
 for name,a in ir['assets'].items():
  require(sha(ROOT/'upstream/MOTHER-Encore'/a['source'])==a['source_sha256'],'Changed NPC asset');target=ROOT/'romfs'/name;target.parent.mkdir(parents=True,exist_ok=True)
  subprocess.run([str(tex3ds),'-f','rgba8','-z','none','-o',str(target),str(ROOT/'upstream/MOTHER-Encore'/a['source'])],check=True);outputs[name]=dict(bytes=target.stat().st_size,sha256=sha(target))
 write(receipt_path(ROOT/'romfs/graphics/npcs',ROOT),dict(schema=1,commit=PIN,recipe_sha256=sha(IR),outputs=outputs,tex3ds_sha256=sha(tex3ds)))

def main():
 p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile','assets']);p.add_argument('--native',type=Path);p.add_argument('--source',type=Path);p.add_argument('--tex3ds',type=Path);a=p.parse_args()
 if a.action=='extract':require(a.native and a.source,'NPC extraction requires complete official native/source exports');ir=extract(a.native,a.source);validate(ir);write(IR,ir)
 else:
  ir=read(IR);validate(ir);inventory=read(ROOT/'compatibility/upstream-inventory.json')['files']
  for path,h in ir['sources'].items():require(sha(ROOT/'upstream/MOTHER-Encore'/path)==h==inventory[path]['sha256'],'Changed NPC source '+path)
  if a.action=='assets':require(a.tex3ds,'Need real tex3ds');assets(ir,a.tex3ds)
  else:payload=pack(ir);OUT.parent.mkdir(parents=True,exist_ok=True);OUT.write_bytes(payload);print('Field NPC:',len(payload),'bytes;',len(ir['npcs']),'source NPCs;',len(ir['programs']),'source programme references')
if __name__=='__main__':
 try:main()
 except (ValueError,KeyError,OSError,struct.error,subprocess.SubprocessError) as e:sys.exit('FIELD NPC ERROR: '+str(e))
