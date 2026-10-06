#!/usr/bin/env python3
"""Checked original global source namespace and actual persistent Canvas recipe.
This admits no autoload script constructor/Ready and contains no runtime JSON.
"""
from __future__ import annotations
import argparse,hashlib,json,re,struct,sys,zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import PIN,read,write,sha,require,decode
from tools.field_node_recipe import CLASSES,numbers
IR=ROOT/'content/field-global-registry.json';REVIEW=ROOT/'compatibility/reviews/field-global-registry-v0410.json';OUT=ROOT/'romfs/data/global.encregistry';CANVAS='Nodes/Ui/mainCanvasLayer.tscn'
def stable(source,node):return int.from_bytes(hashlib.sha256(('registry:'+source+'#'+node).encode()).digest()[:4],'little')
def source_record(name,filename,ordinal,kind,inv,sources):
 raw=(ROOT/'upstream/MOTHER-Encore'/filename).read_bytes();require(sha(ROOT/'upstream/MOTHER-Encore'/filename)==inv[filename]['sha256'],'Changed registry source');sources[filename]=inv[filename]['sha256'];text=raw.decode('utf-8');script='';script_sha='0'*64
 if filename.endswith('.gd'):
  match=re.search(r'^extends (Node|Node2D|CanvasLayer|Control)\s*$',text,re.M);require(match,'Unknown registry native script superclass');native=match[1];script=filename;script_sha=sources[filename]
 else:
  from tools.scene_reference import external
  node=re.search(r'^\[node name="[^"\n]+" type="([^"\n]+)"\]',text,re.M);require(node,'Registry scene root type unsupported');native=node[1];tail=text[node.end():];tail=tail[:tail.index('\n[')]if'\n['in tail else tail;assign=re.search(r'^script = ExtResource\( ([0-9]+) \)',tail,re.M)
  if assign:
   refs=[external(line)for line in text.splitlines()if line.startswith('[ext_resource')];s=next(r for r in refs if r['id']==int(assign[1]));require(s['type']=='Script','Registry root script type');script=s['path'];sources[script]=inv[script]['sha256'];require(sha(ROOT/'upstream/MOTHER-Encore'/script)==sources[script],'Changed registry root script');script_sha=sources[script]
 return dict(id=stable('project.godot',name),name=name,kind=kind,ordinal=ordinal,path=filename,native_class=native,source_sha256=sources[filename],script=script,script_sha256=script_sha)
def function_proof(path,name,sources,inv):
 text=(ROOT/'upstream/MOTHER-Encore'/path).read_text(encoding='utf-8');require(sha(ROOT/'upstream/MOTHER-Encore'/path)==inv[path]['sha256'],'Changed global function source');sources[path]=inv[path]['sha256'];match=re.search(r'^func '+re.escape(name)+r'\([^\n]*\n.*?(?=^func |\Z)',text,re.S|re.M);require(match,'Missing global source function '+name);return {'source':path,'method':name,'sha256':hashlib.sha256(match[0].encode()).hexdigest(),'line':text[:match.start()].count('\n')+1}
