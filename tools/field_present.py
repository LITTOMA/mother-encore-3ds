#!/usr/bin/env python3
"""Pinned Podunk ItemHolder/Present source, independent of House Present."""
from __future__ import annotations
import argparse,hashlib,json,math,re,struct,sys,zlib,subprocess
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import decode,stable,SCENE,PIN,read,write,sha,require
from tools.extract_battle_entry import Extractor,node,animation,variant
from tools.asset_receipts import receipt_path
PRESENT_SCRIPT='Scripts/Main/Present.gd';DROPPED_SCRIPT='Scripts/Main/DroppedItem.gd'
PRESENT_SCENE='Nodes/Overworld/Objects/Present.tscn';DROPPED_SCENE='Nodes/Overworld/Objects/DroppedItem.tscn'
IR=ROOT/'content/native-field-present.json';REVIEW=ROOT/'reports/field-present/source-review.json';PACK=ROOT/'romfs/data/podunk-presents.encpresent'
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
  if bindings[path] not in (PRESENT_SCRIPT,DROPPED_SCRIPT):continue
  out.append(dict(stable_id=stable(path),node=path,ready_ordinal=ordinal[path],script=bindings[path],overrides=overrides[path],native=names[path],children=[names[p] for p in children[path]]))
 require(len(out)==19,'Podunk item holder binding loss');return out,names,resources,dict(native_export_sha256=sha(native),source_receipt_sha256=sha(receipt),source_files={name:r['sha256'] for name,r in s['files'].items()})

