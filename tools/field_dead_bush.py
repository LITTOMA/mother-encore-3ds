#!/usr/bin/env python3
"""Pinned Podunk DeadBush source lifecycle, tracks and checked native atlas."""
from __future__ import annotations
import argparse,csv,hashlib,io,json,math,re,struct,sys,zlib,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import decode,stable,SCENE,PIN,read,write,sha,require
from tools.extract_battle_entry import Extractor,node,animation
from tools.field_tint import resolve_path
SCRIPT='Scripts/Main/Dead Bush.gd';SOURCE='Nodes/Overworld/Objects/Dead Bush.tscn'
IR=ROOT/'content/native-field-dead-bush.json';PACK=ROOT/'romfs/data/podunk-dead-bush.encbush'
REVIEW=ROOT/'reports/field-dead-bush/source-review.json'
RECEIPT=ROOT/'content/asset-receipts/graphics/objects/dead-bush/source.json'
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
  out.append(dict(stable_id=stable(path),node=path,ready_ordinal=ordinal[path],script=bindings[path],ancestor_bindings=[dict(node=p,script=bindings[p])for p in bindings if path.startswith(p+'/')],overrides=overrides[path],native=names[path],children=[names[p] for p in children[path]]))
 require(len(out)==13,'Podunk DeadBush binding loss');return out,names,resources,dict(native_export_sha256=sha(native),source_receipt_sha256=sha(receipt),source_files={name:r['sha256'] for name,r in s['files'].items()})

