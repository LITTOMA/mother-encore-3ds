#!/usr/bin/env python3
"""Checked source route compiler; candidate output until all scene assets exist."""
from __future__ import annotations
import argparse, hashlib, json, math, re, struct, sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import podunk_field as f
from tools.godot_text import instantiate,Transform2D
from tools.resource_catalog import ROLES
RECIPE=ROOT/'content/world-scene-routes.json'
STRIDES=[1,24,84,36,4]
PROP_STRIDES=[*STRIDES,8]
OBJECT_STRIDES=[*PROP_STRIDES,8]

def strides(recipe):return OBJECT_STRIDES if recipe['schema']==4 else PROP_STRIDES if recipe['schema']==3 else STRIDES
def capability(recipe):return {1:2,2:3,3:4,4:5}[recipe['schema']]

def fields(value,names,what):
 f.require(type(value)is dict and set(value)==set(names),'Unknown/missing '+what+' fields')
def integer(value,low,high,what):
 f.require(type(value)is int and low<=value<=high,'Invalid '+what)
def text(value,what):
 f.require(type(value)is str and value and '\x00'not in value and '\\'not in value and all(p not in ('','.','..')for p in value.split('/')),'Invalid '+what)
def typed(value,role,optional=False):
 integer(value,0 if optional else 1,0xffffffff,'resource identity')
 f.require(role not in ('FieldSteps','FieldProps','FieldObjects')or optional and value==0 or value>=1024,'Field resource requires independent identity')
 f.require(optional and value==0 or value==ROLES[role] or role in ('FieldRoom','FieldMap','FieldStory','FieldMusic','FieldAudio','FieldMusicZones','FieldSteps','FieldProps','FieldObjects')and value>=1024,'Wrong typed resource identity')

def validate(recipe):
 fields(recipe,['schema','kind','commit','scope','door','scenes','routes'],'route recipe')
 f.require(type(recipe['schema'])is int and recipe['schema']in(1,2,3,4) and recipe['kind']=='encore.world-scene-routes.recipe'and recipe['commit']==f.PIN,'Unsupported source route recipe')
 f.require(type(recipe['scope'])is str and recipe['scope']and chr(0)not in recipe['scope'],'Missing/invalid scope')
 fields(recipe['door'],['scene','script','transition','destination_offset_y'],'door adapter')
 for key in ('scene','script','transition'):text(recipe['door'][key],'door source')
 f.require(type(recipe['door']['destination_offset_y'])is int,'Door offset is not an integer')
 f.require(type(recipe['scenes'])is list and 2<=len(recipe['scenes'])<=64,'Scene count')
 ids=set();sources=set()
 for scene in recipe['scenes']:
  fields(scene,['id','source','room','map','assets'],'scene');integer(scene['id'],1,0xffffffff,'scene identity');text(scene['source'],'scene source')
  f.require(scene['source'].startswith('Maps/')and scene['source'].endswith('.tscn')and scene['id']not in ids and scene['source']not in sources,'Duplicate/invalid scene')
  ids.add(scene['id']);sources.add(scene['source'])
  if scene['map']:
   typed(scene['room'],'FieldRoom');typed(scene['map'],'FieldMap');a=scene['assets'];fields(a,['story','music','audio','zones','mick','npcs']+(['steps']if recipe['schema']>=2 else [])+(['props']if recipe['schema']>=3 else [])+(['objects']if recipe['schema']==4 else []),'scene assets')
   for key,role in [('story','FieldStory'),('music','FieldMusic'),('audio','FieldAudio'),('zones','FieldMusicZones'),('mick','MickTreats')]:typed(a[key],role,True)
   if recipe['schema']>=2:typed(a['steps'],'FieldSteps',True)
   if recipe['schema']>=3:typed(a['props'],'FieldProps',True)
   if recipe['schema']==4:typed(a['objects'],'FieldObjects',True)
   f.require(bool(a['music'])==bool(a['audio'])==bool(a['zones']),'Incomplete scene music group')
   f.require(type(a['npcs'])is list and len(a['npcs'])<=128 and len(set(a['npcs']))==len(a['npcs']),'NPC reference count/duplicates')
   for npc in a['npcs']:integer(npc,256,0xffffffff,'NPC resource identity')
  else:typed(scene['room'],'Room');f.require(type(scene['map'])is int and scene['assets']is None,'House cannot own field assets')
 f.require(recipe['schema']<2 or any(scene['map']and scene['assets']['steps']for scene in recipe['scenes']),'Route schema requires actual stepping bindings')
 if recipe['schema']>=3:
  props=[s['assets']['props']for s in recipe['scenes']if s['map']and s['assets']['props']]
  f.require(props and len(set(props))==len(props),'Route schema3 requires unique actual prop owners')
 if recipe['schema']==4:
  objects=[s['assets']['objects']for s in recipe['scenes']if s['map']and s['assets']['objects']]
  f.require(objects and len(set(objects))==len(objects)and not set(objects)&set(props),'Route schema4 requires unique actual object owners')
 f.require(sum(len(scene['assets']['npcs'])for scene in recipe['scenes']if scene['map'])<=4096,'NPC pool capacity')
 f.require(type(recipe['routes'])is list and 1<=len(recipe['routes'])<=256,'Route count')
 routes=set();doors=set()
 for route in recipe['routes']:
  fields(route,['id','from','to','door'],'route');integer(route['id'],1,0xffffffff,'route identity');text(route['door'],'door node')
  integer(route['from'],1,0xffffffff,'route origin');integer(route['to'],1,0xffffffff,'route destination')
  f.require(route['from']in ids and route['to']in ids and route['id']not in routes and (route['from'],route['door'])not in doors,'Unknown/duplicate route')
  routes.add(route['id']);doors.add((route['from'],route['door']))
 return recipe

