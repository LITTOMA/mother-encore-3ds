#!/usr/bin/env python3
"""Complete checked original House geometry; no source lifecycle is admitted."""
from __future__ import annotations
import argparse,hashlib,json,re,struct,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import PIN,read,write,sha,require,decode,stable
from tools import field_geometry as geometry
from tools.scene_reference import quarantine
SCENE='Maps/podunk/Nintens House.tscn'
NATIVE=ROOT/'reports/cloud-world/house-exact.json'
SOURCE=ROOT/'reports/cloud-world/house-source.json'
IR=ROOT/'content/native-house-geometry.json'
REVIEW=ROOT/'reports/house-geometry/source-review.json'
PACK=ROOT/'romfs/data/house.encfieldgeometry'
NONE=geometry.NONE

def multiply(a,b):
 return [[geometry.f(a[0][0]*b[0][0]+a[1][0]*b[0][1]),geometry.f(a[0][1]*b[0][0]+a[1][1]*b[0][1])],
         [geometry.f(a[0][0]*b[1][0]+a[1][0]*b[1][1]),geometry.f(a[0][1]*b[1][0]+a[1][1]*b[1][1])],
         [geometry.f(a[0][0]*b[2][0]+a[1][0]*b[2][1]+a[2][0]),geometry.f(a[0][1]*b[2][0]+a[1][1]*b[2][1]+a[2][1])]]

def scripts(d,s,sources,resources):
 states={v['source'][6:]:v for v in d['scene_states']};assignments={};attachments=[]
 for file in states:
  require(file in sources,'House SceneState source absent '+file)
  clean,_,attached,_=quarantine((ROOT/'upstream/MOTHER-Encore'/file).read_bytes(),file)
  attachments.extend(attached);by={a['node_declaration']:a for a in attached};entries=[];current='';props=[]
  for line in clean.decode().splitlines():
   if line.startswith('['):
    if current:entries.append((current,props))
    current=line if line.startswith('[node ')else'';props=[]
   elif current:props.append(line)
  if current:entries.append((current,props))
  rows=[]
  for decl,lines in entries:
   changed=[v for v in lines if re.match(r'^script\s*=',v)]
   if not changed:continue
   require(len(changed)==1 and re.fullmatch(r'script\s*=\s*null',changed[0]),'House unknown source script assignment')
   name=re.search(r'\bname="([^"\n]+)"',decl)[1].replace("\\'","'");p=re.search(r'\bparent="([^"\n]+)"',decl)
   local='.'if p is None else name if p[1]=='.'else p[1].replace("\\'","'")+'/'+name
   a=by.get(decl);script=a['script']if a else None
   proof=a.get('embedded_script',{}).get('sha256')if a else None
   if script and not proof:require(script in sources,'House source script outside receipt '+script);proof=sources[script]
   rows.append((local,script,proof,decl))
  assignments[file]=rows
 key=lambda v:(v['source'],v['line'],v['node_declaration'])
 require(len(attachments)==25 and sorted(attachments,key=key)==sorted(s['script_attachments'],key=key),'House actual 25 source attachments/embedded declarations changed')
 require(sum('embedded_script'in a for a in attachments)==2,'House embedded source coverage changed')
 roots=[];active=set()
 def visit(root,file):
  require((root,file)not in active and file in states,'House missing/cyclic original SceneState')
  active.add((root,file));roots.append((root,file))
  for n in states[file]['nodes']:
   if n['instance']is not None:
    local=n['path'][2:]if n['path'].startswith('./')else n['path']
    target=root if local=='.'else local if root=='.'else root+'/'+local
    res=resources[n['instance']['id']];require(res['class']=='PackedScene' and res['path'].startswith('res://'),'House unknown instance source')
    visit(target,res['path'][6:])
  active.remove((root,file))
 visit('.',SCENE);names={n['path']for n in d['nodes']};bindings={};nulls={}
 for root,file in sorted(roots,key=lambda v:v[0].count('/')if v[0]!='.'else -1,reverse=True):
  for local,script,proof,decl in assignments[file]:
   target=root if local=='.'else local if root=='.'else root+'/'+local
   require(target in names,'House attachment target absent '+target)
   if script is None:
    if target in bindings:nulls[target]=dict(previous_script=bindings[target][0],source=file,declaration=decl)
    bindings.pop(target,None)
   else:bindings[target]=(script,proof);nulls.pop(target,None)
 return bindings,nulls,attachments

