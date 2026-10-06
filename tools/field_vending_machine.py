#!/usr/bin/env python3
from __future__ import annotations
import argparse,json,re,struct,subprocess,sys,zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import read,decode,stable,SCENE,PIN,require,sha
from tools.extract_battle_entry import Extractor
from tools.field_shop import write
SCRIPT='Scripts/Main/VendingMachine.gd'
IR=ROOT/'content/native-field-vending-machine.json';REVIEW=ROOT/'reports/field-vending-machine/source-review.json';PACK=ROOT/'romfs/data/podunk.encvending';RECEIPT=ROOT/'content/asset-receipts/graphics/objects/vending-machine/source.json'
def source_instances(native,receipt,upstream):
 d=read(native);s=read(receipt);require(d['source']=='res://'+SCENE and d['schema']==1 and d['native_compatible'] is False,'Incomplete Vending native scene')
 require(s['commit']==PIN and s['scene']==SCENE,'Unreviewed Vending native receipt')
 inventory=read(ROOT/'compatibility/upstream-inventory.json')['files']
 for name,record in s['files'].items():require(sha(upstream/name)==record['sha256']==inventory[name]['sha256'],'Changed Vending closure '+name)
 names={n['path']:n for n in d['nodes']};require(len(names)==8686,'Incomplete Podunk enemy scene');require(decode(names['Objects']['world_transform'])==[[1,0],[0,1],[0,0]],'Vending Objects transform requires a source adapter')
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
   require(path in names,'Unresolved Vending binding '+path);bindings[path]=a['script']
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
  out.append(dict(stable_id=stable(path),node=path,ready_ordinal=ordinal[path],script=bindings[path],ancestor_bindings=[dict(node=p,script=bindings[p])for p in bindings if path.startswith(p+'/')],overrides=overrides[path],native=names[path],children=[names[p] for p in children[path]]))
 require(len(out)==1,'Podunk Vending binding loss');return out,names,resources,dict(native_export_sha256=sha(native),source_receipt_sha256=sha(receipt),source_files={name:r['sha256'] for name,r in s['files'].items()})

