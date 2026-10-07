#!/usr/bin/env python3
"""Pinned complete non-TileMap collision hierarchy -> independent ENCFGEO1.

Authoring JSON is never a runtime input. Source scripts remain pending; geometry
admission grants no Ready, physics callback, door, audio, or scene capability.
"""
from __future__ import annotations
import argparse, hashlib, math, struct, sys, zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from tools.podunk_scene import PIN,SCENE,decode,stable,require,sha,read,write
IR=ROOT/'content/podunk-scene-geometry.json'
REVIEW=ROOT/'compatibility/reviews/podunk-scene-geometry-v0410.json'
OUTPUT=ROOT/'romfs/data/podunk.encfieldgeometry'
NONE=0xffffffff
FORMATS={1:'2I',2:'B',3:'8I32s12f',4:'10I11f',5:'8I20f',6:'5I6f',7:'2f'}
KINDS={'RectangleShape2D':1,'CircleShape2D':2,'CapsuleShape2D':3,'ConvexPolygonShape2D':4,'ConcavePolygonShape2D':5,'SegmentShape2D':6,'RayShape2D':7,'LineShape2D':8}
EXPORT_SCRIPT=r'''extends SceneTree
var nodes=[]
func v(p): return ["%.17f" % p.x,"%.17f" % p.y]
func tr(t): return [v(t.x),v(t.y),v(t.origin)]
func visit(n,root):
    if n is CanvasItem: nodes.append({"path":str(root.get_path_to(n)),"local":tr(n.get_transform()),"world":tr(n.get_global_transform())})
    for c in n.get_children(): visit(c,root)
func _init():
    var root=load("res://Maps/podunk/podunk.tscn").instance()
    visit(root,root)
    var f=File.new()
    if f.open("res://field_geometry_detail.json",File.WRITE)!=OK:
        root.free()
        quit(2)
        return
    f.store_string(JSON.print({"schema":1,"scene":"Maps/podunk/podunk.tscn","engine":Engine.get_version_info(),"nodes":nodes}))
    f.close()
    root.free()
    quit(0)
'''
def f(n):
    v=struct.unpack('<f',struct.pack('<f',float(n)))[0]
    require(math.isfinite(v),'Nonfinite geometry value');return v