def tile_collision_proof(d,resources):
 maps=[]
 for n in d['nodes']:
  if n['class']!='TileMap':continue
  p=decode(n['properties']);require(p['format']==1 and p['tile_set']is not None,'House unknown TileMap format/TileSet')
  tiles=resources[p['tile_set']['id']];require(tiles['class']=='TileSet','House TileMap native resource differs')
  props=decode(tiles['properties']);cells=p['tile_data'];require(len(cells)%3==0,'House malformed native tile cells')
  ids=sorted({cells[i+1]&0x1fffffff for i in range(0,len(cells),3)});used=[]
  for id in ids:
   prefix=str(id)+'/'
   exists=any(k.startswith(prefix)for k in props)
   if exists:
    require(props[prefix+'shape']is None and props[prefix+'shapes']==[],'House placed tile native collision requires implementation')
   used.append(dict(id=id,present=exists,shape=None,shapes=[]))
  maps.append(dict(node=n['path'],resource=tiles['path'],cell_count=len(cells)//3,cell_sha256=hashlib.sha256(struct.pack('<'+'i'*len(cells),*cells)).hexdigest(),layer=p['collision_layer'],mask=p['collision_mask'],tiles=used,native_collision_parts=0))
 require([(m['node'],m['cell_count'])for m in maps]==[('Below',1),('Objects',14),('Above',276)],'House full placed TileMap coverage changed')
 require(maps[0]['tiles']==[dict(id=30,present=False,shape=None,shapes=[])] and all(t['present']for m in maps[1:]for t in m['tiles']),'House missing tile30/empty native tile shape source changed')
 return maps

def derive():
 d=read(NATIVE);s=read(SOURCE);inv=read(ROOT/'compatibility/upstream-inventory.json')
 require(d['schema']==s['schema']==1 and d['source']=='res://'+SCENE and s['scene']==SCENE and s['commit']==PIN==inv['commit'] and d['native_compatible']is False and s['native_compatible']is False,'House full native/source identity changed')
 require([d['godot'].get(k)for k in ('major','minor','patch','status')]==[3,6,2,'stable'],'House native engine changed')
 sources={}
 for name,proof in s['files'].items():require(sha(ROOT/'upstream/MOTHER-Encore'/name)==proof['sha256']==inv['files'][name]['sha256'],'House source receipt changed '+name);sources[name]=proof['sha256']
 require(len(sources)==71,'House exact source dependency count changed')
 nm={n['path']:n for n in d['nodes']};resources={r['id']:r for r in d['resources']}
 require(len(nm)==len(d['nodes'])==497 and len(resources)==len(d['resources']) and list(nm)[0]=='.','House complete node/resource export changed')
 require(all(decode(n['properties']).get('script')is None for n in d['nodes']),'House native export executed/retained scripts')
 bindings,nulls,attachments=scripts(d,s,sources,resources)
 children={p:[]for p in nm}
 for p in nm:
  if p!='.':require(geometry.parent(p)in children,'House ancestor missing');children[geometry.parent(p)].append(p)
 ready=[]
 def visit(p):
  for c in children[p]:visit(c)
  ready.append(p)
 visit('.');require(len(ready)==497,'House full tree ordering failed')
 ordinals={p:i for i,p in enumerate(ready)};orders={p:i for i,p in enumerate(nm)}
 bodies=[n for n in d['nodes']if n['class']in('Area2D','StaticBody2D','KinematicBody2D','RigidBody2D')]
 shapes=[n for n in d['nodes']if n['class']in('CollisionShape2D','CollisionPolygon2D')]
 require(len(bodies)==100 and len(shapes)==113 and {k:sum(n['class']==k for n in bodies)for k in('Area2D','StaticBody2D','KinematicBody2D','RigidBody2D')}==dict(Area2D=79,StaticBody2D=15,KinematicBody2D=6,RigidBody2D=0),'House complete owner/shape classes changed')
 required={'.'}
 for n in bodies+shapes:
  p=n['path']
  while p!='.':required.add(p);p=geometry.parent(p)
 paths=[p for p in nm if p in required];indices={p:i for i,p in enumerate(paths)};require(len(paths)==230,'House collision ancestor closure changed')
 nodes=[];worlds={}
 for p in paths:
  n=nm[p];pr=decode(n['properties']);require('world_transform'in n and pr['rotation']==0 and not pr.get('toplevel',False),'House new native transform mechanism needs review '+p)
  position=geometry.vec(pr['position']);scale=geometry.vec(pr['scale']);local=[[scale[0],0],[0,scale[1]],position];world=geometry.transform(n)
  actual=local if p=='.'else multiply(worlds[geometry.parent(p)],local)
  require(actual==world,'House source/native hierarchy matrix differs '+p);worlds[p]=world;b=bindings.get(p)
  nodes.append(dict(stable_id=stable('house-reentry-native:'+SCENE+'#'+p),path=p,parent=NONE if p=='.'else indices[geometry.parent(p)],order=orders[p],ready=ordinals[p],flags=int(pr['visible'])|(pr['pause_mode']<<1),class_name=n['class'],script=b[0]if b else'',script_sha256=b[1]if b else'00'*32,local_transform=local,world_transform=world))
 owners=[];owner_indices={n['path']:i for i,n in enumerate(bodies)};ordered_shapes=[]
 require(all(geometry.parent(n['path'])in owner_indices for n in shapes),'House orphan native collision shape')
 for n in bodies:
  pr=decode(n['properties']);require(pr.get('physics_material_override')is None,'House native physics material needs checked consumer')
  attached=[v for v in shapes if geometry.parent(v['path'])==n['path']]
  flags=sum(int(pr.get(k,False))<<i for i,k in enumerate(('input_pickable','monitoring','monitorable','audio_bus_override','gravity_point','motion/sync_to_physics')))
  owners.append(dict(node=indices[n['path']],kind=('StaticBody2D','KinematicBody2D','RigidBody2D','Area2D').index(n['class'])+1,layer=pr['collision_layer'],mask=pr['collision_mask'],flags=flags,shape_first=len(ordered_shapes),shape_count=len(attached),audio_bus=pr.get('audio_bus_name',''),safe_margin=pr.get('collision/safe_margin',0),space_override=pr.get('space_override',0),priority=pr.get('priority',0),gravity=pr.get('gravity',0),gravity_distance_scale=pr.get('gravity_distance_scale',0),gravity_vec=pr.get('gravity_vec',[0,0]),linear_damp=pr.get('linear_damp',0),angular_damp=pr.get('angular_damp',0),constant_linear_velocity=pr.get('constant_linear_velocity',[0,0]),constant_angular_velocity=pr.get('constant_angular_velocity',0),moving_platform_leave=pr.get('moving_platform_apply_velocity_on_leave',0)))
  ordered_shapes.extend(attached)
 native_owners={v['owner']['path']:decode(v)for n in bodies for v in n.get('physics_shape_owners',[])}
 require(set(native_owners)=={n['path']for n in shapes},'House native shape owner coverage changed')
 primitives=[];shape_rows=[];native_parts=0
 def add(kind,points,parameters,resource):primitives.append(dict(kind=kind,points=points,parameters=parameters,resource=resource));return len(primitives)-1
 for n in ordered_shapes:
  pr=decode(n['properties']);no=native_owners[n['path']];parts=[]
  require(no['disabled']==pr['disabled'] and no['one_way']==pr['one_way_collision'],'House source/native shape state differs')
  require([geometry.vec(v)for v in no['transform']]==nodes[indices[n['path']]]['local_transform'],'House native shape-owner local differs')
  if n['class']=='CollisionShape2D':
   ref=pr['shape'];require(ref is not None and no['shapes']==[ref],'House original primitive/native shape binding changed')
   resource=resources[ref['id']];require(resource['class']in('RectangleShape2D','CircleShape2D'),'House new primitive kind requires review')
   kind=geometry.KINDS[resource['class']];rp=decode(resource['properties']);params=[0]*6
   if kind==1:params[:2]=rp['extents']
   else:params[0]=rp['radius']
   params[5]=rp.get('custom_solver_bias',0);original=add(kind,[],params,resource['path']);parts=[original]
  else:
   require(pr['build_mode']==0,'House new polygon build mode requires review');kind=9;original=add(kind,pr['polygon'],[0]*6,'')
   for ref in no['shapes']:
    resource=resources[ref['id']];require(resource['class']=='ConvexPolygonShape2D','House unexpected native polygon decomposition')
    rp=decode(resource['properties']);params=[0]*6;params[5]=rp.get('custom_solver_bias',0);parts.append(add(4,rp['points'],params,resource['path']))
   require(parts or len(pr['polygon'])<3,'House original convex decomposition failed')
  require(parts==list(range(parts[0],parts[0]+len(parts)))if parts else True,'House noncontiguous native shape parts');native_parts+=len(parts)
  shape_rows.append(dict(node=indices[n['path']],owner=owner_indices[geometry.parent(n['path'])],kind=kind,flags=int(pr['disabled'])|(int(pr['one_way_collision'])<<1),geometry=original,part_first=parts[0]if parts else NONE,part_count=len(parts),margin=pr['one_way_collision_margin'],owner_margin=no['one_way_margin'],owner_transform=no['transform'],cached_transform_before_enter_tree=no['cached_transform_before_enter_tree'],transform=geometry.transform(n)))
 require(native_parts==197,'House native primitive-part closure changed')
 return dict(schema=1,kind='encore.field-geometry.source-ir',commit=PIN,scene=SCENE,scene_id=stable('house-geometry:'+SCENE),source_sha256=sources[SCENE],scene_admitted=False,sources=sources,native_sha256=sha(NATIVE),source_receipt_sha256=sha(SOURCE),producer_sha256=sha(Path(__file__)),geometry_producer_sha256=sha(ROOT/'tools/field_geometry.py'),source_attachments=attachments,script_null_overrides=nulls,script_binding_count=len(bindings),tile_collision=tile_collision_proof(d,resources),native_parts=native_parts,nodes=nodes,owners=owners,shapes=shape_rows,geometry=primitives)

def review(d):
 return dict(schema=1,commit=PIN,ir_sha256=sha(IR),sources=d['sources'],native_sha256=d['native_sha256'],source_receipt_sha256=d['source_receipt_sha256'],producer_sha256=d['producer_sha256'],geometry_producer_sha256=d['geometry_producer_sha256'],nodes=len(d['nodes']),owners=len(d['owners']),shapes=len(d['shapes']),native_parts=d['native_parts'],scene_admitted=False,semantics=['Complete original non-TileMap collision ancestry; actual native convex decomposition retained','All source files and original script/embedded declarations checked; attachments grant no script Ready','Placed native TileMap cells have no collision shapes; absent source tile30 is retained as missing, no geometry synthesized','All hierarchy matrices checked against native export; source scripts and scene lifecycle remain consumer-owned'])
def extract():
 d=derive();write(IR,d);write(REVIEW,review(d));return d
def load():
 d=read(IR);require(d==derive() and read(REVIEW)==review(d),'House geometry source/review stale');return d
def pack(d):return geometry.pack(d,sha(IR))
def stage_files(root):
 d=load();raw=pack(d);relative=Path('data/house.encfieldgeometry');require((Path(root)/relative).read_bytes()==raw,'House geometry staged pack differs');return {relative:raw}
def main():
 p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile','verify']);a=p.parse_args();d=extract()if a.action=='extract'else load()
 if a.action=='extract':return
 raw=pack(d)
 if a.action=='compile':PACK.parent.mkdir(parents=True,exist_ok=True);PACK.write_bytes(raw)
 else:require(PACK.read_bytes()==raw,'House geometry binary stale')
 print('House geometry:',len(d['nodes']),'nodes;',len(d['owners']),'owners;',len(d['shapes']),'shape nodes;',d['native_parts'],'native parts;',len(raw),'bytes; source lifecycle not admitted')
if __name__=='__main__':
 try:main()
 except(ValueError,OSError,KeyError,TypeError,AttributeError,struct.error)as e:sys.exit('HOUSE GEOMETRY ERROR: '+str(e))
