#!/usr/bin/env python3
"""Complete original UiManager preload recipes, immutable native resource graphs."""
from pathlib import Path
import argparse,hashlib,json,struct,sys,zlib
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import PIN,read,write,sha,require,decode
from tools.field_ui_preload_reference import roster
import tools.field_ui_manager_recipes as recipes
from tools.field_battle_bg_resources import value
CLASSES=recipes.CLASSES+['WorldEnvironment','ParallaxBackground','ParallaxLayer']
IR=ROOT/'content/field-ui-preloads.json';REVIEW=ROOT/'compatibility/reviews/field-ui-preloads-v0410.json';OUT=ROOT/'romfs/data/global.encuipreloads'
SOURCE='Scripts/global/uiManager.gd';DIALOGUE=ROOT/'content/dialogue-node-recipe.json'
def extract(directory):
 directory=Path(directory);entries=[];inv=read(ROOT/'compatibility/upstream-inventory.json')['files'];sources={SOURCE:sha(ROOT/'upstream/MOTHER-Encore'/SOURCE)};require(sources[SOURCE]==inv[SOURCE]['sha256'],'Changed UI source')
 for index,p in enumerate(roster()):
  if p['path']=='Nodes/Ui/DialogueBox.tscn':
   d=read(DIALOGUE);require(d['scene']==p['path']and d['commit']==PIN and len(d['records'])==47 and d['source_sha256']==p['sha'],'Actual Dialogue owner proof changed');entries.append(dict(preload=p,borrowed=True,scene_id=d['scene_id'],node_count=47,recipe_ir_sha256=sha(DIALOGUE),dependency='content/dialogue-node-recipe.json'));sources.update(d['sources']);continue
  root=directory/('scene-'+str(index));old=recipes.CLASSES
  try:
   recipes.CLASSES=CLASSES;d=recipes.extract_recipe(p['path'],root/'native.json',root/'structure.json',root/'source.json')
  finally:recipes.CLASSES=old
  native=read(root/'native.json');sourceproof=read(root/'source.json');native['original_signal_connections']=sourceproof['signal_connections'];native['original_script_attachments']=sourceproof['script_attachments'];native['original_source_files']=sourceproof['files'];structure=read(root/'structure.json');by={n['path']:n for n in native['nodes']};recipes.SCENE=p['path']
  for n in structure['nodes']:
   if n['class']=='ParallaxBackground':
    c=n['canvas_layer'];props=decode(by[n['path']]['properties']);require(not c['custom_viewport']and props.get('custom_viewport')is None,'Source Parallax custom viewport pending');c={k:recipes.numbers(v)if isinstance(v,list)else recipes.scalar(v)if k in ['rotation','follow_scale']else v for k,v in c.items()};require(c['transform']==props['transform']and c['layer']==props['layer'],'Parallax native layer proof');d['canvas_layers'].append(dict(id=recipes.stable(n['path']),visible=props['visible'],world_2d_binding=0,**c))
  d['classes']=CLASSES;d['native_exporter']='tools/godot_exporter/field_ui_preload_native.gd'if 'tools/godot_exporter/field_ui_preload_native.gd'in sourceproof['tools']else'tools/godot_exporter/scene_data.gd';d['preload_exporter_sha256']=sourceproof['tools'][d['native_exporter']];require(d['preload_exporter_sha256']==sha(ROOT/d['native_exporter']),'Native exporter source proof changed');canonical=hashlib.sha256(json.dumps(d,ensure_ascii=False,sort_keys=True,separators=(',',':')).encode()).hexdigest();entries.append(dict(preload=p,borrowed=False,scene_id=d['scene_id'],node_count=len(d['records']),recipe_ir_sha256=canonical,recipe=d,native_graph=native));sources.update(d['sources'])
 write(IR,dict(schema=1,kind='encore.field-ui-preloads.source-ir',commit=PIN,scene=SOURCE,scene_id=int.from_bytes(hashlib.sha256(('ui-preloads:'+SOURCE).encode()).digest()[:4],'little'),source_sha256=sources[SOURCE],sources=sources,ui_manager_ir_sha256=sha(ROOT/'content/field-ui-manager.json'),dialogue_ir_sha256=sha(DIALOGUE),classes=CLASSES,entries=entries,scene_admitted=False,pending=['Immutable full PackedScene loading does not grant native/script Ready','WorldEnvironment/Parallax source values retained; native rendering/lifecycle adapter remains pending','Dialogue47 actual owner is borrowed with exact source/pin/IR identity; never duplicate factory resource']))
