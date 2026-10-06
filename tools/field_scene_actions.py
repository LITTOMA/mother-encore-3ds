#!/usr/bin/env python3
"""Actual Podunk Reparenter14/EventActivator4; typed scene operations only."""
from __future__ import annotations
import argparse,hashlib,json,math,posixpath,re,struct,sys,zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import decode,stable,SCENE,PIN,read,write,sha,require
from tools.extract_battle_entry import Extractor
REPAR='Scripts/Main/Reparenter.gd';EVENT='Scripts/Main/Event Activator.gd';SCRIPTS={REPAR,EVENT}
IR=ROOT/'content/native-field-scene-actions.json';REVIEW=ROOT/'reports/field-scene-actions/source-review.json';PACK=ROOT/'romfs/data/podunk-scene-actions.encsact'
ENGINE={'core/message_queue.cpp':'73f9c4d593d62fd1b5dfe227ea5c941bb8be2c3ae562e172432ec8c9c0bdf9b3','core/object.cpp':'7d9bb073fde7ba0c1d7b057fbefcdba292148eeb4cdd58c1c975c9e8f8f3b8c6','scene/main/node.cpp':'0c2e3230b37644c8496f5b1aee59f428de6a497a380c247613c489e44cd508a9'}
def source_instances(native,receipt,upstream):
 d=read(native);s=read(receipt);require(d['source']=='res://'+SCENE and d['schema']==1 and d['native_compatible'] is False,'Incomplete CutsceneArea native scene')
 require(s['commit']==PIN and s['scene']==SCENE,'Unreviewed CutsceneArea native receipt')
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
  if bindings[path]not in SCRIPTS:continue
  out.append(dict(stable_id=stable(path),node=path,ready_ordinal=ordinal[path],script=bindings[path],overrides=overrides[path],native=names[path],children=[names[p] for p in children[path]]))
 require(len(out)==18,'Podunk scene-action binding loss');return out,names,resources,dict(native_export_sha256=sha(native),source_receipt_sha256=sha(receipt),source_files={name:r['sha256'] for name,r in s['files'].items()},script_bindings=bindings)


