#!/usr/bin/env python3
"""Original DialogueBox complete source NodeRecipe, never native/script approval."""
from __future__ import annotations
import argparse,hashlib,re,struct,sys,zlib,math,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import PIN,read,write,decode,sha,require
from tools.field_node_tree import CLASSES as STATIC_CLASSES,METHODS
SCENE='Nodes/Ui/DialogueBox.tscn'
CLASSES=STATIC_CLASSES+['CanvasLayer','Control','NinePatchRect','GridContainer','RichTextLabel','VScrollBar']
CONTROLS=['TextureRect','HBoxContainer','Label','ReferenceRect','Control','NinePatchRect','GridContainer','RichTextLabel','VScrollBar']
IR=ROOT/'content/dialogue-node-recipe.json';REVIEW=ROOT/'compatibility/reviews/dialogue-node-recipe-v0410.json';OUT=ROOT/'romfs/data/dialogue.encnoderecipe'
def stable(path):
 n=int.from_bytes(hashlib.sha256(('recipe:'+SCENE+'#'+path).encode()).digest()[:4],'little');require(n,'Zero recipe identity');return n
def scalar(v):
 n=struct.unpack('<f',struct.pack('<f',float(v)))[0];require(math.isfinite(n),'Nonfinite recipe native scalar');return n
def numbers(v):return [numbers(x)if isinstance(x,list)else scalar(x)for x in v]
def source_scripts(d,proof,sources,inv):
 from tools.scene_reference import quarantine
 states={s['source'][6:]:s for s in d['scene_states']};resources={r['id']:r for r in d['resources']};names={n['path']for n in d['nodes']};roots=[];assignments={}
 for file in states:
  require(file in sources,'Recipe missing SceneState source '+file)
  clean,_,attachments,_=quarantine((ROOT/'upstream/MOTHER-Encore'/file).read_bytes(),file);attached={a['node_declaration']:a for a in attachments};current='';props=[];entries=[]
  for line in clean.decode().splitlines():
   if line.startswith('['):
    if current:entries.append((current,props))
    current=line if line.startswith('[node ')else'';props=[]
   elif current:props.append(line)
  if current:entries.append((current,props))
  rows=[]
  for decl,lines in entries:
   changes=[v for v in lines if re.match(r'^script\s*=',v)]
   if not changes:continue
   require(len(changes)==1 and re.fullmatch(r'script\s*=\s*null',changes[0]),'Unknown recipe script assignment')
   name=re.search(r'\bname="([^"\n]+)"',decl)[1];parent=re.search(r'\bparent="([^"\n]+)"',decl);local='.'if parent is None else name if parent[1]=='.'else parent[1]+'/'+name
   a=attached.get(decl);script=a['script']if a else None;h=(a.get('embedded_script',{}).get('sha256')or inv[script]['sha256'])if script else None;rows.append((local,script,h,decl))
  assignments[file]=rows
 def visit(root,file):
  require((root,file)not in roots,'Recipe scene cycle');roots.append((root,file))
  for n in states[file]['nodes']:
   if n['instance']is not None:
    local=n['path'][2:]if n['path'].startswith('./')else n['path'];target=root if local=='.'else local if root=='.'else root+'/'+local;visit(target,resources[n['instance']['id']]['path'][6:])
 visit('.',SCENE);bindings={};nulls={}
 for root,file in sorted(roots,key=lambda r:r[0].count('/')if r[0]!='.'else -1,reverse=True):
  for local,script,h,decl in assignments[file]:
   path=root if local=='.'else local if root=='.'else root+'/'+local;require(path in names,'Recipe script target missing '+path)
   if script is None:
    if path in bindings:nulls[path]=dict(previous_script=bindings[path][0],source=file,source_sha256=inv[file]['sha256'],declaration=decl)
    bindings.pop(path,None)
   else:bindings[path]=(script,h);nulls.pop(path,None)
 return bindings,nulls