def load():
 d=read(IR);r=read(REVIEW);inv=read(ROOT/'compatibility/upstream-inventory.json')['files'];require(d['schema']==1 and d['commit']==PIN and d['classes']==CLASSES and not d['scene_admitted']and len(d['entries'])==12 and r['ir_sha256']==sha(IR)and r['commit']==PIN and d['ui_manager_ir_sha256']==sha(ROOT/'content/field-ui-manager.json')and d['dialogue_ir_sha256']==sha(DIALOGUE),'UI preload source review rejected');require([r['preload']for r in d['entries']]==roster(),'Complete original source preload roster changed')
 for f,h in d['sources'].items():require(h==inv[f]['sha256']==sha(ROOT/'upstream/MOTHER-Encore'/f),'Changed preload source '+f)
 for e in d['entries']:
  if not e['borrowed']:require(e['recipe']['preload_exporter_sha256']==sha(ROOT/e['recipe']['native_exporter'])and e['recipe']['exporter_sha256']==sha(ROOT/'tools/godot_exporter/field_ui_manager.gd'),'Preload exact source exporter changed')
 return d
def encode(d):
 b=bytearray(128)
 def u(*v):b.extend(struct.pack('<'+'I'*len(v),*v))
 def t(s):v=s.encode();u(len(v));b.extend(v)
 t(d['scene']);u(len(d['sources']))
 for f,h in d['sources'].items():t(f);b.extend(bytes.fromhex(h))
 u(len(d['entries']))
 for e in d['entries']:
  p=e['preload'];u(p['id'],int(e['borrowed']),e['scene_id'],e['node_count']);t(p['name']);t(p['path']);t(p['native']);b.extend(bytes.fromhex(p['sha']));b.extend(bytes.fromhex(e['recipe_ir_sha256']))
  if e['borrowed']:u(0,0);continue
  raw=bytearray(recipes.encode_recipe(e['recipe']));struct.pack_into('<I',raw,28,5);u(len(raw));b.extend(raw);raw=value(e['native_graph']);u(len(raw));b.extend(raw)
 struct.pack_into('<8s8I',b,0,b'ENCFUPR1',1,128,len(b),0,0x454e004b,1,len(d['entries']),d['scene_id']);b[40:60]=bytes.fromhex(PIN);b[60:92]=bytes.fromhex(d['source_sha256']);b[92:124]=hashlib.sha256(IR.read_bytes()).digest();struct.pack_into('<I',b,20,zlib.crc32(b[128:]));return bytes(b)
def stage_files(source):
 raw=encode(load());require((Path(source)/'data/global.encuipreloads').read_bytes()==raw,'Staged preloads differ');return{Path('data/global.encuipreloads'):raw}
def main():
 p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile','verify']);p.add_argument('--native-directory',type=Path);a=p.parse_args()
 if a.action=='extract':require(a.native_directory,'Explicit complete11 native directory required');extract(a.native_directory);return
 raw=encode(load())
 if a.action=='verify':require(OUT.read_bytes()==raw,'Stale UI preload resource')
 else:OUT.parent.mkdir(parents=True,exist_ok=True);OUT.write_bytes(raw)
 print('Complete original12 UI preloads:',len(raw),'bytes; actual Dialogue47 owner borrowed; native Ready remains pending')
if __name__=='__main__':
 try:main()
 except(ValueError,KeyError,TypeError,OSError,struct.error)as e:sys.exit('UI PRELOAD ERROR: '+str(e))
