#!/usr/bin/env python3
"""Complete Podunk Sparkles source roster; original atlas and native clock."""
from __future__ import annotations
import argparse,hashlib,posixpath,re,struct,sys,zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import PIN,SCENE,read,write,decode,stable,sha,require
from tools.present_sparkles import ENGINE,build as build_prototype
SCRIPT='Scripts/misc/sparkles.gd';PROTOTYPE='Nodes/Reusables/Effects/Sparkles.tscn'
IR=ROOT/'content/podunk-sparkles.json';REVIEW=ROOT/'compatibility/reviews/podunk-sparkles-v0410.json';OUT=ROOT/'romfs/data/podunk.encsparkles'

def extract(native):
 d=read(native);g=read(ROOT/'content/podunk-scene.json');inv=read(ROOT/'compatibility/upstream-inventory.json')['files'];nm={n['path']:n for n in d['nodes']};rs={r['id']:r for r in d['resources']};ss={s['source'][6:]:s for s in d['scene_states']};roots=[];sources=dict(g['sources'])
 require(d['source']=='res://'+SCENE and len(nm)==8686 and sha(native)==g['export_sha256'],'Sparkles requires complete official native export')
 def visit(root,f):
  roots.append((root,f))
  for n in ss[f]['nodes']:
   if n['instance']:
    p=n['path'].removeprefix('./');visit(p if root=='.' else root+'/'+p,rs[n['instance']['id']]['path'][6:])
 visit('.',SCENE);ov={}
 for root,f in sorted(roots,key=lambda x:x[0].count('/') if x[0]!='.' else -1,reverse=True):
  for n in ss[f]['nodes']:
   p=n['path'].removeprefix('./');p=root if p=='.' else p if root=='.' else root+'/'+p
   if p in nm:ov.setdefault(p,{}).update(decode(n['properties']))
 # The audited parser reads original AtlasTexture regions, omitted from the
 # official native export's external codec payload. Never infer them from IDs.
 prototype=build_prototype();a=prototype['animation'];asset=prototype['resource'];records=[];animations=[];bindings={b['node']:b for b in g['pending']}
 for b in g['pending']:
  if b['script']!=SCRIPT:continue
  p=b['node'];n=nm[p];v=decode(n['properties']);o=ov[p];frames=rs[v['frames']['id']]
  require(n['class']=='AnimatedSprite' and frames['path']=='res://'+PROTOTYPE+'::14','Sparkles unreviewed SpriteFrames source')
  raw=decode(frames['properties'])['animations'];require(len(raw)==1,'Sparkles animation roster differs');an=dict(raw[0]['pairs'])
  require(an['name']==a['name'] and an['speed']==a['speed'] and an['loop']==a['loop'] and len(an['frames'])==len(a['frames']),'Sparkles native animation differs')
  # Every ordered native reference is crosschecked to the original subresource
  # chain, including all repeated idle frames and the overshoot clamp target.
  source_text=(ROOT/'upstream/MOTHER-Encore'/PROTOTYPE).read_text(encoding='utf8');ids=[int(x) for x in re.search(r'"frames": \[([^\n]+)\]',source_text)[1].replace('SubResource','').replace('(','').replace(')','').split(',')]
  require([rs[x['id']]['path'] for x in an['frames']]==['res://'+PROTOTYPE+'::'+str(i) for i in ids],'Sparkles atlas frame order differs')
  require(v['material'] is None and not v['use_parent_material'] and v['light_mask']==1 and v['rotation']==0 and v['scale']==[1,1] and not v['flip_h'] and not v['flip_v'] and v['offset']==[0,0] and v['centered'] and v['pause_mode']==0 and not v['show_behind_parent'],'Sparkles unreviewed native rendering/process property')
  color=lambda x:[x[k] for k in ['r','g','b','a']]
  require(color(v['modulate'])==[1,1,1,1] and color(v['self_modulate'])==[1,1,1,1],'Sparkles unreviewed local material/tint')
  flags=sum(int(x)<<i for i,x in enumerate([v['playing'],v['visible'],v['centered'],a['pixel_snap'],v['z_as_relative']]))
  profile=dict(name=v['animation'],speed=an['speed'],loop=an['loop'],frames=a['frames'])
  if profile not in animations:animations.append(profile)
  world=decode(n['world_transform']);require(world[0][1]==0 and world[1][0]==0,'Sparkles static rotated/sheared basis requires admitted GPU backend')
  parent=p.rsplit('/',1)[0];parent_script=bindings.get(parent,{}).get('script','');owner={'Scripts/Main/Present.gd':1,'Scripts/Main/DroppedItem.gd':2}.get(parent_script,0)
  records.append(dict(id=b['stable_id'],parent_id=stable(parent),owner=owner,ready=b['ready_ordinal'],node=p,parent=parent,profile=animations.index(profile),frame=o.get('frame',0),flags=flags,z_index=v['z_index'],pause_mode=v['pause_mode'],process_priority=v['process_priority'],speed_scale=v['speed_scale'],position=v['position'],offset=v['offset'],world=world))
 require(len(records)==22 and animations and [sum(n['owner']==i for n in records) for i in range(3)]==[3,16,3],'Sparkles full source/owner coverage differs');records.sort(key=lambda n:n['ready'])
 # Quarantined native connections are empty by design; inspect actual pinned
 # scene declarations through every inherited instance expansion instead.
 sparkle_nodes={n['node'] for n in records};listeners=[]
 for root,f in roots:
  for decl in re.findall(r'^\[connection [^\n]+\]',(ROOT/'upstream/MOTHER-Encore'/f).read_text(encoding='utf8'),re.M):
   local=re.search(r'\bfrom="([^"]+)"',decl)[1];source=posixpath.normpath(local if root=='.' else root+'/'+local)
   if source in sparkle_nodes:listeners.append(dict(node=source,source=f,declaration=decl))
 require(not listeners,'Sparkles static signal listener needs actual owner signal consumer')
 for p,h in prototype['sources'].items():sources[p]=h
 for p,h in sources.items():require(h==inv[p]['sha256']==sha(ROOT/'upstream/MOTHER-Encore'/p),'Changed Sparkles source '+p)
 write(IR,dict(schema=1,kind='encore.field-sparkles.source-ir',commit=PIN,scene=SCENE,scene_id=stable('.'),scene_admitted=False,source_sha256=sources[SCENE],sources=sources,native_sha256=sha(native),engine_reference=ENGINE,random_range=a['random_range'],texture=asset,animations=animations,records=records,source_signal_listeners=listeners,pending=['Only Sparkles.gd is admitted; actual ancestor script/material/visibility/process lifetime remains owned by SceneHost','Full Present/Item/FlagLandmark consumers must invoke real play/stop/visibility setters; no proximity or assumed unopened parent','Source static Sparkles listener roster is empty; borrowed parent clock admits no dynamic frame/finish listeners, Host must check live signal bus','Formal Podunk activation still rejects every uncovered Ready mechanism']))

