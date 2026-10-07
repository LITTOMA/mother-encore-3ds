#!/usr/bin/env python3
"""Original dialogue Actor preload: complete checked native graph, no instance."""
from pathlib import Path
import argparse, hashlib, json, re, struct, sys, zlib
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import PIN, read, write, sha, require
import tools.field_ui_manager_recipes as recipes
from tools.field_battle_bg_resources import value
IR=ROOT/'content/native-dialogue-actor-resource.json'
REVIEW=ROOT/'reports/dialogue-actor-resource/source-review.json'
OUT=ROOT/'romfs/data/dialogue.encactorrecipe'
ROOTSCRIPT='Nodes/Ui/DialogueBox.tscn::19'

def extract(directory):
 directory=Path(directory)
 root=read(ROOT/'content/native-field-dialogue-root-script.json')
 # The audited Root source determines the resource path. Never guess a scene.
 script=root.get('script',ROOTSCRIPT)
 f=ROOT/'upstream/MOTHER-Encore/Nodes/Ui/DialogueBox.tscn'
 script_file=ROOT/'upstream/MOTHER-Encore/Scripts/UI/DialogueBox.gd'
 match=re.search(r'onready var (\w+)\s*:=\s*preload\("res://([^\"]+)"\)',script_file.read_text(encoding='utf-8'))
 require(match and match[2]==root['constructor_resource'],'Actual source Actor preload changed');scene=match[2]
 require(root['commit']==PIN and root['scene_sha256']==sha(f),'Original Actor preload declaration source changed')
 d=recipes.extract_recipe(scene,directory/'native.json',directory/'structure.json',directory/'source.json')
 native=read(directory/'native.json');proof=read(directory/'source.json')
 native['original_signal_connections']=proof['signal_connections']
 native['original_script_attachments']=proof['script_attachments']
 native['original_source_files']=proof['files']
 sources=dict(d['sources']);relative=f.relative_to(ROOT/'upstream/MOTHER-Encore').as_posix();sources[relative]=sha(f);sources[script_file.relative_to(ROOT/'upstream/MOTHER-Encore').as_posix()]=sha(script_file)
 write(IR,dict(schema=1,kind='encore.dialogue-actor-resource.source-ir',commit=PIN,scene=scene,scene_id=d['scene_id'],source_sha256=d['source_sha256'],sources=sources,resource_name=match[1],recipe=d,native_graph=native,scene_admitted=False,instance_admitted=False))
 write(REVIEW,dict(schema=1,commit=PIN,ir_sha256=sha(IR),capability=1,review=['Complete original native nodes, properties, resources and SceneStates retained','Original scripts and signal connections retained in immutable native graph and recipe','Source preload only; Actor.instance and every native/script lifecycle remain unapproved'],source_receipt_sha256=sha(directory/'source.json'),native_sha256=sha(directory/'native.json'),tree_sha256=sha(directory/'structure.json')))

def load():
 d=read(IR);r=read(REVIEW);inv=read(ROOT/'compatibility/upstream-inventory.json')['files']
 require(d['schema']==1 and d['commit']==PIN and not d['scene_admitted'] and not d['instance_admitted'] and r['commit']==PIN and r['ir_sha256']==sha(IR),'Actor source review rejected')
 for p,h in d['sources'].items():require(h==inv[p]['sha256']==sha(ROOT/'upstream/MOTHER-Encore'/p),'Actor source changed '+p)
 require(d['recipe']['scene']==d['scene'] and d['recipe']['source_sha256']==d['source_sha256'] and len(d['recipe']['records'])==len(d['native_graph']['nodes']) and d['recipe']['exporter_sha256']==sha(ROOT/'tools/godot_exporter/field_ui_manager.gd'),'Actor full recipe/exporter rejected')
 return d

def encode(d):
 b=bytearray(128)
 def u(*v):b.extend(struct.pack('<'+'I'*len(v),*v))
 def t(v):raw=v.encode();u(len(raw));b.extend(raw)
 t(d['scene']);t(d['resource_name']);u(len(d['sources']))
 for p,h in d['sources'].items():t(p);b.extend(bytes.fromhex(h))
 recipe=recipes.encode_recipe(d['recipe']);u(len(recipe));b.extend(recipe)
 graph=value(d['native_graph']);u(len(graph));b.extend(graph)
 struct.pack_into('<8s8I',b,0,b'ENCDACT1',1,128,len(b),0,0x454e0074,1,len(d['recipe']['records']),d['scene_id'])
 b[40:60]=bytes.fromhex(PIN);b[60:92]=bytes.fromhex(d['source_sha256']);b[92:124]=hashlib.sha256(IR.read_bytes()).digest();struct.pack_into('<I',b,20,zlib.crc32(b[128:]));return bytes(b)

def stage_files(source):
 raw=encode(load());require((Path(source)/'data/dialogue.encactorrecipe').read_bytes()==raw,'Stale Actor PackedScene');return {Path('data/dialogue.encactorrecipe'):raw}

def main():
 p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile','verify']);p.add_argument('--native-directory',type=Path);a=p.parse_args()
 if a.action=='extract':require(a.native_directory,'Explicit source-only original export required');extract(a.native_directory);return
 raw=encode(load())
 if a.action=='compile':OUT.parent.mkdir(parents=True,exist_ok=True);OUT.write_bytes(raw)
 else:require(OUT.read_bytes()==raw,'Stale Actor complete PackedScene')
 print('Complete checked Actor PackedScene:',len(raw),'bytes; instance remains unapproved')
if __name__=='__main__':
 try:main()
 except (OSError,ValueError,KeyError,TypeError,struct.error) as e:sys.exit('ACTOR RESOURCE ERROR: '+str(e))
