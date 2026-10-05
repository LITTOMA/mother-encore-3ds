#!/usr/bin/env python3
"""Checked Podunk CharacterSprite/Fetcher source lifecycle bridge; no generic Godot VM."""
from __future__ import annotations
import argparse,hashlib,json,math,re,struct,sys,zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import decode,stable,SCENE,PIN,read,write,sha,require
from tools.extract_battle_entry import Extractor,node,variant
from tools.field_tint import resolve_path,source_id
CHARACTER='Scripts/Main/character_sprite.gd';FETCHER='Scripts/Main/SpriteDataFetcher.gd'
IR=ROOT/'content/native-field-sprite-bridge.json';PACK=ROOT/'romfs/data/podunk-sprites.encsprite'
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
  if bindings[path] not in (CHARACTER,FETCHER):continue
  out.append(dict(stable_id=stable(path),node=path,ready_ordinal=ordinal[path],script=bindings[path],overrides=overrides[path],native=names[path],children=[names[p] for p in children[path]]))
 require(len(out)==150,'Podunk sprite child binding loss');return out,names,resources,dict(native_export_sha256=sha(native),source_receipt_sha256=sha(receipt),source_files={name:r['sha256'] for name,r in s['files'].items()})
def identity(kind,path):return int.from_bytes(hashlib.sha256((kind+':'+path).encode()).digest()[:4],'little')
def build(records,nodes,resources,provenance,npcs):
 ex=Extractor(ROOT);char=ex.text(CHARACTER);fetch=ex.text(FETCHER);ex.text('Scripts/misc/FloorReflector.gd');ex.text('Scripts/misc/character_reflection.gd');ex.text('Nodes/Reusables/character_sprite.tscn');ex.text('LICENSE')
 require('var animTree = AnimationTree.new()'in char and '_directional_tags.append([i, directionalAnims])'in char and '_all_tags.clear()'in char and '_directional_tags.clear()'not in char and 'offset += Vector2(_json_data["offset"][0], _json_data["offset"][1])'in char and 'emit_signal("sprite_changed")'in char,'Changed CharacterSprite lifecycle')
 require('elif !(reflect_node):'in fetch and 'if(is_instance_valid(_obj_reflection)):'in fetch and fetch.index('_obj_reflection.queue_free()')<fetch.index('_has_reflection = false',fetch.index('func delete_reflection')),'Changed Fetcher reflection lifecycle')
 queried=re.search(r'get_node_or_null\("([^"]+)"\)',fetch)[1];require(queried not in nodes,'Podunk has a reflector: explicit native reflection adapter required')
 fetch_offset=float(re.search(r'export var reflect_offset: float = ([0-9.]+)',fetch)[1]);auto_offset=re.search(r'export var _auto_offset = (true|false)',char)[1]=='true'
 fallback=re.search(r'travel\("([^"]+)"\)',char)[1];textures={};animations={};out=[]
 def texture(path):
  path=path.removeprefix('res://');size=ex.png_size(path);key=identity('field-sprite-texture',path)
  textures[key]=dict(id=key,path=path,size=size);return key
 def animation(path):
  path=path.removeprefix('res://')
  if not path:return 0
  key=identity('field-sprite-animation',path)
  if key in animations:return key
  d=ex.yaml(path);require(set(d)=={'animations','offset','size','type'}and d['type']==0,'Unknown CharacterSprite YAML schema');motions=[]
  for name,m in d['animations'].items():
   require(set(m)=={'directions','type'}and m['type']in(0,1)and len(m['directions'])in(1,2,4,8),'Unknown sprite motion');directions=[]
   basis=[[0,0]]if len(m['directions'])==1 else [[-1,0],[1,0]]if len(m['directions'])==2 else [[0,1],[-1,0],[1,0],[0,-1],[-2**-.5,2**-.5],[2**-.5,2**-.5],[-2**-.5,-2**-.5],[2**-.5,-2**-.5]]
   for i,keys in enumerate(m['directions']):
    require(len(keys)==len(m['directions'][0]),'Source first-direction frameCount requires explicit adapter');t=float(keys[0]);frames=[]
    for frame,duration in keys[1:]:frames.append([t,frame-1]);t+=duration
    directions.append(dict(vector=basis[i],duration=t,keys=frames))
   motions.append(dict(name=name,loop=m['type']==0,directions=directions))
  animations[key]=dict(id=key,path=path,columns=d['size'][0],rows=d['size'][1],offset=d['offset'],motions=motions);return key
 npcs={n['node']:n for n in npcs['npcs']}
 for rec in records:
  p=decode(rec['native']['properties']);v=rec['overrides'];parent=rec['node'].rsplit('/',1)[0];kind=1 if rec['script']==CHARACTER else 2
  r=dict(id=rec['stable_id'],parent_id=stable(parent),ready_ordinal=rec['ready_ordinal'],kind=kind,node=rec['node'],target_path='',target_id=0,flags=0,columns=0,rows=0,frame=0,texture=0,sprite=0,initial_animation=0,setup_texture=0,setup_animation=0,offset=[0,0],extra_offset=[0,0],reflect_offset=0,connections=[])
  if kind==1:
   n=npcs[parent];r.update(columns=p['hframes'],rows=p['vframes'],frame=p['frame'],texture=texture(resources[p['texture']['id']]['path']),sprite=texture(v.get('sprite','')) if v.get('sprite','') else 0,initial_animation=animation(v.get('yaml','')),offset=p['offset'],flags=int(v.get('_auto_offset',auto_offset))|int(p['visible'])<<1,setup_texture=texture(n['sprite']),setup_animation=animation(n['animation']),connections=n['connections'])
   a=animations[r['setup_animation']];r['extra_offset']=[n['sprite_offset'][0]-a['offset'][0],n['sprite_offset'][1]+int(n['size'][1]/float(n['rows']*2))-a['offset'][1]]
   require(rec['node']+'/AnimationPlayer'in nodes and rec['node']+'/emotes'in nodes,'CharacterSprite required source children absent')
  else:
   ref=v['_sprite_path'];require(ref['type']=='NodePath','Fetcher source NodePath');target=resolve_path(rec['node'],ref['value']);require(target in nodes and nodes[target]['class']in('Sprite','AnimatedSprite'),'Fetcher nullable Sprite binding needs explicit adapter')
   r.update(target_path=ref['value'],target_id=stable(target),flags=int(v.get('_ignore_reflection',False))|int(v.get('_toggle_flip_reflection',True))<<1,reflect_offset=v.get('reflect_offset',fetch_offset))
  out.append(r)
 sources={**provenance['source_files'],**ex.sources}
 for name,h in provenance['source_files'].items():ex.data(name);require(ex.sources[name]==h,'Changed native source closure')
 sources.update(ex.sources)
 return dict(schema=1,kind='encore.field-sprite-bridge.source-ir',commit=PIN,scene=SCENE,scene_id=stable('.'),reflector=dict(query_path=queried,exists=False),fallback=fallback,textures=list(textures.values()),animations=list(animations.values()),records=out,sources=dict(sorted(sources.items())),provenance=provenance,semantics=['All 67 CharacterSprite and 83 Fetcher source postorder Ready bindings; no RNG','Character Ready always creates an AnimationTree; one actual child initializes 4dir before parent setup','Parent NPC checked set_sprite, set_animation, set_spritesheet, set_sprite_offset order; no second animation VM','Directional tags append across rebuilds; unknown travel is the explicit source no-op','Spritesheet auto Y offset truncates and YAML offset adds to current offset; sprite_changed synchronously invokes source emotes connection','Fetcher resolves nullable target at onready and re-queries a checked current scene every process; absence queues only valid reflection for deferred deletion','Podunk root FloorReflector absence proved by complete official 8686-node native export; reflective scene requires all explicit typed Host lifecycle callbacks'],unsupported=['Dynamic BasicEnemy/actor factories require a separate complete native postorder/source template admission','FloorReflector material/reflection draw implementation is an explicit external Host dependency for reflective scenes, never admitted through the Podunk absence fact'],unverified=['Manual tests written, not run','3DS integration, emulator and hardware pending'])

