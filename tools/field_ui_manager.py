#!/usr/bin/env python3
"""Original UI source factory recipes and initializer data, never blanket Ready."""
from __future__ import annotations
import argparse,hashlib,json,re,struct,sys,zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import PIN,read,write,sha,require
from tools.field_ui_manager_recipes import encode_recipe,CLASSES
IR=ROOT/'content/field-ui-manager.json';RECIPES=ROOT/'content/field-ui-manager-recipes.json';REVIEW=ROOT/'compatibility/reviews/field-ui-manager-v0410.json';OUT=ROOT/'romfs/data/global.encuimanager'
SOURCE='Scripts/global/uiManager.gd'
def stable(s):return int.from_bytes(hashlib.sha256(('ui-manager:'+s).encode()).digest()[:4],'little')
def extract():
 inv=read(ROOT/'compatibility/upstream-inventory.json')['files'];recipes=read(RECIPES);require(recipes['commit']==PIN and len(recipes['recipes'])==9 and not recipes['scene_admitted'],'UI complete source recipe roster changed');sources={}
 def source(f):
  h=sha(ROOT/'upstream/MOTHER-Encore'/f);require(h==inv[f]['sha256'],'Changed original UI source '+f);sources[f]=h;return(ROOT/'upstream/MOTHER-Encore'/f).read_text(encoding='utf-8')
 text=source(SOURCE);gd=source('Scripts/global/globalData.gd');shader=source('Shaders/MenuFlavors.tres');preloads=[]
 for m in re.finditer(r'^(onready )?(?:const|var) (\w+)(?:\s*:?=)\s*preload\("res://([^"\n]+)"\)',text,re.M):
  source(m[3]);preloads.append(dict(id=stable(m[2]),name=m[2],path=m[3],onready=bool(m[1]),sha=sources[m[3]],native='ShaderMaterial'if m[3].endswith('.tres')else'PackedScene'))
 require(len(preloads)==20,'UI original preload roster changed')
 onready=[]
 for m in re.finditer(r'^onready var (\w+): (\w+) = (\w+)\.instance\(\)',text,re.M):
  ref=next(r for r in preloads if r['name']==m[3]);r=next(r for r in recipes['recipes']if r['scene']==ref['path']);onready.append(dict(name=m[1],resource=ref['id'],native=m[2],recipe=r['scene_id']))
 require(len(onready)==7,'UI source onready7 changed')
 flavors=json.loads(re.search(r'^const FLAVORS(?:\s*:?=|\s*=)\s*(\[[^\n]+\])',gd,re.M)[1]);palette=[[int(h,16)for h in row]for row in re.findall(r'\[([^\]\n]+)\]',re.search(r'^var menuFlavors := (.*?)(?=^func )',text,re.M|re.S)[1])for row in [re.findall(r'"([0-9a-f]{6})"',row)]if row];require(len(flavors)==len(palette)==7 and all(len(r)==8 for r in palette),'UI source palette shape changed')
 thresholds=re.findall(r'distance\(curr_pixel, OLDCOLOR[1-8]\) < ([0-9]+(?:\.[0-9]+)?)',shader);require(len(thresholds)==8 and len(set(thresholds))==1,'UI shader branch thresholds require mapped review');threshold=float(thresholds[0]);require(0<threshold<=2,'UI source shader threshold rejected')
 params={m[1]:[float(v.strip())for v in m[2].split(',')]for m in re.finditer(r'^shader_param/(\w+) = Color\( ([^\n]+) \)',shader,re.M)};require(set(params)=={'OLDCOLOR'+str(i)for i in range(1,9)}|{'NEWCOLOR'+str(i)for i in range(1,9)},'UI source shader defaults changed')
 for r in recipes['recipes']:
  for f,h in r['sources'].items():require(h==inv[f]['sha256']==sha(ROOT/'upstream/MOTHER-Encore'/f),'UI recipe original source changed');sources[f]=h
 functions=[]
 for method in ['_ready','set_menu_flavors','_load_battle_bgs','_add_to_canvas','add_ui']:
  m=re.search(r'^func '+method+r'\([^\n]*\n.*?(?=^func |\Z)',text,re.M|re.S);require(m,'Missing UI source function');functions.append(dict(name=method,sha=hashlib.sha256(m[0].encode()).hexdigest()))
 write(IR,dict(schema=1,kind='encore.field-ui-manager.source-ir',commit=PIN,scene=SOURCE,scene_id=stable(SOURCE),source_sha256=sources[SOURCE],sources=sources,recipes_sha256=sha(RECIPES),recipes_count=9,preloads=preloads,onready=onready,flavors=flavors,palette=palette,shader_defaults=params,color_distance_threshold=threshold,functions=functions,scene_admitted=False,pending=['Missing PackedScene preloads need complete checked recipes, not source hash proxies','Complete original50 PackedScene directory resources use actual typed owners before stableCanvas; native BG instance/rendering remains pending','Every native UI constructor/Ready/layout/script requires its typed actual owner','Ready tail source seven add_to_canvas/Fade/camera remains pending until those native/script consumers complete']))