def build(records,nodes,resources,provenance):
 ex=Extractor(ROOT);script=ex.text(SCRIPT);require(re.findall(r'^func (\w+)\(',script,re.M)==['interact','_end'],'Unknown Vending source callbacks');require('uiManager.set_current_shop(shop)\n\tuiManager.open_dialogue_box(DIALOG)'in script and 'global.get_player().unpause()'in script,'Changed Vending interaction order');program=re.search(r'const DIALOG = "([^\"]+)"',script)[1];ex.yaml('Data/Dialogue/'+program+'.yaml');rec=records[0];p=rec['node'];shop=rec['overrides']['shop'];ex.yaml('Data/Shops/'+shop+'.yaml');source='Nodes/Reusables/VendingMachineExterior.tscn';scene=ex.text(source);require('[node name="interact_dialog" parent="." instance=ExtResource( 1 )]\nscript = null'in scene,'Vending interaction Area script is not intentionally null');sp=decode(nodes[p+'/main']['properties']);require(sp['material']is None and sp['centered']and sp['frame']==0 and sp['hframes']==sp['vframes']==1 and not sp['flip_h']and not sp['flip_v']and not sp['region_enabled']and sp['rotation']==0 and sp['scale']==[1,1],'Unsupported Vending Sprite variation');texture=resources[sp['texture']['id']]['path'][6:];ex.data(texture);require('flags/filter=false'in ex.text(texture+'.import'),'Vending source nearest filter');size=ex.png_size(texture);require(all(0<v<=1024 for v in size),'Vending source Sprite extent');geometry=read(ROOT/'content/podunk-scene-geometry.json');gn={r['path']:r for r in geometry['nodes']};require(geometry['commit']==PIN and gn[p]['script_sha256']==ex.sources[SCRIPT],'Vending geometry Script binding');n=nodes[p+'/main'];area=decode(nodes[p+'/interact_dialog']['properties']);require(area['collision_layer']==area['collision_mask']==1,'Vending source interaction mask variation');prompt=p+'/interact_dialog/ButtonPrompt';ex.text('Nodes/Ui/ButtonPrompt.tscn');ex.text('Scripts/UI/Button Prompt.gd');ex.text('LICENSE')
 for path,h in provenance['source_files'].items():ex.data(path);require(ex.sources[path]==h,'Changed Vending source closure')
 colors=[]
 for k in ['modulate','self_modulate']:colors.append([sp[k][q]for q in ['r','g','b','a']])
 d=dict(schema=1,kind='encore.field-vending-machine.source-ir',commit=PIN,scene=SCENE,scene_id=stable('.'),scene_sha256=ex.sources[SCENE],script_sha256=ex.sources[SCRIPT],id=rec['stable_id'],node=p,area_path=p+'/interact_dialog',body_path=p+'/StaticBody2D',shape_path=p+'/interact_dialog/CollisionShape2D',script_source=SCRIPT,ready_ordinal=rec['ready_ordinal'],sprite_id=stable(p+'/main'),area_id=stable(p+'/interact_dialog'),prompt_id=stable(prompt),body_id=stable(p+'/StaticBody2D'),shop=shop,program=program,root_position=decode(nodes[p]['properties'])['position'],sprite_position=sp['position'],sprite_offset=sp['offset'],centered=sp['centered'],visible=sp['visible'],z=sp['z_index'],z_relative=sp['z_as_relative'],canvas_order=list(nodes).index(p+'/main'),colors=colors,texture=dict(source=texture,path='graphics/objects/vending-machine.t3x',size=size),interaction=dict(layer=area['collision_layer'],mask=area['collision_mask'],shape_id=stable(p+'/interact_dialog/CollisionShape2D')),geometry_sha256=sha(ROOT/'content/podunk-scene-geometry.json'),sources=dict(sorted(ex.sources.items())),provenance=provenance,semantics=['Exactly one original Podunk VendingMachine; root Sprite no _ready callback','interact sets current shop then opens original dialogue, phrase1 actual null shop suspends/resumes existing dialogue callback','_end unpauses actual controlled player; does not write flags or fake shop success','Source main Sprite no shader; real nearest 1:1 PNG and single application of actual child local position/offset','Source null interact_dialog is geometry only; ancestor Vending typed interaction and existing checked ButtonPrompt own behavior','Source Prompt white flash/glow default modifier0 handled by checked existing FieldPrompt shader bridge'],unverified=['Manual tests not executed','Main/SceneHost/Shop integration pending','Emulator/hardware unverified'])
 validate(d);write(IR,d);write(REVIEW,dict(schema=1,commit=PIN,ir_sha256=sha(IR),sources=d['sources'],geometry_sha256=d['geometry_sha256'],semantics=d['semantics'],unverified=d['unverified']));return d
def validate(d):
 require(set(d)==set('schema kind commit scene scene_id scene_sha256 script_sha256 id node area_path body_path shape_path script_source ready_ordinal sprite_id area_id prompt_id body_id shop program root_position sprite_position sprite_offset centered visible z z_relative canvas_order colors texture interaction geometry_sha256 sources provenance semantics unverified'.split()),'Unknown/missing Vending source IR field')
 require(d['schema']==1 and d['kind']=='encore.field-vending-machine.source-ir'and d['commit']==PIN and d['scene']==SCENE and d['scene_id']==stable('.')and d['id']==stable(d['node'])and d['scene_sha256']==d['sources'][SCENE]and d['script_sha256']==d['sources'][SCRIPT],'Vending source identity');require(d['shop']=='zoo_vm'and d['program']=='Reusable/vendingmachine'and d['centered']is True and d['z_relative']is True and d['z']==0 and len(d['texture']['size'])==2,'Vending source scope');return d
