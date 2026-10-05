#!/usr/bin/env python3
"""Complete actual Podunk CharacterTint child source -> native data/consumer."""
from __future__ import annotations
import argparse,hashlib,json,math,re,struct,sys,zlib,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import decode,stable,SCENE,PIN,read,write,sha,require
from tools.extract_battle_entry import Extractor,node,properties,variant
SCRIPT='Scripts/misc/character_tint.gd';FACTORY='Nodes/Overworld/Enemies/Basic Enemy.tscn'
IR=ROOT/'content/native-field-tint.json';PACK=ROOT/'romfs/data/podunk-tint.enctint'
def source_instances(native,receipt,upstream):
 d=read(native);s=read(receipt);require(d['source']=='res://'+SCENE and d['schema']==1 and d['native_compatible'] is False,'Incomplete NPC native scene')
 require(s['commit']==PIN and s['scene']==SCENE,'Unreviewed NPC native receipt')
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
  out.append(dict(stable_id=stable(path),node=path,ready_ordinal=ordinal[path],overrides=overrides[path],native=names[path],children=[names[p] for p in children[path]]))
 require(len(out)==87,'Podunk Tint binding loss');return out,names,resources,dict(native_export_sha256=sha(native),source_receipt_sha256=sha(receipt),source_files={name:r['sha256'] for name,r in s['files'].items()})

def resolve_path(base,relative):
 parts=base.split('/') if base!='.' else []
 for p in relative.split('/'):
  if p in ('','.'):continue
  if p=='..':
   require(parts,'Tint NodePath escapes scene');parts.pop()
  else:parts.append(p)
 return '/'.join(parts) or '.'

def source_id(scene,path):return int.from_bytes(hashlib.sha256(('field:'+scene+'#'+path).encode()).digest()[:4],'little')

def build(records,nodes,provenance):
 ex=Extractor(ROOT);script=ex.text(SCRIPT);ex.text('Scripts/misc/SpriteTinter.gd');ex.text('Scripts/Main/actor.gd');ex.text('LICENSE')
 require(re.findall(r'^func ([A-Za-z_]+)\(',script,re.M)==['_ready','_set_targets','connect_tint','set_tint'] and 'var _tint := Color.white' in script and '_targets.append(node)' in script and 'if !_targets: _set_targets()' in script and script.index('node.self_modulate = _tint')<script.index('emit_signal("changed_tint", _tint)') and 'character_tint.set_tint(_tint)'in script,'Unreviewed Tint source mechanism')
 allowed={'Sprite','Node2D','KinematicBody2D','StaticBody2D','Area2D','AnimatedSprite','Light2D','TileMap','Control'}
 out=[]
 for rec in records:
  paths=rec['overrides'].get('sprite_paths',[]);targets=[]
  for ref in paths:
   require(isinstance(ref,dict)and ref.get('type')=='NodePath','Unreviewed tint target NodePath');path=resolve_path(rec['node'],ref['value']);exists=path in nodes;kind=nodes[path]['class']if exists else '';require(not exists or kind in allowed,'Unsupported tint target CanvasItem class '+kind);color=decode(nodes[path]['properties']['self_modulate'])if exists else [1,1,1,1]
   if isinstance(color,dict):require(set(color)=={'type','r','g','b','a'}and color['type']=='Color','Unsupported native Color');color=[color[k]for k in('r','g','b','a')]
   targets.append(dict(node_path=ref['value'],node=path,source_id=stable(path),exists=exists,kind=kind,initial_self_modulate=color))
  out.append(dict(id=rec['stable_id'],scene=SCENE,scene_id=source_id(SCENE,'.'),node=rec['node'],ready_ordinal=rec['ready_ordinal'],kind=1,targets=targets))
 for kind,factory in [(2,FACTORY),(3,'Nodes/Reusables/actor.tscn')]:
  source=ex.text(factory);props=node(source,'CharacterTint');targets=[];parsed={}
  resources={int(i):p for p,i in re.findall(r'\[ext_resource path="res://([^"]+)" type="PackedScene" id=(\d+)\]',source)}
  for m in re.finditer(r'^\[node ([^\n]+)\]\n(.*?)(?=^\[|\Z)',source,re.M|re.S):
   attrs=dict(re.findall(r'(\w+)="([^"]*)"',m[1]));local='.'if 'parent'not in attrs else attrs['name']if attrs['parent']=='.'else attrs['parent']+'/'+attrs['name'];typ=attrs.get('type')
   if not typ:
    inst=re.search(r'instance=ExtResource\( (\d+) \)',m[1]);require(inst,'Unresolved prototype target type');child=ex.text(resources[int(inst[1])]);typ=re.search(r'^\[node name="[^"]+" type="([^"]+)"\]',child,re.M)[1]
   color=re.search(r'^self_modulate = (.+)$',m[2],re.M);parsed[local]=(typ,{'self_modulate':variant(color[1])}if color else {})
  order=list(parsed);ready_ordinal=0xffffffff # Actual factory Host supplies complete postorder Ready, never declaration index
  for ref in props['sprite_paths']:
   path=resolve_path('CharacterTint',ref);exists=path in parsed;typ=parsed[path][0]if exists else '';require(not exists or typ in allowed,'Unsupported prototype tint target');color=parsed[path][1].get('self_modulate',[1,1,1,1])if exists else [1,1,1,1]
   targets.append(dict(node_path=ref,node=path,source_id=source_id(factory,path),exists=exists,kind=typ,initial_self_modulate=color))
  out.append(dict(id=source_id(factory,'CharacterTint'),scene=factory,scene_id=source_id(factory,'.'),node='CharacterTint',ready_ordinal=ready_ordinal,kind=kind,targets=targets))
 for name,h in provenance['source_files'].items():ex.data(name);require(ex.sources[name]==h,'Tint fullscene source changed')
 return dict(schema=1,kind='encore.field-tint.source-ir',commit=PIN,scene=SCENE,scene_id=source_id(SCENE,'.'),default_tint=[1,1,1,1],records=out,sources=dict(sorted(ex.sources.items())),provenance=provenance,semantics=['Ready resolves source nullable NodePaths in exported array order and appends without clearing','set_tint repopulates only when cached target array is empty, writes self_modulate per target then synchronous changed_tint','connect_tint attempts source connection then always pushes current tint immediately; duplicate source connection does not duplicate later delivery','Scene instance and checked enemy/actor prototype namespaces are independent; no RNG or implicit game colors in C++','Dynamic factory Ready ordinal is an explicit unavailable sentinel; Host supplies actual full factory postorder rather than guessing from declaration index'],unsupported=['Signal cycles not admitted: original connect_tint graph has no reviewed cycle','SpriteDataFetcher FloorReflector getter/reflection lifecycle remains independent pending slice','CharacterSprite source creation/animation binding admission remains independent pending slice'])