def validate(d):
 require(d['schema']==1 and d['kind']=='encore.field-sprite-bridge.source-ir'and d['commit']==PIN and d['scene']==SCENE and d['scene_id']==stable('.')and d['reflector']=={'query_path':'FloorReflector','exists':False},'Sprite source scene/capability fact')
 require(len(d['records'])==150 and sum(r['kind']==1 for r in d['records'])==67,'Sprite child topology')
 tex={t['id']:t for t in d['textures']};anim={a['id']:a for a in d['animations']};require(len(tex)==len(d['textures'])and len(anim)==len(d['animations']),'Duplicate sprite resource IDs')
 for t in tex.values():require(t['id']==identity('field-sprite-texture',t['path'])and t['path']in d['sources']and len(t['size'])==2 and all(type(n)is int and 0<n<=16384 for n in t['size']),'Sprite texture source/extent')
 for a in anim.values():
  require(a['id']==identity('field-sprite-animation',a['path'])and a['path']in d['sources']and all(type(n)is int and 0<n<=1024 for n in(a['columns'],a['rows']))and len(a['offset'])==2,'Sprite animation source/grid')
  names=set()
  for m in a['motions']:
   require(m['name']and m['name']not in names and type(m['loop'])is bool and len(m['directions'])in(1,2,4,8),'Sprite motion');names.add(m['name'])
   for v in m['directions']:
    require(len(v['vector'])==2 and 0<v['duration']<=86400 and v['keys'],'Sprite motion direction');last=-1
    for t,f in v['keys']:require(type(f)is int and 0<=f<a['columns']*a['rows']and math.isfinite(t)and last<=t<v['duration'],'Sprite discrete key');last=t
 ids=set();last=-1
 for r in d['records']:
  require(r['id']==stable(r['node'])and r['id']not in ids and r['ready_ordinal']>last and r['kind']in(1,2)and r['flags']in range(4),'Sprite source identity/Ready');ids.add(r['id']);last=r['ready_ordinal']
  require(all(type(n)in(int,float)and math.isfinite(n)and abs(n)<=1000000 for n in r['offset']+r['extra_offset']+[r['reflect_offset']]),'Sprite finite source parameters')
  if r['kind']==1:
   require(r['texture']in tex and (r['sprite']==0 or r['sprite']in tex)and(r['initial_animation']==0 or r['initial_animation']in anim)and r['setup_texture']in tex and r['setup_animation']in anim and 0<r['columns']<=1024 and 0<r['rows']<=1024 and 0<=r['frame']<r['columns']*r['rows'],'Sprite character resource refs')
   require(all(len(c)==3 and c[2]in(0,1,2)for c in r['connections']),'Sprite source transitions')
  else:require(r['target_path']and r['target_id']==stable(resolve_path(r['node'],r['target_path']))and not any(r[k]for k in('columns','rows','frame','texture','sprite','initial_animation','setup_texture','setup_animation'))and not r['connections'],'Fetcher source target')
 return d

