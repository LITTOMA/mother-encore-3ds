#!/usr/bin/env python3
"""Compile reviewed exact native static body geometry; never use rounded JSON."""
from __future__ import annotations
import argparse,hashlib,json,math,struct
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
COMMIT='7d9246600fffe518408f5830d4848635019005a3'
def f32(n):return struct.unpack('<f',struct.pack('<f',n))[0]
def real(v):
    if not isinstance(v,dict) or v.get('type')!='real' or not isinstance(v.get('value'),str):raise ValueError('Exact native decimal real required')
    n=float(v['value'])
    if not math.isfinite(n):raise ValueError('Nonfinite geometry')
    return f32(n)
def vec(v):
    if v.get('type')!='Vector2':raise ValueError('Vector2 required')
    return(real(v['x']),real(v['y']))
def transform(t,p):
    if t.get('type')!='Transform2D':raise ValueError('Transform2D required')
    x,y,o=vec(t['x']),vec(t['y']),vec(t['origin'])
    # Godot real_t is float. Match its multiply/add ordering.
    return tuple(f32(f32(f32(x[i]*p[0])+f32(y[i]*p[1]))+o[i]) for i in (0,1))
def shape_points(r):
    p=r['properties']
    if r['class']=='ConvexPolygonShape2D':return[vec(v) for v in p['points']['value']]
    if r['class']=='RectangleShape2D':
        x,y=vec(p['extents']);return[(-x,-y),(x,-y),(x,y),(-x,y)]
    raise ValueError('Unreviewed collision shape: '+r['class'])
def geometry(house,player):
    for d in (house,player):
        if d.get('schema')!=1:raise ValueError('Unreviewed data schema')
        if d.get('godot',{}).get('string')!='3.6.2-stable (official)':
            # Official engine reports its exact fields below, string varies build suffix.
            v=d.get('godot',{})
            if (v.get('major'),v.get('minor'),v.get('patch'))!=(3,6,2):raise ValueError('Unreviewed engine/data schema')
    root=player['nodes'][0]
    if root['class']!='KinematicBody2D' or root['properties']['collision_mask']['value']!='4353':raise ValueError('Player collision profile changed')
    resources={r['id']:r for r in player['resources']};owners=root['physics_shape_owners']
    if len(owners)!=1 or owners[0]['disabled'] or len(owners[0]['shapes'])!=1:raise ValueError('Player shape profile changed')
    o=owners[0];actor=[transform(o['transform'],p) for p in shape_points(resources[o['shapes'][0]['id']])]
    rs={r['id']:r for r in house['resources']};polygons=[]
    for n in house['nodes']:
        if n['class'] not in ('StaticBody2D','KinematicBody2D'):continue
        layer=int(n['properties']['collision_layer']['value']);mask=int(n['properties']['collision_mask']['value'])
        if not(layer&4353 or mask&1):continue
        for owner in n.get('physics_shape_owners',[]):
            if owner['one_way']:raise ValueError('One-way collision requires adapter')
            for s in owner['shapes']:
                pts=[transform(n['world_transform'],transform(owner['transform'],p)) for p in shape_points(rs[s['id']])]
                if len(pts)<3 or len(pts)>32:raise ValueError('Unsupported convex point count')
                polygons.append({'body':n['path'],'owner':owner['owner']['path'],'disabled':owner['disabled'],'vertices':pts})
    if len(polygons)!=105:raise ValueError('Unreviewed opening body shape count: '+str(len(polygons)))
    return actor,polygons

def main():
    a=argparse.ArgumentParser();a.add_argument('--house',type=Path,default=ROOT/'reports/cloud-world/house-exact.json');a.add_argument('--player',type=Path,default=ROOT/'reports/cloud-world/player-exact.json');a.add_argument('--out',type=Path,default=ROOT/'build/native/geometry.json');args=a.parse_args()
    review=json.loads((ROOT/'compatibility/reviews/opening-world-v0410.json').read_text())
    lock=json.loads((ROOT/'upstream.lock').read_text())
    if review.get('commit')!=lock.get('commit') or review.get('game_version')!=lock.get('game_version'):raise ValueError('World review differs from source lock')
    for path in (args.house,args.player):
        key=path.resolve().relative_to(ROOT).as_posix()
        if review['inputs'].get(key)!=hashlib.sha256(path.read_bytes()).hexdigest():raise ValueError('Unreviewed exact geometry input: '+key)
    actor,polys=geometry(json.loads(args.house.read_text()),json.loads(args.player.read_text()))
    if args.out.suffix != '.json':raise ValueError('Geometry output must be external JSON; C++ content generation is disabled')
    args.out.parent.mkdir(parents=True,exist_ok=True)
    args.out.write_text(json.dumps({'schema':1,'commit':COMMIT,'actor':actor,'polygons':polys},indent=2)+'\n')
    print('Exported',len(polys),'source-backed convex body shapes as external data')
if __name__=='__main__':main()