def vec(v):return [f(x) for x in v]
def transform(n):return [vec(v) for v in decode(n.get('world_transform',{'type':'Transform2D','x':[1,0],'y':[0,1],'origin':[0,0]}))]
def parent(path):return path.rsplit('/',1)[0] if '/' in path else '.'
def flatten(t):return [x for v in t for x in v]
def extract(native,source,detail,upstream):
    d,s,t=read(native),read(source),read(detail)
    require(d['source']=='res://'+SCENE and d['schema']==1 and d['native_compatible'] is False,'Unreviewed geometry scene')
    require(s['scene']==t['scene']==SCENE and s['commit']==PIN,'Changed geometry source identity')
    require([d['godot'].get(k) for k in ('major','minor','patch','status')]==[3,6,2,'stable'] and [t['engine'].get(k) for k in ('major','minor','patch','status')]==[3,6,2,'stable'],'Unreviewed geometry engine')
    inv=read(ROOT/'compatibility/upstream-inventory.json')['files'];sources={}
    for path,r in s['files'].items():
        require(sha(upstream/path)==r['sha256']==inv[path]['sha256'],'Changed geometry source '+path);sources[path]=r['sha256']
    nm={n['path']:n for n in d['nodes']};resources={r['id']:r for r in d['resources']}
    require(len(nm)==8686,'Incomplete full geometry export')
    grass=read(ROOT/'content/podunk-scene.json')
    require(grass['commit']==PIN and grass['export_sha256']==sha(native) and grass['source_sha256']==sources[SCENE],'Geometry lifecycle identity mismatch')
    bindings={b['node']:b for b in grass['pending']}
    for b in grass['grass']:bindings[b['node']]={'script':'Scripts/misc/grass spawner.gd','source_sha256':sources['Scripts/misc/grass spawner.gd'],'ready_ordinal':b['ready_ordinal']}
    from tools.field_script_bindings import actual_scripts
    actual,nulls,_=actual_scripts(native,source)
    require(len(nulls)==1 and len(actual)==2156,'Actual geometry script assignment coverage differs')
    bindings={p:dict(script=v[0],source_sha256=v[1]) for p,v in actual.items()}
    # Original explicit-null overrides are applied before physics admission.
    children={p:[] for p in nm}
    for p in nm:
        if p!='.':children[parent(p)].append(p)
    ready=[]
    def visit(p):
        for child in children[p]:visit(child)
        ready.append(p)
    visit('.');ordinals={p:i for i,p in enumerate(ready)};orders={p:i for i,p in enumerate(nm)}
    bodies=[n for n in d['nodes'] if n['class'] in ('Area2D','StaticBody2D','KinematicBody2D','RigidBody2D')]
    shapes=[n for n in d['nodes'] if n['class'] in ('CollisionShape2D','CollisionPolygon2D')]
    require(len(bodies)==902 and len(shapes)==941,'Full geometry owner/shape count differs')
    required={'.'}
    for n in bodies+shapes:
        p=n['path']
        while p!='.':required.add(p);p=parent(p)
    paths=[p for p in nm if p in required];indices={p:i for i,p in enumerate(paths)}
    nodes=[];detail_nodes={r['path']:r for r in t['nodes']}
    for p in paths:
        n=nm[p];pr=decode(n['properties']);b=bindings.get(p)
        require(p=='.' or parent(p) in indices,'Geometry ancestor loss')
        require(p in detail_nodes or 'world_transform' not in n,'Missing native Node2D transform '+p)
        local=[vec(v) for v in detail_nodes[p]['local']] if p in detail_nodes else [[1,0],[0,1],[0,0]]
        world=[vec(v) for v in detail_nodes[p]['world']] if p in detail_nodes else transform(n)
        if 'world_transform' in n:require(world==transform(n),'Native hierarchy transform export differs '+p)
        nodes.append(dict(stable_id=stable(p),path=p,parent=NONE if p=='.' else indices[parent(p)],order=orders[p],ready=ordinals[p],flags=int(pr.get('visible',True))|(int(pr.get('pause_mode',0))<<1),class_name=n['class'],script=b['script'] if b else '',script_sha256=b['source_sha256'] if b else '00'*32,local_transform=local,world_transform=world))
    owners=[];owner_indices={n['path']:i for i,n in enumerate(bodies)};ordered_shapes=[]
    for n in bodies:
        pr=decode(n['properties']);kind=('StaticBody2D','KinematicBody2D','RigidBody2D','Area2D').index(n['class'])+1
        attached=[x for x in shapes if parent(x['path'])==n['path']]
        require(all(parent(x['path']) in owner_indices for x in shapes),'Orphan geometry shape')
        flags=int(pr.get('input_pickable',False))|(int(pr.get('monitoring',False))<<1)|(int(pr.get('monitorable',False))<<2)|(int(pr.get('audio_bus_override',False))<<3)|(int(pr.get('gravity_point',False))<<4)|(int(pr.get('motion/sync_to_physics',False))<<5)
        owners.append(dict(node=indices[n['path']],kind=kind,layer=pr['collision_layer'],mask=pr['collision_mask'],flags=flags,shape_first=len(ordered_shapes),shape_count=len(attached),audio_bus=pr.get('audio_bus_name',''),safe_margin=pr.get('collision/safe_margin',0),space_override=pr.get('space_override',0),priority=pr.get('priority',0),gravity=pr.get('gravity',0),gravity_distance_scale=pr.get('gravity_distance_scale',0),gravity_vec=pr.get('gravity_vec',[0,0]),linear_damp=pr.get('linear_damp',0),angular_damp=pr.get('angular_damp',0),constant_linear_velocity=pr.get('constant_linear_velocity',[0,0]),constant_angular_velocity=pr.get('constant_angular_velocity',0),moving_platform_leave=pr.get('moving_platform_apply_velocity_on_leave',0)))
        ordered_shapes.extend(attached)
    # The original complete official-engine export already contains the actual
    # PhysicsServer shape-owner decomposition after its source-safe tree entry.
    # Prefer these real native shapes over recalculating a polygon decomposition.
    native_owners={o['owner']['path']:o for n in bodies for o in n.get('physics_shape_owners',[])}
    require(set(native_owners)=={n['path'] for n in shapes},'Native shape owner coverage mismatch')
    geometry=[];shape_rows=[]
    def addgeom(kind,points,parameters,resource):
        geometry.append(dict(kind=kind,points=points,parameters=parameters,resource=resource));return len(geometry)-1
    for n in ordered_shapes:
        pr=decode(n['properties']);parts=[];original=NONE;no=decode(native_owners[n['path']])
        require(no['disabled']==pr['disabled'] and no['one_way']==pr['one_way_collision'],'Native shape owner state differs')
        if n['class']=='CollisionShape2D':
            ref=pr['shape']
            if ref is None:kind=0;original=addgeom(0,[],[0]*6,'')
            else:
                r=resources[ref['id']];require(r['class'] in KINDS,'Unsupported source geometry '+r['class']);kind=KINDS[r['class']];rp=decode(r['properties']);params=[0]*6;points=[]
                if kind==1:params[:2]=rp['extents']
                elif kind==2:params[0]=rp['radius']
                elif kind==3:params[:2]=[rp['radius'],rp['height']]
                elif kind==4:points=rp['points']
                elif kind==5:points=rp['segments']
                elif kind==6:points=[rp['a'],rp['b']]
                elif kind==7:params[:2]=[rp['length'],int(rp['slips_on_slope'])]
                elif kind==8:params[:3]=rp['normal']+[rp['d']]
                params[5]=rp.get('custom_solver_bias',0);original=addgeom(kind,points,params,r['path']);parts=[original]
        else:
            mode=pr['build_mode'];require(mode in (0,1),'Unknown collision polygon build mode');kind=9 if mode==0 else 10
            original=addgeom(kind,pr['polygon'],[0]*6,'')
            if mode==0:
                for ref in no['shapes']:
                    native_shape=resources[ref['id']];require(native_shape['class']=='ConvexPolygonShape2D','Unexpected native polygon part')
                    parts.append(addgeom(4,decode(native_shape['properties']['points']),[0]*6,native_shape['path']))
                require(bool(parts) or len(pr['polygon'])<3,'Native convex decomposition failed')
            else:
                pp=pr['polygon'];segments=[]
                for j in range(len(pp)):segments.extend([pp[j],pp[(j+1)%len(pp)]])
                if segments:parts=[addgeom(5,segments,[0]*6,'')]
        require(parts==list(range(parts[0],parts[0]+len(parts))) if parts else True,'Noncontiguous geometry parts')
        shape_rows.append(dict(node=indices[n['path']],owner=owner_indices[parent(n['path'])],kind=kind,flags=int(pr['disabled'])|(int(pr['one_way_collision'])<<1),geometry=original,part_first=parts[0] if parts else NONE,part_count=len(parts),margin=pr['one_way_collision_margin'],owner_margin=no['one_way_margin'],owner_transform=no['transform'],cached_transform_before_enter_tree=no['cached_transform_before_enter_tree'],transform=transform(n)))
    result=dict(schema=1,kind='encore.field-geometry.source-ir',commit=PIN,scene=SCENE,scene_id=stable('.'),source_sha256=sources[SCENE],scene_admitted=False,sources=sources,native_sha256=sha(native),detail_sha256=sha(detail),nodes=nodes,owners=owners,shapes=shape_rows,geometry=geometry)
    write(IR,result);return result