def build(records,nodes,resources,provenance):
 ex=Extractor(ROOT);ex.sources.update(provenance['source_files']);source={p:ex.text(p)for p in [PRESENT_SCRIPT,DROPPED_SCRIPT,'Scripts/Main/ItemHolder.gd','Scripts/Main/FlaggableObject.gd',PRESENT_SCENE,DROPPED_SCENE,'Scripts/global/Inventory.gd','Scripts/global/Item.gd','Scripts/global/global.gd','Scripts/misc/sparkles.gd','Nodes/Reusables/Effects/Sparkles.tscn','LICENSE']}
 holder=source['Scripts/Main/ItemHolder.gd'];present=source[PRESENT_SCRIPT];flags=source['Scripts/Main/FlaggableObject.gd']
 for fact in ['reset_when_leaving_region = false','Inventory.has_inventory_space() or globaldata.get_item_data(item).get("keyitem", false)','global.item = Inventory.add_item_available(item)','_play_collect_item()','_set_flag_status()','_update_state()','uiManager.open_dialogue_box(dialog)','global.item = Item.new(item)','uiManager.open_dialogue_box(dialog_full)','if (dialog_empty != ""):\n\t\tuiManager.open_dialogue_box("ItemDialogue/presentempty")']:require(fact in holder,'Changed ItemHolder source '+fact)
 require('yield($AnimationPlayer,"animation_finished")'in present and '$AnimationPlayer.play("Wrapped")'in present,'Changed source Present revert wait')
 require('global.currentScene.name + "/" + name'in flags and 'region_changed and reset_when_leaving_region'in flags,'Changed default object flag/reset source')
 scene=source[PRESENT_SCENE];sprite=node(scene,'Sprite');player=node(scene,'AnimationPlayer');clips=[]
 for name in ['Unwrapped','Wrapped']:
  a=animation(scene,player['anims/'+name]['SubResource'],PRESENT_SCENE,name);require(not a['loop'],'Unknown Present looping animation');keys=[]
  for track_index,tr in enumerate(a['tracks']):
   role=1 if tr['path']=='Sprite:frame'else 2 if tr['path']=='AudioStreamPlayer:playing'else 0;require(role and tr['keys']['update']==1 and tr['enabled'],'Unknown Present track')
   keys.extend(dict(time=t,role=role,value=int(v),track=track_index)for t,v in zip(tr['keys']['times'],tr['keys']['values']))
  clips.append(dict(name=name,length=a['length'],keys=sorted(keys,key=lambda k:(k['time'],k['track']))))
 from tools.present_sparkles import build as sparkle_build
 sp=sparkle_build(ROOT);ex.sources.update(sp['sources']);sparkles=dict(animation=sp['animation'],resource=sp['resource'],engine=sp['engine_reference'])
 holders=[];items={};scene_name=nodes['.']['name'];dropped=[]
 def geometry(path):
  n=nodes[path];props=decode(n['properties']);ref=props['shape'];shape=resources[ref['id']];require(shape['class']=='RectangleShape2D','Present interaction shape capability');ext=decode(shape['properties'])['extents'];transform=decode(n['world_transform']);require(transform[0][1]==transform[1][0]==0 and transform[0][0]>0 and transform[1][1]>0,'Present rotated/skew collision pending');return[transform[2][0],transform[2][1],ext[0]*transform[0][0],ext[1]*transform[1][1]]
 for a in records:
  o=a['overrides'];path=a['node'];kind=1 if a['script']==PRESENT_SCRIPT else 0;item=o.get('item','');require(type(item)is str,'Holder source item id')
  if item and item not in items:
   data=ex.yaml('Data/Items/'+item+'.yaml');items[item]=dict(key=item,source='Data/Items/'+item+'.yaml',keyitem=bool(data.get('keyitem',False)),doses=data.get('doses',1));require(type(items[item]['doses'])is int and 0<items[item]['doses']<=65535,'Present source dose range')
  rawflag=o.get('flag','');objectflag=o.get('is_object_flag',False)or not rawflag;flag=rawflag or scene_name+'/'+nodes[path]['name'];wt=decode(a['native']['world_transform']);props=decode(a['native']['properties']);require(wt[0][1]==wt[1][0]==0 and wt[0][0]>0 and wt[1][1]>0,'Holder transform pending')
  if not kind:dropped.append(dict(stable_id=a['stable_id'],node=path,ready_ordinal=a['ready_ordinal'],script=a['script'],overrides=o));continue
  require(o.get('type','')=='item','Present type capability pending');child=nodes[path+'/Sparkles'];childprops=decode(child['properties']);visual=decode(nodes[path+'/Sprite']['properties']);require(visual['hframes']==sprite['hframes']and visual['vframes']==1 and visual['centered']and not visual['region_enabled']and not visual['flip_h']and not visual['flip_v'],'Present source sprite layout')
  require(visual['offset']==[0,0]and visual['rotation']==0 and visual['scale']==[1,1]and childprops['centered']and childprops['offset']==[0,0]and childprops['rotation']==0 and childprops['scale']==[1,1]and not childprops['flip_h']and not childprops['flip_v']and childprops['speed_scale']==sparkles['animation']['speed_scale']and childprops['playing'],'Present unsupported sprite/Sparkles transform or initial lifecycle')
  values=dict(id=a['stable_id'],node=path,ready_ordinal=a['ready_ordinal'],sparkles_id=stable(path+'/Sparkles'),sparkles_ready=next(i for i,n in enumerate(nodes)if n==path+'/Sparkles'),flag=flag,object_flag=objectflag,emit=o.get('emit_flag_updated_signal',False),reset_area=o.get('reset_when_leaving_area',False),reset_region=False,can_pickup=o.get('can_pickup',True),item=item,dialog=o.get('dialog','')or'ItemDialogue/itemcheck',dialog_full=o.get('dialog_full','')or'ItemDialogue/itemfull',dialog_empty=o.get('dialog_empty','')or'ItemDialogue/presentempty',position=wt[2],scale=[wt[0][0],wt[1][1]],sprite_position=decode(nodes[path+'/Sprite']['world_transform'])[2],sparkles_position=decode(child['world_transform'])[2],collision=geometry(path+'/StaticBody2D/CollisionShape2D'),interaction=geometry(path+'/interact/CollisionShape2D'),collision_layer=decode(nodes[path+'/StaticBody2D']['properties'])['collision_layer'],interaction_layer=decode(nodes[path+'/interact']['properties'])['collision_layer'],initial_frame=visual['frame'],opened_frame=int(re.search(r'\$Sprite.frame = (\d+)',present)[1]),button_prompt_id=stable(path+'/interact/ButtonPrompt'),tint_id=stable(path+'/CharacterTint'),fetcher_id=stable(path+'/SpriteDataFetcher'),visible=props['visible'])
  # Source derived Ready defaults run after ItemHolder Ready. Serialized Present
  # defaults already provide presentcheck/full; only missing empty gets this rule.
  require(values['collision_layer']==517,'Present blocker layer loss');holders.append(values)
  for key in ['dialog','dialog_full','dialog_empty']:ex.text('Data/Dialogue/'+values[key]+'.yaml')
  ex.text('Data/Dialogue/ItemDialogue/presentempty.yaml')
 require(len(holders)==16 and len(dropped)==3,'Podunk holder loss')
 source_image=re.search(r'get_node\("Sprite"\).texture = load\("res://([^"\n]+)"\)',present)[1];size=ex.png_size(source_image);ex.data(source_image+'.import');sound=re.search(r'ext_resource path="res://([^"\n]+)" type="AudioStream"',scene)[1];ex.data(sound);ex.data(sound+'.import')
 # Sparkles Ready order must use the same true child-before-parent postorder.
 children={p:[]for p in nodes}
 for path in nodes:
  if path!='.':children[path.rsplit('/',1)[0]if'/'in path else'.'].append(path)
 ready=[]
 def visit(path):
  for child in children[path]:visit(child)
  ready.append(path)
 visit('.');ordinal={p:i for i,p in enumerate(ready)}
 for h in holders:h['sparkles_ready']=ordinal[h['node']+'/Sparkles'];require(h['sparkles_ready']<h['ready_ordinal'],'Present child Ready order')
 return dict(schema=1,kind='encore.field-present.source-ir',commit=PIN,scene=SCENE,scene_name=scene_name,empty_message=re.search(r'uiManager.open_dialogue_box\("(ItemDialogue/presentempty)"\)',holder)[1],holders=holders,pending_dropped=dropped,items=list(items.values()),clips=clips,sparkles=sparkles,resource=dict(source=source_image,width=size[0],height=size[1],frames=sprite['hframes'],path='graphics/objects/present/present.t3x'),sound=sound,sources=dict(sorted(ex.sources.items())),provenance=provenance,semantics=['Full Podunk inner-to-outer overrides; true child Ready postorder for sixteen Presents and three typed-pending DroppedItems','ItemHolder region reset override false; source default object flag is scene name plus short instance name, not full node path','Child Sparkles Ready consumes shared global RNG even if parent collected flag is true; hidden playing Sparkles idle continues','Present interact plays Unwrapped before inventory capacity check; keyitem bypass; UID allocation remains typed shared Host with real source clock','Successful ItemHolder order grant/select current item, collect hook, flag, Sparkles state, Room source dialogue; full path creates unowned Item UID context','Full/no-item branches await any AnimationPlayer animation_finished then Wrapped; concurrent waiters resume FIFO and lifecycle exit cancels them','StaticBody layer517 remains blocking after collection; no source collision disabling invented'],unsupported=['DroppedItem source data preserved; its timer/tween/prompt reparent execution remains explicit typed pending','Actual inventory item definitions, source Room dialogue, prompt child Ready and tint/fetcher endpoints must be admitted by live SceneHost before activation'],unverified=['Manual tests not run','No emulator/hardware visual or timing claim'])

