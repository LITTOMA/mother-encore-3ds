#!/usr/bin/env python3
"""Exact source-ready/music Area bridge to the existing real MusicRegion service."""
from __future__ import annotations
import argparse,hashlib,json,math,re,struct,sys,zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import decode,stable,SCENE,PIN,read,write,sha,require
from tools.extract_battle_entry import Extractor
from tools import podunk_music as music
SCRIPT='Nodes/Overworld/MusicChanger.tscn::3';AREA='Nodes/Overworld/MusicChanger.tscn'
IR=ROOT/'content/native-field-music-changer.json';REVIEW=ROOT/'reports/field-music-changer/source-review.json';PACK=ROOT/'romfs/data/podunk-music-changers.encmarea'
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
  if bindings[path]!=SCRIPT:continue
  out.append(dict(stable_id=stable(path),node=path,ready_ordinal=ordinal[path],script=bindings[path],overrides=overrides[path],native=names[path],children=[names[p] for p in children[path]]))
 require(len(out)==13,'Podunk MusicChanger binding loss');return out,names,resources,dict(native_export_sha256=sha(native),source_receipt_sha256=sha(receipt),source_files={name:r['sha256'] for name,r in s['files'].items()})


def build(records,nodes,resources,provenance):
 ex=Extractor(ROOT);ex.sources.update(provenance['source_files']);scene=ex.text(AREA);ex.text('Scripts/global/audioManager.gd');ex.text('Scripts/global/globalData.gd');ex.text('Nodes/Reusables/Player.tscn');ex.text('LICENSE')
 source=music.read_source(ROOT/'upstream/MOTHER-Encore');music.checked(source,ROOT/'upstream/MOTHER-Encore');regions={r['source_path']:r for r in source['regions']}
 script=json.loads(re.search(r'^script/source = ("(?:[^"\\]|\\.)*")$',scene,re.M)[1],strict=False)
 defaults={k:json.loads(re.search(r'^export(?: \([^)]*\))? var '+k+r' = (.+)$',script,re.M)[1])for k in ['music','loop','appear_flag','disappear_flag','diegetic','volume_db','fadein_length','fadeout_length','disabled']}
 for fact in ['get_child(0).disabled = disabled','get_child(0).disabled = value','yield(get_tree(), "idle_frame")','if !player_inside:','stop_music(fadeout_length)','body == global.get_player() and !uiManager.is_in_cutscene() and !uiManager.is_in_battle() and _check_flags()','body == global.get_player() and body.has_collisions() and !uiManager.is_in_cutscene()']:
  require(fact in script,'Unreviewed MusicChanger lifecycle '+fact)
 stop_default=float(re.search(r'func stop_music\(fadeoutLength = ([\d.]+)\):',script)[1]);connections=re.findall(r'\[connection signal="([^"]+)" from="\." to="\." method="([^"]+)"\]',scene);require(len(connections)==3,'Unknown MusicChanger signal topology')
 bindings=[]
 def transform(wt,point):return[wt[2][i]+wt[0][i]*point[0]+wt[1][i]*point[1]for i in [0,1]]
 for a in records:
  path=a['node'];root=decode(a['native']['properties']);r=regions[path];values=defaults.copy();values.update({k:v for k,v in a['overrides'].items()if k in defaults});require(values['music']==''and values['diegetic']is False and values['loop']and values['disabled']is False,'Current MusicRegion service supports actual loop-only non-diegetic enabled source scope')
  for k,p in [('volume_db','volume_db'),('fadein_length','fadein_seconds'),('fadeout_length','fadeout_seconds'),('appear_flag','appear_flag'),('disappear_flag','disappear_flag'),('disabled','disabled')]:require(values[k]==r[p],'Music native overrides/service source mismatch')
  track=next(t for t in source['tracks']if t['stable_id']==r['track_id']);require(track['source_path']=='res://Audio/Music/'+values['loop'],'Music source track mismatch');ex.data(track['source_path'][6:]);ex.data(track['source_path'][6:]+'.import')
  require(root['space_override']==0 and not root['audio_bus_override']and root['pause_mode']==0 and root['process_priority']==0,'Music area unsupported source override')
  owners=a['native']['physics_shape_owners'];require(owners and [o['owner']['path']for o in owners]==r['shape_paths'],'Music exact native shape ownership/order mismatch');shape_records=[]
  for order,o in enumerate(owners):
   child=nodes[o['owner']['path']];v=decode(child['properties']);wt=decode(child['world_transform']);require(wt[0]==[1,0]and wt[1]==[0,1],'Music current native geometry needs translation-only source capability');require(child['class']in['CollisionShape2D','CollisionPolygon2D']and not o['one_way'],'Unsupported Music area collision owner')
   parts=[]
   for shape in o['shapes']:
    resource=resources[shape['id']];props=decode(resource['properties']);kind=resource['class']
    if kind=='RectangleShape2D':
     x,y=props['extents'];points=[[-x,-y],[x,-y],[x,y],[-x,y]]
    elif kind=='ConvexPolygonShape2D':points=props['points']
    else:raise ValueError('Unsupported Music collider '+kind)
    parts.append([transform(wt,p)for p in points])
   require(parts,'Empty Music collider');shape_records.append(dict(id=stable(child['path']),node=child['path'],order=order,disabled=o['disabled'],parts=parts))
  bindings.append(dict(id=a['stable_id'],node=path,ready_ordinal=a['ready_ordinal'],region_id=r['id'],track_id=r['track_id'],collision_layer=root['collision_layer'],collision_mask=root['collision_mask'],monitoring=root['monitoring'],monitorable=root['monitorable'],shapes=shape_records))
 return dict(schema=1,kind='encore.field-music-changer.source-ir',commit=PIN,scene=SCENE,script=SCRIPT,stop_default=stop_default,connections=[dict(role=i+1,signal=s,method=m)for i,(s,m)in enumerate(connections)],music_regions=source,bindings=bindings,sources=dict(sorted(ex.sources.items())),provenance=provenance,semantics=['All13 real source MusicChanger roots; all are loop-only, non-diegetic and initially enabled, reuse existing real five-track Podunk MusicRegion service','Fullscene source postorder Ready IDs plus16 native owner children and original engine convex decomposition in owner/shape order; retain parent-hidden physical areas','Ready/set_disabled modifies get_child(0) only, leaving additional owners unchanged; retain source initial disabled state of all children','Actual Area-body bilateral OR collision filtering with source Player polygon; never investigation ray/point/AABB-only replacement','Existing MusicRegionController is the authoritative inside/registration/attached-voice/fade and deferred-idle exit consumer; this bridge owns no second fake audio log or clock','Body enter guards actual current player/cutscene/battle/flags; exit guards actual current player/collisions/cutscene but not battle; direct Room play/stop bypass Area guards','Global MusicRegion service idle-frame completion occurs in actual source wait sequence; NDSP update/fades are driven exactly once by app audio owner after source audio request order','Tree_exiting calls existing source stop semantics; voices survive scene switch until original fade and global tween completion cleanup','Embedded ENCMUS01 checked source region identities/tuning/shapes cross-match exported geometry and genuine AudioBank source digests; no new audio copies'],unsupported=['Future intro+loop/diegetic regions or initially disabled nodes require evolved shared real MusicRegion service before admission','Unknown external idle listeners that interleave unrelated music mutations with the service batch must be rejected by actual scene host admission','Audio preparation/scene commit/Room history and real NDSP remain mandatory service Host duties, never replaced by successful fake records'])

