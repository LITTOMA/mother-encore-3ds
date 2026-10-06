#!/usr/bin/env python3
"""Pinned Player effect factories: full native source resources, no fake Ready."""
from pathlib import Path
import argparse, hashlib, json, math, re, struct, sys, zlib
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import PIN,read,write,sha,require,decode
from tools.extract_battle_entry import Extractor
from tools.scene_reference import quarantine
from tools.field_ui_manager_recipes import extract_recipe,encode_recipe
IR=ROOT/'content/native-player-effects.json';REVIEW=ROOT/'reports/player-effects/source-review.json';OUT=ROOT/'romfs/data/player.enceffects'
PLAYER=ROOT/'content/native-player-initialization.json'
SCENES=['Nodes/Reusables/Effects/After Image.tscn','Nodes/Reusables/Effects/Dust.tscn']
CREATORS=['Scripts/misc/AfterImageCreator.gd','Scripts/misc/DustCreator.gd']
SCRIPTS=['Scripts/misc/After Image.gd','Scripts/misc/character_tint.gd']
FAMILY=0x454e0059

def prepare(directory):
 directory=Path(directory).resolve();require(directory.is_relative_to((ROOT/'build').resolve())and not directory.exists(),'Fresh isolated build source directory required')
 ex=Extractor(ROOT)
 for index,scene in enumerate(SCENES):
  out=directory/str(index);todo=[scene];files={};payloads={};attachments=[];signals=[]
  while todo:
   path=todo.pop()
   if path in files:continue
   raw=ex.data(path);entry=dict(sha256=ex.sources[path],bytes=len(raw))
   if path.endswith(('.tscn','.tres')):
    raw,refs,scripts,connections=quarantine(raw,path);entry.update(external_resources=refs,reference_sha256=hashlib.sha256(raw).hexdigest());todo.extend(x['path']for x in refs);attachments.extend(scripts);signals.extend(connections);payloads[path]=raw
   elif path.endswith('.png'):payloads[path]=raw
   else:require(path.endswith('.gd'),'Unknown effect source codec');entry['quarantined']='Original script constructor/Ready never executed'
   files[path]=entry
  out.mkdir(parents=True)
  for path,raw in payloads.items():
   p=out/path;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(raw)
  (out/'project.godot').write_bytes(b'config_version=4\n[application]\nconfig/name="Original player effects data"\n[logging]\nfile_logging/enable_logging=false\n')
  for tool in ['scene_data.gd','field_ui_manager.gd']:(out/tool).write_bytes((ROOT/'tools/godot_exporter'/tool).read_bytes())
  write(out/'source.json',dict(schema=1,commit=PIN,scene=scene,files=files,script_attachments=attachments,signal_connections=signals,case_aliases={},native_compatible=False))