def validate(d):
 require(d['schema']==1 and d['kind']=='encore.field-present.source-ir'and d['commit']==PIN and d['scene']==SCENE and len(d['holders'])==16 and len(d['pending_dropped'])==3 and len(d['clips'])==2,'Present schema/source counts')
 ids=set();names=set();items={i['key']for i in d['items']}
 for h in d['holders']:
  require(h['id']==stable(h['node'])and h['id']not in ids and h['node']not in names and h['sparkles_id']==stable(h['node']+'/Sparkles')and h['sparkles_ready']<h['ready_ordinal']and h['collision_layer']==517 and h['initial_frame']<d['resource']['frames']and h['opened_frame']<d['resource']['frames']and (not h['item']or h['item']in items),'Present checked identity/source binding');ids.add(h['id']);names.add(h['node'])
  for key in ['position','scale','sprite_position','sparkles_position','collision','interaction']:require(all(type(v)in(int,float)and math.isfinite(v)and abs(v)<=1e6 for v in h[key]),'Present geometry finite')
 for clip in d['clips']:require(0<clip['length']<=60 and 0<len(clip['keys'])<=64 and all(k['role']in(1,2)and 0<=k['time']<=clip['length']and k['value']>=0 for k in clip['keys']),'Present source clips')
 return d

def load():
 d=validate(read(IR));review=read(REVIEW);require(review['ir_sha256']==sha(IR)and review['sources']==d['sources']and review['commit']==PIN,'Present review receipt');inventory=read(ROOT/'compatibility/upstream-inventory.json')['files']
 for p,h in d['sources'].items():require(sha(ROOT/'upstream/MOTHER-Encore'/p)==h==inventory[p]['sha256'],'Changed Present source '+p)
 return d