def validate(d):
 require(d['schema']==1 and d['kind']=='encore.field-music-changer.source-ir'and d['commit']==PIN and d['scene']==SCENE and d['script']==SCRIPT,'MusicChanger source version');music.validate(d['music_regions']);require(len(d['bindings'])==13 and len(d['connections'])==3 and 0<=d['stop_default']<=60,'MusicChanger source capability')
 require([(c['role'],c['signal'],c['method'])for c in d['connections']]==[(1,'body_entered','_on_Area2D_body_entered'),(2,'body_exited','_on_MusicArea_body_exited'),(3,'tree_exiting','_on_MusicArea_tree_exiting')],'MusicChanger source connection topology')
 ids=set();last=-1;regions={r['source_path']:r for r in d['music_regions']['regions']}
 for b in d['bindings']:
  require(b['id']==stable(b['node'])and b['id']not in ids and b['ready_ordinal']>last,'MusicChanger source identity/order');ids.add(b['id']);last=b['ready_ordinal'];r=regions[b['node']];require(b['region_id']==r['id']and b['track_id']==r['track_id']and [o['node']for o in b['shapes']]==r['shape_paths'],'MusicChanger source service geometry binding')
  require(type(b['monitoring'])is bool and type(b['monitorable'])is bool and 0<=b['collision_layer']<=0xffffffff and 0<=b['collision_mask']<=0xffffffff,'Music area collision policy')
  for i,o in enumerate(b['shapes']):
   require(o['id']==stable(o['node'])and o['id']not in ids and o['order']==i and type(o['disabled'])is bool and o['parts'],'MusicChanger shape-owner identity/order');ids.add(o['id'])
   for points in o['parts']:
    require(3<=len(points)<=128 and all(len(p)==2 and all(type(x)in(int,float)and math.isfinite(x)and abs(x)<=1e6 for x in p)for p in points),'MusicChanger source finite convex vertices')
    cross=[(points[(j+1)%len(points)][0]-p[0])*(points[(j+2)%len(points)][1]-points[(j+1)%len(points)][1])-(points[(j+1)%len(points)][1]-p[1])*(points[(j+2)%len(points)][0]-points[(j+1)%len(points)][0])for j,p in enumerate(points)]
    require(any(x!=0 for x in cross)and (all(x>=0 for x in cross)or all(x<=0 for x in cross)),'MusicChanger exported convex geometry not convex')
 return d