def method_inventory(script,inv,sources,cache,classes):
 if script in cache:return cache[script]
 file,_,sub=script.partition('::');path=ROOT/'upstream/MOTHER-Encore'/file;require(file in inv and sha(path)==inv[file]['sha256'],'Recipe script source changed');sources[file]=inv[file]['sha256'];text=path.read_text(encoding='utf-8')
 if sub:
  block=re.search(r'\[sub_resource type="GDScript" id='+re.escape(sub)+r'\]\n(.*?)(?=\n\[|\Z)',text,re.S);require(block,'Recipe embedded source missing');q=re.search(r'script/source = ',block[1]);require(q,'Recipe embedded source missing');text=json.JSONDecoder(strict=False).raw_decode(block[1][q.end():])[0]
 ext=re.search(r'^extends\s+(?:"([^"\n]+)"|([A-Za-z_][A-Za-z_0-9]*))',text,re.M);require(ext,'Recipe script superclass missing');base=ext[1]or ext[2];mask=0
 if ext[1]:
  base=base[6:]if base.startswith('res://')else str((Path(file).parent/base).as_posix());mask=method_inventory(base,inv,sources,cache,classes)
 elif base in classes:mask=method_inventory(classes[base],inv,sources,cache,classes)
 else:require(base in CLASSES+['CanvasItem','Object','Reference','Resource','CollisionObject2D'],'Recipe unknown source superclass '+base)
 for i,name in enumerate(METHODS):
  if re.search(r'^func\s+'+name+r'\s*\(',text,re.M):mask|=1<<i
 cache[script]=mask;return mask

