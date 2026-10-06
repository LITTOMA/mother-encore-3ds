#!/usr/bin/env python3
"""Actual three Podunk SteppingSounds areas, source Player/PartyObject dispatch."""
from __future__ import annotations
import argparse,math,re,struct,sys,zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import decode,stable,SCENE,PIN,read,write,sha,require
from tools.extract_battle_entry import Extractor
SCRIPT='Scripts/Main/Stepping Sounds.gd';AREA='Nodes/Reusables/stepping sounds.tscn'
IR=ROOT/'content/native-field-stepping-sounds.json';REVIEW=ROOT/'reports/field-stepping-sounds/source-review.json';PACK=ROOT/'romfs/data/podunk-stepping-sounds.encsteps'
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
 require(len(out)==3,'Podunk SteppingSounds binding loss');return out,names,resources,dict(native_export_sha256=sha(native),source_receipt_sha256=sha(receipt),source_files={name:r['sha256'] for name,r in s['files'].items()})


def build(records,nodes,resources,provenance):
 ex=Extractor(ROOT);ex.sources.update(provenance['source_files']);script=ex.text(SCRIPT);scene=ex.text(AREA);party=ex.text('Scripts/Main/party/party_object.gd');player=ex.text('Scripts/Main/party/Player.gd');shadow=ex.text('Scripts/Main/Shadow.gd');ss=ex.text('Nodes/Reusables/Shadow.tscn');ex.text('LICENSE')
 for fact in ['body == global.get_player() and enabled','body is PartyObject and enter_shadow_effect != ""','exiting_sound != "" and enabled','body is PartyObject and exit_shadow_effect != ""','global.get_player().run_sound = sound','func enable():\n\tenabled = true','func disable():\n\tenabled = false']:require(fact in script,'Unreviewed SteppingSounds source '+fact)
 require('$Shadow.set_anim(anim)'in party and 'play(anim)'in shadow and '_set_behind_parent(!anim in in_front_anims)'in shadow and 'Footsteps/%s.mp3' in player,'Stepping actual Player/PartyObject endpoints changed')
 defaults={k:re.search(r'^export[^\n]* var '+k+r' = "([^"\n]*)"',script,re.M)[1]for k in ['entering_sound','exiting_sound','enter_shadow_effect','exit_shadow_effect']};enabled=re.search(r'^var enabled = (true|false)',script,re.M)[1]=='true'
 signals=[dict(role=i+1,signal=s,method=m)for i,(s,m)in enumerate(re.findall(r'\[connection signal="([^"]+)" from="\." to="\." method="([^"]+)"\]',scene))]
 bindings=[];sounds={};effects={}
 for a in records:
  p=decode(a['native']['properties']);v=defaults.copy();v.update({k:x for k,x in a['overrides'].items()if k in defaults});owners=[]
  require(p['space_override']==0 and not p['audio_bus_override']and p['pause_mode']==0 and p['process_priority']==0,'Stepping area override unsupported')
  for order,o in enumerate(a['native']['physics_shape_owners']):
   child=nodes[o['owner']['path']];wt=decode(child['world_transform']);require(wt[0]==[1,0]and wt[1]==[0,1]and not o['one_way'],'Stepping current geometry capability');parts=[]
   for shape in o['shapes']:
    r=resources[shape['id']];q=decode(r['properties'])
    if r['class']=='RectangleShape2D':
     x,y=q['extents'];points=[[-x,-y],[x,-y],[x,y],[-x,y]]
    elif r['class']=='ConvexPolygonShape2D':points=q['points']
    else:raise ValueError('Unknown stepping source shape '+r['class'])
    parts.append([[wt[2][i]+pt[i]for i in [0,1]]for pt in points])
   owners.append(dict(id=stable(child['path']),node=child['path'],order=order,disabled=o['disabled'],parts=parts))
  for k in ['entering_sound','exiting_sound']:
   name=v[k]
   if name and name not in sounds:
    path='Audio/Sound effects/Footsteps/'+name+'.mp3';ex.data(path);ex.data(path+'.import');sounds[name]=dict(name=name,path=path)
  for k in ['enter_shadow_effect','exit_shadow_effect']:
   name=v[k]
   if not name or name in effects:continue
   require(name=='shadow','Current Podunk shadow capability requires reviewed additional animations')
   require('"name": "shadow",\n"speed": 5.0' in ss and '"frames": [ SubResource( 504 ) ]'in ss,'Actual Shadow animation topology')
   ex.data('Graphics/Character Sprites/Shadow.png');effects[name]=dict(name=name,scene='Nodes/Reusables/Shadow.tscn',texture='Graphics/Character Sprites/Shadow.png',frames=1,fps=5.0,behind=True)
  bindings.append(dict(id=a['stable_id'],node=a['node'],ready_ordinal=a['ready_ordinal'],layer=p['collision_layer'],mask=p['collision_mask'],monitoring=p['monitoring'],monitorable=p['monitorable'],enabled=enabled,**v,shapes=owners))
 return dict(schema=1,kind='encore.field-stepping-sounds.source-ir',commit=PIN,scene=SCENE,script=SCRIPT,connections=signals,sounds=list(sounds.values()),effects=list(effects.values()),bindings=bindings,sources=dict(sorted(ex.sources.items())),provenance=provenance,semantics=['All three actual source areas preserve complete official convex decomposition in shape-owner order; hidden parent does not disable physics','Actual source body bilateral OR collision filtering (Area layer2048 mask1); PartyObject identity and current global player equality come from real runtime objects','Entered player/enabled writes actual run_sound before PartyObject Shadow.set_anim; shadow branch remains active when enabled=false','Exited empty sound/effect does not write; other actual run_sound is used only by subsequent real Player running audio consumer, not immediately played by this area','EventActivator enable/disable directly changes source enabled without changing physics monitoring or shadow policy','Stone polygons28/29 may detach/reparent/add back; actual shape-owner host supplies current registered geometry, keeping original stable shape identity and no duplicate Ready','Host admission validates actual Player runner and PartyObject AnimatedSprite source endpoints before mutations; no second timer/RNG/audio or replacement physics backend'],unsupported=['Unknown party shadow/effect or footsteps assets require an evolved reviewed endpoint before admission','Missing real dynamic shape-owner tree backend, actual PartyObject renderer or Player running audio consumer must refuse admission','Global physics signal order must remain actual source order; this component does not synthesize trigger events from arbitrary ray points'])

