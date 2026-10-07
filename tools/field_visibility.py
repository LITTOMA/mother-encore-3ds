#!/usr/bin/env python3
"""Full source-native World2D visibility; no scene/script Ready approval."""
from __future__ import annotations
import argparse,hashlib,json,re,struct,sys,zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import PIN,SCENE,read,write,decode,stable,sha,require
IR=ROOT/'content/native-field-visibility.json';REVIEW=ROOT/'compatibility/reviews/native-field-visibility-v0410.json';OUT=ROOT/'romfs/data/podunk.encvisibility'
TREE=ROOT/'content/podunk-node-tree.json';LIFE=ROOT/'content/podunk-scene-lifecycle.json'
FLAGS=['pause_animations','freeze_bodies','pause_particles','pause_animated_sprites','process_parent','physics_process_parent']
# Typed script adapters, independent from script filenames stored in the IR.
SCRIPTS={'Scripts/misc/grass spawner.gd':1,'Scripts/Main/npc.gd':2,'Scripts/misc/butterfly.gd':3,'Scripts/misc/dandelion spawner.gd':4,'Scripts/misc/birds.gd':5,'Scripts/Main/Enemy Spawner.gd':6}

def extract(native,engine):
 d=read(native);tree=read(TREE);inventory=read(ROOT/'compatibility/upstream-inventory.json')['files'];nm={n['path']:n for n in d['nodes']};rm={r['id']:r for r in d['resources']};states={s['source'][6:]:s for s in d['scene_states']};tr={r['node']:r for r in tree['records']}
 require(d['source']=='res://'+SCENE and len(nm)==len(tr)==8686 and sha(native)==tree['native_sha256'] and not d['native_compatible'],'Visibility requires full official native export')
 engine=Path(engine);proof=read(engine/'engine-source.json');sources=dict(tree['sources']);roots=[]
 for p,v in proof.items():
  if isinstance(v,dict) and 'sha256'in v:require(sha(engine/p)==v['sha256'],'Changed official engine source '+p)
 w=(engine/'world_2d.cpp').read_text();v=(engine/'visibility_notifier_2d.cpp').read_text()
 require('GLOBAL_DEF("world/2d/cell_size", 100)'in w and 'Point2i begin = p_rect.position;'in w and 'i <= end.x'in w and 'added.front()'in w,'Unknown native spatial source')
 require('c->get_filename() != String()'in v and 'CONNECT_REFERENCE_COUNTED'in v and '_change_node_state(p_node, true)'in v,'Unknown native enabler source')
 project=(ROOT/'upstream/MOTHER-Encore/project.godot').read_text();setting=re.search(r'^world/2d/cell_size=(\d+)$',project,re.M);cell=int(setting[1])if setting else int(re.search(r'GLOBAL_DEF\("world/2d/cell_size", (\d+)\)',w)[1]);require(cell>0,'Invalid source cell size')
 def local_path(root,local):
  local=local[2:]if local.startswith('./')else local
  return root if local=='.'else local if root=='.'else root+'/'+local
 def visit(root,file):
  roots.append((root,file))
  for n in states[file]['nodes']:
   if n['instance']is not None:visit(local_path(root,n['path']),rm[n['instance']['id']]['path'][6:])
 visit('.',SCENE);boundaries={r for r,f in roots};children={p:[]for p in nm}
 for p in nm:
  if p!='.':children[p.rsplit('/',1)[0]if'/'in p else'.'].append(p)
 rows=[];visibility={p for p,n in nm.items()if n['class']in['VisibilityNotifier2D','VisibilityEnabler2D']}
 for p in nm:
  if p not in visibility:continue
  n=nm[p];prop=decode(n['properties']);require(prop['script']is None and not tr[p]['script'] and prop['material']is None and not prop['use_parent_material'],'Unreviewed notifier native subclass/material')
  parent=p.rsplit('/',1)[0]if'/'in p else'.';kind=1 if n['class']=='VisibilityNotifier2D'else 2;scope=p
  while scope not in boundaries:scope=scope.rsplit('/',1)[0]if'/'in scope else'.'
  tracked=[]
  def find(q):
   c=nm[q]['class'];k={'AnimationPlayer':1,'AnimatedSprite':2,'RigidBody2D':3,'Particles2D':4}.get(c)
   if k:tracked.append(dict(id=stable(q),kind=k,node=q))
   for child in children[q]:
    if child not in boundaries:find(child)
  if kind==2:find(scope)
  rect=prop['rect'][0]+prop['rect'][1];require(rect[2]>=0 and rect[3]>=0,'Negative source visibility rect')
  rows.append(dict(id=stable(p),parent=stable(parent),scope=stable(scope),kind=kind,ready=tr[p]['ready'],node=p,rect=rect,flags=sum(int(prop[k])<<i for i,k in enumerate(FLAGS))if kind==2 else 0,tracked=tracked))
 links=[];seen=set()
 for root,file in roots:
  text=(ROOT/'upstream/MOTHER-Encore'/file).read_text(encoding='utf8')
  for line in re.findall(r'^\[connection ([^\n]+)\]$',text,re.M):
   params=dict(re.findall(r'(\w+)="([^"\n]*)"',line));require(all(k in params for k in ['signal','from','to','method']),'Unknown connection syntax')
   emitter=local_path(root,params['from'])
   if emitter not in visibility:continue
   target=local_path(root,params['to']);require(target in tr and params['signal']in['screen_entered','screen_exited']and not re.search(r'\b(?:binds|flags)=',line),'Unknown visibility source connection')
   script=tr[target]['script'];adapter=SCRIPTS.get(script);require(adapter,'Unknown visibility source script '+script)
   require(re.search(r'^func\s+'+re.escape(params['method'])+r'\s*\(\s*\)',(ROOT/'upstream/MOTHER-Encore'/script).read_text(),re.M),'Unknown visibility source callback')
   key=(stable(emitter),stable(target),params['signal'],params['method'])
   if key in seen:continue
   seen.add(key);links.append(dict(emitter=key[0],target=key[1],signal=key[2],method=key[3],adapter=adapter,script=script,script_sha=tr[target]['script_sha'],source=file,source_sha=inventory[file]['sha256']))
 require(len(rows)==len(visibility)and rows,'Missing native visibility closure')
 for p,h in sources.items():require(h==inventory[p]['sha256']==sha(ROOT/'upstream/MOTHER-Encore'/p),'Visibility upstream source changed '+p)
 scan=int(re.search(r'visible_cells > (\d+)',w)[1])
 write(IR,dict(schema=1,kind='encore.field-visibility.source-ir',commit=PIN,scene=SCENE,scene_id=tree['scene_id'],source_sha256=tree['source_sha256'],tree_ir_sha256=sha(TREE),native_sha256=sha(native),sources=sources,engine_sources=proof,cell_size=cell,scan_cutoff=scan,flags=FLAGS,records=rows,connections=links,scene_admitted=False,semantics=['Signed integer truncation into inclusive World2D cells; not exact rectangle intersections','Actual native ObjectID order within spatial cells and viewport removal map; additions before exits','screen_entered before Enabler enable; viewport_exited before screen_exited before disable','Instanced source scene boundaries excluded from Enabler child traversal','Original source callback methods dispatch only the existing unique typed runtime'],pending=['Actual dynamic grass/enemy native factories must join their separate checked recipe visibility scope','No arbitrary viewport/editor mode, RigidBody/Particles implementation, or script VM admission']))