def extract(native,tree,receipt):
 d=read(native);a=read(tree);proof=read(receipt);inv=read(ROOT/'compatibility/upstream-inventory.json')['files']
 require(d['source']=='res://'+SCENE and d['native_compatible']is False and a['schema']==1 and a['scene']==SCENE and a['scene_entered']is False and [a['engine'].get(k)for k in ['major','minor','patch','status']]==[3,6,2,'stable']and [d['godot'].get(k)for k in ['major','minor','patch','status']]==[3,6,2,'stable'],'Recipe original engine/scene rejected')
 require(proof['commit']==PIN and proof['scene']==SCENE,'Recipe source receipt pin rejected');sources={}
 for file,r in proof['files'].items():require(sha(ROOT/'upstream/MOTHER-Encore'/file)==r['sha256']==inv[file]['sha256'],'Recipe source changed '+file);sources[file]=r['sha256']
 nm={n['path']:n for n in d['nodes']};rows=a['nodes'];require(len(nm)==len(rows)==47 and [n['path']for n in rows]==list(nm),'Recipe full original 47 nodes required');rm={n['path']:n for n in rows};script,nulls=source_scripts(d,proof,sources,inv);classes={};cache={}
 for f in(ROOT/'upstream/MOTHER-Encore').rglob('*.gd'):
  c=re.search(r'^class_name\s+(\w+)',f.read_text(encoding='utf-8'),re.M)
  if c:require(c[1]not in classes,'Duplicate recipe class_name');classes[c[1]]=f.relative_to(ROOT/'upstream/MOTHER-Encore').as_posix()
 records=[];children={p:[]for p in nm};controls=[];layers=[]
 for n in rows:
  path=n['path'];require(n['class']==nm[path]['class']and n['class']in CLASSES and n['name']==nm[path]['name'],'Recipe native class/name rejected');parent=''if path=='.'else path.rsplit('/',1)[0]if'/'in path else'.';require(n['parent']==parent and(not n['owner']or n['owner']in rm)and not n['unique'],'Recipe owner/unique path rejected');props=decode(nm[path]['properties']);require(n['pause']==props['pause_mode']and n['priority']==props['process_priority'],'Recipe source process state rejected')
  if parent:children[parent].append(path)
  flags=int(n['canvas']);local=[[1,0],[0,1],[0,0]];world=local;mod=[1,1,1,1];selfmod=mod;cp='';z=light=0
  if n['canvas']:
   local=numbers(n['local']);world=numbers(n['world']);mod=numbers(n['modulate']);selfmod=numbers(n['self_modulate']);cp=n['canvas_parent'];require(cp==(''if n['top_level']or not parent or not rm[parent]['canvas']else parent),'Recipe Canvas ancestry rejected');require('world_transform'not in nm[path]or world==decode(nm[path]['world_transform']),'Recipe native world transform rejected');flags|=sum(int(n[k])<<(i+1)for i,k in enumerate(['visible','top_level','behind','use_parent_material','material','z_relative','y_sort','notify_transform','notify_local_transform']));z=n['z'];light=n['light_mask']
  s,h=script.get(path,('','0'*64));mask=method_inventory(s,inv,sources,cache,classes)if s else 0;generated=n['name'].startswith('@');require(not generated or (n['class']=='VScrollBar'and parent and rm[parent]['class']=='RichTextLabel'and not n['owner']and re.fullmatch(r'@@[0-9]+',n['name'])),'Unknown native internal constructor node');records.append(dict(native_generated=generated,id=stable(path),node=path,name=n['name'],class_index=CLASSES.index(n['class']),native_class=n['class'],parent=stable(parent)if parent else 0,owner=stable(n['owner'])if n['owner']else 0,canvas_parent=stable(cp)if cp else 0,index=n['index'],pause=n['pause'],priority=n['priority'],flags=flags,z=z,light_mask=light,local=local,world=world,modulate=mod,self_modulate=selfmod,groups=n['groups'],script=s,script_sha=h,script_methods=mask,ready=0))
  if n['class']in CONTROLS:
   c=n['control'];c={k:numbers(v)if isinstance(v,list)else scalar(v)if k in ['rotation','stretch']else v for k,v in c.items()};require(c['anchors']==[props['anchor_'+v]for v in ['left','top','right','bottom']]and c['margins']==[props['margin_'+v]for v in ['left','top','right','bottom']]and c['scale']==props['rect_scale']and c['rotation']==props['rect_rotation']and c['min_size']==props['rect_min_size'],'Recipe native Control properties rejected');controls.append(dict(id=stable(path),**c))
  if n['class']=='CanvasLayer':
   c=n['canvas_layer'];require(c['custom_viewport']is False and props.get('custom_viewport')is None,'Recipe source custom Viewport not supported');c={k:numbers(v)if isinstance(v,list)else scalar(v)if k in ['rotation','follow_scale']else v for k,v in c.items()};require(c['transform']==props['transform']and c['layer']==props['layer'],'Recipe CanvasLayer native properties rejected');layers.append(dict(id=stable(path),visible=props['visible'],world_2d_binding=0,**c))
 order=[]
 def visit(p):
  for child in children[p]:visit(child)
  order.append(p)
 visit('.');ready={p:i for i,p in enumerate(order)}
 for r in records:r['ready']=ready[r['node']]
 require(len(order)==47,'Recipe incomplete ready traversal')
 write(IR,dict(schema=1,kind='encore.field-node-recipe.source-ir',commit=PIN,scene=SCENE,scene_id=stable('.'),source_sha256=sources[SCENE],sources=sources,native_sha256=sha(native),tree_export_sha256=sha(tree),source_receipt_sha256=sha(receipt),exporter_sha256=sha(ROOT/'tools/godot_exporter/field_node_recipe.gd'),classes=CLASSES,script_methods=METHODS,script_null_overrides=nulls,records=records,controls=controls,canvas_layers=layers,scene_admitted=False,pending=['Native CanvasLayer external Viewport/World2D must be supplied by real host at ENTER','Every native class and inherited script requires actual typed owner callbacks','RichTextLabel native VScrollBar children retained; no fake Control Ready/layout/render approval','Source timers/animations/fonts/audio/signals owned by independent checked consumers']))