def load():
 d=read(IR);r=read(REVIEW);inv=read(ROOT/'compatibility/upstream-inventory.json')['files'];require(d['schema']==1 and d['commit']==PIN and d['scene']==SCENE and d['engine_reference']==ENGINE and not d['scene_admitted'] and r['ir_sha256']==sha(IR) and r['commit']==PIN,'Sparkles semantic review absent/stale')
 for p,h in d['sources'].items():require(h==inv[p]['sha256']==sha(ROOT/'upstream/MOTHER-Encore'/p),'Changed Sparkles source '+p)
 prototype=build_prototype();require(d['texture']==prototype['resource'] and d['random_range']==prototype['animation']['random_range'],'Sparkles source atlas/range differs')
 # Reuse the genuine immutable conversion proof, not the old ignored House
 # review report. This slice's own checked semantic review covers the atlas.
 source_ir=ROOT/'content/present-sparkles.json';receipt=read(ROOT/'content/asset-receipts/graphics/story/present-sparkles/source.json');require(read(source_ir)==prototype and receipt['schema']==1 and receipt['commit']==PIN and receipt['ir_sha256']==sha(source_ir) and receipt['producer_sha256']==sha(ROOT/'tools/present_sparkles.py') and re.fullmatch('[0-9a-f]{64}',receipt['tex3ds_sha256'] or '') and set(receipt['outputs'])=={d['texture']['output']},'Sparkles genuine conversion lineage differs')
 t=d['texture'];o=receipt['outputs'][t['output']];p=ROOT/'romfs'/t['output'];require(o['bytes']>0 and p.stat().st_size==o['bytes'] and sha(p)==o['sha256'] and o['size']==t['size'],'Sparkles genuine atlas bytes/extent differ');return d,receipt

