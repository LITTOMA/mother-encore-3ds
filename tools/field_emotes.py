#!/usr/bin/env python3
"""Complete source Podunk emotes Ready, signal, tracks, source idle lifecycle."""
from __future__ import annotations
import argparse,hashlib,json,math,re,struct,sys,zlib,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import decode,stable,SCENE,PIN,read,write,sha,require
from tools.extract_battle_entry import Extractor,node,variant,animation
from tools.field_tint import resolve_path
from tools.asset_receipts import receipt_path
SOURCE='Nodes/Ui/emotes.tscn';SCRIPT=SOURCE+'::6'
IR=ROOT/'content/native-field-emotes.json';PACK=ROOT/'romfs/data/podunk-emotes.encemote'
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
  if bindings[path] not in (SCRIPT,):continue
  out.append(dict(stable_id=stable(path),node=path,ready_ordinal=ordinal[path],script=bindings[path],ancestor_bindings=[dict(node=p,script=bindings[p])for p in bindings if path.startswith(p+'/')],overrides=overrides[path],native=names[path],children=[names[p] for p in children[path]]))
 require(len(out)==71,'Podunk emotes binding loss');return out,names,resources,dict(native_export_sha256=sha(native),source_receipt_sha256=sha(receipt),source_files={name:r['sha256'] for name,r in s['files'].items()})

def ident(kind,s):return int.from_bytes(hashlib.sha256((kind+':'+s).encode()).digest()[:4],'little')
ENGINE=dict(commit='3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8',player_sha256='72c1a32819a8d2b7cd1464d7f14057ebea3db4513f615d6fa594e6ce071e4e51',resource_sha256='0c49f1f325d040c645b35b0f39d0972ccd7812e0a575b6ad97ba5d9f3de0dd1b',audio_sha256='ddee1dc22916416899a916bfb85ebd207ff68462087b2b0672a02084cc84604a',audio_url='https://raw.githubusercontent.com/godotengine/godot/3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8/scene/audio/audio_stream_player.cpp',player_url='https://raw.githubusercontent.com/godotengine/godot/3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8/scene/animation/animation_player.cpp',resource_url='https://raw.githubusercontent.com/godotengine/godot/3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8/scene/resources/animation.cpp')
def build(records,nodes,provenance):
 ex=Extractor(ROOT);source=ex.text(SOURCE);ex.text('Nodes/Reusables/character_sprite.tscn');ex.text('Scripts/Main/character_sprite.gd');ex.text('LICENSE');root=node(source,'.');player=node(source,'AnimationPlayer');audio=node(source,'AudioStreamPlayer')
 raw=re.search(r'\[sub_resource type="GDScript" id=6\]\nscript/source = (".*?")\n__meta__',source,re.S)[1];script=json.loads(raw,strict=False)
 require('position.y = -(object.texture.get_height() / object.vframes +'in script and 'while !emotive_parent.has_method("get_direction"):'in script and 'if direction.x < 0:'in script and 'scale.x = -1'in script,'Changed emotes offset/ancestor source')
 sensitive=json.loads(re.search(r'if anim_name in (\[[^\]]+\])',script)[1]);gap=float(re.search(r'object.vframes \+ ([0-9.]+)',script)[1]);exts={int(i):p for p,i in re.findall(r'\[ext_resource path="res://([^"]+)" type="[^"]+" id=(\d+)\]',source)}
 sounds=[]
 for id,path in exts.items():
  if path.startswith('Audio/'):
   ex.data(path);ex.data(path+'.import');sounds.append(dict(id=ident('field-emote-sound',path),source=path,pcm='sound/effects/emote-'+str(ident('field-emote-sound',path))+'.pcm',gain_db=audio.get('volume_db',0),resource=id))
 soundmap={s['resource']:s['id']for s in sounds};clips=[]
 roles={'.:frame':1,'.:offset':2,'AudioStreamPlayer:stream':3,'AudioStreamPlayer:playing':4}
 for prop,ref in player.items():
  if not prop.startswith('anims/'):continue
  name=prop[6:];a=animation(source,ref['SubResource'],SOURCE,name);require(not a['loop'],'Looping emotes requires explicit clock review');tracks=[]
  for t in a['tracks']:
   require(t['type']=='value'and t['path']in roles and t['interp']==1 and t['loop_wrap']is True,'Unreviewed emotes track');k=t['keys'];require(k['update']in(0,1,2)and all(v==1 for v in k['transitions']),'Unreviewed emotes interpolation/easing');role=roles[t['path']];values=[]
   for value in k['values']:
    if role==1:require(type(value)is int and 0<=value<root['hframes']*root['vframes'],'Emote source frame');values.append([value,0])
    elif role==2:require(isinstance(value,list)and len(value)==2,'Emote offset');values.append(value)
    elif role==3:require(type(value)is dict and set(value)=={'ExtResource'}and value['ExtResource']in soundmap,'Emote audio resource');values.append([soundmap[value['ExtResource']],0])
    else:require(type(value)is bool,'Emote playing');values.append([int(value),0])
   require(k['update']!=0 or role==2 or len(values)==1,'Unreviewed continuous non-vector emote track');require(role==2 or k['update']!=2,'Only reviewed offset capture supported');tracks.append(dict(role=role,update=k['update'],keys=[dict(time=t,value=v)for t,v in zip(k['times'],values)]))
  clips.append(dict(id=ident('field-emote-animation',name),name=name,length=a['length'],direction_sensitive=name in sensitive,tracks=tracks))
 out=[]
 for r in records:
  props=decode(r['native']['properties']);ref=r['overrides'].get('objectPath');path=ref['value']if ref else '';target=resolve_path(r['node'],path)if path else '';require(not target or target in nodes,'Emote source objectPath missing')
  directions=[]
  for binding in sorted(r['ancestor_bindings'],key=lambda b:b['node'].count('/'),reverse=True):
   if '::'in binding['script']:continue
   if re.search(r'^func get_direction\(',ex.text(binding['script']),re.M):directions.append(binding);break
  out.append(dict(id=r['stable_id'],parent_id=stable(r['node'].rsplit('/',1)[0]),ready_ordinal=r['ready_ordinal'],node=r['node'],object_path=path,object_id=stable(target)if target else 0,direction_id=stable(directions[0]['node'])if directions else 0,position=props['position'],offset=props['offset'],scale=props['scale'],columns=props['hframes'],rows=props['vframes'],frame=props['frame'],visible=props['visible'],centered=props['centered'],z_index=props['z_index']))
 for p,h in provenance['source_files'].items():ex.data(p);require(ex.sources[p]==h,'Changed emotes native closure')
 texture=exts[root['texture']['ExtResource']];size=ex.png_size(texture)
 return dict(schema=1,kind='encore.field-emotes.source-ir',commit=PIN,scene=SCENE,scene_id=stable('.'),engine=ENGINE,bubble_gap=gap,texture=dict(source=texture,output='graphics/actors/emotes.t3x',size=size),default_sound=soundmap[audio['stream']['ExtResource']],bus=audio['bus'],sounds=sounds,clips=clips,records=out,sources=dict(sorted(ex.sources.items())),provenance=provenance,license_review='Pinned upstream LICENSE permits source assets in this game-related 3DS port; existing reviewed actor atlas is borrowed, no duplicate image conversion.',semantics=['71 actual Podunk source emotes children; onready nullable objectPath resolution does not call set_bubble_offset','CharacterSprite sprite_changed synchronously invokes source bubble Y from actual current texture height/vframes plus source gap','sweat/angry search nearest source ancestor get_direction; negative X writes -1, nonnegative X retains previous flip; all other clips write +1','AnimationPlayer play does not immediately update tracks; same assigned animation resumes unless ended, animation_started signal runs synchronously','Ordered source frame/offset/stream/playing tracks, float32 clock, half-open discrete key intervals with source end tolerance; hidden children continue processing','UPDATE_CAPTURE first time-zero offset is ignored for value, captures current property and interpolates to next key; all source easing is linear','Clip end preserves actual source final properties and emits animation_finished after property publication'],unsupported=['Dynamic factory emotes require complete native source postorder admission','PayPhone empty objectPath setter and directional clips without a get_direction ancestor reject actual invalid source call; ordinary clips remain supported'],unverified=['No manual tests run','3DS integrated draw/audio and emulator/hardware pending'])