def ident(kind,value):return int.from_bytes(hashlib.sha256((kind+':'+value).encode()).digest()[:4],'little')
def build(records,nodes,provenance):
 ex=Extractor(ROOT);scene=ex.text(SOURCE);script=ex.text(SCRIPT);ex.text('Scripts/Main/CutsceneArea.gd');global_source=ex.text('Scripts/global/global.gd');global_data=ex.text('Scripts/global/globalData.gd');ex.text('LICENSE')
 require(re.findall(r'^func ([A-Za-z_]+)\(',script,re.M)==['_ready','grow','interact','_on_Hitbox_area_entered','_on_AnimationPlayer_animation_finished'],'Changed DeadBush methods')
 required=['visibilityNotifier.connect("screen_entered", self, "show")','visibilityNotifier.connect("screen_exited", self, "hide")','if globaldata.flags.get(flag):','if New_parent == null:','new_parent = get_parent()','yield(_anim_player, "animation_finished")','var Roots = sprite.duplicate()','Roots.frame = 1','new_parent.add_child(Roots)','Roots.position = sprite.global_position','$AudioStreamPlayer.play()','$interact/ButtonPrompt.set_enabled(false)','globaldata.flags.has(object_function_disappear_flag)','calledObject.call_deferred(call_object_function)','queue_free()']
 require(all(x in script for x in required),'Changed DeadBush lifecycle source')
 require(script.index('Roots.frame = 1')<script.index('new_parent.add_child(Roots)')<script.index('Roots.position = sprite.global_position')<script.index('_anim_player.play("Break")')<script.index('$AudioStreamPlayer.play()')<script.index('$interact/ButtonPrompt.set_enabled(false)'),'Changed DeadBush hit order')
 sprite=node(scene,'Sprite');audio=node(scene,'AudioStreamPlayer');player=node(scene,'AnimationPlayer');exts={int(i):p for p,i in re.findall(r'\[ext_resource path="res://([^"]+)" type="[^"]+" id=(\d+)\]',scene)}
 texture=exts[sprite['texture']['ExtResource']];size=ex.png_size(texture);import_text=ex.text(texture+'.import');require('flags/filter=false' in import_text and 'flags/mipmaps=false' in import_text and 'process/premult_alpha=false' in import_text,'Unreviewed bush texture filtering/alpha');require(size[0]%sprite['hframes']==0 and size[1]<=1024 and size[0]<=1024,'DeadBush atlas exceeds reviewed extent')
 sound=exts[audio['stream']['ExtResource']];ex.data(sound);ex.data(sound+'.import')
 clips=[];roles={'Sprite:frame':1,'Sprite:visible':2,'StaticBody2D/CollisionShape2D:disabled':3,'Hitbox/CollisionShape2D:disabled':4,'interact/CollisionShape2D:disabled':5};names=['Break','Grow','Hidden','Idle','RESET']
 for name in names:
  a=animation(scene,player['anims/'+name]['SubResource'],SOURCE,name);require(not a['loop'],'Unreviewed bush looping animation');tracks=[]
  for t in a['tracks']:
   require(t['type']=='value' and t['path'] in roles and t['interp']==1 and t['loop_wrap']is True and t['keys']['update']in(0,1) and all(v==1 for v in t['keys']['transitions']),'Unknown DeadBush animation path/easing/update')
   role=roles[t['path']];keys=[]
   for time,value in zip(t['keys']['times'],t['keys']['values']):
    require((type(value)is int and 0<=value<sprite['hframes'])if role==1 else type(value)is bool,'DeadBush frame/boolean source key');keys.append(dict(time=time,value=int(value)))
   require(t['keys']['update']==1 or len(keys)==1,'Continuous bush track requires source review');tracks.append(dict(role=role,update=t['keys']['update'],keys=keys))
  clips.append(dict(id=names.index(name)+1,name=name,length=a['length'],tracks=tracks))
 gate=re.search(r'func start_joy_vibration\([^\n]+\):\n\s*if globaldata\.([A-Za-z_]+):\n\s*Input.start_joy_vibration\(device_id, weak_magnitude, strong_magnitude, duration\)',global_source)[1];default=re.search(r'^var '+gate+r' := (true|false)$',global_data,re.M)[1]=='true'
 vibr=[float(v.strip())for v in re.search(r'start_joy_vibration\(([^)]+)\)',script)[1].split(',')];require(len(vibr)==4 and int(vibr[0])==vibr[0],'Bush vibration source signature')
 bat=re.search(r'if globaldata.flags\["([^"]+)"\]',script)[1];dialogues=re.findall(r'open_dialogue_box\("([^"]+)"\)',script);require(len(dialogues)==2,'Bush dialogue branches changed')
 csvpath='Translations/TranslatedText/dialogue_Reusable - sheet.csv';table={r['key']:r for r in csv.DictReader(io.StringIO(ex.text(csvpath)))};dialogue=[]
 for p in dialogues:
  raw=ex.yaml('Data/Dialogue/'+p+'.yaml');require(set(raw)=={'0'} and set(raw['0'])=={'text'},'DeadBush dialogue scope changed');key=raw['0']['text'];require(key in table,'Bush dialogue translation absent');dialogue.append(dict(program=p,key=key,en=table[key]['en'],zh_CN=table[key]['zh_CN']))
 out=[]
 for rec in records:
  p=rec['node'];v=rec['overrides'];parent=p.rsplit('/',1)[0];sp=decode(nodes[p+'/Sprite']['properties']);notifier=decode(nodes[p+'/VisibilityNotifier2D']['properties']);require(notifier['rotation']==0 and notifier['material']is None,'Unreviewed bush viewport notifier transform/material');np=v.get('New_parent');cp=v.get('called_object');new=resolve_path(p,np['value'])if np is not None and np['value']else parent if np is None else '';called=resolve_path(p,cp['value'])if cp is not None and cp['value']else '';require(new in nodes,'Source DeadBush New_parent null resolution rejected');require(not called or called in nodes,'Source bush calledObject missing')
  method=v.get('call_object_function','');kind=0
  if method:
   require(called and method=='check_start','Unreviewed bush deferred method');binding=next((b for b in read(ROOT/'content/podunk-scene-lifecycle.json')['roster'] if b['node']==called),None);require(binding and binding['script']=='Scripts/Main/CutsceneArea.gd','Bush target is not the source CutsceneArea');kind=1
  flags=int(decode(nodes[p]['properties'])['visible'])|int(sp['visible'])<<1|int(sp['centered'])<<2|int(sp['flip_h'])<<3|int(sp['flip_v'])<<4|int(np is None)<<5|int(decode(nodes[p+'/StaticBody2D/CollisionShape2D']['properties'])['disabled'])<<6|int(decode(nodes[p+'/Hitbox/CollisionShape2D']['properties'])['disabled'])<<7|int(decode(nodes[p+'/interact/CollisionShape2D']['properties'])['disabled'])<<8
  require(sp['rotation']==0 and not sp['region_enabled'] and sp['material']is None,'Unreviewed bush source Sprite transform/material')
  out.append(dict(id=rec['stable_id'],parent_id=stable(parent),ready_ordinal=rec['ready_ordinal'],node=p,sprite_id=stable(p+'/Sprite'),notifier_id=stable(p+'/VisibilityNotifier2D'),notifier_rect=notifier['rect'],notifier_position=notifier['position'],notifier_scale=notifier['scale'],prompt_id=stable(p+'/interact/ButtonPrompt'),body_shape_id=stable(p+'/StaticBody2D/CollisionShape2D'),hit_shape_id=stable(p+'/Hitbox/CollisionShape2D'),interact_shape_id=stable(p+'/interact/CollisionShape2D'),new_parent_id=stable(new),new_parent_path=np['value']if np is not None else '',called_id=stable(called)if called else 0,called_path=cp['value']if cp is not None else '',call_kind=kind,call_method=method,flag=v.get('flag',''),disappear_flag=v.get('object_function_disappear_flag',''),flags=flags,columns=sp['hframes'],rows=sp['vframes'],frame=sp['frame'],sprite_position=sp['position'],sprite_offset=sp['offset'],sprite_scale=sp['scale'],hit_layer=node(scene,'Hitbox').get('collision_layer',1),hit_mask=node(scene,'Hitbox').get('collision_mask',1),hit_monitorable=node(scene,'Hitbox').get('monitorable',True)))
 for p,h in provenance['source_files'].items():ex.data(p);require(ex.sources[p]==h,'Changed bush complete native source closure')
 return dict(schema=1,kind='encore.field-dead-bush.source-ir',commit=PIN,scene=SCENE,scene_id=stable('.'),scene_sha256=ex.sources[SCENE],script_sha256=ex.sources[SCRIPT],texture=dict(source=texture,output='graphics/objects/dead-bush.t3x',size=size),sound=dict(id=ident('field-dead-bush-sound',sound),source=sound,pcm='sound/effects/dead-bush.pcm',gain_db=audio.get('volume_db',0),bus=audio.get('bus','Master')),roots_frame=int(re.search(r'Roots.frame = (\d+)',script)[1]),vibration=vibr,vibration_gate=gate,vibration_default=default,bat_flag=bat,dialogues=dialogue,clips=clips,records=out,sources=dict(sorted(ex.sources.items())),provenance=provenance,semantics=['All 13 true source postorder Ready; viewport show/hide continues animation idle while hidden','New_parent nullable source variable fallback only when null, not blanket missing-node fallback','Actual source Sprite duplicate -> frame1 -> add_child -> local position assigned sprite.global_position in this exact order','Hitbox source callback has no bat/visible guard; source geometry owns collision dispatch','Source Break disables static/hit/interaction shapes; Hidden does not disable Hitbox; Grow re-enables three shapes','Source flags.get for initial branch; interact flags[bat] must exist; animation_finished flags.has gates BOTH deferred call and queue_free','Grow coroutine waits for any animation_finished even after interruption; all waiters resume Idle after existing Break handler','One deferred target is actual CutsceneArea7.check_start, never directly grant shovel or flags'],unsupported=['Complete town activation still requires all other pending source Ready','CutsceneArea7 typed deferred receiver must actually implement check_start before call is admitted','Dynamic Roots ownership and parent Canvas transform must be real Host sprite lifecycle, not fake static world coordinates'],license_review='Pinned upstream LICENSE permits game-related fork assets; original art and sound retain upstream terms.',unverified=['No tests run','Integration/emulator/hardware unverified'])