def extract(native,tree,receipt,object_source,node_source,scene_tree_source,message_queue_source):
 inv=read(ROOT/'compatibility/upstream-inventory.json')['files'];d=read(native);a=read(tree);proof=read(receipt);require(d['source']=='res://'+CANVAS and d['native_compatible']is False and a['scene']==CANVAS and a['scene_entered']is False and len(d['nodes'])==len(a['nodes'])==1 and len(d['scene_states'])==1 and not d['resources']and proof['scene']==CANVAS and proof['commit']==PIN,'Original complete mainCanvas source required');require([d['godot'].get(k)for k in ['major','minor','patch','status']]==[3,6,2,'stable'],'Unknown canvas engine');sources={}
 for f,r in proof['files'].items():require(sha(ROOT/'upstream/MOTHER-Encore'/f)==r['sha256']==inv[f]['sha256'],'Changed canvas source');sources[f]=r['sha256']
 n=a['nodes'][0];props=decode(d['nodes'][0]['properties']);require(n['path']=='.'and n['class']=='CanvasLayer'and n['parent']==n['owner']==''and not n['canvas']and props['script']is None,'Unknown mainCanvas original node');c=n['canvas_layer'];require(not c['custom_viewport'],'Unknown mainCanvas customViewport');canvas=dict(id=int.from_bytes(hashlib.sha256(('recipe:'+CANVAS+'#.').encode()).digest()[:4],'little'),name=n['name'],class_index=CLASSES.index('CanvasLayer'),pause=n['pause'],priority=n['priority'],groups=n['groups'],layer=c['layer'],visible=props['visible'],follow_viewport=c['follow_viewport'],follow_scale=float(c['follow_scale']),transform=numbers(c['transform']),offset=numbers(c['offset']),rotation=float(c['rotation']),scale=numbers(c['scale']),source_sha256=sources[CANVAS]);require(canvas['transform']==props['transform']and canvas['layer']==props['layer'],'Native mainCanvas source mismatch')
 project=(ROOT/'upstream/MOTHER-Encore/project.godot').read_text(encoding='utf-8');require(sha(ROOT/'upstream/MOTHER-Encore/project.godot')==inv['project.godot']['sha256'],'Changed project source');sources['project.godot']=inv['project.godot']['sha256'];section=re.search(r'^\[autoload\]\n(.*?)(?=^\[|\Z)',project,re.S|re.M)[1];autos=[]
 for line in section.splitlines():
  if not line.strip():continue
  m=re.fullmatch(r'([A-Za-z_][A-Za-z_0-9]*)="(\*?)res://([^"\n]+)"',line);require(m,'Unknown autoload syntax');autos.append(source_record(m[1],m[3],len(autos),2 if m[2]else 1,inv,sources))
 require(len(autos)==11 and len({r['name']for r in autos})==11,'Changed original autoload roster requires review');main=re.search(r'^run/main_scene="res://([^"\n]+)"',project,re.M)[1];sources[main]=inv[main]['sha256'];require(sha(ROOT/'upstream/MOTHER-Encore'/main)==sources[main],'Changed startup main scene')
 functions=[function_proof(p,m,sources,inv)for p,m in [('Scripts/global/global.gd','_init_player'),('Scripts/global/global.gd','add_persistent'),('Scripts/global/global.gd','get_current_scene_player_node'),('Scripts/global/uiManager.gd','_ready'),('Scripts/global/uiManager.gd','add_ui'),('Scripts/global/SceneTransition.gd','_deferred_goto_scene')]]
 ui=(ROOT/'upstream/MOTHER-Encore/Scripts/global/uiManager.gd').read_text(encoding='utf-8');require('_stable_canvas_layer = load("res://'+CANVAS+'").instance()\n\tglobal.currentScene.add_child(_stable_canvas_layer)\n\tglobal.add_persistent(_stable_canvas_layer)'in ui,'Changed source stableCanvas creation sequence');require('_stable_canvas_layer.call_deferred("add_child", ui)'in ui,'Changed source deferred UI add_child');g=(ROOT/'upstream/MOTHER-Encore/Scripts/global/global.gd').read_text(encoding='utf-8');require('currentScene.get_node_or_null("YSort" if currentScene.has_node("YSort") else "Objects")'in g,'Changed player/persistent parent selector')
 obj=object_source.read_text(encoding='utf-8');node=node_source.read_text(encoding='utf-8');st=scene_tree_source.read_text(encoding='utf-8');require('ObjectID ObjectDB::instance_counter = 1;'in obj and 'ObjectID instance_id = ++instance_counter;'in obj and 'node_hrcr_count.init(1);'in node and 'root->set_name("root");'in st,'Unreviewed native object/name/root counter source')
 mq=message_queue_source.read_text(encoding='utf-8');require('SWAP(read_buffer, write_buffer);'in mq and 'while (buffers[read_buffer].data.size())'in mq and 'ObjectDB::get_instance(message->instance_id)'in mq,'Unreviewed global native deferred queue source')
 engine_sources={'core/message_queue.cpp':sha(message_queue_source),'core/object.cpp':sha(object_source),'scene/main/node.cpp':sha(node_source),'scene/main/scene_tree.cpp':sha(scene_tree_source)}
 write(IR,dict(schema=1,kind='encore.field-global-registry.source-ir',commit=PIN,scene='project.godot',scene_id=stable('project.godot','root'),source_sha256=sources['project.godot'],sources=sources,root_name='root',root_native='Viewport',kernel_native='SceneTree',initial_object_counter=1,initial_fast_name_counter=1,main_scene=main,player_parents=['YSort','Objects'],autoloads=autos,ui_autoload=next(a['id']for a in autos if a['script']=='Scripts/global/uiManager.gd'),global_autoload=next(a['id']for a in autos if a['script']=='Scripts/global/global.gd'),functions=functions,engine_commit='3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8',engine_sources=engine_sources,canvas_scene=CANVAS,canvas=canvas,native_sha256=sha(native),tree_export_sha256=sha(tree),source_receipt_sha256=sha(receipt),exporter_sha256=sha(ROOT/'tools/godot_exporter/field_global_registry.gd'),scene_admitted=False,pending=['Autoload constructors/Ready require actual typed source owners; namespace declaration is not execution','UiManager source onready7 and randomize/menu_flavors/battlebg preceding mainCanvas create must actually complete','External Viewport actual 3DS dimensions/canvas/world binding must come from live native owner','SceneTransition player/leave/free/followers/flags/waits still require complete mapped source consumer','Persistent same-ObjectID subtree transfer implemented; SceneTransition source ordering/await and typed native owner dispatch still require actual host']))
def load():
 d=read(IR);r=read(REVIEW);inv=read(ROOT/'compatibility/upstream-inventory.json')['files'];require(d['schema']==1 and d['commit']==PIN and d['scene']=='project.godot'and d['scene_admitted']is False and r['ir_sha256']==sha(IR)and r['commit']==PIN and d['exporter_sha256']==sha(ROOT/'tools/godot_exporter/field_global_registry.gd'),'Global registry review/source mismatch')
 for p,h in d['sources'].items():require(h==inv[p]['sha256']==sha(ROOT/'upstream/MOTHER-Encore'/p),'Changed global registry source '+p)
 return d

