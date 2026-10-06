#!/usr/bin/env python3
"""Compile source-reviewed field lifecycle data, without approving pending scripts.

The full native scene export is produced by the existing Godot 3.6.2 exporter
in an isolated project. It stays outside Git. Only typed adapter records enter
the runtime pack; no runtime JSON or script evaluator is used.
"""
from __future__ import annotations
import argparse, hashlib, json, math, re, struct, subprocess, sys, zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PIN = '7d9246600fffe518408f5830d4848635019005a3'
SCENE = 'Maps/podunk/podunk.tscn'
GRASS = 'Scripts/misc/grass spawner.gd'
MAGIC = b'ENCFILD1'
HEADER = 128
DIR = 24
FORMATS = {1:'2I', 2:'B', 3:'5I4H7f2d2f2I6f4H', 4:'8I6f', 5:'3I4HI64s', 6:'6I32s'}

def require(value, text):
    if not value: raise ValueError(text)

def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()
def read(path): return json.loads(path.read_text(encoding='utf-8'))
def write(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes((json.dumps(value, ensure_ascii=False, indent=2)+'\n').encode('utf-8'))
def decode(v):
    if isinstance(v, list): return [decode(i) for i in v]
    if not isinstance(v, dict): return v
    kind = v.get('type')
    if kind == 'real':
        n = float(v['value']); require(math.isfinite(n), 'Nonfinite field source real')
        return struct.unpack('<f', struct.pack('<f', n))[0]
    if kind == 'int64': return int(v['value'])
    if kind == 'Vector2': return [decode(v['x']), decode(v['y'])]
    if kind == 'Transform2D': return [decode(v[k]) for k in ('x','y','origin')]
    if kind == 'Rect2': return [decode(v['position']), decode(v['size'])]
    if kind in ('Array','PoolStringArray','PoolVector2Array','PoolIntArray','PoolRealArray'):
        return [decode(i) for i in v['value']]
    return {k:decode(x) for k,x in v.items()}

def stable(path):
    n = int.from_bytes(hashlib.sha256(('field:'+SCENE+'#'+path).encode()).digest()[:4], 'little')
    require(n != 0, 'Zero stable identity'); return n

def godot_hash(name):
    # Godot String::hash is DJB2 over Unicode code points. Checked against the
    # native hash export, never substituted for a source identity checksum.
    h = 5381
    for c in name: h = (h*33+ord(c)) & 0xffffffff
    return h

def extract(native, receipt, hashes, upstream):
    d, source, engine_hashes = read(native), read(receipt), read(hashes)
    require(d['source']=='res://'+SCENE and d['schema']==1 and d['native_compatible'] is False,
            'Field requires complete, unapproved native source data')
    require([d['godot'].get(k) for k in ('major','minor','patch','status')]==[3,6,2,'stable'],
            'Unreviewed native field engine')
    require(source['commit']==PIN and source['scene']==SCENE, 'Unreviewed field source pin')
    inventory = read(ROOT/'compatibility/upstream-inventory.json')['files']
    sources = {}
    for name, r in source['files'].items():
        require(sha(upstream/name)==r['sha256']==inventory[name]['sha256'], 'Changed field source '+name)
        sources[name] = r['sha256']
    for name in ('LICENSE','Nodes/Overworld/Grass/grass.tscn','Scripts/misc/grass.gd',
                 'Graphics/Objects/Grass/Podunk/1.png'):
        require(sha(upstream/name)==inventory[name]['sha256'], 'Changed grass dynamic source '+name)
        sources[name] = inventory[name]['sha256']
    names = {n['path']:n for n in d['nodes']}
    require(len(names)==8686 and sum(len(n.get('cells',[])) for n in names.values())==173211,
            'Incomplete full Podunk scene export')
    require(set(names)==set(engine_hashes), 'Incomplete native name hash export')
    for path,n in names.items():
        require(engine_hashes[path]==godot_hash(n['name']), 'Native name hash differs '+path)
    resources = {r['id']:r for r in d['resources']}
    states = {s['source'][6:]:s for s in d['scene_states']}
    roots = []
    def instances(path, filename):
        roots.append((path,filename))
        for decl in states[filename]['nodes']:
            r = decl['instance']
            if r is not None:
                local = decl['path'].removeprefix('./') if hasattr(str,'removeprefix') else decl['path'][2:]
                target = local if path=='.' else path+'/'+local
                instances(target,resources[r['id']]['path'][6:])
    instances('.', SCENE)
    require(len(set(p for p,_ in roots))==len(roots), 'Duplicate native instance root')
    bindings, overrides = {}, {}
    embedded = {a['script']:a['embedded_script']['sha256'] for a in source['script_attachments'] if '::' in a['script']}
    for root, filename in sorted(roots,key=lambda r:r[0].count('/') if r[0]!='.' else -1,reverse=True):
        for decl in states[filename]['nodes']:
            local = decl['path'][2:] if decl['path'].startswith('./') else decl['path']
            path = root if local=='.' else local if root=='.' else root+'/'+local
            if path in names: overrides.setdefault(path,{}).update(decode(decl['properties']))
        for a in source['script_attachments']:
            if a['source']!=filename: continue
            name = re.search(r'\bname="([^"]+)"',a['node_declaration'])[1]
            parent = re.search(r'\bparent="([^"]+)"',a['node_declaration'])
            local = '.' if parent is None else name if parent[1]=='.' else parent[1]+'/'+name
            path = root if local=='.' else local if root=='.' else root+'/'+local
            require(path in names, 'Unresolved script binding '+path)
            bindings[path] = a['script']
    children = {p:[] for p in names}
    for p in names:
        if p!='.': children[p.rsplit('/',1)[0] if '/' in p else '.'].append(p)
    ready = []
    def visit(p):
        for c in children[p]: visit(c)
        ready.append(p)
    visit('.')
    ordinals = {p:i for i,p in enumerate(ready)}
    grass, pending = [], []
    for path in sorted(bindings,key=lambda p:ordinals[p]):
        script = bindings[path]
        if script != GRASS:
            pending.append(dict(stable_id=stable(path),node=path,script=script,ready_ordinal=ordinals[path],
                                adapter_kind=0,flags=1 if '::' in script else 0,
                                source_sha256=embedded[script] if '::' in script else sources[script]))
            continue
        n = names[path]; props = overrides[path]
        require(props.get('sprite','Podunk')=='Podunk', 'Unreviewed grass sprite group')
        count = props.get('grass_types',1); require(type(count) is int and 1<=count<=2,'Unknown grass texture variants')
        t = decode(n['world_transform']); require(t[:2]==[[1,0],[0,1]],'Unsupported grass transform')
        require(decode(n['properties']['position'])==t[2], 'Grass factory local/global position mismatch needs adapter')
        notifier = names[path+'/VisibilityNotifier2D']; np = decode(notifier['properties'])
        nt = decode(notifier['world_transform']); pos,size = np['rect']
        require(nt[0][1]==nt[1][0]==0 and nt[0][0]>0 and nt[1][1]>0,'Unsupported grass notifier transform')
        bounds = [nt[2][0]+pos[0]*nt[0][0],nt[2][1]+pos[1]*nt[1][1],
                  size[0]*nt[0][0],size[1]*nt[1][1]]
        grass.append(dict(stable_id=stable(path),node=path,name=n['name'],ready_ordinal=ordinals[path],
                          seed=engine_hashes[path],profile_index=0,kind=1,flags=0,
                          position=t[2],visibility_rect=bounds,grass_types=count))
    require(len(grass)==len([p for p,s in bindings.items() if s==GRASS]),'Grass binding loss')
    # Exact schema tuning comes from the audited grass node and method source.
    profile = dict(stable_id=stable('grass-profile'),texture_first=0,texture_count=2,
                   collision_layer=2816,collision_mask=256,frames=[0,1,2,3],
                   sprite_offset=[0,-12],collision_offset=[0,7],collision_extents=[9,4],
                   idle_delay=2,enter_tween=.1,exit_tween=.2,squash=.8,blend_divisor=8,flags=0,
                   blend_points=[[0,0],[-1,0],[1,0]],blend_frames=[2,3,1])
    # These values are source facts, with the entire implementation hash
    # admitted below. Runtime contains the execution mechanism only.
    require(sources['Nodes/Overworld/Grass/grass.tscn']==inventory['Nodes/Overworld/Grass/grass.tscn']['sha256'],
            'Grass profile source review changed')
    return dict(schema=1,kind='encore.field.source-ir',commit=PIN,scene=SCENE,scene_id=stable('.'),
                native_compatible=False,source_sha256=sources[SCENE],sources=sources,
                export_sha256=sha(native),name_hash_export_sha256=sha(hashes),
                counts=dict(native_nodes=len(names),tilemaps=44,cells=173211,script_bindings=len(bindings)),
                profiles=[profile],grass=grass,pending=pending)

def textures(upstream, output):
    rows = []
    for i in range(2):
        source = 'Graphics/Objects/Grass/Podunk/'+str(i)+'.png'
        raw = (upstream/source).read_bytes(); require(raw[:8]==b'\x89PNG\r\n\x1a\n','Grass texture must be PNG')
        w,h = struct.unpack('>II',raw[16:24]); require((w,h)==(96,24),'Grass image geometry changed')
        name = 'grass-'+str(i)+'.t3x'
        rows.append(dict(stable_id=stable(source),source=source,output='graphics/world/podunk/'+name,
                         width=w,height=h,columns=4,rows=1,flags=0,source_sha256=sha(upstream/source),
                         output_sha256=sha(output/name)))
    return rows

def export_scene(upstream, work, logs, engine):
    sys.path.insert(0,str(ROOT))
    from tools.scene_reference import prepare
    require(not work.exists(), 'Use a fresh isolated scene work directory')
    version=subprocess.check_output([str(engine),'--version'],text=True).strip()
    require(version=='3.6.2.stable.official.3cd3caab6','Unreviewed scene extraction engine')
    prepare(upstream,work,SCENE);logs.mkdir(parents=True,exist_ok=True)
    def run(stage,args):
        path=logs/(stage+'.log')
        with path.open('wb') as stream:
            r=subprocess.run([str(engine),'--path',str(work),*args],stdout=stream,stderr=subprocess.STDOUT,timeout=120)
        text=path.read_text(encoding='utf-8',errors='replace')
        diagnostics=[line for line in text.splitlines() if line.startswith(('ERROR:','WARNING:','SCRIPT ERROR:'))]
        # Official headless renderer produces this known null-RID cleanup
        # diagnostic on native source teardown. Preserve its count and log;
        # every other diagnostic blocks export. This approves no gameplay.
        cleanup='ERROR: VisualServer attempted to free a NULL RID.'
        require(r.returncode==0 and all(line==cleanup for line in diagnostics),'Scene extraction failed; inspect '+str(path))
        return dict(returncode=r.returncode,log_sha256=sha(path),headless_null_rid_cleanup=diagnostics.count(cleanup))
    steps={}
    steps['import']=run('import',['--editor'])
    require('ENCORE_IMPORT_COMPLETE' in (logs/'import.log').read_text(encoding='utf-8'),'Scene import did not complete')
    native=logs/'podunk-exact.json'
    steps['native']=run('native',['--script','res://scene_data.gd','--encore-exact-reals=true',
                                 '--encore-scene=res://'+SCENE,'--encore-out='+str(native)])
    d=read(native);names=logs/'names-only.json';hashes=logs/'name-hashes.json'
    write(names,{n['path']:n['name'] for n in d['nodes']})
    # Native names are already resolved by Godot. Ask that same engine for its
    # String hash; this small script never instances or enters any game scene.
    script='''extends SceneTree
func _init():
    var f = File.new()
    if f.open(%s, File.READ) != OK:
        quit(1)
        return
    var names = JSON.parse(f.get_as_text()).result
    f.close()
    var hashes = {}
    for path in names: hashes[path] = str(names[path]).hash()
    if f.open(%s, File.WRITE) != OK:
        quit(1)
        return
    f.store_string(JSON.print(hashes))
    f.close()
    quit(0)
'''%(json.dumps(names.as_posix()),json.dumps(hashes.as_posix()))
    (work/'field_hashes.gd').write_text(script,encoding='utf-8')
    steps['hashes']=run('hashes',['--script','res://field_hashes.gd'])
    write(logs/'extraction-receipt.json',dict(schema=1,commit=PIN,engine_version=version,engine_sha256=sha(engine),
          source_receipt_sha256=sha(work/'source.json'),native_sha256=sha(native),hashes_sha256=sha(hashes),
          exporter_sha256=sha(ROOT/'tools/godot_exporter/scene_data.gd'),steps=steps,
          coverage='Complete native data only; original scripts, Ready, signals and factories are not executed',
          native_compatible=False))
    return native,work/'source.json',hashes

def pack(ir, assets):
    require(ir['schema']==1 and ir['commit']==PIN and ir['native_compatible'] is False,'Unreviewed field IR')
    strings, strings_index = [], {}
    def string(s):
        require(isinstance(s,str) and '\0' not in s and len(s.encode())<=1024,'Invalid field string')
        if s not in strings_index: strings_index[s]=len(strings); strings.append(s)
        return strings_index[s]
    rows = {3:[],4:[],5:[],6:[]}
    for p in ir['profiles']:
        rows[3].append(struct.pack('<'+FORMATS[3],p['stable_id'],p['texture_first'],p['texture_count'],
              p['collision_layer'],p['collision_mask'],*p['frames'],*p['sprite_offset'],*p['collision_offset'],
              *p['collision_extents'],p['idle_delay'],p['enter_tween'],p['exit_tween'],p['squash'],
              p['blend_divisor'],p['flags'],0,*[v for point in p['blend_points'] for v in point],*p['blend_frames'],0))
    for g in ir['grass']:
        rows[4].append(struct.pack('<'+FORMATS[4],g['stable_id'],string(g['node']),string(g['name']),
                      g['ready_ordinal'],g['seed'],g['profile_index'],g['grass_types'],g['flags'],
                      *g['position'],*g['visibility_rect']))
    for r in assets:
        rows[5].append(struct.pack('<'+FORMATS[5],r['stable_id'],string(r['source']),string(r['output']),
              r['width'],r['height'],r['columns'],r['rows'],r['flags'],
              bytes.fromhex(r['source_sha256']+r['output_sha256'])))
    for p in ir['pending']:
        rows[6].append(struct.pack('<'+FORMATS[6],p['stable_id'],string(p['node']),string(p['script']),
                  p['ready_ordinal'],p['adapter_kind'],p['flags'],bytes.fromhex(p['source_sha256'])))
    refs, data = [], bytearray()
    for s in strings:
        encoded = s.encode('utf-8'); refs.append(struct.pack('<2I',len(data),len(encoded)));data+=encoded+b'\0'
    sections = [(1,b''.join(refs),len(refs)),(2,bytes(data),len(data))]
    sections += [(k,b''.join(rows[k]),len(rows[k])) for k in (3,4,5,6)]
    out = bytearray(HEADER+len(sections)*DIR)
    for i,(kind,payload,count) in enumerate(sections):
        while len(out)%4: out.append(0)
        offset = len(out);out += payload
        struct.pack_into('<6I',out,HEADER+i*DIR,kind,offset,len(payload),count,struct.calcsize('<'+FORMATS[kind]),0)
    struct.pack_into('<8s10I',out,0,MAGIC,1,HEADER,len(out),zlib.crc32(out[HEADER:]),
                     0x454e0017,1,1,ir['scene_id'],len(sections),DIR)
    out[48:68]=bytes.fromhex(PIN);out[68:100]=bytes.fromhex(ir['source_sha256'])
    require(len(out)<=2*1024*1024,'Field lifecycle pack exceeds defensive limit')
    return bytes(out)

def main():
    p=argparse.ArgumentParser();p.add_argument('action',choices=['export','extract','compile','verify'])
    p.add_argument('--native',type=Path);p.add_argument('--source',type=Path);p.add_argument('--hashes',type=Path)
    p.add_argument('--root',type=Path,default=ROOT/'upstream/MOTHER-Encore')
    p.add_argument('--ir',type=Path,default=ROOT/'content/podunk-scene.json')
    p.add_argument('--assets',type=Path,default=ROOT/'content/podunk-scene-assets.json')
    p.add_argument('--output',type=Path,default=ROOT/'romfs/data/podunk.encfield')
    p.add_argument('--graphics',type=Path,default=ROOT/'romfs/graphics/world/podunk')
    p.add_argument('--tex3ds',type=Path)
    p.add_argument('--work',type=Path);p.add_argument('--logs',type=Path);p.add_argument('--godot',type=Path)
    a=p.parse_args()
    if a.action=='export':
        require(a.work and a.logs and a.godot,'Scene export needs explicit isolated work/logs/official engine')
        a.native,a.source,a.hashes=export_scene(a.root,a.work,a.logs,a.godot)
    if a.action in ('extract','export'):
        require(a.native and a.source and a.hashes,'Extraction requires complete native/source/hash records')
        d=extract(a.native,a.source,a.hashes,a.root);write(a.ir,d)
        print('Podunk source:',len(d['grass']),'grass bindings;',len(d['pending']),'pending adapters')
        return
    ir=read(a.ir)
    review=read(ROOT/'compatibility/reviews/podunk-scene-grass-v0410.json')
    require(review['schema']==1 and review['commit']==PIN and
            review['ir_sha256']==sha(a.ir) and review['scene']==SCENE and
            review['native_compatible'] is False, 'Field IR semantic review missing or stale')
    inv=read(ROOT/'compatibility/upstream-inventory.json')
    require(inv['commit']==PIN,'Changed field inventory commit')
    for name,h in ir['sources'].items():
        require(sha(a.root/name)==h==inv['files'][name]['sha256'],'Changed field source '+name)
    require(review['grass_sources']=={name:ir['sources'][name] for name in review['grass_sources']},
            'Changed grass mechanism semantic review')
    if a.action=='compile' and a.tex3ds:
        a.graphics.mkdir(parents=True,exist_ok=True)
        for i in range(2):
            subprocess.run([str(a.tex3ds),'-f','rgba8','-z','none','-o',str(a.graphics/('grass-'+str(i)+'.t3x')),
                            str(a.root/('Graphics/Objects/Grass/Podunk/'+str(i)+'.png'))],check=True)
    assets=textures(a.root,a.graphics)
    expected=pack(ir,assets)
    if a.action=='compile':
        write(a.assets,dict(schema=1,commit=PIN,resources=assets));a.output.parent.mkdir(parents=True,exist_ok=True)
        a.output.write_bytes(expected)
    else:
        require(read(a.assets)==dict(schema=1,commit=PIN,resources=assets),'Changed field graphics receipt')
        require(a.output.read_bytes()==expected,'Stale field binary resource')
    print('Field binary:',len(expected),'bytes; complete scene activation remains pending')

if __name__=='__main__':
    try: main()
    except (ValueError,KeyError,OSError,subprocess.SubprocessError) as e: sys.exit('PODUNK FIELD ERROR: '+str(e))