def encode(d):
 validate(d);b=bytearray(64)
 def u(*v):b.extend(struct.pack('<'+'I'*len(v),*v))
 def f(*v):b.extend(struct.pack('<'+'f'*len(v),*v))
 def t(s):raw=s.encode();u(len(raw));b.extend(raw)
 t(d['scene']);t(d['scene_name']);t(d['empty_message']);a=d['resource'];u(a['width'],a['height'],a['frames']);t(a['source']);t(a['path']);t(d['sound'])
 raw=(ROOT/'romfs'/a['path']).read_bytes()if(ROOT/'romfs'/a['path']).exists()else b'';rec=receipt_path(ROOT/'romfs/graphics/objects/present',ROOT)
 if raw:
  rc=read(rec);require(rc['commit']==PIN and rc['recipe_sha256']==sha(IR)and rc['outputs'][a['path']]['sha256']==hashlib.sha256(raw).hexdigest(),'Present genuine texture receipt')
 sp=d['sparkles']['resource'];spraw=(ROOT/'romfs'/sp['output']).read_bytes();from tools.present_sparkles import verify_receipt
 verify_receipt(ROOT);u(len(raw),zlib.crc32(raw)&0xffffffff if raw else 0,len(spraw),zlib.crc32(spraw)&0xffffffff,*sp['size'])
 u(len(d['items']))
 for i in d['items']:t(i['key']);t(i['source']);u(int(i['keyitem']),i['doses'])
 for c in d['clips']:
  t(c['name']);f(c['length']);u(len(c['keys']))
  for k in c['keys']:f(k['time']);u(k['role'],k['value'])
 a=d['sparkles']['animation'];f(a['speed'],a['speed_scale'],*a['random_range']);u(len(a['frames']));t(d['sparkles']['resource']['output'])
 for rect in a['frames']:u(*rect)
 u(len(d['holders']))
 for h in d['holders']:
  flags=int(h['object_flag'])|int(h['emit'])<<1|int(h['reset_area'])<<2|int(h['can_pickup'])<<3|int(h['visible'])<<4
  u(h['id'],h['ready_ordinal'],h['sparkles_id'],h['sparkles_ready'],h['button_prompt_id'],h['tint_id'],h['fetcher_id'],flags,h['initial_frame'],h['opened_frame'],h['collision_layer'],h['interaction_layer'])
  for key in ['node','flag','item','dialog','dialog_full','dialog_empty']:t(h[key])
  for key in ['position','scale','sprite_position','sparkles_position','collision','interaction']:f(*h[key])
 u(len(d['pending_dropped']))
 for h in d['pending_dropped']:u(h['stable_id'],h['ready_ordinal']);t(h['node']);t(h['script'])
 u(len(d['sources']))
 for p,h in d['sources'].items():t(p);b.extend(bytes.fromhex(h))
 struct.pack_into('<8s6I20s12x',b,0,b'ENCPRE01',1,len(b),0,1,1,len(d['holders']),bytes.fromhex(PIN));struct.pack_into('<I',b,16,zlib.crc32(b)&0xffffffff);return bytes(b)

def assets(tex):
 d=load();a=d['resource'];target=ROOT/'romfs'/a['path'];target.parent.mkdir(parents=True,exist_ok=True)
 def convert(a):subprocess.run([str(tex),'-f','rgba8','-z','none','-o',str(target),str(ROOT/'upstream/MOTHER-Encore'/a['source'])],check=True);return a['path'],dict(bytes=target.stat().st_size,sha256=sha(target))
 with ThreadPoolExecutor(max_workers=4)as pool:outputs=dict(pool.map(convert,[a]))
 write(receipt_path(ROOT/'romfs/graphics/objects/present',ROOT),dict(schema=1,commit=PIN,recipe_sha256=sha(IR),tex3ds_sha256=sha(tex),outputs=outputs))

def audio_bindings():
 d=load();return[dict(source=d['sound'],identity=dict(kind='stable',value=stable('source:'+d['sound'])),pcm='sound/effects/field-present.pcm',gain_db=0,conversion=None)]

def stage_files(source):
 d=load();b=encode(d);require((Path(source)/'data/podunk-presents.encpresent').read_bytes()==b,'Stale Podunk Present binary');path=Path(d['resource']['path']);raw=(Path(source)/path).read_bytes();require(raw==(ROOT/'romfs'/path).read_bytes(),'Stale staged Present texture');return{Path('data/podunk-presents.encpresent'):b,path:raw}

def main():
 p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile','assets']);p.add_argument('--native',type=Path);p.add_argument('--source',type=Path);p.add_argument('--tex3ds',type=Path);a=p.parse_args()
 if a.action=='extract':
  require(a.native and a.source,'Full official native/source exports required');records,nodes,resources,provenance=source_instances(a.native,a.source,ROOT/'upstream/MOTHER-Encore');d=validate(build(records,nodes,resources,provenance));write(IR,d);write(REVIEW,dict(schema=1,commit=PIN,ir_sha256=sha(IR),sources=d['sources'],semantics=d['semantics'],unsupported=d['unsupported']));print('Podunk source ItemHolder: sixteen Present + three pending DroppedItem');return
 if a.action=='assets':require(a.tex3ds and a.tex3ds.is_file(),'Genuine tex3ds required');assets(a.tex3ds);return
 d=load();b=encode(d);PACK.parent.mkdir(parents=True,exist_ok=True);PACK.write_bytes(b);print('Podunk checked Present:',len(b),'bytes; true source flags/UID/order/revert/Sparkles')
if __name__=='__main__':
 try:main()
 except(ValueError,KeyError,OSError,TypeError,struct.error)as e:sys.exit('FIELD PRESENT ERROR: '+str(e))
