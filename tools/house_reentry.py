#!/usr/bin/env python3
"""Original exterior House door continuation; no cold LOAD or source Ready."""
from __future__ import annotations
import argparse, hashlib, re, struct, sys, zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import PIN,read,write,sha,require,stable
from tools.extract_battle_entry import Extractor,node,one
IR=ROOT/'content/native-house-reentry.json'
REVIEW=ROOT/'reports/house-reentry/source-review.json'
PACK=ROOT/'romfs/data/house.encreentry'
FAMILY=0x454e0075
SOURCE='Maps/podunk/podunk.tscn'
TARGET='Maps/podunk/Nintens House.tscn'
AREA='Scripts/Main/RoomTypes/AreaRoom.gd'
DEPENDENCIES=['content/podunk-door.json','content/native-opening.json','content/native-house.json','content/house-source-bindings.json','content/native-house-geometry.json']
def derive(room_ir=DEPENDENCIES[1],room_pack='data/opening.encroom'):
 dependencies=list(DEPENDENCIES);dependencies[1]=room_ir
 ex=Extractor(ROOT);house=ex.text(TARGET);outdoor=ex.text(SOURCE)
 area=ex.text(AREA);door=ex.text('Scripts/Main/Door.gd');transition=ex.text('Scripts/global/SceneTransition.gd');glob=ex.text('Scripts/global/global.gd')
 house_script=ex.text('Maps/podunk/Ninten_s room.gd');landmark=ex.text('Scripts/Main/Flag Landmarks.gd')
 ex.data('Nodes/Overworld/Door.tscn');ex.data('Nodes/Reusables/flag landmarks.tscn');ex.data('Scripts/global/globalData.gd')
 require(house_script.strip()=='extends AreaRoom\n\nfunc _on_AnimationPlayer_animation_finished(anim_name):\n\tif anim_name == "cutscene":\n\t\tuiManager.start_battle()','House root requires new source body review')
 root=node(house,'.');require(root['script']=={'ExtResource':20},'House root script binding changed')
 rootname=one(r'^\[node name="([^"]+)" type="Node2D"\]',house,'House root')[1].replace("\\'","'")
 d=read(ROOT/DEPENDENCIES[0]);rows=[r for r in d['records']if r['target_path']==TARGET];require(len(rows)==1,'Expected one exterior House destination')
 entry=rows[0];require(entry['target_name'] and not entry['flag'],'House return changed flag behavior')
 require(d['sources'][SOURCE]==ex.sources[SOURCE] and d['sources'][TARGET]==ex.sources[TARGET],'Door target source fingerprint differs')
 inst=node(outdoor,entry['node']);require(inst['targetScene']==entry['target_name'] and [inst['targetX'],inst['targetY']]==entry['target'] and inst['dir']==entry['direction'],'Exterior door source overrides differ')
 require('_target_scene_params'not in inst and 'var _target_scene_params := []'in door,'Nonempty return init_params needs implementation')
 require('var same_scene: bool = !door.targetScene or global.currentScene.get_name() == door.targetScene'in transition,'Scene comparison behavior changed')
 parent=one(r'get_node_or_null\("([^\"]+)" if currentScene.has_node\("([^\"]+)"\) else "([^\"]+)"\)',glob,'Current scene player parent')
 require(parent[1]==parent[2] and not re.search(r'^\[node name="'+re.escape(parent[1])+r'"[^\n]*parent="\."',house,re.M),'House player parent requires alternate source branch')
 parentpath=parent[3];parent_properties=node(house,parentpath)
 for v in [root,parent_properties]:require(not any(k in v for k in ('position','rotation','rotation_degrees','scale','transform')),'Return native parent transform requires source implementation')
 require(one(r'^\[node name="'+re.escape(parentpath)+r'" type="([^\"]+)" parent="\."\]',house,'House actual player container')[1]=='TileMap','House container native class changed')
 native=[dict(id=stable('house-reentry-native:'+TARGET+'#.'),parent=0,owner_role=1,node='.',name=rootname,native_class='Node2D',script='Maps/podunk/Ninten_s room.gd',script_sha256=ex.sources['Maps/podunk/Ninten_s room.gd'],local=[1,0,0,1,0,0])]
 native.append(dict(id=stable('house-reentry-native:'+TARGET+'#'+parentpath),parent=native[0]['id'],owner_role=2,node=parentpath,name=parentpath,native_class='TileMap',script='',script_sha256='00'*32,local=[1,0,0,1,0,0]))
 ready=one(r'func _ready\(\):\n\s*([^\n]+)\n\s*([^\n]+)',area,'Area ready order')
 require(ready[1].strip()=='_update_visit_flags()' and ready[2].strip()=='_update_flying_man_status()','Area Ready calls changed')
 switch=one(r'func _init\(\):\n\tconnect\("([^\"]+)", self, "([^\"]+)"\)',area,'Area source constructor signal connection')
 require(one(r'^signal '+re.escape(switch[1])+r' \(emitter, state, silent\)$',area,'Area switch signal declaration') and one(r'^func '+re.escape(switch[2])+r'\(emitter: TwoStatesSwitch, value: bool, silent: bool\):\n\t_switches_state = value$',area,'Area switch callback body'),'Area switch constructor/body changed')
 require(one(r'^func leave_for\(new_scene\):\n\temit_signal\("area_left", new_scene.get_region_name\(\) != self.get_region_name\(\)\)$',area,'Area left source argument'),'Area left argument changed')
 region=root['_region_name'];source_region=node(outdoor,'.')['_region_name']
 visits=one(r'const REGION_VISIT_FLAGS := \{([\s\S]*?)\n\}',area,'Visit mapping')[1]
 visit=one(r'"'+re.escape(region)+r'": "([^\"]+)"',visits,'House visit flag')[1]
 flying=one(r'var is_magicant = \(_region_name == "([^\"]+)"\)',area,'Flying region')[1]
 flyingflag=one(r'globaldata.flags\["([^\"]+)"\]',area,'Flying party flag')[1]
 character_match=one(r'is_magicant != \(globaldata.characters\.([a-z]+) in global\.([A-Za-z]+)\)',area,'Flying roster');character=[character_match[1],character_match[2]]
 require('globaldata.set_flag(flag, true)'in area and 'global.partyNpcs.erase(globaldata.characters.flyingman)'in area,'Area Ready mutation requires review')
 # Every operation remains in original deferred source order, not fixture reset.
 selectors=['remove_child(global.get_player())','set_collisions(false)','for node in global.get_persistent():','var new_scene = ResourceLoader.load(path).instance()','global.currentScene.leave_for(new_scene)','global.currentScene.free()','global.currentScene = new_scene','global.currentScene.callv("init_params", params)','get_tree().get_root().add_child(global.currentScene)','global.get_current_scene_player_node().add_child(global.get_player())','global.create_party_followers()','global.set_party_position(player_pos, player_dir)','global.get_current_scene_player_node().add_child(node)','get_tree().set_current_scene(global.currentScene)','uiManager.update_key_indicator()','global.get_player().set_collisions(true)']
 body=one(r'func _deferred_goto_scene\([^\n]+\):\n([\s\S]*?)(?=\nfunc )',transition,'Deferred transition')[1]
 at=-1
 for s in selectors:at=body.find(s,at+1);require(at>=0,'Deferred operation order changed '+s)
 signals=[one(r'^signal '+n+r'$',door,'Door signal '+n)[0].split()[1]for n in ('entered','moved_player','done')]
 body_signal=one(r'\[connection signal="([^\"]+)" from="\." to="\." method="'+re.escape(d['body_method'])+r'"\]',ex.text('Nodes/Overworld/Door.tscn'),'Door body connection')[1]
 changed=one(r'^signal (scene_changed)$',glob,'Global scene signal')[1]
 followers=one(r'^func (create_party_followers)\(emit_signal:= true\):\n([\s\S]*?)(?=\nfunc )',glob,'Original party follower signal continuation')
 follower_tail=one(r'\n\tif emit_signal: \n\t\tyield\(get_tree\(\), "([^\"]+)"\)\n\t\tprint\("emitting party changed"\)\n\t\temit_signal\("([^\"]+)"\)\n$',followers[2],'Party changed next source idle boundary')
 require(one(r'^signal '+re.escape(follower_tail[2])+r'$',glob,'Global party changed signal declaration') and followers[2].find('partyObjects.resize(1)')<followers[2].find('if emit_signal:') and followers[2].find('follow.init_with_follower_idx(i)')<followers[2].find('if emit_signal:'),'Original party changed construction ordering changed')
 deferred=one(r'call_deferred\("([^\"]+)", path, player_pos, player_dir, params\)',transition,'Deferred method')[1]
 left=one(r'^signal (area_left)$',area,'Area left')[1]
 room=read(ROOT/room_ir);h=read(ROOT/DEPENDENCIES[2]);bindings=read(ROOT/DEPENDENCIES[3]);ss=room['strings'];rr=room['sections']
 require(len(rr['Scene'])==1 and ss[rr['Scene'][0]['source_scene_string']]=='res://'+TARGET,'Existing Room scene differs')
 flags=[dict(index=i,id=x['stable_id'],name=ss[x['name_string']])for i,x in enumerate(rr['Flag'])]
 fn={f['name']for f in flags};require(visit in fn and flyingflag in fn,'House Ready flags absent from Room')
 actors=[]
 for i,n in enumerate(h['npcs']):
  x=rr['ActorInstance'][n['room_actor_index']];bind=[b for b in bindings['npcs']if b['source_path']==n['source_path']];require(len(bind)==1 and bind[0]['actor_stable_id']==x['stable_id'],'House actor source mapping differs')
  require(node(house,n['source_path'])['position']==n['position'],'House actor location differs from source')
  actors.append(dict(house_index=i,room_index=n['room_actor_index'],id=x['stable_id'],body=n['body_id'],node=n['source_path'],position=x['position'],direction=x['direction']))
 bodies=[dict(index=i,id=x['body_id'],node=ss[x['source_path_string']])for i,x in enumerate(rr['BodyRule'])]
 require(len(set(x['node']for x in bodies))==len(bodies),'Duplicate Room source body')
 landmarks=[]
 for m in re.finditer(r'^\[node name="([^\"]+)" parent="([^\"]+)" instance=ExtResource\( 17 \)\]',house,re.M):
  path=m[1]if m[2]=='.'else m[2]+'/'+m[1];v=node(house,path)
  appear=v.get('appear_flag','');disappear=v.get('disappear_flag','');require((not appear or appear in fn)and(not disappear or disappear in fn),'Unknown House landmark flag')
  default=one(r'export var delete_if_hidden = (true|false)',landmark,'Landmark delete default')[1]=='true'
  landmarks.append(dict(node=path,appear=appear,disappear=disappear,delete_if_hidden=v.get('delete_if_hidden',default)))
 require('queue_free()'in landmark and 'global.connect("flags_updated", self, "_check_flags")'in landmark,'Landmark mechanism changed')
 # The complete native export proves collision absence for every placed cell,
 # including the source's missing tile. This is not inferred from BodyRule.
 from tools.house_geometry import load as load_geometry
 geometry=load_geometry()
 require(geometry['scene']==TARGET and geometry['commit']==PIN and geometry['source_sha256']==ex.sources[TARGET],'House tile proof scene differs')
 for p,h in geometry['sources'].items():
  require(p not in ex.sources or ex.sources[p]==h,'House tile proof source differs '+p)
  ex.sources[p]=h
 tilemaps=[]
 for m in geometry['tile_collision']:
  resource=m['resource'];require(resource.startswith('res://'),'House TileSet resource is not source-owned')
  resource_source=resource[6:].split('::',1)[0]
  require(resource_source in ex.sources and m['native_collision_parts']==0 and all(t['shape']is None and t['shapes']==[] for t in m['tiles']),'House TileMap collision needs implementation')
  tilemaps.append(dict(id=stable('house-reentry-native:'+TARGET+'#'+m['node']),**m,resource_source=resource_source,resource_sha256=ex.sources[resource_source]))
 require(len(tilemaps)==3 and len({m['id']for m in tilemaps})==3,'House complete TileMap proof differs')
 packrows=[]
 for role,path in [(1,room_pack),(2,'data/opening.enchouse')]:
  raw=(ROOT/'romfs'/path).read_bytes();packrows.append(dict(role=role,path=path,size=len(raw),sha256=hashlib.sha256(raw).hexdigest()))
 return dict(schema=1,format=2,family=FAMILY,capability=1,rules=1,commit=PIN,scene_id=stable('house-reentry:'+TARGET),source_sha256=ex.sources[TARGET],sources=ex.sources,tilemaps=tilemaps,
  dependencies={p:sha(ROOT/p)for p in dependencies},packs=packrows,door_ir_sha256=sha(ROOT/DEPENDENCIES[0]),source_scene=SOURCE,target_scene=TARGET,door_id=entry['id'],target_root_name=rootname,source_region=source_region,target_region=region,player_parent=parentpath,position=[entry['target'][0],entry['target'][1]-d['ground_offset']],direction=entry['direction'],empty_target_params=True,
  body_signal=body_signal,body_method=d['body_method'],door_signals=signals,scene_changed_signal=changed,deferred_method=deferred,area_left_signal=left,area_left_arguments=1,party_changed_signal=follower_tail[2],party_changed_wait_signal=follower_tail[1],party_changed_method=followers[1],party_changed_body_sha256=hashlib.sha256(followers[2].encode()).hexdigest(),steps=[dict(step=i,source=s)for i,s in enumerate(selectors)],
  ready=dict(script=AREA,visit_method=ready[1].strip()[:-2],flying_method=ready[2].strip()[:-2],visit_flag=visit,magicant_region=flying,flying_flag=flyingflag,flying_character=character[0],party_npcs_member=character[1],switch_signal=switch[1],switch_method=switch[2]),flags=flags,actors=actors,bodies=bodies,landmarks=landmarks,native_nodes=native,
  admission=dict(full_cold_ready=False,read_only_pack=True,reset_initial_flags=False,reseed=False,new_player=False,notes='Actual current flags and source object owners supplied by continuation. House source root extends AreaRoom; root has no Ready override. Its animation_finished battle callback is not run by return Ready. Existing bounded House/Room execute their original mapped behavior; this resource grants no additional unknown content.'))