def encode(d):
 validate(d);out=bytearray(64)
 def u(*v):out.extend(struct.pack('<'+'I'*len(v),*v))
 def f(*v):out.extend(struct.pack('<'+'f'*len(v),*v))
 def s(v):b=v.encode();u(len(b));out.extend(b)
 s(d['fallback']);s(d['reflector']['query_path']);u(int(d['reflector']['exists']));u(len(d['textures']))
 for t in d['textures']:u(t['id'],*t['size']);s(t['path'])
 u(len(d['animations']))
 for a in d['animations']:
  u(a['id'],a['columns'],a['rows']);f(*a['offset']);s(a['path']);u(len(a['motions']))
  for m in a['motions']:
   s(m['name']);u(int(m['loop']),len(m['directions']))
   for v in m['directions']:
    f(*v['vector'],v['duration']);u(len(v['keys']))
    for t,frame in v['keys']:f(t);u(frame)
 for r in d['records']:
  u(r['id'],r['parent_id'],r['ready_ordinal'],r['kind'],r['flags'],r['target_id'],r['columns'],r['rows'],r['frame'],r['texture'],r['sprite'],r['initial_animation'],r['setup_texture'],r['setup_animation']);f(*r['offset'],*r['extra_offset'],r['reflect_offset']);s(r['node']);s(r['target_path']);u(len(r['connections']))
  for a,b,mode in r['connections']:s(a);s(b);u(mode)
 u(len(d['sources']))
 for name,h in d['sources'].items():s(name);out.extend(bytes.fromhex(h))
 struct.pack_into('<8s6I20sII4x',out,0,b'ENCSPR01',1,len(out),0,1,1,len(d['records']),bytes.fromhex(PIN),d['scene_id'],len(d['sources']));struct.pack_into('<I',out,16,zlib.crc32(out)&0xffffffff);return bytes(out)

def load():
 d=validate(read(IR));review=read(ROOT/'reports/field-sprite-bridge/source-review.json');require(review['ir_sha256']==sha(IR)and review['sources']==d['sources']and review['commit']==PIN,'Sprite source review receipt');ex=Extractor(ROOT)
 for p,h in d['sources'].items():ex.data(p);require(ex.sources[p]==h,'Changed sprite source '+p)
 return d

def stage_files(source):
 raw=encode(load());require((Path(source)/'data/podunk-sprites.encsprite').read_bytes()==raw,'Stale sprite bridge binary');return{Path('data/podunk-sprites.encsprite'):raw}

def main():
 p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile','verify']);p.add_argument('--native',type=Path);p.add_argument('--source',type=Path);a=p.parse_args()
 if a.action=='extract':
  require(a.native and a.source,'Official complete native/source exports required');from tools.field_npc import extract as extract_npc
  npcs=extract_npc(a.native,a.source);records,nodes,resources,provenance=source_instances(a.native,a.source,ROOT/'upstream/MOTHER-Encore');d=validate(build(records,nodes,resources,provenance,npcs));write(IR,d);write(ROOT/'reports/field-sprite-bridge/source-review.json',dict(schema=1,commit=PIN,ir_sha256=sha(IR),sources=d['sources'],semantics=d['semantics'],unsupported=d['unsupported'],unverified=d['unverified']));print('Sprite bridge source: 67 CharacterSprite, 83 Fetchers;',len(d['animations']),'actual animation resources; checked no FloorReflector');return
 raw=encode(load())
 if a.action=='compile':PACK.parent.mkdir(parents=True,exist_ok=True);PACK.write_bytes(raw)
 else:require(PACK.read_bytes()==raw,'Stale Sprite bridge pack')
 print('Field sprite bridge:',len(raw),'checked bytes; no animation VM or RNG')
if __name__=='__main__':
 try:main()
 except(ValueError,KeyError,OSError,TypeError,struct.error)as e:sys.exit('FIELD SPRITE BRIDGE ERROR: '+str(e))