def load():
 d=read(IR);r=read(REVIEW);inv=read(ROOT/'compatibility/upstream-inventory.json')['files'];require(d['schema']==1 and d['commit']==PIN and d['scene']==SCENE and d['scene_admitted']is False and d['classes']==CLASSES and d['script_methods']==METHODS and r['ir_sha256']==sha(IR)and r['commit']==PIN and d['exporter_sha256']==sha(ROOT/'tools/godot_exporter/field_node_recipe.gd'),'Recipe schema/source review rejected')
 for p,h in d['sources'].items():require(inv[p]['sha256']==h==sha(ROOT/'upstream/MOTHER-Encore'/p),'Recipe changed source '+p)
 require(len(d['records'])==47 and len({r['id']for r in d['records']})==47 and all(r['id']==stable(r['node'])and r['native_class']==CLASSES[r['class_index']]for r in d['records']),'Recipe node source identity rejected');return d

def encode(d):
 b=bytearray(128)
 def u(*v):b.extend(struct.pack('<'+'I'*len(v),*v))
 def i(*v):b.extend(struct.pack('<'+'i'*len(v),*v))
 def f(*v):b.extend(struct.pack('<'+'f'*len(v),*v))
 def t(s):v=s.encode();u(len(v));b.extend(v)
 t(d['scene']);u(len(CLASSES));[t(v)for v in CLASSES]
 for r in d['records']:
  u(*[r[k]for k in ['id','parent','owner','canvas_parent','class_index','ready','pause','flags','light_mask','script_methods']]);i(r['index'],r['priority'],r['z']);f(*[v for row in r['local']for v in row],*[v for row in r['world']for v in row],*r['modulate'],*r['self_modulate']);t(r['node']);t(r['name']);t(r['script']);b.extend(bytes.fromhex(r['script_sha']));u(len(r['groups']));[t(v)for v in r['groups']];u(int(r['native_generated']))
 u(len(d['sources']))
 for p,h in d['sources'].items():t(p);b.extend(bytes.fromhex(h))
 u(len(d['canvas_layers']))
 for c in d['canvas_layers']:
  u(c['id']);i(c['layer']);u(int(c['follow_viewport']),int(c['custom_viewport']),c['world_2d_binding'],int(c['visible']));f(c['follow_scale'],*[v for row in c['transform']for v in row],*c['offset'],c['rotation'],*c['scale'])
 u(len(d['controls']))
 for c in d['controls']:
  u(c['id'],int(c['clip']),c['mouse'],c['focus']);i(*[int(v)for v in c['grow']]);u(*[int(v)for v in c['size_flags']]);f(*c['anchors'],*c['margins'],*c['position'],*c['size'],*c['scale'],c['rotation'],*c['pivot'],*c['min_size'],c['stretch'])
 struct.pack_into('<8s8I',b,0,b'ENCFNRC1',1,128,len(b),0,0x454e003d,3,len(d['records']),d['scene_id']);b[40:60]=bytes.fromhex(PIN);b[60:92]=bytes.fromhex(d['source_sha256']);b[92:124]=hashlib.sha256(IR.read_bytes()).digest();struct.pack_into('<I',b,20,zlib.crc32(b[128:]));return bytes(b)

def stage_files(source):
 raw=encode(load());require((Path(source)/'data/dialogue.encnoderecipe').read_bytes()==raw,'Staged Recipe differs');return {Path('data/dialogue.encnoderecipe'):raw}
def main():
 p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile','verify']);p.add_argument('--native',type=Path);p.add_argument('--tree',type=Path);p.add_argument('--source',type=Path);a=p.parse_args()
 if a.action=='extract':require(a.native and a.tree and a.source,'Explicit complete original native/tree/source receipt required');extract(a.native,a.tree,a.source);return
 raw=encode(load())
 if a.action=='verify':require(OUT.read_bytes()==raw,'Recipe stale output')
 else:OUT.parent.mkdir(parents=True,exist_ok=True);OUT.write_bytes(raw)
 print('Dialogue NodeRecipe: 47 complete native nodes;',len(raw),'bytes; native/script Ready not approved')
if __name__=='__main__':
 try:main()
 except(ValueError,KeyError,TypeError,OSError,struct.error)as e:sys.exit('FIELD NODE RECIPE ERROR: '+str(e))