def load(path=RECIPE):
 def unique(pairs):
  out={}
  for k,v in pairs:f.require(k not in out,'Duplicate recipe field');out[k]=v
  return out
 return validate(json.loads(Path(path).read_text(encoding='utf-8'),object_pairs_hook=unique))

def transforms(nodes):
 out={}
 for node in nodes:
  parent=f.parent_path(node.path);base=out[parent]if parent is not None else Transform2D(1,0,0,1,0,0)
  if node.type not in f.NODE2D and node.type not in f.CANVAS_CONTROL and parent is not None:out[node.path]=Transform2D(1,0,0,1,0,0)
  else:out[node.path]=f.compose(base,f.local_transform(node))
 return out

def rows(recipe,sources):
 validate(recipe);script=sources.text(recipe['door']['script']);transition=sources.text(recipe['door']['transition'])
 cross=re.search(r'Vector2[(]targetX, targetY - ([0-9]+)[)]',script);same=re.search(r'_new_pos[.]global_position - Vector2[(]0, ([0-9]+)[)]',script)
 f.require(cross and same and int(cross[1])==int(same[1])==-recipe['door']['destination_offset_y'],'Door destination offset source changed')
 f.require('var same_scene: bool = !door.targetScene or global.currentScene.get_name() == door.targetScene'in transition and 'if !same_scene and door.fadeout_music_on_scene_change:'in transition,'Door same-scene/fade semantics changed')
 scenes={s['id']:s for s in recipe['scenes']};nodes={};xforms={}
 for identity,scene in scenes.items():
  flat=instantiate(sources.doc,scene['source']);nodes[identity]={n.path:n for n in flat};xforms[identity]=transforms(flat)
 out=[]
 for route in recipe['routes']:
  source=nodes[route['from']];f.require(route['door']in source,'Source door is absent')
  values,target=f.door_parameters(sources,recipe,source,route['door'])
  same_scene=not values['targetScene']or source['.'].name==values['targetScene']
  f.require(same_scene==(route['from']==route['to']),'Route ownership disagrees with original same-scene condition')
  if same_scene:
   f.require(target in xforms[route['from']],'Internal door has no destination node');pose=xforms[route['from']][target];x,y=pose.ox,pose.oy+recipe['door']['destination_offset_y']
  else:
   f.require('Maps/'+values['targetScene']+'.tscn'==scenes[route['to']]['source'],'Route target differs from source door');x,y=values['targetX'],values['targetY']+recipe['door']['destination_offset_y']
  row=dict(id=route['id'],frm=route['from'],to=route['to'],door=route['door'],x=f.f32(x),y=f.f32(y),dx=f.f32(values['dir'].x),dy=f.f32(values['dir'].y),sound=''if values['sound']=='None'else'res://Audio/Sound effects/'+values['sound'],end_sound=''if values['end_sound']=='None'else'res://Audio/Sound effects/'+values['end_sound'],in_kind=f.TRANSITIONS[values['transit_in_anim']],out_kind=f.TRANSITIONS[values['transit_out_anim']or values['transit_in_anim']],in_speed=f.f32(values['fade_in_speed']),out_speed=f.f32(values['fade_out_speed']),music_fade=f.f32(values['fadeout_music_length'])if not same_scene and values['fadeout_music_on_scene_change']else 0,flag=values['flag_set'],flag_value=bool(values['set_flag_state']))
  f.require(all(math.isfinite(row[k])for k in ('x','y','dx','dy','in_speed','out_speed','music_fade')),'Nonfinite route')
  for key in ('sound','end_sound'):
   if row[key]:sources.data(row[key].removeprefix('res://'))
  out.append(row)
 return out