def validate(d):
 require(d['schema']==1 and d['kind']=='encore.field-emotes.source-ir'and d['commit']==PIN and d['scene']==SCENE and d['engine']==ENGINE and len(d['records'])==71 and len(d['clips'])==14 and len(d['sounds'])==2 and math.isfinite(d['bubble_gap'])and 0<=d['bubble_gap']<=1024,'Emotes schema/source topology')
 soundids={s['id']for s in d['sounds']};ids=set();last=-1
 for r in d['records']:
  require(r['id']==stable(r['node'])and r['id']not in ids and r['ready_ordinal']>last and r['parent_id']and type(r['visible'])is bool and type(r['centered'])is bool and 0<r['columns']<=1024 and 0<r['rows']<=1024 and 0<=r['frame']<r['columns']*r['rows'],'Emotes descriptor identity');ids.add(r['id']);last=r['ready_ordinal']
  require(all(type(v)in(int,float)and math.isfinite(v)and abs(v)<=1000000 for v in r['position']+r['offset']+r['scale']),'Emotes transforms')
  require((r['object_path']and r['object_id']==stable(resolve_path(r['node'],r['object_path'])))or(not r['object_path']and not r['object_id']),'Emotes nullable objectPath')
 ids=set()
 for a in d['clips']:
  require(a['id']==ident('field-emote-animation',a['name'])and a['id']not in ids and 0<a['length']<=86400 and type(a['direction_sensitive'])is bool and 0<len(a['tracks'])<=8,'Emote clip');ids.add(a['id']);roles=set()
  for t in a['tracks']:
   require(t['role']in(1,2,3,4)and t['role']not in roles and t['update']in(0,1,2)and(t['update']!=2 or t['role']==2)and(t['update']!=0 or t['role']==2 or len(t['keys'])==1)and 0<len(t['keys'])<=1024,'Emote track');roles.add(t['role']);last=-1
   for k in t['keys']:
    require(math.isfinite(k['time'])and last<=k['time']<=a['length'] and len(k['value'])==2 and all(type(v)in(int,float)and math.isfinite(v)for v in k['value']),'Emote key');last=k['time'];v=k['value'][0]
    if t['role']==1:require(type(v)is int and 0<=v<144,'Emote frame')
    if t['role']==3:require(v in soundids,'Emote audio reference')
    if t['role']==4:require(v in(0,1),'Emote bool')
 require(d['default_sound']in soundids and d['texture']['source']in d['sources']and d['texture']['size']==[372,384],'Emote atlas/default sound');return d