def validate(d):
 require(d['schema']==1 and d['kind']=='encore.field-tint.source-ir'and d['commit']==PIN and d['scene']==SCENE and len(d['records'])==89 and d['default_tint']==[1,1,1,1],'Tint schema/source scope')
 ids=set();last=-1
 for r in d['records']:
  require(type(r['id'])is int and 0<r['id']<0xffffffff and r['id']not in ids and r['kind']in(1,2,3)and len(r['targets'])<=32,'Tint record');ids.add(r['id'])
  if r['kind']==1:require(r['ready_ordinal']>last,'Tint source Ready order');last=r['ready_ordinal']
  for t in r['targets']:
   require(type(t['exists'])is bool and type(t['source_id'])is int and 0<t['source_id']<0xffffffff and t['node_path']and t['node']and (t['kind']in {'Sprite','Node2D','KinematicBody2D','StaticBody2D','Area2D','AnimatedSprite','Light2D','TileMap','Control'}if t['exists']else t['kind']==''),'Tint target binding');require(len(t['initial_self_modulate'])==4 and all(type(v)in(int,float)and math.isfinite(v)for v in t['initial_self_modulate']),'Tint source color')
 return d

def extract(native,receipt):
 records,nodes,resources,provenance=source_instances(native,receipt,ROOT/'upstream/MOTHER-Encore');d=validate(build(records,nodes,provenance));write(IR,d);write(ROOT/'reports/field-tint/source-review.json',dict(schema=1,commit=PIN,ir_sha256=sha(IR),sources=d['sources'],semantics=d['semantics'],unsupported=d['unsupported'],unverified=['Tests not run','3DS integrated build/render pending','Emulator/hardware source visual parity pending']));return d

def load():
 d=validate(read(IR));r=read(ROOT/'reports/field-tint/source-review.json');require(r['schema']==1 and r['commit']==PIN and r['ir_sha256']==sha(IR)and r['sources']==d['sources'],'Tint source review');ex=Extractor(ROOT)
 for p,h in d['sources'].items():ex.data(p);require(ex.sources[p]==h,'Changed tint source '+p)
 return d

def encode(d):
 validate(d);out=bytearray(64)
 def text(s):raw=s.encode();out.extend(struct.pack('<I',len(raw)));out.extend(raw)
 out.extend(struct.pack('<4f',*d['default_tint']))
 for r in d['records']:
  out.extend(struct.pack('<5I',r['id'],r['scene_id'],r['ready_ordinal'],r['kind'],len(r['targets'])));text(r['scene']);text(r['node'])
  for t in r['targets']:
   out.extend(struct.pack('<II4f',t['source_id'],int(t['exists']),*t['initial_self_modulate']));text(t['node_path']);text(t['node']);text(t['kind'])
 out.extend(struct.pack('<I',len(d['sources'])))
 for p,h in d['sources'].items():text(p);out.extend(bytes.fromhex(h))
 struct.pack_into('<8s6I20sII4x',out,0,b'ENCTINT1',1,len(out),0,1,1,len(d['records']),bytes.fromhex(PIN),d['scene_id'],len(d['sources']));struct.pack_into('<I',out,16,zlib.crc32(out)&0xffffffff);return bytes(out)

def stage_files(source):
 raw=encode(load());require((Path(source)/'data/podunk-tint.enctint').read_bytes()==raw,'Stale field tint binary');return{Path('data/podunk-tint.enctint'):raw}

def main():
 p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile','verify']);p.add_argument('--native',type=Path);p.add_argument('--source',type=Path);a=p.parse_args()
 if a.action=='extract':require(a.native and a.source,'Official lossless native/source exports required');d=extract(a.native,a.source);print('Full Podunk tint:',sum(r['kind']==1 for r in d['records']),'static child Ready records plus enemy/actor prototypes');return
 raw=encode(load())
 if a.action=='compile':PACK.parent.mkdir(parents=True,exist_ok=True);PACK.write_bytes(raw)
 else:require(PACK.read_bytes()==raw,'Stale tint binary')
 print('Field tint:',len(raw),'checked bytes; ordered self_modulate/signal source consumer')
if __name__=='__main__':
 try:main()
 except(ValueError,KeyError,OSError,TypeError,struct.error,subprocess.SubprocessError)as e:sys.exit('FIELD TINT ERROR: '+str(e))