def load():
 d=validate(read(IR));r=read(REVIEW);require(r['commit']==PIN and r['ir_sha256']==sha(IR)and r['sources']==d['sources'],'MusicChanger semantic review stale');inv=read(ROOT/'compatibility/upstream-inventory.json')['files']
 for p,h in d['sources'].items():require(sha(ROOT/'upstream/MOTHER-Encore'/p)==h==inv[p]['sha256'],'Changed MusicChanger source '+p)
 music.checked(d['music_regions'],ROOT/'upstream/MOTHER-Encore');return d

def encode(d):
 validate(d);b=bytearray(64)
 def u(*v):b.extend(struct.pack('<'+'I'*len(v),*v))
 def text(v):raw=v.encode();u(len(raw));b.extend(raw)
 text(d['scene']);text(d['script']);b.extend(struct.pack('<f',d['stop_default']));inner=music.checked(d['music_regions'],ROOT/'upstream/MOTHER-Encore');u(len(inner));b.extend(inner);u(len(d['connections']))
 for c in d['connections']:u(c['role']);text(c['signal']);text(c['method'])
 u(len(d['bindings']))
 for a in d['bindings']:
  u(a['id'],a['ready_ordinal'],a['region_id'],a['track_id'],a['collision_layer'],a['collision_mask'],int(a['monitoring'])|int(a['monitorable'])<<1);text(a['node']);u(len(a['shapes']))
  for o in a['shapes']:
   u(o['id'],o['order'],int(o['disabled']));text(o['node']);u(len(o['parts']))
   for part in o['parts']:
    u(len(part))
    for x,y in part:b.extend(struct.pack('<2f',x,y))
 u(len(d['sources']))
 for p,h in d['sources'].items():text(p);b.extend(bytes.fromhex(h))
 struct.pack_into('<8s6I20s12x',b,0,b'ENCMCA01',1,len(b),0,1,1,len(d['bindings']),bytes.fromhex(PIN));struct.pack_into('<I',b,16,zlib.crc32(b)&0xffffffff);return bytes(b)

def audio_bindings():return load()['music_regions']['tracks']
def stage_files(source):
 b=encode(load());p=Path('data/podunk-music-changers.encmarea');require((Path(source)/p).read_bytes()==b,'Stale MusicChanger binary');return{p:b}
def main():
 p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile']);p.add_argument('--native',type=Path);p.add_argument('--source',type=Path);a=p.parse_args()
 if a.action=='extract':
  require(a.native and a.source,'Actual complete Podunk source exports required');d=validate(build(*source_instances(a.native,a.source,ROOT/'upstream/MOTHER-Encore')));write(IR,d);write(REVIEW,dict(schema=1,commit=PIN,ir_sha256=sha(IR),sources=d['sources'],semantics=d['semantics'],unsupported=d['unsupported']));print('MusicChanger source:13 real areas /16 owners /'+str(sum(len(o['parts'])for b in d['bindings']for o in b['shapes']))+' original native convex shapes');return
 b=encode(load());PACK.parent.mkdir(parents=True,exist_ok=True);PACK.write_bytes(b);print('MusicChanger checked resource:',len(b),'bytes')
if __name__=='__main__':
 try:main()
 except(ValueError,KeyError,OSError,TypeError,struct.error)as e:sys.exit('FIELD MUSIC CHANGER ERROR: '+str(e))
