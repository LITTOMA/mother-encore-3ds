#!/usr/bin/env python3
"""Full source-backed Podunk TileMap compiler; scripts remain separate capabilities.

The native scene and supplemental TileSet geometry are extracted by the official
Godot 3.6.2 engine in the quarantined source project. Runtime reads ENCFMAP1,
never this authoring IR. Missing native tile IDs remain explicit cell records.
"""
from __future__ import annotations
import argparse, hashlib, json, math, struct, subprocess, sys, zlib
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
from dataclasses import dataclass
from typing import Callable
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from tools.podunk_scene import PIN, SCENE, decode, stable, require, sha, read, write
MAGIC=b'ENCFMAP1'
FORMATS={1:'2I',2:'B',3:'12I4f',4:'3h4HIHIH',5:'5I2f',6:'10I4f',7:'2f',8:'8I2f64s',9:'6Ii2f',10:'5I',11:'7I4f',12:'2Ii10f',13:'4I4f',14:'7I',15:'2f',16:'4I4f',17:'I',18:'6f'}
NONE=0xffffffff

def write_ir(path,ir):
    # One complete typed record per line remains reviewable, without expanding
    # each of the 173211 source cells into a dozen whitespace-only lines.
    path.parent.mkdir(parents=True,exist_ok=True)
    with path.open('w',encoding='utf8',newline='\n') as output:
        output.write('{\n')
        keys=list(ir)
        for i,key in enumerate(keys):
            output.write('  '+json.dumps(key)+': ')
            value=ir[key]
            if isinstance(value,list):
                output.write('[\n')
                for j,row in enumerate(value):output.write('    '+json.dumps(row,ensure_ascii=False,separators=(',',':'))+(',' if j+1<len(value) else '')+'\n')
                output.write('  ]')
            else:output.write(json.dumps(value,ensure_ascii=False,separators=(',',':')))
            output.write(',' if i+1<len(keys) else '')
            output.write('\n')
        output.write('}\n')