def load():
 d=validate(read(IR));r=read(REVIEW);require(r['ir_sha256']==sha(IR)and r['commit']==PIN and r['sources']==d['sources']and r['geometry_sha256']==d['geometry_sha256']==sha(ROOT/'content/podunk-scene-geometry.json'),'Vending source review/geometry stale');ex=Extractor(ROOT)
 for p,h in d['sources'].items():ex.data(p);require(ex.sources[p]==h,'Changed Vending source '+p)
 return d
def encode(d,a):
 out=bytearray(64)
 def u(*v):out.extend(struct.pack('<'+'I'*len(v),*v))
 def f(*v):out.extend(struct.pack('<'+'f'*len(v),*v))
 def s(v):b=v.encode();u(len(b));out.extend(b)
 out.extend(bytes.fromhex(d['scene_sha256']));out.extend(bytes.fromhex(d['script_sha256']));u(*(d[k]for k in ['id','ready_ordinal','sprite_id','area_id','prompt_id','body_id','canvas_order']),int(d['centered']),int(d['visible']),d['z'],int(d['z_relative']),d['interaction']['layer'],d['interaction']['mask'],d['interaction']['shape_id']);f(*d['root_position'],*d['sprite_position'],*d['sprite_offset'],*d['colors'][0],*d['colors'][1]);u(*d['texture']['size'],a['bytes']);out.extend(bytes.fromhex(a['sha256']));
 for k in ['scene','node','shop','program','area_path','body_path','shape_path','script_source']:s(d[k])
 for k in ['source','path']:s(d['texture'][k])
 u(len(d['sources']))
 for p,h in d['sources'].items():s(p);out.extend(bytes.fromhex(h))
 struct.pack_into('<8s6I20sIII',out,0,b'ENCFVM01',1,len(out),0,1,1,1,bytes.fromhex(PIN),d['scene_id'],len(d['sources']),0);struct.pack_into('<I',out,16,zlib.crc32(out)&0xffffffff);return bytes(out)
def compile_assets(tool):
 d=load();require(tool and Path(tool).is_file(),'Actual Vending tex3ds required');p=ROOT/'romfs'/d['texture']['path'];p.parent.mkdir(parents=True,exist_ok=True);subprocess.run([str(tool),'-f','rgba8','-z','none','-o',str(p),str(ROOT/'upstream/MOTHER-Encore'/d['texture']['source'])],check=True);a=dict(schema=1,commit=PIN,ir_sha256=sha(IR),producer_sha256=sha(ROOT/'tools/field_vending_machine.py'),tex3ds_sha256=sha(tool),path=d['texture']['path'],bytes=p.stat().st_size,sha256=sha(p));write(RECEIPT,a);raw=encode(d,a);PACK.write_bytes(raw);return raw
def compile_pack():
 d=load();a=read(RECEIPT);p=ROOT/'romfs'/a['path'];require(a['commit']==PIN and a['ir_sha256']==sha(IR)and a['producer_sha256']==sha(ROOT/'tools/field_vending_machine.py')and a['bytes']==p.stat().st_size and a['sha256']==sha(p),'Vending genuine receipt stale');raw=encode(d,a);PACK.write_bytes(raw);return raw
def stage_files(source):
 d=load();a=read(RECEIPT);root=Path(source);raw=encode(d,a);p=root/a['path'];require((root/'data/podunk.encvending').read_bytes()==raw and p.stat().st_size==a['bytes']and sha(p)==a['sha256'],'Stale staged Vending actual resources');return{Path('data/podunk.encvending'):raw,Path(a['path']):p.read_bytes()}
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile','pack']);p.add_argument('--native',type=Path);p.add_argument('--source',type=Path);p.add_argument('--tex3ds',type=Path);a=p.parse_args()
 try:
  if a.action=='extract':r,n,s,proof=source_instances(a.native,a.source,ROOT/'upstream/MOTHER-Encore');d=build(r,n,s,proof);print(d['id'])
  else:print(len(compile_assets(a.tex3ds)if a.action=='compile'else compile_pack()))
 except(ValueError,KeyError,TypeError,OSError,struct.error,subprocess.CalledProcessError)as e:sys.exit('VENDING ERROR: '+str(e))