def validate(d):
 require(d['schema']==1 and d['kind']=='encore.field-stepping-sounds.source-ir'and d['commit']==PIN and d['scene']==SCENE and d['script']==SCRIPT,'Stepping source version')
 require(len(d['bindings'])==3 and [(x['role'],x['signal'],x['method'])for x in d['connections']]==[(1,'body_entered','_on_Stepping_Sounds_body_entered'),(2,'body_exited','_on_Stepping_Sounds_body_exited')],'Stepping source count/signal topology')
 sounds={s['name']for s in d['sounds']};effects={s['name']for s in d['effects']};require(len(sounds)==len(d['sounds'])and len(effects)==len(d['effects']),'Stepping duplicate endpoint')
 for s in d['sounds']:require(s['path']=='Audio/Sound effects/Footsteps/'+s['name']+'.mp3'and s['path']in d['sources']and s['path']+'.import'in d['sources'],'Stepping sound/source binding')
 for s in d['effects']:require(s['name']=='shadow'and s['frames']==1 and s['fps']==5 and s['behind']is True and s['texture']in d['sources']and s['scene']in d['sources'],'Stepping actual shadow binding')
 ids=set();last=-1
 for b in d['bindings']:
  require(b['id']==stable(b['node'])and b['id']not in ids and b['ready_ordinal']>last,'Stepping identity/order');ids.add(b['id']);last=b['ready_ordinal']
  require(all(type(b[k])is bool for k in ['monitoring','monitorable','enabled'])and all(type(b[k])is int and 0<=b[k]<=0xffffffff for k in ['layer','mask']),'Stepping collision policy')
  require(b['entering_sound']in sounds and (not b['exiting_sound']or b['exiting_sound']in sounds)and b['enter_shadow_effect']in effects and (not b['exit_shadow_effect']or b['exit_shadow_effect']in effects),'Stepping endpoint reference')
  require(b['shapes'],'Stepping missing actual shape owner')
  for i,o in enumerate(b['shapes']):
   require(o['id']==stable(o['node'])and o['id']not in ids and o['order']==i and type(o['disabled'])is bool and o['parts'],'Stepping shape ownership');ids.add(o['id'])
   for pts in o['parts']:
    require(3<=len(pts)<=128 and all(len(p)==2 and all(type(x)in(int,float)and math.isfinite(x)and abs(x)<=1e6 for x in p)for p in pts),'Stepping finite geometry')
    cr=[(pts[(j+1)%len(pts)][0]-p[0])*(pts[(j+2)%len(pts)][1]-pts[(j+1)%len(pts)][1])-(pts[(j+1)%len(pts)][1]-p[1])*(pts[(j+2)%len(pts)][0]-pts[(j+1)%len(pts)][0])for j,p in enumerate(pts)]
    require(any(x!=0 for x in cr)and(all(x>=0 for x in cr)or all(x<=0 for x in cr)),'Stepping official convex decomposition')
 return d