# Supplement only native resource facts not present in the original opaque
# Texture export. Instantiation is outside the tree: no game script callbacks.
EXPORT_SCRIPT=r'''extends SceneTree
var maps=[]
var sort_axis=0
var failed=false
func v(p): return ["%.17f" % p.x,"%.17f" % p.y]
func tr(t): return [v(t.x),v(t.y),v(t.origin)]
func compare_bvh(a,b):
    return a.rect.position[sort_axis]+a.rect.size[sort_axis]*0.5 < b.rect.position[sort_axis]+b.rect.size[sort_axis]*0.5
func bvh(leaves,nodes,order):
    var rect=leaves[0].rect
    for i in range(1,leaves.size()): rect=rect.merge(leaves[i].rect)
    var idx=nodes.size()
    nodes.append({"left":-1,"right":-1,"segment":-1,"bounds":[v(rect.position),v(rect.position+rect.size)]})
    if leaves.size()==1:
        nodes[idx].segment=leaves[0].segment
        order.append(leaves[0].segment)
    else:
        sort_axis=0 if rect.size.x>rect.size.y else 1
        leaves.sort_custom(self,"compare_bvh")
        var mid=leaves.size()/2
        nodes[idx].left=bvh(leaves.slice(0,mid-1),nodes,order)
        nodes[idx].right=bvh(leaves.slice(mid,leaves.size()-1),nodes,order)
    return idx
func geometry(s):
    if s==null: return null
    var points=[]
    if s is ConvexPolygonShape2D:
        for p in s.points: points.append(v(p))
    elif s is ConcavePolygonShape2D:
        for p in s.segments: points.append(v(p))
    else:
        printerr("Unsupported tile shape "+s.get_class())
        failed=true
        return null
    var parts=[]
    if s.has_meta("decomposed"):
        for p in s.get_meta("decomposed"): parts.append(geometry(p))
    var result={"class":s.get_class(),"points":points,"parts":parts}
    if s is ConcavePolygonShape2D:
        var leaves=[]
        for i in range(s.segments.size()/2):
            var rect=Rect2(s.segments[i*2],Vector2()).expand(s.segments[i*2+1])
            leaves.append({"segment":i,"rect":rect})
        var nodes=[]
        var order=[]
        if not leaves.empty(): bvh(leaves,nodes,order)
        result["bvh"]=nodes
        result["leaf_order"]=order
    return result
func texture(t):
    if t==null: return null
    var r={"path":t.resource_path,"size":v(t.get_size()),"class":t.get_class()}
    if t is AnimatedTexture:
        r["fps"]="%.17f" % t.fps
        r["pause"]=t.pause
        r["oneshot"]=t.oneshot
        r["current_frame"]=t.current_frame
        r["frames"]=[]
        for i in range(t.frames):
            r.frames.append({"path":t.get_frame_texture(i).resource_path,"delay":"%.17f" % t.get_frame_delay(i)})
    return r
func visit(n,root):
    if n is TileMap:
        var tiles=[]
        if n.tile_set!=null:
            var ts=n.tile_set
            for id in ts.get_tiles_ids():
                var shapes=[]
                for s in ts.tile_get_shapes(id):
                    shapes.append({"geometry":geometry(s.shape),"transform":tr(s.shape_transform),"autotile":v(s.autotile_coord),"one_way":s.one_way,"margin":"%.17f" % s.one_way_margin})
                var z=[]
                for c in n.get_used_cells():
                    if n.get_cellv(c)==id:
                        var a=n.get_cell_autotile_coord(c.x,c.y)
                        z.append([int(c.x),int(c.y),ts.autotile_get_z_index(id,a)])
                tiles.append({"id":id,"texture":texture(ts.tile_get_texture(id)),"shapes":shapes,"z":z})
        maps.append({"path":str(root.get_path_to(n)),"tiles":tiles})
    for c in n.get_children(): visit(c,root)
func _init():
    var packed=load("res://Maps/podunk/podunk.tscn")
    var root=packed.instance()
    visit(root,root)
    if failed:
        root.free()
        quit(2)
        return
    var f=File.new()
    if f.open("res://field_map_detail.json",File.WRITE)!=OK:
        quit(3)
        return
    f.store_string(JSON.print({"schema":1,"scene":"Maps/podunk/podunk.tscn","engine":Engine.get_version_info(),"maps":maps}))
    f.close()
    root.free()
    quit(0)
'''

def f(v): return struct.unpack('<f',struct.pack('<f',float(v)))[0]
def add(a,b): return [f(a[i]+b[i]) for i in range(2)]
def basis(t,p): return [f(f(t[0][i]*p[0])+f(t[1][i]*p[1])) for i in range(2)]
def xf(t,p): return add(basis(t,p),t[2])
def dictionary(v):
    v=decode(v)
    if isinstance(v,dict) and v.get('type')=='Dictionary': return {k:dictionary(x) for k,x in v['pairs']}
    if isinstance(v,list): return [dictionary(x) for x in v]
    return v

@dataclass(frozen=True)
class MapScene:
    scene:str
    scene_id:int
    map_count:int
    cell_count:int
    node_id:Callable[[str],int]
    scene_ir:Path
    native_key:str='export_sha256'
    script_key:str='pending'

PODUNK=MapScene(SCENE,stable('.'),44,173211,stable,ROOT/'content/podunk-scene.json')