def load():
 d=read(IR);r=read(REVIEW);recipes=read(RECIPES);inv=read(ROOT/'compatibility/upstream-inventory.json')['files'];require(d['schema']==1 and d['commit']==PIN and not d['scene_admitted']and r['ir_sha256']==sha(IR)and r['recipes_sha256']==d['recipes_sha256']==sha(RECIPES)and r['commit']==PIN and len(recipes['recipes'])==9,'UI source review rejected')
 for f,h in d['sources'].items():require(h==inv[f]['sha256']==sha(ROOT/'upstream/MOTHER-Encore'/f),'UI changed source '+f)
 return d,recipes['recipes']
def encode(d,recipes):
 b=bytearray(128)
 def u(*v):b.extend(struct.pack('<'+'I'*len(v),*v))
 def t(s):v=s.encode();u(len(v));b.extend(v)
 t(d['scene']);u(len(d['preloads']))
 for r in d['preloads']:u(r['id'],int(r['onready']));t(r['name']);t(r['path']);t(r['native']);b.extend(bytes.fromhex(r['sha']))
 u(len(d['onready']))
 for r in d['onready']:t(r['name']);t(r['native']);u(r['resource'],r['recipe'])
 u(len(d['flavors']))
 for name,row in zip(d['flavors'],d['palette']):t(name);u(*row)
 for prefix in ['OLD','NEW']:
  for i in range(1,9):b.extend(struct.pack('<4f',*d['shader_defaults'][prefix+'COLOR'+str(i)]))
 b.extend(struct.pack('<f',d['color_distance_threshold']))
 u(len(d['functions']))
 for r in d['functions']:t(r['name']);b.extend(bytes.fromhex(r['sha']))
 u(len(d['sources']))
 for f,h in d['sources'].items():t(f);b.extend(bytes.fromhex(h))
 u(len(recipes))
 for r in recipes:raw=encode_recipe(r);u(len(raw));b.extend(raw)
 struct.pack_into('<8s8I',b,0,b'ENCFUIM1',2,128,len(b),0,0x454e0045,1,len(recipes),d['scene_id']);b[40:60]=bytes.fromhex(PIN);b[60:92]=bytes.fromhex(d['source_sha256']);b[92:124]=hashlib.sha256(IR.read_bytes()).digest();struct.pack_into('<I',b,20,zlib.crc32(b[128:]));return bytes(b)
def stage_files(source):
 d,r=load();raw=encode(d,r);require((Path(source)/'data/global.encuimanager').read_bytes()==raw,'Staged UI differs');return{Path('data/global.encuimanager'):raw}
def main():
 p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile','verify']);a=p.parse_args()
 if a.action=='extract':extract();return
 d,r=load();raw=encode(d,r)
 if a.action=='verify':require(OUT.read_bytes()==raw,'Stale UI resource')
 else:OUT.parent.mkdir(parents=True,exist_ok=True);OUT.write_bytes(raw)
 print('Original UI resources:',len(raw),'bytes;',sum(len(v['records'])for v in r),'complete native nodes; source Ready requires typed consumers')
if __name__=='__main__':
 try:main()
 except(ValueError,KeyError,TypeError,OSError,struct.error)as e:sys.exit('FIELD UI MANAGER ERROR: '+str(e))