def build(records,nodes,resources,provenance,engine):
 ex=Extractor(ROOT);ex.sources.update(provenance['source_files']);repar=ex.text(REPAR);event=ex.text(EVENT);rs=ex.text('Nodes/Reusables/Reparenter.tscn');es=ex.text('Nodes/Reusables/event activator.tscn');steps=ex.text('Scripts/Main/Stepping Sounds.gd');ex.text('LICENSE')
 for p,h in ENGINE.items():require(sha(engine/p.rsplit('/',1)[-1])==h,'Unreviewed actual engine source '+p)
 for fact in ['objectNodes.append(get_node_or_null(object))','newParentNode = get_node_or_null(new_parent)','item == null or newParentNode == null or item.get_parent() == newParentNode or item.get_parent() == null','item.get_parent().remove_child(item)','newParentNode.call_deferred("add_child", item)','item.set_deferred("collision_mask", newParentNode.collision_mask)','item.set_deferred("collision_layer", newParentNode.collision_layer)']:require(fact in repar,'Unknown Reparenter source '+fact)
 for fact in ['object = get_node_or_null(object_path)','globaldata.flags.has(check_flag) and globaldata.flags[check_flag] == check_flag_state','object.has_method(function)','object.call(function)','flag_set != "" and globaldata.flags.has(flag_set)','globaldata.flags[flag_set] = set_flag_state']:require(fact in event,'Unknown EventActivator source '+fact)
 method_names=re.findall(r'call_deferred\("([^"]+)"',repar);properties=re.findall(r'set_deferred\("([^"]+)"',repar);require(len(method_names)==1 and len(properties)==2,'Scene action queue source topology')
 source_scripts=provenance.pop('script_bindings');targets={};bindings=[]
 def nodepath(value):
  return value['value']if isinstance(value,dict)and value.get('type')=='NodePath'else value
 def resolve(origin,relative):
  if relative=='':return '',0
  name=posixpath.normpath(origin+'/'+relative);require(name in nodes and not name.startswith('..'),'Unresolved source NodePath '+relative);return name,stable(name)
 def reference(name):
  if not name:return 0
  if name in targets:return targets[name]['id']
  n=nodes[name];v=decode(n['properties']);kind={'TileMap':1,'CollisionPolygon2D':2,'Area2D':3,'Node2D':4}.get(n['class'],0);require(kind,'Unsupported scene-action target '+n['class']);desc=[p for p in nodes if p.startswith(name+'/')];wt=decode(n['world_transform']);parent=name.rsplit('/',1)[0]if'/'in name else'.';attached=source_scripts.get(name,'');require(not attached or(name=='Stepping Sounds/Stone'and attached=='Scripts/Main/Stepping Sounds.gd'),'Runtime-mutating script target needs explicit binding receipt')
  targets[name]=dict(id=stable(name),node=name,kind=kind,parent=stable(parent),script=attached,collision_mask=v.get('collision_mask',0),collision_layer=v.get('collision_layer',0),has_collision_properties=kind in[1,3],local_position=v['position'],local_scale=v['scale'],local_rotation=v['rotation'],world_transform=[*wt[0],*wt[1],*wt[2]],descendants=len(desc),camera_descendants=sum(nodes[p]['class']=='Camera2D'for p in desc),viewport_descendants=sum(nodes[p]['class']=='Viewport'for p in desc));return targets[name]['id']
 for a in records:
  path=a['node'];v=decode(a['native']['properties']);o=a['overrides'];kind=1 if a['script']==REPAR else 2;shapes=[]
  for owner in a['native']['physics_shape_owners']:
   child=nodes[owner['owner']['path']];wt=decode(child['world_transform']);require(wt[0][1]==wt[1][0]==0 and wt[0][0]>0 and wt[1][1]>0,'Scene action rotated area capability');parts=[]
   for shape in owner['shapes']:
    r=resources[shape['id']];p=decode(r['properties']);require(r['class']=='RectangleShape2D','Unsupported scene-action area shape');x,y=p['extents'];parts.append([wt[2][0],wt[2][1],x*wt[0][0],y*wt[1][1]])
   shapes.append(dict(id=stable(child['path']),node=child['path'],disabled=owner['disabled'],parts=parts))
  b=dict(id=a['stable_id'],node=path,kind=kind,ready_ordinal=a['ready_ordinal'],collision_layer=v['collision_layer'],collision_mask=v['collision_mask'],monitoring=v['monitoring'],monitorable=v['monitorable'],shapes=shapes)
  require(v['space_override']==0 and not v['audio_bus_override']and v['pause_mode']==0 and v['process_priority']==0,'Unsupported scene action area override')
  if kind==1:
   raw=nodepath(o.get('new_parent',''));name,id=resolve(path,raw);reference(name);objects=[]
   for relative in o.get('object_paths',[]):
    relative=nodepath(relative);name,id=resolve(path,relative);reference(name);objects.append(dict(path=relative,id=id))
   b.update(parent_path=raw,parent_id=resolve(path,raw)[1],objects=objects,copy=o.get('copy_collisions',False));require(b['copy']is True,'Source14 copy-collisions scope changed')
  else:
   raw=nodepath(o.get('object_path',''));name,id=resolve(path,raw);reference(name);method=o.get('function','');action=0
   if method:
    require(name=='Stepping Sounds/Stone'and method in['enable','disable'],'Unknown actual EventActivator target method');match=re.search(r'func '+method+r'\(\):\n\tenabled = (true|false)',steps);require(match,'Unknown stepping source method body');action=1 if match[1]=='true'else 2
   b.update(object_path=raw,object_id=id,method=method,action=action,check_flag=o.get('check_flag',''),check_state=o.get('check_flag_state',False),set_flag=o.get('flag_set',''),set_state=o.get('set_flag_state',True))
  bindings.append(b)
 return dict(schema=1,kind='encore.field-scene-actions.source-ir',commit=PIN,scene=SCENE,scripts=[REPAR,EVENT],deferred_method=method_names[0],deferred_properties=properties,connections=[dict(kind=kind,signal=signal,method=method)for kind,scene in[(1,rs),(2,es)]for signal,method in re.findall(r'\[connection signal="([^"]+)" from="\." to="\." method="([^"]+)"\]',scene)],references=list(targets.values()),bindings=bindings,sources=dict(sorted(ex.sources.items())),provenance=provenance,engine=[dict(url='https://raw.githubusercontent.com/godotengine/godot/3.6.2-stable/'+p,sha256=h)for p,h in ENGINE.items()],semantics=['Actual14 Reparenter and4 EventActivator; source NodePaths cache object identities once in onready/Ready, not re-resolved after removal or renaming','For each cached object skip known null/newparent-null/equal-parent/no-parent; otherwise immediate remove_child then global FIFO deferred add_child, then mask then layer, with parent property reads in source expression order','Local transform remains unchanged; actual SceneTree exit/enter, map render depth/inherited visibility/tint and physical quadrant/shape registration must be handled by typed actual scene host','Reparent targets are six leaf TileMaps and two leaf CollisionPolygon2D (no target-camera/viewport/script descendants); source parents are TileMap/Stone Area2D; no fabricated Camera/PersistTree changes or request_ready','Godot3.6.2 MessageQueue TYPE_SET uses Object.set without r_valid; no-script CollisionPolygon2D absent collision_mask/layer writes are known source no-ops but still occupy FIFO positions; other unknown property/capability refuses','EventActivator exact player enter checks flags.has AND state, absent checkflag does not activate; direct activate_event bypasses trigger guard; method call precedes flag-set','Method present is checked actual SteppingSounds.enable/disable; missing/null/empty method follows source no-call branch, not fabricated generic call; actual stepping consumer admission mandatory','set_flag changes only existing dictionary key and never creates missing flags; default true remains binary source tuning','No RNG, timers, cutscene polling or generic GDScript evaluator added; deferred entries use scene-global issued identities and actual source queue flush ordering'],unsupported=['Scene host without real dynamic TileMap quadrant or CollisionPolygon parent owner detach/attach must refuse reparent plan before first mutation','Stepping Sounds typed consumer method unavailable rejects prior to source call or subsequent flag write','Any future Camera/Viewport/Persist subtree, scripted reparented target, arbitrary method or property requires a reviewed typed capability'])

