#!/usr/bin/env python3
"""Pinned native SceneTree/Viewport source policy; not autoload Ready approval."""
import argparse,hashlib,json,struct,zlib,sys,re
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from tools.podunk_scene import PIN,read,write,sha,require
IR=ROOT/'content/field-native-root.json'
REVIEW=ROOT/'compatibility/reviews/field-native-root-v0410.json'
OUT=ROOT/'romfs/data/global.encnativeroot'
ENGINE='3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8'
FILES=['scene/main/scene_tree.cpp','scene/main/node.cpp','scene/main/viewport.cpp','scene/2d/canvas_item.cpp','scene/main/canvas_layer.cpp','main/main.cpp']
MARKERS={FILES[0]:['root = memnew(Viewport);','root->set_name("root");','root->set_handle_input_locally(false);','root->_set_tree(this);','ERR_FAIL_COND(p_scene && p_scene->get_parent() != root);'],FILES[1]:['void Node::_propagate_ready()','void Node::add_child(Node *p_child, bool p_force_readable_name)','void Node::remove_child(Node *p_child)'],FILES[2]:['world_2d = Ref<World2D>(memnew(World2D));','VisualServer::get_singleton()->viewport_attach_canvas(viewport, current_canvas);','find_world_2d()->_register_viewport(this, Rect2());','set_physics_process_internal(true);'],FILES[3]:['void CanvasItem::_notification(int p_what)'],FILES[4]:['void CanvasLayer::_notification(int p_what)'],FILES[5]:['to_add.push_back(n);','sml->get_root()->add_child(E->get());','sml->add_current_scene(scene);']}
def extract(directory):
 registry=read(ROOT/'content/field-global-registry.json');inventory=read(ROOT/'compatibility/upstream-inventory.json')['files'];project=ROOT/'upstream/MOTHER-Encore/project.godot';require(sha(project)==inventory['project.godot']['sha256'],'Changed original project')
 sources={}
 for path in FILES:
  source=directory/path.replace('/','-');body=source.read_text(encoding='utf-8');require(all(x in body for x in MARKERS[path]),'Unknown native root source '+path);sources[path]=sha(source)
 for path in FILES[:2]:require(sources[path]==registry['engine_sources'][path],'Different reviewed native registry source')
 match=re.search(r'^environment/default_clear_color=Color\( ([^\n]+) \)$',project.read_text(encoding='utf-8'),re.M);require(match,'Original clear color missing');color=[float(x)for x in match[1].split(', ')];require(len(color)==4 and all(0<=x<=1 for x in color),'Unknown clear color')
 d=dict(schema=1,kind='encore.native-root.source-ir',commit=PIN,engine_commit=ENGINE,scene='project.godot',scene_id=registry['scene_id'],source_sha256=sha(project),registry_ir_sha256=sha(ROOT/'content/field-global-registry.json'),engine_sources=sources,root_name=registry['root_name'],root_native=registry['root_native'],kernel_native=registry['kernel_native'],clear_color=color,handle_input_locally=False,audio_listener=True,audio_listener_2d=True,viewport_internal_physics=True,adapter_scope=['actual ObjectDB SceneTree/Viewport owners','source ordered root add/remove/move child with actual Tree lifecycle','actual SceneTree.current_scene separate from global.currentScene','Citro2D target and affine root Canvas','actual viewport input registration'],pending=['Unknown autoload/native child lifecycle fails closed','3D scene/camera/listener/rendering require separate source consumer','Viewport GUI tooltip/object-picking/internal physics traversal require actual adapter','SceneTree pause notifications require complete source native dispatcher','Original Main::start stages all 11 autoloads and main scene before whole-root Enter/Ready; per-autoload immediate Ready is not original cold startup'])
 write(IR,d)
 write(REVIEW,dict(schema=1,kind='encore.native-root.semantic-review',commit=PIN,engine_commit=ENGINE,ir_sha256=sha(IR),source_scope=d['adapter_scope'],engine_sources=sources,capability=1,whole_scene_admitted=False))
def load():
 d=read(IR);review=read(REVIEW);registry=read(ROOT/'content/field-global-registry.json');require(d['schema']==1 and d['commit']==PIN and d['engine_commit']==ENGINE and review['ir_sha256']==sha(IR) and review['engine_sources']==d['engine_sources'] and d['registry_ir_sha256']==sha(ROOT/'content/field-global-registry.json'),'Native root source/review mismatch');require(d['source_sha256']==sha(ROOT/'upstream/MOTHER-Encore/project.godot') and all(d['engine_sources'][p]==registry['engine_sources'][p]for p in FILES[:2]),'Native root source binding mismatch');return d
def encode(d):
 b=bytearray(128)
 def u(*v):b.extend(struct.pack('<'+'I'*len(v),*v))
 def t(s):v=s.encode();u(len(v));b.extend(v)
 t(d['scene']);t(d['root_name']);t(d['root_native']);t(d['kernel_native']);b.extend(bytes.fromhex(d['engine_commit']));b.extend(bytes.fromhex(d['registry_ir_sha256']));u(14);b.extend(struct.pack('<4f',*d['clear_color']));u(len(FILES))
 for p in FILES:t(p);b.extend(bytes.fromhex(d['engine_sources'][p]))
 struct.pack_into('<8s8I',b,0,b'ENCFNVP1',1,128,len(b),0,0x454e004c,1,1,d['scene_id']);b[40:60]=bytes.fromhex(PIN);b[60:92]=bytes.fromhex(d['source_sha256']);b[92:124]=bytes.fromhex(sha(IR));struct.pack_into('<I',b,20,zlib.crc32(b[128:]));return bytes(b)
def main():
 p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile','verify']);p.add_argument('--engine',type=Path);a=p.parse_args()
 if a.action=='extract':require(a.engine,'Explicit fixed native source files required');extract(a.engine);return
 raw=encode(load())
 if a.action=='compile':OUT.parent.mkdir(parents=True,exist_ok=True);OUT.write_bytes(raw)
 else:require(OUT.read_bytes()==raw,'Stale native root resource')
 print('Native root checked source:',len(raw),'bytes; unknown native/script lifecycle not admitted')
if __name__=='__main__':
 try:main()
 except(ValueError,KeyError,TypeError,OSError,struct.error)as e:sys.exit('NATIVE ROOT ERROR: '+str(e))