def pack(d,receipt):
 out=bytearray(128)
 def u(*v):out.extend(struct.pack('<'+'I'*len(v),*v))
 def f(*v):out.extend(struct.pack('<'+'f'*len(v),*v))
 def s(v):b=v.encode();u(len(b));out.extend(b)
 s(d['scene']);s(SCRIPT);out.extend(bytes.fromhex(ENGINE['sha256']));f(*d['random_range']);t=d['texture'];s(t['source']);s(t['output']);u(*t['size']);out.extend(bytes.fromhex(t['source_sha256']+receipt['outputs'][t['output']]['sha256']));u(len(d['animations']))
 for a in d['animations']:
  s(a['name']);f(a['speed']);u(int(a['loop']),len(a['frames']))
  for rect in a['frames']:u(*rect)
 for n in d['records']:
  u(n['id'],n['parent_id'],n['owner'],n['ready'],n['profile'],n['frame'],n['flags']);out.extend(struct.pack('<i',n['z_index']));u(n['pause_mode']);out.extend(struct.pack('<i',n['process_priority']));f(n['speed_scale'],*n['position'],*n['offset'],*[v for row in n['world'] for v in row]);s(n['node']);s(n['parent'])
 u(len(d['sources']))
 for p,h in d['sources'].items():s(p);out.extend(bytes.fromhex(h))
 struct.pack_into('<8s8I',out,0,b'ENCFSPL1',1,128,len(out),0,0x454e0021,3,len(d['records']),d['scene_id']);out[40:60]=bytes.fromhex(PIN);out[60:92]=bytes.fromhex(d['source_sha256']);out[92:124]=hashlib.sha256(IR.read_bytes()).digest();struct.pack_into('<I',out,20,zlib.crc32(out[128:]));return bytes(out)

def stage_files(source):
 d,r=load();raw=pack(d,r);source=Path(source);require((source/'data/podunk.encsparkles').read_bytes()==raw,'Stale Sparkles pack');path=d['texture']['output'];p=source/path;expected=r['outputs'][path];require(p.stat().st_size==expected['bytes'] and sha(p)==expected['sha256'],'Sparkles staged genuine atlas differs');return {Path('data/podunk.encsparkles'):raw,Path(path):p.read_bytes()}

def main():
 p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile','verify']);p.add_argument('--native',type=Path);a=p.parse_args()
 if a.action=='extract':require(a.native,'Explicit complete native export required');extract(a.native);return
 d,r=load();raw=pack(d,r)
 if a.action=='verify':require(OUT.read_bytes()==raw,'Sparkles compiled pack differs')
 else:OUT.parent.mkdir(parents=True,exist_ok=True);OUT.write_bytes(raw)
 print('Sparkles complete source:',len(d['records']),'Ready;',len(d['animations']),'native profiles;',len(raw),'bytes; scene admitted=False')
if __name__=='__main__':
 try:main()
 except(ValueError,KeyError,TypeError,OSError,struct.error)as e:sys.exit('FIELD SPARKLES ERROR: '+str(e))