def extract(directory):
 ex=Extractor(ROOT);player=read(PLAYER);require(player['commit']==PIN and player['ready_admitted']is False,'Actual Player source prerequisite rejected')
 creators=[]
 for kind,path in enumerate(CREATORS):
  source=ex.text(path);rows=[r for r in player['records']if r['script']==path];require(len(rows)==1,'Complete actual Player creator attachment required')
  node=rows[0];preload=re.search(r'^onready var (\w+) = preload\("res://([^"\n]+)"\)',source,re.M);require(preload and preload[2]==SCENES[kind],'Unknown effect onready resource')
  row=dict(kind=kind,id=node['id'],path=node['node'],native=node['native_class'],script=path,script_sha=ex.sources[path],resource_member=preload[1],scene=SCENES[kind],methods=re.findall(r'^func (\w+)\(',source,re.M))
  if kind==0:
   timer=re.search(r'^onready var (\w+) = \$(\w+)',source,re.M);sprite=re.search(r'^onready var (\w+) = get_node_or_null\((\w+)\)',source,re.M);require(timer and sprite,'Unknown source Creator node binding')
   original=ex.text('Nodes/Reusables/Player.tscn');sprite_path=re.search(r'^spritePath = NodePath\("([^"\n]+)"\)',original,re.M);require(sprite_path,'Actual Player spritePath missing')
   row.update(timer_member=timer[1],timer_path=timer[2],sprite_member=sprite[1],sprite_path=sprite_path[1],sprite_export=sprite[2],parent_hops=len(re.findall(r'get_parent\(\)',source)),timeout_method='_on_Timer_timeout')
  else:
   limits=re.findall(r'round\(rand_range\(([-.0-9]+),([- .0-9]+)\)\)',source);require(len(limits)==2 and limits[0]==limits[1],'Dust source RNG expression changed')
   target=re.search(r'global\.(\w+)\.get_node\("([^"\n]+)"\)',source);depth=re.search(r' - ([-.0-9]+) \* global\.(\w+)\.size\(\)',source);anim=re.search(r'get_node\("([^"\n]+)"\)\.play\("([^"\n]+)"\)',source);connect=re.search(r'\.connect\("([^"\n]+)", self, "([^"\n]+)", \[dust\], (\w+)\)',source);created=re.search(r'^var (\w+) := \[\]',source,re.M);require(target and depth and anim and connect and created,'Dust source operation mapping missing')
   row.update(scene_member=target[1],objects_path=target[2],rng_bounds=[float(x)for x in limits[0]],party_depth=float(depth[1]),party_member=depth[2],animation_path=anim[1],animation=anim[2],finished_signal=connect[1],finished_method=connect[2],connect_flag=connect[3],created_member=created[1])
  creators.append(row)
 timer_path=creators[0]['path']+'/'+creators[0]['timer_path'];timer_node=next(r for r in player['records']if r['node']==timer_path);timer_native=next(r for r in player['native_snapshot']['nodes']if r['path']==timer_path);properties=decode(timer_native['properties']);require(timer_node['native_class']=='Timer'and not timer_node['script']and properties['process_mode']in (0,1),'Unknown actual AfterImage native Timer');timer=dict(id=timer_node['id'],scene=player['scene_id'],mode=properties['process_mode'],flags=int(properties['one_shot'])|int(properties['autostart'])<<1,wait=properties['wait_time'],scene_sha=player['source_sha256'],script_sha=timer_node['script_sha'])
 recipes=[];native=[];signals=[]
 for index,scene in enumerate(SCENES):
  d=Path(directory)/str(index);recipe=extract_recipe(scene,d/'native.json',d/'tree.json',d/'source.json');recipes.append(recipe);snapshot=read(d/'native.json');native.append(snapshot);signals.append(read(d/'source.json')['signal_connections'])
  ex.sources.update(recipe['sources'])
 for p in SCRIPTS+['Shaders/Flash.tres','Nodes/Reusables/Effects/AfterImageCreator.tscn','LICENSE']:ex.text(p)
 after=ex.text(SCRIPTS[0]);tint=ex.text(SCRIPTS[1]);ready=re.search(r'anim_player.play\("([^"\n]+)"\)',after);require(ready,'AfterImage source Ready animation missing')
 copied=re.findall(r'^\t(\w+) = sprite\.(\w+)$',after,re.M);require(copied and all(a==b for a,b in copied),'Unknown AfterImage copied member binding')
 dust_text=ex.text(SCENES[1]);target_property=re.search(r'^sprite_paths = \[ (.*) \]$',dust_text,re.M);require(target_property,'Dust CharacterTint actual targets missing');target_paths=re.findall(r'NodePath\("([^"\n]+)"\)',target_property[1]);require(target_paths and 'var _tint := Color.white'in tint,'CharacterTint source default/targets changed')
 policy=dict(after_animation=ready[1],after_animation_path=re.search(r'^onready var \w+ = \$(\w+)',after,re.M)[1],after_finished_method=re.search(r'^func (_on_\w+)\(',after,re.M)[1],copied_members=[a for a,b in copied],tint_targets_member=re.search(r'^export\s*\(Array, NodePath\) var (\w+)',tint,re.M)[1],tint_script=SCRIPTS[1],tint_paths=target_paths,tint_default=[1,1,1,1])
 require(len(signals[0])==1 and not signals[1],'Unknown source effect signal connection');connection=dict(re.findall(r'(\w+)="([^"\n]+)"',signals[0][0]['declaration']));require(connection.get('from')==policy['after_animation_path']and connection.get('to')=='.'and connection.get('method')==policy['after_finished_method'],'AfterImage source signal mapping changed');policy['after_finished_signal']=connection['signal']
 functions=[]
 for path in CREATORS+SCRIPTS:
  text=ex.text(path)
  for m in re.finditer(r'^func (\w+)\([^\n]*\n.*?(?=^func |\Z)',text,re.M|re.S):functions.append(dict(source=path,method=m[1],sha=hashlib.sha256(m[0].encode()).hexdigest()))
 write(IR,dict(schema=1,kind='encore.player-effects.source-ir',commit=PIN,family=FAMILY,player_ir_sha256=sha(PLAYER),scene=player['scene'],scene_id=player['scene_id'],source_sha256=player['source_sha256'],sources=ex.sources,creators=creators,policy=policy,timer=timer,recipes=recipes,native=native,signals=signals,functions=functions,scene_admitted=False,pending=['Actual native Node/Canvas/Sprite lifecycle remains same SceneTree owner','Sprite.duplicate default complete four-node Player sprite/emotes subtree needs real duplicate owner; no cropped Sprite copy','Actual AnimationPlayer and material/texture owners must accept all checked properties/tracks before effect instance enters','Dust source currentScene/Objects parent and actual live party supplied by same global owner']))