def encode(d):
 validate(d);out=bytearray(64)
 def u(*v):out.extend(struct.pack('<'+'I'*len(v),*v))
 def f(*v):out.extend(struct.pack('<'+'f'*len(v),*v))
 def s(v):b=v.encode();u(len(b));out.extend(b)
 f(d['bubble_gap']);u(d['default_sound'],*d['texture']['size']);s(d['texture']['source']);s(d['texture']['output']);s(d['bus']);u(len(d['sounds']))
 for sound in d['sounds']:u(sound['id']);f(sound['gain_db']);s(sound['source']);s(sound['pcm'])
 u(len(d['clips']))
 for a in d['clips']:
  u(a['id'],int(a['direction_sensitive']));f(a['length']);s(a['name']);u(len(a['tracks']))
  for t in a['tracks']:
   u(t['role'],t['update'],len(t['keys']))
   for k in t['keys']:
    f(k['time'])
    if t['role']==2:f(*k['value'])
    else:u(*k['value'])
 for r in d['records']:
  u(r['id'],r['parent_id'],r['ready_ordinal'],r['object_id'],r['direction_id'],r['columns'],r['rows'],r['frame'],int(r['visible'])|int(r['centered'])<<1);out.extend(struct.pack('<i',r['z_index']));f(*r['position'],*r['offset'],*r['scale']);s(r['node']);s(r['object_path'])
 u(len(d['sources']))
 for name,h in d['sources'].items():s(name);out.extend(bytes.fromhex(h))
 struct.pack_into('<8s6I20sII4x',out,0,b'ENCEMO01',1,len(out),0,1,1,len(d['records']),bytes.fromhex(PIN),d['scene_id'],len(d['sources']));struct.pack_into('<I',out,16,zlib.crc32(out)&0xffffffff);return bytes(out)

def load():
 d=validate(read(IR));r=read(ROOT/'reports/field-emotes/source-review.json');require(r['ir_sha256']==sha(IR)and r['sources']==d['sources']and r['engine']==ENGINE and r['commit']==PIN,'Emotes reviewed source receipt');ex=Extractor(ROOT)
 for p,h in d['sources'].items():ex.data(p);require(ex.sources[p]==h,'Changed emotes source '+p)
 return d

def audio_bindings(root=ROOT):
 d=load();return[dict(source=s['source'],identity=dict(kind='stable',value=s['id']),pcm=s['pcm'],gain_db=s['gain_db'],conversion=None)for s in d['sounds']]

def stage_files(source):
 d=load();raw=encode(d);require((Path(source)/'data/podunk-emotes.encemote').read_bytes()==raw,'Stale emotes binary');return{Path('data/podunk-emotes.encemote'):raw}

def main():
 p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile','verify']);p.add_argument('--native',type=Path);p.add_argument('--source',type=Path);a=p.parse_args()
 if a.action=='extract':
  require(a.native and a.source,'Complete official native/source export required');records,nodes,resources,provenance=source_instances(a.native,a.source,ROOT/'upstream/MOTHER-Encore');d=validate(build(records,nodes,provenance));write(IR,d);write(ROOT/'reports/field-emotes/source-review.json',dict(schema=1,commit=PIN,ir_sha256=sha(IR),sources=d['sources'],engine=ENGINE,semantics=d['semantics'],unsupported=d['unsupported'],unverified=d['unverified']));print('Emotes source: 71 source Ready bindings, 14 complete clips, 2 source sounds; shared atlas');return
 raw=encode(load())
 if a.action=='compile':PACK.parent.mkdir(parents=True,exist_ok=True);PACK.write_bytes(raw)
 else:require(PACK.read_bytes()==raw,'Stale emotes binary')
 print('Field emotes:',len(raw),'checked bytes; ordered source frame/offset/audio tracks')
if __name__=='__main__':
 try:main()
 except(ValueError,KeyError,OSError,TypeError,struct.error)as e:sys.exit('FIELD EMOTES ERROR: '+str(e))