def canvas_recipe(d):
 c=d['canvas'];b=bytearray(128)
 def u(*v):b.extend(struct.pack('<'+'I'*len(v),*v))
 def i(*v):b.extend(struct.pack('<'+'i'*len(v),*v))
 def f(*v):b.extend(struct.pack('<'+'f'*len(v),*v))
 def t(s):v=s.encode();u(len(v));b.extend(v)
 t(d['canvas_scene']);u(len(CLASSES));[t(v)for v in CLASSES];u(c['id'],0,0,0,c['class_index'],0,c['pause'],0,0,0);i(-1,c['priority'],0);f(1,0,0,1,0,0,1,0,0,1,0,0,1,1,1,1,1,1,1,1);t('.');t(c['name']);t('');b.extend(bytes(32));u(len(c['groups']));[t(v)for v in c['groups']];u(0);u(1);t(d['canvas_scene']);b.extend(bytes.fromhex(c['source_sha256']));u(1,c['id']);i(c['layer']);u(int(c['follow_viewport']),0,0,int(c['visible']));f(c['follow_scale'],*[v for row in c['transform']for v in row],*c['offset'],c['rotation'],*c['scale']);u(0)
 struct.pack_into('<8s8I',b,0,b'ENCFNRC1',1,128,len(b),0,0x454e003d,3,1,c['id']);b[40:60]=bytes.fromhex(PIN);b[60:92]=bytes.fromhex(c['source_sha256']);b[92:124]=hashlib.sha256(json.dumps(c,sort_keys=True).encode()).digest();struct.pack_into('<I',b,20,zlib.crc32(b[128:]));return bytes(b)
def encode(d):
 b=bytearray(128)
 def u(*v):b.extend(struct.pack('<'+'I'*len(v),*v))
 def t(s):v=s.encode();u(len(v));b.extend(v)
 t(d['scene']);t(d['root_name']);t(d['root_native']);t(d['kernel_native']);b.extend(struct.pack('<QQ',d['initial_object_counter'],d['initial_fast_name_counter']));u(d['ui_autoload'],d['global_autoload']);t(d['main_scene']);t(d['canvas_scene']);u(d['canvas']['id']);b.extend(bytes.fromhex(d['canvas']['source_sha256']));u(len(d['player_parents']));[t(v)for v in d['player_parents']];u(len(d['autoloads']))
 for a in d['autoloads']:u(a['id'],a['kind'],a['ordinal']);[t(a[k])for k in ['name','path','native_class','script']];b.extend(bytes.fromhex(a['source_sha256']));b.extend(bytes.fromhex(a['script_sha256']))
 u(len(d['functions']))
 for a in d['functions']:t(a['source']);t(a['method']);u(a['line']);b.extend(bytes.fromhex(a['sha256']))
 u(len(d['sources']))
 for p,h in d['sources'].items():t(p);b.extend(bytes.fromhex(h))
 t(d['engine_commit']);u(len(d['engine_sources']))
 for p,h in d['engine_sources'].items():t(p);b.extend(bytes.fromhex(h))
 nested=canvas_recipe(d);u(len(nested));b.extend(nested)
 struct.pack_into('<8s8I',b,0,b'ENCFGRG1',1,128,len(b),0,0x454e0042,3,len(d['autoloads']),d['scene_id']);b[40:60]=bytes.fromhex(PIN);b[60:92]=bytes.fromhex(d['source_sha256']);b[92:124]=hashlib.sha256(IR.read_bytes()).digest();struct.pack_into('<I',b,20,zlib.crc32(b[128:]));return bytes(b)
def stage_files(source):
 raw=encode(load());require((Path(source)/'data/global.encregistry').read_bytes()==raw,'Staged global registry differs');return {Path('data/global.encregistry'):raw}
def main():
 p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile','verify']);[p.add_argument('--'+k,type=Path)for k in ['native','tree','source','object-source','node-source','scene-tree-source','message-queue-source']];a=p.parse_args()
 if a.action=='extract':require(all(getattr(a,k)for k in ['native','tree','source','object_source','node_source','scene_tree_source','message_queue_source']),'Explicit complete source exports and fixed official engine files required');extract(a.native,a.tree,a.source,a.object_source,a.node_source,a.scene_tree_source,a.message_queue_source);return
 raw=encode(load())
 if a.action=='verify':require(OUT.read_bytes()==raw,'Stale global registry output')
 else:OUT.parent.mkdir(parents=True,exist_ok=True);OUT.write_bytes(raw)
 print('Global registry source:',len(raw),'bytes; autoload/script Ready pending')
if __name__=='__main__':
 try:main()
 except(ValueError,KeyError,TypeError,OSError,struct.error)as e:sys.exit('FIELD GLOBAL REGISTRY ERROR: '+str(e))
