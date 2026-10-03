#!/usr/bin/env python3
"""Extract checked fresh-house initialization additions into ENCREST1.

Room/House v6 retain authored transforms and flag/deletion mechanisms. This
container adds only event-position order, source music areas and inventory-load
allocation metadata; it executes neither scene restoration nor random draws.
"""
from __future__ import annotations
import argparse, hashlib, json, math, re, struct, sys, zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from tools.extract_battle_entry import Extractor, node, one, properties, require
IR=ROOT/'content/native-restore.json'
PACK=ROOT/'romfs/data/opening.encrestore'
NO_INDEX=0xffffffff
HOUSE='Maps/podunk/Nintens House.tscn'

def read(path):return json.loads(Path(path).read_text())
def digest(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def fields(value,names,label):require(isinstance(value,dict) and set(value)==set(names),'Unknown/missing '+label+' fields')
def finite(x):return isinstance(x,(int,float)) and not isinstance(x,bool) and math.isfinite(x)
def vector(v,positive=False):require(isinstance(v,list) and len(v)==2 and all(finite(x) and abs(x)<=1000000 and (not positive or x>0) for x in v),'Restore geometry rejected')
def safe_path(p):return isinstance(p,str) and p and not p.startswith('/') and ':'not in p and '\\'not in p and all(x not in ('','.','..')for x in p.split('/'))
def extract():
 ex=Extractor(ROOT);source=ex.text(HOUSE)
 room=read(ROOT/'content/native-opening.json');house=read(ROOT/'content/native-house.json')
 require(room['rules']==room['capabilities']and room['rules']in(6,7)and house['schema']==6,'Restore requires reviewed Room/House v6')
 flags={room['strings'][f['name_string']]:(i,f['stable_id'])for i,f in enumerate(room['sections']['Flag'])}
 def condition(name,value):
  require(name in flags,'Unknown restore flag '+name);i,identity=flags[name]
  return dict(flag_index=i,flag_id=identity,flag_name=name,expected_value=bool(value))
 npc_script=ex.text('Scripts/Main/npc.gd')
 event_body=one(r'^func _set_event_positions\(\):\n(.*?)(?=^func )',npc_script,'NPC event-position function',re.M|re.S)[1]
 require(event_body.strip()=='for flags in event_positions:\n\t\tvar flag = flags[0]\n\t\tvar newpositionx = flags[1]\n\t\tvar newpositiony = flags[2]\n\t\tif flag != "":\n\t\t\tif globaldata.flags.get(flag, false):\n\t\t\t\t\tglobal_position = Vector2(float(newpositionx), float(newpositiony))','Unreviewed NPC event-position semantics')
 npcs=[]
 for index,n in enumerate(house['npcs']):
  original=node(source,n['source_path']);require(original['position']==n['position'],'House NPC authored position mismatch')
  for row in original.get('event_positions',[]):
   require(len(row)==3 and row[0],'Unsupported empty NPC event-position flag')
   actor=n['room_actor_index'];require(actor<len(room['sections']['ActorInstance']),'Missing NPC actor binding')
   npcs.append(dict(npc_index=index,npc_id=n['id'],actor_index=actor,actor_id=room['sections']['ActorInstance'][actor]['stable_id'],body_id=n['body_id'],source_path=n['source_path'],condition=condition(row[0],True),position=[float(row[1]),float(row[2])]))
 music=ex.text('Nodes/Overworld/MusicChanger.tscn')
 script=json.loads(one(r'^script/source = ("[\s\S]*?")\n__meta__',music,'MusicChanger embedded script')[1],strict=False)
 require('globaldata.check_appear_disappear_flags(appear_flag, disappear_flag)'in script and 'body == global.get_player() and !uiManager.is_in_cutscene() and !uiManager.is_in_battle() and _check_flags()'in script,'MusicChanger flags/entry semantics changed')
 defaults={name:float(one(r'^export (?:\(int\) )?var '+name+r' = (-?[0-9.]+)$',script,'MusicChanger '+name)[1])for name in ['volume_db','fadein_length','fadeout_length']}
 resources={room['strings'][r['path_string']]:(i,r)for i,r in enumerate(room['sections']['Resource'])}
 music_ext=int(one(r'^\[ext_resource path="res://Nodes/Overworld/MusicChanger.tscn" type="PackedScene" id=(\d+)\]',source,'MusicChanger resource')[1])
 areas=[]
 for m in re.finditer(r'^\[node name="([^"]+)" parent="([^"]+)" instance=ExtResource\( '+str(music_ext)+r' \)\]',source,re.M):
  path=m[1]if m[2]=='.'else m[2]+'/'+m[1];a=node(source,path);shape=node(source,path+'/CollisionShape2D')
  require(not a.get('music') and not a.get('diegetic',False) and not a.get('disabled',False) and 'scale'not in a and 'scale'not in shape and 'rotation'not in shape,'Unreviewed music area transform/policy')
  extents=properties(one(r'^\[sub_resource type="RectangleShape2D" id='+str(shape['shape']['SubResource'])+r'\]\n(.*?)(?=^\[)',source,'Music area rectangle',re.M|re.S)[1])['extents']
  conditions=[]
  if m[2]!='.':
   parent=node(source,m[2]);require(not parent.get('appear_flag') and 'position'not in parent and 'scale'not in parent,'Unreviewed music parent')
   if parent.get('disappear_flag'):conditions.append(condition(parent['disappear_flag'],False))
  for key,value in [('appear_flag',True),('disappear_flag',False)]:
   if a.get(key):conditions.append(condition(a[key],value))
  resource_path='res://Audio/Music/'+a['loop'];raw=ex.data(resource_path[6:]);found=resources.get(resource_path)
  if found:require(found[1]['kind']==2 and found[1]['sha256']==hashlib.sha256(raw).hexdigest(),'Restore audio source fingerprint mismatch')
  areas.append(dict(id=len(areas)+1,source_path=path,resource_path=resource_path,supported=found is not None,room_resource_index=found[0]if found else NO_INDEX,room_resource_id=found[1]['stable_id']if found else 0,resource_sha256=hashlib.sha256(raw).hexdigest(),center=[a['position'][j]+shape.get('position',[0,0])[j]for j in range(2)],extents=extents,conditions=conditions,volume_db=a.get('volume_db',defaults['volume_db']),fadein_seconds=a.get('fadein_length',defaults['fadein_length']),fadeout_seconds=a.get('fadeout_length',defaults['fadeout_length'])))
 global_script=ex.text('Scripts/global/global.gd');registry=ex.text('Scripts/global/globalData.gd');pm=ex.text('Scripts/global/PartyMember.gd');pn=ex.text('Scripts/global/PartyNPC.gd');character=ex.text('Scripts/global/Character.gd');inv=ex.text('Scripts/global/Inventory.gd');item=ex.text('Scripts/global/Item.gd')
 load=one(r'^func _load_dict_to_game\(.*?\n(.*?)(?=^func )',global_script,'load dict',re.M|re.S)[1]
 required=['globaldata.key_items.init_from_serialized(save_data.get("key_items", []))','globaldata.storage.init_from_serialized(save_data.get("storage", []))','for char_id in globaldata.characters:','globaldata.characters[char_id].init_from_dict(save_data.get(char_id, {}))']
 require([load.index(s)for s in required]==sorted(load.index(s)for s in required),'Inventory load order changed')
 require('_inventory = Inventory.new(Inventory.InvType.NORMAL, dict.get("inventory", []))'in pm and 'Inventory'not in character and 'Inventory'not in pn,'Character inventory initialization changed')
 require('int(item.get("uid", Item.get_uid(Item.used_uids_tab)))'in inv and 'randomize()\n\tvar new_uid = randi()\n\twhile(new_uid in used_uids):\n\t\tnew_uid = randi()\n\tused_uids.append(new_uid)'in item,'Inventory eager UID allocation semantics changed')
 new=ex.yaml('Data/save_new_game.yaml');overrides=ex.yaml('Data/save_overrides.yaml')
 orders=[]
 def projected(serialized):
  out=[]
  for entry in serialized:
   data=ex.yaml('Data/Items/'+entry['item_name']+'.yaml')
   out.append(dict(item_id=entry['item_name'],equipped=entry.get('equipped',False),doses=entry.get('doses',data.get('doses',1))))
  return out
 for kind,key in [(1,'key_items'),(2,'storage')]:orders.append(dict(order_id=len(orders)+1,kind=kind,character_id='',rebuilds_inventory=True,projected_items=projected(new.get(key,[]))))
 reg=one(r'^var characters := \{\n(.*?)\n\}',registry,'registered characters',re.M|re.S)[1]
 entries=re.findall(r'^\s*(PartyMember|PartyNPC)\.([A-Z_]+): (PartyMember|PartyNPC)\.new\(\),?$',reg,re.M)
 require(len(entries)==len(reg.splitlines()),'Unrecognized registered character row')
 for owner,symbol,constructor in entries:
  require(owner==constructor,'Registered character constructor mismatch')
  identity=one(r'^const '+symbol+r' := "([^"]+)"',pm if owner=='PartyMember'else pn,'character constant')[1]
  require('inventory'not in overrides.get(identity,{}),'Inventory override requires review')
  orders.append(dict(order_id=len(orders)+1,kind=3,character_id=identity,rebuilds_inventory=owner=='PartyMember',projected_items=projected(new.get(identity,{}).get('inventory',[]))if owner=='PartyMember'else []))
 # Review existing machinery instead of compiling duplicate landmark records.
 ex.text('Scripts/Main/Flag Landmarks.gd');ex.text('Nodes/Reusables/flag landmarks.tscn')
 ex.text('Scripts/Main/Openable Door.gd');ex.text('Scripts/Main/roomshaker.gd')
 ex.text('Maps/Testing/phone.gd');ex.text('Scripts/global/SceneTransition.gd')
 dependencies=['content/native-opening.json','content/native-house.json','romfs/data/opening.encroom','romfs/data/opening.enchouse']
 fingerprints={name:dict(bytes=(ROOT/path).stat().st_size,sha256=digest(ROOT/path))for name,path in [('room',dependencies[2]),('house',dependencies[3])]}
 result=dict(schema=1,kind='encore.native-restore.source-ir',commit=ex.lock['commit'],sources=ex.sources,dependencies={p:digest(ROOT/p)for p in dependencies},fingerprints=fingerprints,scene_id=room['sections']['Scene'][0]['stable_id'],scene_path=room['strings'][room['sections']['Scene'][0]['source_scene_string']],npc_event_positions=npcs,music_areas=areas,inventory_load_order=orders,uid_policy=1,projection_policy='Absent native characters retain frozen source initial inventories; this is a bounded native save projection, not original missing-character fallback.',existing_coverage={'authored_npc_positions':'House Npcs and Room ActorInstance remain authoritative; reinstantiate every load','landmark_deletion':'Room DeferredFlagBodyDeletion and existing Room/House trigger conditions; refresh then scene-frame flush','unsupported_landmarks':'Door NPC remains outside the native slice; optional Pillow/Minnie trigger lifetimes are handled by checked House conditions','phone_and_roomshaker':'Fresh instances start idle; flags alone do not start their animations/timers'})
 validate(result,room,house);return result

def validate(r,room=None,house=None):
 fields(r,['schema','kind','commit','sources','dependencies','fingerprints','scene_id','scene_path','npc_event_positions','music_areas','inventory_load_order','uid_policy','projection_policy','existing_coverage'],'restore IR')
 require(r['schema']==1 and r['kind']=='encore.native-restore.source-ir' and r['uid_policy']==1,'Restore schema/policy rejected')
 room=room or read(ROOT/'content/native-opening.json');house=house or read(ROOT/'content/native-house.json')
 require(r['commit']==room['upstream_commit']==house['commit'],'Restore source pin mismatch')
 require(r['scene_id']==room['sections']['Scene'][0]['stable_id'] and r['scene_path']==room['strings'][room['sections']['Scene'][0]['source_scene_string']],'Restore scene binding rejected')
 for name,path in [('room','romfs/data/opening.encroom'),('house','romfs/data/opening.enchouse')]:
  fields(r['fingerprints'][name],['bytes','sha256'],'fingerprint');require(r['fingerprints'][name]==dict(bytes=(ROOT/path).stat().st_size,sha256=digest(ROOT/path)),'Restore dependency fingerprint changed')
 def condition(c):
  fields(c,['flag_index','flag_id','flag_name','expected_value'],'condition');require(type(c['flag_index'])is int and 0<=c['flag_index']<len(room['sections']['Flag']) and type(c['expected_value'])is bool,'Restore condition index/value')
  f=room['sections']['Flag'][c['flag_index']];require(f['stable_id']==c['flag_id'] and room['strings'][f['name_string']]==c['flag_name'],'Restore flag binding mismatch')
 require(len(r['npc_event_positions'])<=256 and 0<len(r['music_areas'])<=128,'Restore record count')
 seen=set()
 for n in r['npc_event_positions']:
  fields(n,['npc_index','npc_id','actor_index','actor_id','body_id','source_path','condition','position'],'NPC event');require(type(n['npc_index'])is int and 0<=n['npc_index']<len(house['npcs']),'Restore NPC index')
  h=house['npcs'][n['npc_index']];require(h['id']==n['npc_id'] and h['source_path']==n['source_path'] and h['body_id']==n['body_id'] and h['room_actor_index']==n['actor_index'] and 0<=n['actor_index']<len(room['sections']['ActorInstance']) and room['sections']['ActorInstance'][n['actor_index']]['stable_id']==n['actor_id'],'Restore NPC binding mismatch')
  require(any(b['body_id']==n['body_id']and room['strings'][b['source_path_string']]==n['source_path']for b in room['sections']['BodyRule']),'Restore NPC body path mismatch')
  condition(n['condition']);vector(n['position']);key=(n['npc_index'],n['condition']['flag_index']);require(key not in seen,'Duplicate NPC event flag');seen.add(key)
 seen=set()
 for m in r['music_areas']:
  fields(m,['id','source_path','resource_path','supported','room_resource_index','room_resource_id','resource_sha256','center','extents','conditions','volume_db','fadein_seconds','fadeout_seconds'],'music area')
  require(m['id']>0 and m['id']not in seen and safe_path(m['source_path']) and m['resource_path'].startswith('res://') and safe_path(m['resource_path'][6:]) and type(m['supported'])is bool,'Restore music identity');seen.add(m['id']);vector(m['center']);vector(m['extents'],True)
  require(all(finite(m[k])for k in ['volume_db','fadein_seconds','fadeout_seconds']) and -100<=m['volume_db']<=24 and 0<=m['fadein_seconds']<=120 and 0<=m['fadeout_seconds']<=120,'Restore audio parameter')
  require(0<len(m['conditions'])<=16,'Restore music conditions');[condition(c)for c in m['conditions']]
  require(len({c['flag_index']for c in m['conditions']})==len(m['conditions']),'Duplicate music flag')
  if m['supported']:
   require(0<=m['room_resource_index']<len(room['sections']['Resource']),'Restore resource index');q=room['sections']['Resource'][m['room_resource_index']]
   require(q['stable_id']==m['room_resource_id'] and q['kind']==2 and room['strings'][q['path_string']]==m['resource_path'] and q['sha256']==m['resource_sha256'],'Restore audio binding mismatch')
  else:require(m['room_resource_index']==NO_INDEX and m['room_resource_id']==0 and not any(room['strings'][q['path_string']]==m['resource_path']for q in room['sections']['Resource']),'Unsupported audio must have no mapped resource')
  require(len(bytes.fromhex(m['resource_sha256']))==32,'Restore source audio fingerprint')
 require(3<=len(r['inventory_load_order'])<=128,'Restore inventory registry count');seen=set()
 for i,row in enumerate(r['inventory_load_order']):
  fields(row,['order_id','kind','character_id','rebuilds_inventory','projected_items'],'inventory load');require(row['order_id']==i+1 and row['kind']==(i+1 if i<2 else 3) and type(row['rebuilds_inventory'])is bool,'Restore inventory order/kind')
  identity=row['character_id'];require((not identity and row['rebuilds_inventory'])if i<2 else (bool(re.fullmatch('[a-z][a-z0-9_]*',identity))and identity not in seen),'Restore inventory identity');seen.add(identity)
  require(len(row['projected_items'])<=256 and (row['rebuilds_inventory']or not row['projected_items']),'Restore noninventory character items')
  for item in row['projected_items']:
   fields(item,['item_id','equipped','doses'],'projected item');require(bool(re.fullmatch('[A-Za-z][A-Za-z0-9_]*',item['item_id'])) and type(item['equipped'])is bool and type(item['doses'])is int and 1<=item['doses']<=1000000,'Restore projected item')

class Writer:
 def __init__(self):self.raw=bytearray()
 def put(self,fmt,*values):self.raw.extend(struct.pack('<'+fmt,*values))
 def text(self,s):b=s.encode('ascii');self.put('I',len(b));self.raw.extend(b)
 def condition(self,c):self.put('3I',c['flag_index'],c['flag_id'],c['expected_value']);self.text(c['flag_name'])
def encode(r):
 validate(r);w=Writer()
 for name in ['room','house']:
  f=r['fingerprints'][name];w.put('I32s',f['bytes'],bytes.fromhex(f['sha256']))
 w.put('I',r['scene_id']);w.text(r['scene_path']);w.put('I',len(r['npc_event_positions']))
 for n in r['npc_event_positions']:
  w.put('5I',*[n[k]for k in ['npc_index','npc_id','actor_index','actor_id','body_id']]);w.text(n['source_path']);w.condition(n['condition']);w.put('2f',*n['position'])
 w.put('I',len(r['music_areas']))
 for m in r['music_areas']:
  w.put('4I',m['id'],m['room_resource_index'],m['room_resource_id'],m['supported']);w.text(m['source_path']);w.text(m['resource_path']);w.put('32s4f3d',bytes.fromhex(m['resource_sha256']),*m['center'],*m['extents'],m['volume_db'],m['fadein_seconds'],m['fadeout_seconds']);w.put('I',len(m['conditions']));[w.condition(c)for c in m['conditions']]
 w.put('2I',r['uid_policy'],len(r['inventory_load_order']))
 for row in r['inventory_load_order']:
  w.put('3I',row['order_id'],row['kind'],row['rebuilds_inventory']);w.text(row['character_id']);w.put('I',len(row['projected_items']))
  for item in row['projected_items']:w.text(item['item_id']);w.put('2I',item['equipped'],item['doses'])
 return struct.pack('<8s4I',b'ENCREST1',1,len(w.raw)+24,zlib.crc32(w.raw),1)+w.raw

def verify_recipe(r):require(r==extract(),'Restore recipe differs from pinned source or reviewed scope')
def stage_files(source_root):
 r=read(IR);verify_recipe(r);root=Path(source_root).resolve();relative=Path('data/opening.encrestore');target=(root/relative).resolve();require(target.is_relative_to(root),'Restore stage path escapes root');raw=target.read_bytes();require(raw==encode(r),'Stale staged restore pack');return {relative:raw}
def main():
 ap=argparse.ArgumentParser();ap.add_argument('action',choices=['extract','compile','verify'],nargs='?',default='compile');a=ap.parse_args()
 if a.action=='extract':r=extract();IR.write_text(json.dumps(r,indent=2)+'\n')
 else:r=read(IR);verify_recipe(r)
 raw=encode(r)
 if a.action=='verify':require(PACK.read_bytes()==raw,'Stale restore pack')
 else:PACK.parent.mkdir(parents=True,exist_ok=True);PACK.write_bytes(raw)
 print('Restore content:',len(raw),'bytes; source NPC event order, music areas, inventory allocation metadata')
if __name__=='__main__':
 try:main()
 except (ValueError,KeyError,OSError,TypeError,struct.error,OverflowError)as e:print('RESTORE CONTENT ERROR:',e,file=sys.stderr);raise SystemExit(1)