def validate(d):
 require(d['schema']==1 and d['kind']=='encore.field-dead-bush.source-ir' and d['commit']==PIN and d['scene']==SCENE and d['scene_id']==stable('.') and len(d['records'])==13,'DeadBush source identity/coverage')
 require(d['scene_sha256']==d['sources'][SCENE] and d['script_sha256']==d['sources'][SCRIPT] and {a['name']for a in d['clips']}=={'Break','Grow','Hidden','Idle','RESET'},'DeadBush exact source class/clips')
 ids=set();last=-1
 for r in d['records']:
  require(r['id']==stable(r['node'])and r['id']not in ids and r['ready_ordinal']>last and r['flags']<512 and r['columns']>0 and r['rows']>0 and r['frame']<r['columns']*r['rows'],'DeadBush descriptor source postorder');ids.add(r['id']);last=r['ready_ordinal']
  require(r['notifier_id']==stable(r['node']+'/VisibilityNotifier2D')and len(r['notifier_rect'])==2 and all(len(v)==2 for v in r['notifier_rect'])and len(r['notifier_position'])==2 and len(r['notifier_scale'])==2 and all(math.isfinite(v)and abs(v)<=1000000 for a in r['notifier_rect']+[r['notifier_position'],r['notifier_scale']]for v in a)and all(v>0 for v in r['notifier_rect'][1])and all(v!=0 for v in r['notifier_scale']),'DeadBush source viewport notifier rectangle/transform')
  require(r['call_kind']in(0,1)and bool(r['call_method'])==bool(r['call_kind'])and (not r['call_kind']or r['called_id']and r['call_method']=='check_start'),'DeadBush exact deferred source method')
 for c in d['clips']:
  require(c['id']in(1,2,3,4,5)and math.isfinite(c['length'])and c['length']>0 and len(c['tracks'])>0,'Bush clip identity/length')
  used=set()
  for t in c['tracks']:
   require(t['role']in(1,2,3,4,5)and t['role']not in used and t['update']in(0,1)and (t['update']!=0 or len(t['keys'])==1),'Bush property/update');used.add(t['role']);last=-1
   for k in t['keys']:
    require(type(k['value'])is int and math.isfinite(k['time'])and last<=k['time']<=c['length']and (0<=k['value']<d['records'][0]['columns']*d['records'][0]['rows']if t['role']==1 else k['value']in(0,1)),'Bush bounded source key');last=k['time']
 return d