def load():
 d=validate(read(IR));r=read(REVIEW);require(r['commit']==PIN and r['ir_sha256']==sha(IR)and r['sources']==d['sources'],'Stepping semantic review stale');inv=read(ROOT/'compatibility/upstream-inventory.json')['files']
 for p,h in d['sources'].items():require(sha(ROOT/'upstream/MOTHER-Encore'/p)==h==inv[p]['sha256'],'Changed stepping source '+p)
 return d

def encode(d):
 validate(d);b=bytearray(64)
 def u(*v):b.extend(struct.pack('<'+'I'*len(v),*v))
 def text(v):raw=v.encode();u(len(raw));b.extend(raw)
 text(d['scene']);text(d['script']);u(len(d['connections']))
 for c in d['connections']:u(c['role']);text(c['signal']);text(c['method'])
 u(len(d['sounds']))
 for s in d['sounds']:text(s['name']);text(s['path'])
 u(len(d['effects']))
 for s in d['effects']:text(s['name']);text(s['scene']);text(s['texture']);u(s['frames']);b.extend(struct.pack('<f',s['fps']));u(int(s['behind']))
 u(len(d['bindings']))
 for a in d['bindings']:
  u(a['id'],a['ready_ordinal'],a['layer'],a['mask'],int(a['monitoring'])|int(a['monitorable'])<<1,int(a['enabled']));text(a['node'])
  for k in ['entering_sound','exiting_sound','enter_shadow_effect','exit_shadow_effect']:text(a[k])
  u(len(a['shapes']))
  for o in a['shapes']:
   u(o['id'],o['order'],int(o['disabled']));text(o['node']);u(len(o['parts']))
   for pts in o['parts']:
    u(len(pts))
    for p in pts:b.extend(struct.pack('<2f',*p))
 u(len(d['sources']))
 for p,h in d['sources'].items():text(p);b.extend(bytes.fromhex(h))
 struct.pack_into('<8s6I20s12x',b,0,b'ENCSTP01',1,len(b),0,1,1,len(d['bindings']),bytes.fromhex(PIN));struct.pack_into('<I',b,16,zlib.crc32(b)&0xffffffff);return bytes(b)

def stage_files(source):
 b=encode(load());p=Path('data/podunk-stepping-sounds.encsteps');require((Path(source)/p).read_bytes()==b,'Stale stepping pack');return{p:b}

def main():
 p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile']);p.add_argument('--native',type=Path);p.add_argument('--source',type=Path);a=p.parse_args()
 if a.action=='extract':
  require(a.native and a.source,'Actual complete Podunk source exports required');d=validate(build(*source_instances(a.native,a.source,ROOT/'upstream/MOTHER-Encore')));write(IR,d);write(REVIEW,dict(schema=1,commit=PIN,ir_sha256=sha(IR),sources=d['sources'],semantics=d['semantics'],unsupported=d['unsupported']));print('SteppingSounds actual source:',len(d['bindings']),'areas;',sum(len(b['shapes'])for b in d['bindings']),'owners;',sum(len(o['parts'])for b in d['bindings']for o in b['shapes']),'official convex pieces');return
 b=encode(load());PACK.parent.mkdir(parents=True,exist_ok=True);PACK.write_bytes(b);print('SteppingSounds checked binary:',len(b),'bytes')
if __name__=='__main__':
 try:main()
 except(ValueError,KeyError,OSError,TypeError,struct.error)as e:sys.exit('FIELD STEPPING SOUNDS ERROR: '+str(e))