def pack(ir,ir_sha256=None):
    strings=[];lookup={};blob=bytearray()
    def string(v):
        if v not in lookup:lookup[v]=len(strings);raw=v.encode();strings.append((len(blob),len(raw)));blob.extend(raw+b'\0')
        return lookup[v]
    scene_index=string(ir['scene']);rows={i:[] for i in FORMATS};points=[]
    for n in ir['nodes']:rows[3].append((n['stable_id'],string(n['path']),n['parent'],n['order'],n['ready'],n['flags'],string(n['class_name']),string(n['script']),bytes.fromhex(n['script_sha256']),*flatten(n['local_transform']),*flatten(n['world_transform'])))
    for o in ir['owners']:
        rows[4].append((o['node'],o['kind'],o['layer'],o['mask'],o['flags'],o['shape_first'],o['shape_count'],string(o['audio_bus']),o['space_override'],o['moving_platform_leave'],o['safe_margin'],o['priority'],o['gravity'],o['gravity_distance_scale'],*o['gravity_vec'],o['linear_damp'],o['angular_damp'],*o['constant_linear_velocity'],o['constant_angular_velocity']))
    for s in ir['shapes']:rows[5].append((s['node'],s['owner'],s['kind'],s['flags'],s['geometry'],s['part_first'],s['part_count'],0,s['margin'],s['owner_margin'],*flatten(s['transform']),*flatten(s['owner_transform']),*flatten(s['cached_transform_before_enter_tree'])))
    for g in ir['geometry']:
        first=len(points);points.extend(g['points']);rows[6].append((g['kind'],first,len(g['points']),string(g['resource']),0,*g['parameters']))
    rows[7]=points;rows[1]=strings;rows[2]=[(b,) for b in blob]
    result=bytearray(128+24*len(FORMATS));directory=[]
    for k,fmt in FORMATS.items():
        payload=b''.join(struct.pack('<'+fmt,*row) for row in rows[k]);directory.append((k,len(rows[k]),struct.calcsize('<'+fmt),len(result),len(payload),0));result.extend(payload)
    struct.pack_into('<8s8I',result,0,b'ENCFGEO1',1,128,len(result),len(FORMATS),zlib.crc32(result[128+24*len(FORMATS):]),0x454e001b,1,ir['scene_id'])
    proof=sha(IR)if ir_sha256 is None else ir_sha256
    require(isinstance(proof,str)and len(proof)==64 and len(bytes.fromhex(proof))==32,'Geometry explicit IR proof rejected')
    result[40:60]=bytes.fromhex(PIN);result[60:92]=bytes.fromhex(ir['source_sha256']);result[92:124]=bytes.fromhex(proof);struct.pack_into('<I',result,124,scene_index)
    for i,row in enumerate(directory):struct.pack_into('<6I',result,128+i*24,*row)
    return bytes(result)