def load():
 d=validate(read(IR));r=read(REVIEW);require(r['commit']==PIN and r['ir_sha256']==sha(IR) and r['sources']==d['sources'],'Bush source review stale');ex=Extractor(ROOT)
 for p,h in d['sources'].items():ex.data(p);require(ex.sources[p]==h,'Changed DeadBush source '+p)
 return d

def texture_receipt(d):
 r=read(RECEIPT);p=ROOT/'romfs'/d['texture']['output'];require(r['schema']==1 and r['commit']==PIN and r['ir_sha256']==sha(IR) and r['producer_sha256']==sha(ROOT/'tools/field_dead_bush.py')and re.fullmatch('[0-9a-f]{64}',r['tex3ds_sha256'])and r['output']==d['texture']['output'] and r['bytes']==p.stat().st_size and r['sha256']==sha(p),'Bush genuine texture receipt mismatch');return r

def encode(d,receipt):
 validate(d);out=bytearray(64)
 def u(*v):out.extend(struct.pack('<'+'I'*len(v),*v))
 def f(*v):out.extend(struct.pack('<'+'f'*len(v),*v))
 def s(v):b=v.encode();u(len(b));out.extend(b)
 out.extend(bytes.fromhex(d['scene_sha256']));out.extend(bytes.fromhex(d['script_sha256']));u(*d['texture']['size']);s(d['texture']['source']);s(d['texture']['output']);out.extend(bytes.fromhex(receipt['sha256']));u(d['sound']['id']);f(d['sound']['gain_db']);s(d['sound']['source']);s(d['sound']['pcm']);s(d['sound']['bus']);u(d['roots_frame'],int(d['vibration'][0]));f(*d['vibration'][1:]);s(d['vibration_gate']);u(int(d['vibration_default']));s(d['bat_flag']);u(len(d['dialogues']))
 for p in d['dialogues']:s(p['program']);s(p['key']);s(p['en']);s(p['zh_CN'])
 u(len(d['clips']))
 for c in d['clips']:
  u(c['id']);f(c['length']);s(c['name']);u(len(c['tracks']))
  for t in c['tracks']:
   u(t['role'],t['update'],len(t['keys']))
   for k in t['keys']:f(k['time']);u(k['value'])
 for r in d['records']:
  u(*(r[k]for k in ('id','parent_id','ready_ordinal','sprite_id','prompt_id','body_shape_id','hit_shape_id','interact_shape_id','new_parent_id','called_id','call_kind','flags','columns','rows','frame','hit_layer','hit_mask')),int(r['hit_monitorable']),r['notifier_id']);f(*r['sprite_position'],*r['sprite_offset'],*r['sprite_scale'],*r['notifier_rect'][0],*r['notifier_rect'][1],*r['notifier_position'],*r['notifier_scale']);s(r['node']);s(r['new_parent_path']);s(r['called_path']);s(r['call_method']);s(r['flag']);s(r['disappear_flag'])
 u(len(d['sources']))
 for p,h in d['sources'].items():s(p);out.extend(bytes.fromhex(h))
 struct.pack_into('<8s6I20sII4x',out,0,b'ENCDBSH1',1,len(out),0,1,1,len(d['records']),bytes.fromhex(PIN),d['scene_id'],len(d['sources']));struct.pack_into('<I',out,16,zlib.crc32(out)&0xffffffff);return bytes(out)