def load():
 d=read(IR);r=read(REVIEW);inv=read(ROOT/'compatibility/upstream-inventory.json')['files'];require(d['schema']==1 and d['commit']==PIN and d['family']==FAMILY and not d['scene_admitted']and d['player_ir_sha256']==sha(PLAYER)and r['ir_sha256']==sha(IR),'Player effect review/dependency rejected')
 for p,h in d['sources'].items():require(inv[p]['sha256']==h==sha(ROOT/'upstream/MOTHER-Encore'/p),'Changed player effect source '+p)
 require(len(d['creators'])==len(d['recipes'])==len(d['native'])==2,'Complete effect resources missing');return d

def encode(d):
 b=bytearray(128)
 def u(*v):b.extend(struct.pack('<'+'I'*len(v),*v))
 def t(s):v=s.encode();u(len(v));b.extend(v)
 def val(v):
  if v is None:u(0)
  elif type(v)is bool:u(1,int(v))
  elif type(v)is int:u(2);b.extend(struct.pack('<q',v))
  elif type(v)is float:require(math.isfinite(v),'Nonfinite effect Variant');u(3);b.extend(struct.pack('<d',v))
  elif isinstance(v,str):u(4);t(v)
  elif isinstance(v,list):u(5,len(v));[val(x)for x in v]
  elif isinstance(v,dict):u(6,len(v));[(t(k),val(x))for k,x in v.items()]
  else:raise ValueError('Unknown effect Variant')
 b.extend(bytes.fromhex(d['player_ir_sha256']));t(d['scene']);u(len(d['sources']))
 for p,h in d['sources'].items():t(p);b.extend(bytes.fromhex(h))
 val(d['creators']);val(d['policy']);val(d['signals']);val(d['functions']);u(len(d['recipes']))
 for recipe,snapshot in zip(d['recipes'],d['native']):
  raw=encode_recipe(recipe);u(len(raw));b.extend(raw);val(snapshot)
 timer=d['timer'];raw=bytes.fromhex(PIN)+struct.pack('<I4If32s32s',1,timer['id'],timer['scene'],timer['mode'],timer['flags'],timer['wait'],bytes.fromhex(timer['scene_sha']),bytes.fromhex(timer['script_sha']));packed=struct.pack('<8s6I',b'ENCFNT01',1,32+len(raw),zlib.crc32(raw),0x454e0044,1,1)+raw;u(len(packed));b.extend(packed)
 struct.pack_into('<8s8I',b,0,b'ENCPFX01',1,128,len(b),zlib.crc32(b[128:]),FAMILY,1,1,d['scene_id']);b[40:60]=bytes.fromhex(PIN);b[60:92]=bytes.fromhex(d['source_sha256']);b[92:124]=bytes.fromhex(sha(IR));return bytes(b)

def stage_files(source):
 raw=encode(load());require((Path(source)/'data/player.enceffects').read_bytes()==raw,'Staged Player effects differ');return {Path('data/player.enceffects'):raw}

def main():
 p=argparse.ArgumentParser();p.add_argument('action',choices=['prepare','extract','compile','verify']);p.add_argument('--directory',type=Path);a=p.parse_args()
 if a.action=='prepare':prepare(a.directory);return
 if a.action=='extract':extract(a.directory);return
 raw=encode(load())
 if a.action=='compile':OUT.parent.mkdir(parents=True,exist_ok=True);OUT.write_bytes(raw)
 else:require(OUT.read_bytes()==raw,'Stale Player effect resource')
 print('Original Player effects:',len(raw),'bytes; complete checked native resources, lifecycle requires actual owner')
if __name__=='__main__':
 try:main()
 except(ValueError,KeyError,TypeError,OSError,struct.error)as e:sys.exit('PLAYER EFFECT SOURCE ERROR: '+str(e))