def validate(d):
 require(d['schema']==1 and d['kind']=='encore.field-scene-actions.source-ir'and d['commit']==PIN and d['scene']==SCENE and d['scripts']==[REPAR,EVENT],'Scene actions source version');require(len(d['bindings'])==18 and sum(b['kind']==1 for b in d['bindings'])==14 and sum(b['kind']==2 for b in d['bindings'])==4,'Scene actions class count');require(d['deferred_method']=='add_child'and d['deferred_properties']==['collision_mask','collision_layer'],'Scene source queue rules')
 require(d['engine']==[dict(url='https://raw.githubusercontent.com/godotengine/godot/3.6.2-stable/'+p,sha256=h)for p,h in ENGINE.items()],'Unreviewed engine semantics')
 require(d['connections']==[dict(kind=1,signal='body_entered',method='_on_Reparenter_body_entered'),dict(kind=2,signal='body_entered',method='_on_Event_Activator_body_entered')],'Unknown source action signal topology')
 refs={r['id']:r for r in d['references']};require(len(refs)==len(d['references']),'Scene reference collision');ids=set();last=-1
 for r in refs.values():
  require(r['id']==stable(r['node'])and 1<=r['kind']<=4 and type(r['has_collision_properties'])is bool and r['has_collision_properties']==(r['kind']in[1,3]),'Source reference type/identity')
  require(all(type(v)in(int,float)and math.isfinite(v)and abs(v)<=1e6 for v in r['local_position']+r['local_scale']+[r['local_rotation']]+r['world_transform'])and len(r['local_position'])==len(r['local_scale'])==2 and len(r['world_transform'])==6 and all(x>0 for x in r['local_scale']),'Source reference finite transform')
 for b in d['bindings']:
  require(b['id']==stable(b['node'])and b['id']not in ids and b['ready_ordinal']>last,'Source action identity/order');ids.add(b['id']);last=b['ready_ordinal'];require(b['shapes'],'Missing source trigger owner')
  if b['kind']==1:
   require(b['parent_id']in refs and b['copy']is True and b['objects']and all(o['id']in refs for o in b['objects']),'Unknown source reparent references')
   require(all(not refs[o['id']]['descendants']and not refs[o['id']]['script']and refs[o['id']]['kind']in[1,2]for o in b['objects']),'Unknown reparented subtree behavior')
  else:require(b['object_id']==0 or b['object_id']in refs,'Unknown event reference');require(b['action']in[0,1,2]and b['method']==['','enable','disable'][b['action']] and all(type(b[k])is bool for k in['check_state','set_state']),'Unknown typed source method/flags')
  for s in b['shapes']:
   require(s['id']==stable(s['node'])and type(s['disabled'])is bool,'Source area shape identity')
   for p in s['parts']:require(len(p)==4 and all(type(v)in(int,float)and math.isfinite(v)and abs(v)<=1e6 for v in p)and p[2]>0 and p[3]>0,'Source area finite rectangle')
 return d

