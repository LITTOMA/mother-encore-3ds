#!/usr/bin/env python3
"""Bounded original Pillow/Minnie source graph, native receipts and dialogue lowering."""
from __future__ import annotations
import argparse, csv, hashlib, io, json, re
from pathlib import Path
import sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT));sys.path.insert(0,str(ROOT/'tools'))
from tools.doll_dialogue import PIN, SOURCES as BASE_SOURCES, run_native, decode, require
from tools.doll_postwin import return_duration
from tools.phone_dialogue import text_segments
from tools.pillow_source_bindings import read as read_bindings,IR as BINDINGS_IR
_PATHS=read_bindings(BINDINGS_IR)['paths']
ATTACK,LEAVE,DOOR,TUTORIAL,OPEN=(_PATHS[key]for key in ['attack','leave','door','tutorial','open'])
PATHS=(ATTACK,LEAVE,DOOR,TUTORIAL,OPEN)
REPORT=ROOT/'reports/pillow-sequence'

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def identity(path):return path.removeprefix('Data/Dialogue/').removesuffix('.yaml')
def load_documents(ex):
 import yaml
 docs={path:yaml.safe_load(ex.text(path))for path in PATHS}
 from tools.pillow_source_bindings import load
 bindings=load(ex.root)
 for row in bindings['append']['native_receipts']:
  path=bindings['paths'][row['role']];emote=row['emote'];receipt_path=row['path']
  receipt=ex.document(receipt_path)if hasattr(ex,'document')else json.loads((ex.root/receipt_path).read_text())
  require(receipt['commit']==PIN and receipt['godot']['major']==3 and receipt['godot']['minor']==6 and receipt['godot']['patch']==2,'Pillow native engine/pin')
  for source,digest in receipt['sources'].items():
   if hasattr(ex,'source'):ex.source(source,digest)
   else:require(sha(ex.upstream/source)==digest,'Changed Pillow native source '+source);ex.data(source)
  require(decode(receipt)['yaml'][0]==docs[path],'Pillow original/Python parser disagreement')
 return docs

def generate(godot):
 REPORT.mkdir(parents=True,exist_ok=True)
 for row in read_bindings(BINDINGS_IR)['append']['native_receipts']:
  path=_PATHS[row['role']];emote=row['emote']
  sources=[path,*BASE_SOURCES[1:]]
  data=run_native(ROOT,godot,path,emote,sources)
  (REPORT/(Path(path).stem+'-native.json')).write_text(json.dumps(data,indent=2)+'\n')
 review=dict(commit=PIN,scope='Pillow attack, Minnie leave/door and tutorial; bounded source handlers, no generic interpreter',
  handlers=['text','actors/ready/idle','wait','talker','teleport','direction','stop_loop','move_queue','turn','shake','jump','animation','emote','camera','flags','battle','text/timer gate','ordered asynchronous restoration'],
  invariants=['Pillow branch optional; no melody prerequisite','Poltergeist parent disappears on doll_melody','minnie_leave only at source phrase6','minnie_door only at source phrase2','NPC event_positions apply on ready only; live teleport remains distinct','Looping movement and shake share source _looping','Repeated jumps wait after final jump too'],
  receipts={p.name:sha(p)for p in REPORT.glob('*-native.json')})
 (REPORT/'source-review.json').write_text(json.dumps(review,indent=2)+'\n')

def compile_linear(path,doc,end_duration,text_ids,turn_default,move_default,jump_default,root=ROOT):
 from tools.pillow_source_bindings import linear
 return linear(path,doc,end_duration,text_ids,turn_default,move_default,jump_default,root)

def tutorial_graph(doc,translations,end,root=ROOT):
 from tools.pillow_source_bindings import tutorial
 return tutorial(doc,translations,end,root)

if __name__=='__main__':
 parser=argparse.ArgumentParser();parser.add_argument('--godot',type=Path,required=True);a=parser.parse_args();generate(a.godot)