def load():
 d=read(IR);r=read(REVIEW);inventory=read(ROOT/'compatibility/upstream-inventory.json')['files'];require(d['schema']==1 and d['commit']==PIN and d['scene']==SCENE and not d['scene_admitted'] and sha(IR)==r['ir_sha256'] and r['commit']==PIN and d['tree_ir_sha256']==sha(TREE)and d['flags']==FLAGS,'Visibility source review stale')
 for p,h in d['sources'].items():require(h==inventory[p]['sha256']==sha(ROOT/'upstream/MOTHER-Encore'/p),'Visibility changed source '+p)
 return d

def encode(d):
 b=bytearray(128)
 def u(*v):b.extend(struct.pack('<'+'I'*len(v),*v))
 def t(v):x=v.encode();u(len(x));b.extend(x)
 t(d['scene']);u(d['cell_size'],d['scan_cutoff'],len(d['records']))
 for r in d['records']:
  u(r['id'],r['parent'],r['scope'],r['kind'],r['ready'],r['flags']);b.extend(struct.pack('<4f',*r['rect']));t(r['node']);u(len(r['tracked']))
  for q in r['tracked']:u(q['id'],q['kind'])
 u(len(d['connections']))
 for r in d['connections']:
  u(r['emitter'],r['target'],r['adapter'],1 if r['signal']=='screen_entered'else 2);t(r['method']);t(r['script']);b.extend(bytes.fromhex(r['script_sha']))
 u(len(d['sources']))
 for p,h in d['sources'].items():t(p);b.extend(bytes.fromhex(h))
 struct.pack_into('<8s8I',b,0,b'ENCFVS01',1,128,len(b),zlib.crc32(b[128:]),0x454e0069,1,1,d['scene_id']);b[40:60]=bytes.fromhex(PIN);b[60:92]=bytes.fromhex(d['source_sha256']);b[92:124]=hashlib.sha256(IR.read_bytes()).digest();return bytes(b)

def stage_files(source):
 raw=encode(load());require((Path(source)/'data/podunk.encvisibility').read_bytes()==raw,'Visibility staged binary differs');return {Path('data/podunk.encvisibility'):raw}
def main():
 p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile','verify']);p.add_argument('--native',type=Path);p.add_argument('--engine',type=Path);a=p.parse_args()
 if a.action=='extract':require(a.native and a.engine,'Explicit official complete native + engine receipt required');extract(a.native,a.engine);return
 d=load();raw=encode(d)
 if a.action=='verify':require(OUT.read_bytes()==raw,'Visibility stale binary')
 else:OUT.parent.mkdir(parents=True,exist_ok=True);OUT.write_bytes(raw)
 print('Visibility full native',len(d['records']),'nodes;',len(d['connections']),'source connections;',len(raw),'bytes; scene_admitted=False')
if __name__=='__main__':
 try:main()
 except(ValueError,KeyError,TypeError,OSError,struct.error)as e:sys.exit('FIELD VISIBILITY ERROR: '+str(e))