def load():
 d=validate(read(IR));r=read(REVIEW);require(r['commit']==PIN and r['ir_sha256']==sha(IR)and r['sources']==d['sources']and r['engine']==d['engine'],'Scene action semantic review stale');inv=read(ROOT/'compatibility/upstream-inventory.json')['files']
 for p,h in d['sources'].items():require(sha(ROOT/'upstream/MOTHER-Encore'/p)==h==inv[p]['sha256'],'Changed scene action source '+p)
 return d

def encode(d):
 validate(d);b=bytearray(64)
 def u(*v):b.extend(struct.pack('<'+'I'*len(v),*v))
 def text(v):raw=v.encode();u(len(raw));b.extend(raw)
 text(d['scene']);u(len(d['scripts']))
 for p in d['scripts']:text(p)
 text(d['deferred_method']);u(len(d['deferred_properties']))
 for p in d['deferred_properties']:text(p)
 u(len(d['connections']))
 for c in d['connections']:u(c['kind']);text(c['signal']);text(c['method'])
 u(len(d['references']))
 for r in d['references']:
  u(r['id'],r['kind'],r['parent'],r['collision_mask'],r['collision_layer'],int(r['has_collision_properties']),r['descendants'],r['camera_descendants'],r['viewport_descendants']);text(r['node']);text(r['script']);b.extend(struct.pack('<11f',*(r['local_position']+r['local_scale']+[r['local_rotation']]+r['world_transform'])))
 u(len(d['bindings']))
 for a in d['bindings']:
  u(a['id'],a['kind'],a['ready_ordinal'],a['collision_layer'],a['collision_mask'],int(a['monitoring'])|int(a['monitorable'])<<1);text(a['node']);u(len(a['shapes']))
  for s in a['shapes']:
   u(s['id'],int(s['disabled']));text(s['node']);u(len(s['parts']))
   for p in s['parts']:b.extend(struct.pack('<4f',*p))
  if a['kind']==1:
   u(a['parent_id'],int(a['copy']));text(a['parent_path']);u(len(a['objects']))
   for o in a['objects']:u(o['id']);text(o['path'])
  else:u(a['object_id'],a['action'],int(a['check_state']),int(a['set_state']));text(a['object_path']);text(a['method']);text(a['check_flag']);text(a['set_flag'])
 u(len(d['sources']))
 for p,h in d['sources'].items():text(p);b.extend(bytes.fromhex(h))
 u(len(d['engine']))
 for e in d['engine']:text(e['url']);b.extend(bytes.fromhex(e['sha256']))
 struct.pack_into('<8s6I20s12x',b,0,b'ENCSAC01',1,len(b),0,1,1,len(d['bindings']),bytes.fromhex(PIN));struct.pack_into('<I',b,16,zlib.crc32(b)&0xffffffff);return bytes(b)

def stage_files(source):
 b=encode(load());p=Path('data/podunk-scene-actions.encsact');require((Path(source)/p).read_bytes()==b,'Stale scene actions binary');return{p:b}
def main():
 p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile']);p.add_argument('--native',type=Path);p.add_argument('--source',type=Path);p.add_argument('--engine',type=Path);a=p.parse_args()
 if a.action=='extract':
  require(a.native and a.source and a.engine,'Actual Podunk and fixed engine sources required');d=validate(build(*source_instances(a.native,a.source,ROOT/'upstream/MOTHER-Encore'),a.engine));write(IR,d);write(REVIEW,dict(schema=1,commit=PIN,ir_sha256=sha(IR),sources=d['sources'],engine=d['engine'],semantics=d['semantics'],unsupported=d['unsupported']));print('Scene action source:14 Reparenter /4 EventActivator');return
 b=encode(load());PACK.parent.mkdir(parents=True,exist_ok=True);PACK.write_bytes(b);print('Scene action checked resource:',len(b),'bytes')
if __name__=='__main__':
 try:main()
 except(ValueError,KeyError,OSError,TypeError,struct.error)as e:sys.exit('FIELD SCENE ACTION ERROR: '+str(e))