def compile_pack():
 d=load();raw=encode(d,texture_receipt(d));PACK.parent.mkdir(parents=True,exist_ok=True);PACK.write_bytes(raw);return raw

def compile_assets(tex3ds):
 d=load();require(tex3ds and Path(tex3ds).is_file(),'Genuine tex3ds required');target=ROOT/'romfs'/d['texture']['output'];target.parent.mkdir(parents=True,exist_ok=True);subprocess.run([str(tex3ds),'-f','rgba8','-z','none','-o',str(target),str(ROOT/'upstream/MOTHER-Encore'/d['texture']['source'])],check=True)
 write(RECEIPT,dict(schema=1,commit=PIN,ir_sha256=sha(IR),producer_sha256=sha(ROOT/'tools/field_dead_bush.py'),tex3ds_sha256=sha(tex3ds),output=d['texture']['output'],bytes=target.stat().st_size,sha256=sha(target)));return compile_pack()

def audio_bindings(root=ROOT):
 d=load();s=d['sound'];return[dict(source=s['source'],identity=dict(kind='stable',value=s['id']),pcm=s['pcm'],gain_db=s['gain_db'],conversion=None)]

def stage_files(source):
 d=load();r=texture_receipt(d);raw=encode(d,r);source=Path(source);require((source/'data/podunk-dead-bush.encbush').read_bytes()==raw,'Stale DeadBush binary');texture=source/d['texture']['output'];require(texture.stat().st_size==r['bytes']and sha(texture)==r['sha256'],'Changed DeadBush staged texture');return{Path('data/podunk-dead-bush.encbush'):raw,Path(d['texture']['output']):texture.read_bytes()}

def main():
 p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile','pack','verify']);p.add_argument('--native',type=Path);p.add_argument('--source',type=Path);p.add_argument('--tex3ds',type=Path);a=p.parse_args()
 if a.action=='extract':
  require(a.native and a.source,'Actual complete native/source exports required');records,nodes,resources,provenance=source_instances(a.native,a.source,ROOT/'upstream/MOTHER-Encore');d=validate(build(records,nodes,provenance));write(IR,d);write(REVIEW,dict(schema=1,commit=PIN,ir_sha256=sha(IR),sources=d['sources'],semantics=d['semantics'],unsupported=d['unsupported'],unverified=d['unverified']));print('DeadBush: 13 actual Ready; 5 complete source clips; source roots/deferred/prompt/audio lifecycle');return
 raw=compile_assets(a.tex3ds)if a.action=='compile'else compile_pack()if a.action=='pack'else encode(load(),texture_receipt(load()))
 if a.action=='verify':require(PACK.read_bytes()==raw,'Stale bush pack')
 print('DeadBush:',len(raw),'checked bytes')
if __name__=='__main__':
 try:main()
 except(ValueError,KeyError,OSError,TypeError,struct.error,subprocess.SubprocessError)as e:sys.exit('FIELD DEAD BUSH ERROR: '+str(e))