def compile_resource(verify=False):
    ir=read(IR);review=read(REVIEW)
    require(ir['schema']==1 and ir['commit']==PIN and ir['scene']==SCENE and ir['scene_admitted'] is False,'Geometry IR identity rejected')
    require(review['ir_sha256']==sha(IR) and review['source_sha256']==ir['source_sha256'] and review['commit']==PIN and review['scene_admitted'] is False,'Geometry source review mismatch')
    inv=read(ROOT/'compatibility/upstream-inventory.json')['files']
    for p,h in ir['sources'].items():require(inv[p]['sha256']==h==sha(ROOT/'upstream/MOTHER-Encore'/p),'Geometry inventory/source mismatch '+p)
    data=pack(ir)
    if verify:require(OUTPUT.read_bytes()==data,'Geometry checked resource differs')
    else:OUTPUT.parent.mkdir(parents=True,exist_ok=True);OUTPUT.write_bytes(data)
    print('Field geometry:',len(data),'bytes;',len(ir['owners']),'owners;',len(ir['shapes']),'source shape nodes; complete scene admitted=False')
def main():
    parser=argparse.ArgumentParser();sub=parser.add_subparsers(dest='mode',required=True)
    e=sub.add_parser('extract');e.add_argument('--native',type=Path,required=True);e.add_argument('--source',type=Path,required=True);e.add_argument('--detail',type=Path,required=True);e.add_argument('--upstream',type=Path,default=ROOT/'upstream/MOTHER-Encore')
    e=sub.add_parser('export-script');e.add_argument('--script',type=Path,required=True)
    sub.add_parser('compile');sub.add_parser('verify');args=parser.parse_args()
    if args.mode=='export-script':args.script.parent.mkdir(parents=True,exist_ok=True);args.script.write_text(EXPORT_SCRIPT,encoding='utf-8')
    elif args.mode=='extract':extract(args.native,args.source,args.detail,args.upstream)
    else:compile_resource(args.mode=='verify')
if __name__=='__main__':main()
