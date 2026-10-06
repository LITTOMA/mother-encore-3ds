#!/usr/bin/env python3
"""Original TSCN actual script assignments, including explicit null overrides.

No lifecycle approval. Return (bindings, null_overrides, source_hashes).
Bindings map full native node path to (script path, whole-file or embedded
section SHA256). Empty overrides remove only the proven actual node identity.
"""
from pathlib import Path
import re
from tools.podunk_scene import ROOT,PIN,SCENE,read,sha,require

def actual_scripts(native,receipt):
 from tools.scene_reference import quarantine
 d=read(native);proof=read(receipt);require(d['source']=='res://'+SCENE and d['native_compatible']is False and len(d['nodes'])==8686,'Full original Podunk source required');inv=read(ROOT/'compatibility/upstream-inventory.json')['files'];require(proof['commit']==PIN and proof['scene']==SCENE,'NodeTree original receipt identity')
 states={s['source'][6:]:s for s in d['scene_states']};resources={r['id']:r for r in d['resources']};names={n['path']for n in d['nodes']};roots=[];assignments={};sources={}
 for file,r in proof['files'].items():require(sha(ROOT/'upstream/MOTHER-Encore'/file)==r['sha256']==inv[file]['sha256'],'NodeTree receipt source differs');sources[file]=r['sha256']
 for file in states:
  require(file in sources,'Missing original SceneState source proof '+file)
  raw=(ROOT/'upstream/MOTHER-Encore'/file).read_bytes();clean,_,attachments,_=quarantine(raw,file);attached={a['node_declaration']:a for a in attachments};current='';props=[];entries=[]
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
   require(len(changes)==1 and re.fullmatch(r'script\s*=\s*null',changes[0]),'Unknown source script assignment')
   name=re.search(r'\bname="([^"\n]+)"',decl)[1];parent=re.search(r'\bparent="([^"\n]+)"',decl);local='.'if parent is None else name if parent[1]=='.'else parent[1]+'/'+name
   a=attached.get(decl);script=a['script']if a else None;h=(a.get('embedded_script',{}).get('sha256')or inv[script]['sha256'])if script else None
   rows.append((local,script,h,decl))
  assignments[file]=rows
 def visit(root,file):
  roots.append((root,file))
  for n in states[file]['nodes']:
   if n['instance']is not None:
    local=n['path'][2:]if n['path'].startswith('./')else n['path'];target=root if local=='.'else local if root=='.'else root+'/'+local;visit(target,resources[n['instance']['id']]['path'][6:])
 visit('.',SCENE);bindings={};nulls={}
 for root,file in sorted(roots,key=lambda r:r[0].count('/')if r[0]!='.'else -1,reverse=True):
  for local,script,h,decl in assignments[file]:
   path=root if local=='.'else local if root=='.'else root+'/'+local
   require(path in names,'NodeTree source script target missing '+path)
   if script is None:
    if path in bindings:nulls[path]=dict(previous_script=bindings[path][0],source=file,source_sha256=inv[file]['sha256'],declaration=decl)
    bindings.pop(path,None)
   else:bindings[path]=(script,h);nulls.pop(path,None)
 return bindings,nulls,sources