def encode(recipe,routes):
 validate(recipe);strings=bytearray();index={}
 def S(value):
  if not value:return 0,0
  if value not in index:raw=value.encode('utf-8');index[value]=len(strings),len(raw);strings.extend(raw)
  return index[value]
 scenes=bytearray();assets=bytearray();npcs=bytearray();routes_blob=bytearray();props=bytearray();objects=bytearray()
 for scene in recipe['scenes']:
  scenes.extend(struct.pack('<6I',scene['id'],*S('res://'+scene['source']),scene['room'],scene['map'],0))
  if scene['map']:
   a=scene['assets'];first=len(npcs)//4;assets.extend(struct.pack('<9I',scene['id'],a['story'],a['music'],a['audio'],a['zones'],a['mick'],first if a['npcs']else 0,len(a['npcs']),a.get('steps',0)))
   if recipe['schema']>=3 and a['props']:props.extend(struct.pack('<2I',scene['id'],a['props']))
   if recipe['schema']==4 and a['objects']:objects.extend(struct.pack('<2I',scene['id'],a['objects']))
   for npc in a['npcs']:npcs.extend(struct.pack('<I',npc))
 f.require(len(routes)==len(recipe['routes'])and [(r['id'],r['frm'],r['to'],r['door'])for r in routes]==[(r['id'],r['from'],r['to'],r['door'])for r in recipe['routes']],'Source route identity order differs')
 for r in routes:
  fields(r,['id','frm','to','door','x','y','dx','dy','sound','end_sound','in_kind','out_kind','in_speed','out_speed','music_fade','flag','flag_value'],'lowered route')
  f.require(all(type(r[k])in (int,float)and math.isfinite(r[k])and abs(r[k])<=1000000 for k in ('x','y','dx','dy','in_speed','out_speed','music_fade')),'Invalid route numbers')
  f.require((r['dx']==0 and abs(r['dy'])in (0,1))or (r['dy']==0 and abs(r['dx'])==1),'Invalid route direction')
  f.require(0<r['in_speed']<=16 and 0<r['out_speed']<=16 and 0<=r['music_fade']<=60 and type(r['flag_value'])is bool,'Invalid route tuning/flag')
  f.require(type(r['in_kind'])is int and 0<=r['in_kind']<=2 and type(r['out_kind'])is int and 0<=r['out_kind']<=2,'Invalid transition kind')
  for key,limit,prefix in [('sound',256,'res://'),('end_sound',256,'res://'),('flag',64,'')]:f.require(type(r[key])is str and len(r[key].encode('utf-8'))<=limit and all(ord(c)>=32 for c in r[key])and (not r[key]or r[key].startswith(prefix)),'Invalid route '+key)
  routes_blob.extend(struct.pack('<3I2I4f4I2H3f2I2I',r['id'],r['frm'],r['to'],*S(r['door']),r['x'],r['y'],r['dx'],r['dy'],*S(r['sound']),*S(r['end_sound']),r['in_kind'],r['out_kind'],r['in_speed'],r['out_speed'],r['music_fade'],*S(r['flag']),int(r['flag_value']),int(r['music_fade']>0)))
 f.require(len(strings)<=32768,'World route text pool capacity')
 return f.pack_container(f.LINK_MAGIC,0,[strings,scenes,routes_blob,assets,npcs]+([props]if recipe['schema']>=3 else [])+([objects]if recipe['schema']==4 else []),strides(recipe),f.LINK_LIMIT,capability=capability(recipe))

def build(recipe=None):
 recipe=load()if recipe is None else validate(recipe);sources=f.Sources();routes=rows(recipe,sources);blob=encode(recipe,routes)
 return blob,{'scope':recipe['scope'],'commit':f.PIN,'recipe_sha256':hashlib.sha256(RECIPE.read_bytes()).hexdigest(),'producer_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),'sources':sources.digests(),'routes':routes,'bytes':len(blob),'sha256':hashlib.sha256(blob).hexdigest()}

def main():
 p=argparse.ArgumentParser();p.add_argument('--out',type=Path);args=p.parse_args();blob,proof=build()
 if args.out:
  dest=args.out.resolve();f.require(dest.is_relative_to((ROOT/'build').resolve()),'Candidate output must stay under build until scene assets are admitted');dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(blob)
 print(json.dumps(proof,ensure_ascii=False,indent=2))
if __name__=='__main__':main()