def extract():
 d=derive();write(IR,d);write(REVIEW,dict(schema=1,commit=PIN,ir_sha256=sha(IR),producer_sha256=sha(Path(__file__)),sources=d['sources'],dependencies=d['dependencies'],admission=d['admission']));return d
def load():
 d=read(IR);require(d==derive(),'House return source/pack dependency changed');r=read(REVIEW);require(r==dict(schema=1,commit=PIN,ir_sha256=sha(IR),producer_sha256=sha(Path(__file__)),sources=d['sources'],dependencies=d['dependencies'],admission=d['admission']),'House return source review stale');return d
def binary(d,ir_sha256=None):
 b=bytearray(128)
 def u(*v):b.extend(struct.pack('<'+'I'*len(v),*v))
 def t(v):raw=v.encode();u(len(raw));b.extend(raw)
 def vec(v):b.extend(struct.pack('<2f',*v))
 for p in d['packs']:u(p['role'],p['size']);b.extend(bytes.fromhex(p['sha256']));t(p['path'])
 b.extend(bytes.fromhex(d['door_ir_sha256']));u(d['door_id'])
 for k in ['source_scene','target_scene','target_root_name','source_region','target_region','player_parent']:t(d[k])
 vec(d['position']);vec(d['direction']);u(d['empty_target_params'])
 for k in ['body_signal','body_method']:t(d[k])
 for s in d['door_signals']:t(s)
 for k in ['scene_changed_signal','deferred_method','area_left_signal']:t(d[k])
 u(d['area_left_arguments'])
 for k in ['party_changed_signal','party_changed_wait_signal']:t(d[k])
 for v in d['ready'].values():t(v)
 u(len(d['flags']))
 for x in d['flags']:u(x['index'],x['id']);t(x['name'])
 u(len(d['actors']))
 for x in d['actors']:u(x['house_index'],x['room_index'],x['id'],x['body']);t(x['node']);vec(x['position']);vec(x['direction'])
 u(len(d['bodies']))
 for x in d['bodies']:u(x['index'],x['id']);t(x['node'])
 u(len(d['landmarks']))
 for x in d['landmarks']:t(x['node']);t(x['appear']);t(x['disappear']);u(x['delete_if_hidden'])
 u(len(d['native_nodes']))
 for x in d['native_nodes']:
  u(x['id'],x['parent'],x['owner_role'])
  for k in ['node','name','native_class','script']:t(x[k])
  b.extend(bytes.fromhex(x['script_sha256']));b.extend(struct.pack('<6f',*x['local']))
 u(len(d['tilemaps']))
 for m in d['tilemaps']:
  u(m['id'],m['layer'],m['mask'],m['cell_count'],m['native_collision_parts'])
  for k in ['node','resource','resource_source']:t(m[k])
  b.extend(bytes.fromhex(m['cell_sha256']));b.extend(bytes.fromhex(m['resource_sha256']))
  u(len(m['tiles']))
  for tile in m['tiles']:u(tile['id'],tile['present'],0)
 u(len(d['steps']))
 for x in d['steps']:u(x['step']);t(x['source'])
 u(len(d['sources']))
 for p,h in d['sources'].items():t(p);b.extend(bytes.fromhex(h))
 struct.pack_into('<8s8I',b,0,b'ENCHRET1',2,128,len(b),zlib.crc32(b[128:]),FAMILY,1,1,d['scene_id']);b[40:60]=bytes.fromhex(PIN);b[60:92]=bytes.fromhex(d['source_sha256']);b[92:124]=bytes.fromhex(ir_sha256 or sha(IR));return bytes(b)
def stage_files(root):
 raw=binary(load());relative=Path('data/house.encreentry');require((Path(root)/relative).read_bytes()==raw,'House return staged pack differs');return {relative:raw}
def main():
 p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile','verify']);a=p.parse_args()
 if a.action=='extract':extract();return
 raw=binary(load())
 if a.action=='compile':PACK.parent.mkdir(parents=True,exist_ok=True);PACK.write_bytes(raw)
 else:require(PACK.read_bytes()==raw,'House return binary stale')
 print('House return resource:',len(raw),'bytes; source door and original House/Room only, Ready not granted')
if __name__=='__main__':
 try:main()
 except (ValueError,OSError,KeyError,TypeError,struct.error)as e:sys.exit('HOUSE RETURN ERROR: '+str(e))