def extract(native,detail,receipt,upstream,scene=PODUNK):
    d,extra,src=read(native),read(detail),read(receipt)
    require(d['source']=='res://'+scene.scene and not d['native_compatible'],'Expected complete unapproved map export')
    require(extra['schema']==1 and extra['scene']==scene.scene and [extra['engine'][k] for k in ('major','minor','patch')]==[3,6,2],'Unreviewed supplemental engine/source')
    require(src['commit']==PIN and src['scene']==scene.scene,'Wrong map source pin')
    inv=read(ROOT/'compatibility/upstream-inventory.json')['files']
    sources={}
    for name,r in src['files'].items():
        require(sha(upstream/name)==r['sha256']==inv[name]['sha256'],'Changed source '+name)
        sources[name]=r['sha256']
    nodes=d['nodes'];by={n['path']:n for n in nodes};resources={r['id']:r for r in d['resources']}
    tilemaps=[n for n in nodes if n['class']=='TileMap'];details={m['path']:m for m in extra['maps']}
    require(len(tilemaps)==scene.map_count and sum(len(m['cells']) for m in tilemaps)==scene.cell_count and set(details)=={m['path'] for m in tilemaps},'Full map coverage mismatch')
    # Script bindings and inner/outer instance overrides are already preserved
    # in the checked lifecycle IR. Reuse only source hashes and typed flag data.
    scene_ir=read(scene.scene_ir)
    require(scene_ir[scene.native_key]==sha(native) and scene_ir['source_sha256']==sources[scene.scene] and scene_ir['scene_id']==scene.scene_id,'Lifecycle/map source differs')
    flag_script='Scripts/Main/Flag Landmarks.gd'
    states={s['source'][6:]:s for s in d['scene_states']};roots=[]
    def visit(root,file):
        roots.append((root,file))
        for dec in states[file]['nodes']:
            if dec['instance'] is not None:
                local=dec['path'].removeprefix('./');visit(root if local=='.' else local if root=='.' else root+'/'+local,resources[dec['instance']['id']]['path'][6:])
    visit('.',scene.scene);overrides={}
    for root,file in sorted(roots,key=lambda r:r[0].count('/') if r[0]!='.' else -1,reverse=True):
        for dec in states[file]['nodes']:
            local=dec['path'].removeprefix('./');path=root if local=='.' else local if root=='.' else root+'/'+local
            overrides.setdefault(path,{}).update(dictionary(dec['properties']))
    gates=[];gateby={}
    for pending in scene_ir[scene.script_key]:
        if pending['script']==flag_script:
            path=pending['node'];p=overrides[path];gateby[path]=len(gates)
            gates.append(dict(stable_id=scene.node_id(path),node=path,appear=p.get('appear_flag',''),disappear=p.get('disappear_flag',''),delete_if_hidden=p.get('delete_if_hidden',True)))
    canvas_paths=set()
    for m in tilemaps:
        p=m['path']
        while True:
            canvas_paths.add(p)
            if p=='.':break
            p=p.rsplit('/',1)[0] if '/' in p else '.'
    canvases=[];canvasby={}
    for native_ordinal,n in enumerate(nodes):
        if n['path'] not in canvas_paths:continue
        path=n['path'];p=decode(n['properties']);t=decode(n.get('world_transform',[[1,0],[0,1],[0,0]]));parent=path.rsplit('/',1)[0] if '/' in path else '.'
        require(t[:2]==[[1,0],[0,1]],'Nontranslation canvas requires transform adapter '+path)
        flags=int(p.get('visible',True))|(int(n['class']=='YSort')<<1)|int(p.get('show_behind_parent',False))<<2|int(p.get('cell_y_sort',False))<<3
        canvasby[path]=len(canvases);canvases.append(dict(stable_id=scene.node_id(path),node=path,parent=NONE if path=='.' else canvasby[parent],order=native_ordinal,flags=flags,gate=gateby.get(path,NONE),z=p.get('z_index',0),position=t[2]))
    maps=[];cells=[];draws=[];polys=[];chunks=[];tex=[];texby={};missing={};geometries=[];geometryby={};transforms=[]
    def texture(t):
        if not t:return NONE
        path=t['path'][6:]
        if path in texby:return texby[path]
        first=len(tex);texby[path]=first
        frames=t.get('frames',[dict(path=t['path'],delay='0')])
        require(not t.get('pause',False) and not t.get('oneshot',False) and t.get('current_frame',0)==0,'Unreviewed initial water texture playback')
        for i,frame in enumerate(frames):
            source=frame['path'][6:];require(source in inv and sha(upstream/source)==inv[source]['sha256'],'Unknown texture source '+source);sources[source]=inv[source]['sha256']
            tex.append(dict(stable_id=stable(source),source=source,width=int(float(t['size'][0])),height=int(float(t['size'][1])),group=first,frame=i,frames=len(frames),fps=f(t.get('fps',0)),delay=f(frame['delay']),source_sha256=sources[source]))
        return first
    for mi,n in enumerate(tilemaps):
        p=decode(n['properties']);world=decode(n['world_transform']);path=n['path'];ts=decode(resources[p['tile_set']['id']]['properties']) if p['tile_set'] else {};dt={t['id']:t for t in details[path]['tiles']};zby={(v[0],v[1]):v[2] for t in dt.values() for v in t['z']}
        require(p['mode']==0 and p['cell_half_offset']==2 and p['cell_tile_origin'] in (0,2) and not p['compatibility_mode'] and not p['centered_textures'] and not p['collision_use_parent'] and not p['collision_use_kinematic'] and not p['bake_navigation'],'Unsupported map setting '+path)
        require(p['material'] is None and all(p[k][c]==1 for k in ('modulate','self_modulate') for c in ('r','g','b','a')),'Canvas material/modulate requires adapter '+path)
        firstcell=len(cells);firstdraw=len(draws);firstpoly=len(polys);qsize=1 if p['cell_y_sort'] else p['cell_quadrant_size'];groups={}
        for c in n['cells']:
            c=decode(c);x,y=map(int,c['position']);groups.setdefault((x//qsize,y//qsize),[]).append(c)
        # Native get_used_cells is y,x ordered; quadrants retain first encounter.
        for qi,(q,cs) in enumerate(groups.items()):
            fc,fd,fp=len(cells),len(draws),len(polys);bounds=[]
            for c in cs:
                x,y=map(int,c['position']);tid=c['tile'];idx=len(cells);df,pf=len(draws),len(polys);flags=int(c['flip_x'])|int(c['flip_y'])<<1|int(c['transpose'])<<2
                require(c['local_origin']==[f(x*p['cell_size'][0]),f(y*p['cell_size'][1])],'Native cell origin mismatch')
                tile=dt.get(tid);prefix=str(tid)+'/'
                if tile is None or tile['texture'] is None:
                    flags|=8 if tile is None else 16;missing[path]=missing.get(path,0)+1
                else:
                    mode=ts[prefix+'tile_mode'];require(mode in (0,1,2),'Unknown tile mode')
                    require(ts[prefix+'normal_map'] is None and ts[prefix+'material'] is None and ts[prefix+'occluder'] is None and ts[prefix+'navigation'] is None,'Tile effect requires adapter '+path+'/'+str(tid))
                    region=[list(a) for a in ts[prefix+'region']];size=region[1]
                    if mode:
                        size=ts[prefix+'autotile/tile_size'];spacing=ts[prefix+'autotile/spacing'];region=[add(region[0],[(size[i]+spacing)*c['autotile'][i] for i in range(2)]),size]
                    elif size==[0,0]:size=[f(v) for v in tile['texture']['size']];region=[[0,0],size]
                    ofs=ts[prefix+'tex_offset'][:]
                    if c['transpose']:ofs.reverse()
                    if c['flip_x']:ofs[0]=-ofs[0]
                    if c['flip_y']:ofs[1]=-ofs[1]
                    pos=add(add(world[2],c['local_origin']),ofs);rs=[f(size[i]+f(.00001)) for i in range(2)];signed=[-rs[0] if c['flip_x'] else rs[0],-rs[1] if c['flip_y'] else rs[1]]
                    z=ts[prefix+'z_index']+zby[x,y]
                    texidx=texture(tile['texture']);color=ts[prefix+'modulate'];draws.append(dict(map=mi,cell=idx,texture=texidx,flags=flags&7,order=len(draws),gate=NONE,z=z,rect=pos+signed,region=region[0]+region[1],color=[color[k] for k in ('r','g','b','a')]))
                    actualsize=list(reversed(size)) if c['transpose'] else size
                    bounds.append([pos[0],pos[1],pos[0]+actualsize[0],pos[1]+actualsize[1]])
                    for si,s in enumerate(tile['shapes']):
                        if s['geometry'] is None or (mode and [f(v) for v in s['autotile']]!=c['autotile']):continue
                        require(not s['one_way'],'One way map collision requires adapter')
                        st=[[f(v) for v in a] for a in s['transform']];origin=st[2][:];dims=size[:];bt=[[1,0],[0,1]]
                        if c['transpose']:origin.reverse();dims.reverse();bt=[[0,1],[1,0]]
                        if c['flip_x']:
                            bt[0][0]=-bt[0][0];bt[1][0]=-bt[1][0];origin[0]=f(dims[0]-origin[0])
                        if c['flip_y']:
                            bt[0][1]=-bt[0][1];bt[1][1]=-bt[1][1];origin[1]=f(dims[1]-origin[1])
                        origin=add(pos,origin)
                        for part,g in enumerate(s['geometry']['parts'] or [s['geometry']]):
                            points=[add(basis(bt,basis(st,[f(v) for v in point])),origin) for point in g['points']]
                            kind=1 if g['class']=='ConvexPolygonShape2D' else 2
                            require((kind==1 and len(points)>=3) or (kind==2 and len(points)>0 and len(points)%2==0),'Invalid native tile geometry')
                            if kind==1 and not any((points[j-1][0]-points[j-2][0])*(points[j][1]-points[j-1][1])-(points[j-1][1]-points[j-2][1])*(points[j][0]-points[j-1][0]) for j in range(len(points))):kind=3
                            local=dict(kind=kind,points=[[f(v) for v in a] for a in g['points']],bvh=g.get('bvh',[]),leaf_order=g.get('leaf_order',[]))
                            key=json.dumps(local,sort_keys=True,separators=(',',':'))
                            if key not in geometryby:geometryby[key]=len(geometries);geometries.append(local)
                            gi=geometryby[key];ti=len(transforms);transforms.append([basis(bt,st[0]),basis(bt,st[1]),origin])
                            b=[min(v[0] for v in points),min(v[1] for v in points),max(v[0] for v in points),max(v[1] for v in points)]
                            polys.append(dict(map=mi,cell=idx,kind=kind,points=points,layer=p['collision_layer'],order=len(polys),gate=NONE,geometry=gi,transform=ti,bounds=b,shape=si,part=part,margin=f(s['margin'])))
                            bounds.append(b)
                cells.append([x,y,tid,int(c['autotile'][0]),int(c['autotile'][1]),qi,flags,df,len(draws)-df,pf,len(polys)-pf])
            b=[min(v[0] for v in bounds),min(v[1] for v in bounds),max(v[2] for v in bounds),max(v[3] for v in bounds)] if bounds else [0,0,0,0]
            chunks.append([mi,fc,len(cells)-fc,fd,len(draws)-fd,fp,len(polys)-fp,*b])
        maps.append(dict(stable_id=scene.node_id(path),node=path,canvas=canvasby[path],cell_first=firstcell,cell_count=len(cells)-firstcell,draw_first=firstdraw,draw_count=len(draws)-firstdraw,poly_first=firstpoly,poly_count=len(polys)-firstpoly,flags=int(p['cell_y_sort'])|int(p['cell_tile_origin']==2)<<1,layer=p['collision_layer'],mask=p['collision_mask'],position=world[2],cell_size=p['cell_size']))
    return dict(schema=1,kind='encore.field-map.source-ir',commit=PIN,scene=scene.scene,scene_id=scene.scene_id,source_sha256=sources[scene.scene],sources=sources,native_sha256=sha(native),detail_sha256=sha(detail),scene_admitted=False,capability='full-static-tilemaps',counts=dict(tilemaps=len(maps),cells=len(cells),draws=len(draws),polygons=len(polys),segments=sum(p['kind']==2 for p in polys),degenerate_native_convex=sum(p['kind']==3 for p in polys),native_skipped_cells=sum(missing.values())),native_skips=missing,maps=maps,cells=cells,draws=draws,polygons=polys,textures=tex,canvases=canvases,gates=gates,chunks=chunks,local_geometries=geometries,shape_transforms=transforms)

def prepare_textures(ir,upstream,work,output_prefix='graphics/world/podunk',manifest_path=None):
    from PIL import Image
    work.mkdir(parents=True,exist_ok=True);pages=[];groups={}
    for i,t in enumerate(ir['textures']):
        image=Image.open(upstream/t['source']).convert('RGBA');require(image.size==(t['width'],t['height']),'Native/source PNG size differs')
        groups.setdefault(t['group'],len(pages))
        for y in range(0,image.height,1024):
            for x in range(0,image.width,1024):
                w,h=min(1024,image.width-x),min(1024,image.height-y);name='map-'+str(i)+'-'+str(x)+'-'+str(y)
                image.crop((x,y,x+w,y+h)).save(work/(name+'.png'))
                pages.append(dict(source_index=i,source=t['source'],source_sha256=t['source_sha256'],crop=[x,y,w,h],width=w,height=h,group=groups[t['group']],frame=t['frame'],frames=t['frames'],fps=t['fps'],delay=t['delay'],png=name+'.png',output=output_prefix+'/'+name+'.t3x'))
    require(all(t['frames']==1 or len([p for p in pages if p['source_index']==i])==1 for i,t in enumerate(ir['textures'])),'Multipage AnimatedTexture requires adapter')
    write(ROOT/'content/podunk-scene-map-assets.json'if manifest_path is None else manifest_path,dict(schema=1,kind='encore.field-map.assets',textures=pages))

def pack(ir,assets,ir_sha256=None):
    require(ir_sha256 is None or len(ir_sha256)==64,'Map explicit source IR fingerprint extent differs')
    rows={k:[] for k in FORMATS};strings={};blob=bytearray()
    def s(v):
        if v not in strings:
            raw=v.encode();strings[v]=len(rows[1]);rows[1].append([len(blob),len(raw)]);blob.extend(raw+b'\0')
        return strings[v]
    scene_string=s(ir['scene'])
    prototypes={}
    for ci,original in enumerate(ir['cells']):
        c=original[:];c[7]=len(rows[5])
        for d in ir['draws'][original[7]:original[7]+original[8]]:
            u,v,w,h=d['region'];x,y,rw,rh=d['rect']
            for ti,a in enumerate(assets):
                if a['source_index']!=d['texture']:continue
                px,py,pw,ph=a['crop'];left,top=max(u,px),max(v,py);right,bottom=min(u+w,px+pw),min(v+h,py+ph)
                if right<=left or bottom<=top:continue
                du,dv,dw,dh=left-u,top-v,right-left,bottom-top
                # Native negative rect sizes encode UV flipping, not a move of
                # the world anchor. Split the source page before transpose.
                if d['flags']&1:du=w-du-dw
                if d['flags']&2:dv=h-dv-dh
                if d['flags']&4:du,dv,dw,dh=dv,du,dh,dw
                fullw,fullh=(h,w) if d['flags']&4 else (w,h)
                ax,ay=abs(rh) if d['flags']&4 else abs(rw),abs(rw) if d['flags']&4 else abs(rh)
                rect=[f(x+du*ax/fullw),f(y+dv*ay/fullh),f(dw*ax/fullw),f(dh*ay/fullh)]
                if d['flags']&1:rect[2]=-rect[2]
                if d['flags']&2:rect[3]=-rect[3]
                if d['flags']&4:rect[2],rect[3]=rect[3],rect[2]
                prototype=(ti,d['flags'],d['z'],*rect[2:],left-px,top-py,right-left,bottom-top,*d['color'])
                if prototype not in prototypes:prototypes[prototype]=len(rows[12]);rows[12].append(prototype)
                rows[5].append([d['map'],ci,prototypes[prototype],len(rows[5]),d['gate'],*rect[:2]])
        c[8]=len(rows[5])-c[7];rows[4].append(c)
    for m in ir['maps']:
        first=rows[4][m['cell_first']][7] if m['cell_count'] else 0;last=rows[4][m['cell_first']+m['cell_count']-1] if m['cell_count'] else None;count=last[7]+last[8]-first if last else 0
        rows[3].append([m['stable_id'],s(m['node']),m['canvas'],m['cell_first'],m['cell_count'],first,count,m['poly_first'],m['poly_count'],m['flags'],m['layer'],m['mask'],*m['position'],*m['cell_size']])
    for p in ir['polygons']:
        rows[6].append([p['map'],p['cell'],p['kind'],len(rows[7]),len(p['points']),p['layer'],p['order'],p['gate'],p['geometry'],p['transform'],*p['bounds']]);rows[7].extend(p['points'])
    for a in assets:
        t=ir['textures'][a['source_index']]
        require(a['source']==t['source'] and a['source_sha256']==t['source_sha256'],'Texture/source identity mismatch')
        output=ROOT/'romfs'/a['output'];require(sha(output)==a['output_sha256'],'Texture output differs')
        rows[8].append([stable(a['output']),s(t['source']),s(a['output']),a['width'],a['height'],a['group'],a['frame'],a['frames'],t['fps'],t['delay'],bytes.fromhex(t['source_sha256']+a['output_sha256'])])
    for c in ir['canvases']:rows[9].append([c['stable_id'],s(c['node']),c['parent'],c['order'],c['flags'],c['gate'],c['z'],*c['position']])
    for g in ir['gates']:rows[10].append([g['stable_id'],s(g['node']),s(g['appear']),s(g['disappear']),int(g['delete_if_hidden'])])
    for original in ir['chunks']:
        c=original[:];first=rows[4][c[1]][7];last=rows[4][c[1]+c[2]-1];c[3],c[4]=first,last[7]+last[8]-first
        boxes=[]
        for dr in rows[5][first:first+c[4]]:
            proto=rows[12][dr[2]];w,h=abs(proto[3]),abs(proto[4])
            if proto[1]&4:w,h=h,w
            boxes.append([dr[5],dr[6],f(dr[5]+w),f(dr[6]+h)])
        boxes.extend(pr[10:] for pr in rows[6][c[5]:c[5]+c[6]])
        c[7:]=[min(b[0] for b in boxes),min(b[1] for b in boxes),max(b[2] for b in boxes),max(b[3] for b in boxes)] if boxes else [0,0,0,0]
        rows[11].append(c)
    def spatial(indices):
        boxes=[rows[11][i][7:] for i in indices]
        b=[min(v[0] for v in boxes),min(v[1] for v in boxes),max(v[2] for v in boxes),max(v[3] for v in boxes)]
        idx=len(rows[13]);rows[13].append([NONE,NONE,NONE,0,*b])
        if len(indices)==1:rows[13][idx][2]=indices[0]
        else:
            axis=0 if b[2]-b[0]>=b[3]-b[1] else 1
            indices.sort(key=lambda i:(rows[11][i][7+axis]+rows[11][i][9+axis],i));mid=len(indices)//2
            rows[13][idx][0]=spatial(indices[:mid]);rows[13][idx][1]=spatial(indices[mid:])
        return idx
    if rows[11]:spatial(list(range(len(rows[11]))))
    for g in ir['local_geometries']:
        bf=len(rows[16]);lf=len(rows[17]);rows[14].append([g['kind'],len(rows[15]),len(g['points']),bf,len(g['bvh']),lf,len(g['leaf_order'])]);rows[15].extend(g['points'])
        for n in g['bvh']:rows[16].append([NONE if n['left']==-1 else bf+n['left'],NONE if n['right']==-1 else bf+n['right'],NONE if n['segment']==-1 else n['segment'],0,*[f(v) for p in n['bounds'] for v in p]])
        rows[17].extend([[i] for i in g['leaf_order']])
    rows[18]=[[v for p in t for v in p] for t in ir['shape_transforms']]
    rows[2]=[[v] for v in blob]
    payload=bytearray();directory=bytearray();base=128+len(rows)*24
    for k,fmt in FORMATS.items():
        data=b''.join(struct.pack('<'+fmt,*r) for r in rows[k]);directory+=struct.pack('<6I',k,base+len(payload),len(rows[k]),struct.calcsize('<'+fmt),len(data),0);payload+=data
    result=bytearray(128);result[:8]=MAGIC
    struct.pack_into('<8I',result,8,1,128,base+len(payload),len(rows),zlib.crc32(directory+payload),0x454e0019,1,ir['scene_id'])
    result[40:60]=bytes.fromhex(ir['commit']);result[60:92]=bytes.fromhex(ir['source_sha256']);result[92:124]=hashlib.sha256(json.dumps(ir,sort_keys=True,separators=(',',':')).encode()).digest() if ir_sha256 is None else bytes.fromhex(ir_sha256)
    struct.pack_into('<I',result,124,scene_string)
    return bytes(result+directory+payload)

def main():
    p=argparse.ArgumentParser();p.add_argument('command',choices=['export-script','extract','prepare-textures','compile','verify']);p.add_argument('--native',type=Path);p.add_argument('--detail',type=Path);p.add_argument('--source',type=Path);p.add_argument('--upstream',type=Path,default=ROOT/'upstream/MOTHER-Encore');p.add_argument('--script',type=Path);p.add_argument('--work',type=Path,default=ROOT/'build/podunk-map-textures');p.add_argument('--tex3ds',type=Path);a=p.parse_args()
    if a.command=='export-script':a.script.write_text(EXPORT_SCRIPT,encoding='utf8');return
    path=ROOT/'content/podunk-scene-map.json'
    if a.command=='extract':
        ir=extract(a.native,a.detail,a.source,a.upstream);write_ir(path,ir);print(json.dumps(ir['counts']));return
    ir=read(path);inventory=read(ROOT/'compatibility/upstream-inventory.json')['files']
    for name,digest in ir['sources'].items():require(sha(a.upstream/name)==digest==inventory[name]['sha256'],'Map source changed '+name)
    if a.command=='prepare-textures':prepare_textures(ir,a.upstream,a.work);return
    review=read(ROOT/'compatibility/reviews/podunk-scene-map-v0410.json')
    require(review['commit']==PIN and review['scene']==SCENE and review['source_sha256']==ir['source_sha256'] and review['ir_sha256']==sha(path) and review['scene_admitted'] is False,'Map source/IR requires semantic review')
    if a.command=='compile' and a.tex3ds:
        prepare_textures(ir,a.upstream,a.work);assetpath=ROOT/'content/podunk-scene-map-assets.json';doc=read(assetpath)
        def convert(t):
            out=ROOT/'romfs'/t['output'];out.parent.mkdir(parents=True,exist_ok=True)
            subprocess.run([str(a.tex3ds),'-f','rgba8','-z','none','-o',str(out),str(a.work/t['png'])],check=True)
            t['output_sha256']=sha(out)
        with ThreadPoolExecutor(max_workers=4) as pool:list(pool.map(convert,doc['textures']))
        write(assetpath,doc)
    assets=read(ROOT/'content/podunk-scene-map-assets.json')['textures'];data=pack(ir,assets);out=ROOT/'romfs/data/podunk.encfieldmap'
    if a.command=='verify':require(out.read_bytes()==data,'Map binary regeneration differs')
    else:out.write_bytes(data)
    print('Field map:',len(data),'bytes;',ir['counts'])
if __name__=='__main__':main()
