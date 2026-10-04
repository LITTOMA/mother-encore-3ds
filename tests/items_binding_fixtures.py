#!/usr/bin/env python3
"""Explicit build preparation; ordinary fixture verification is read-only."""
import argparse,copy,json,os,sys
from pathlib import Path
from unittest.mock import patch
ROOT=Path(os.environ.get('ENCORE_SOURCE_ROOT',Path(__file__).resolve().parents[1])).resolve()
sys.dont_write_bytecode=True;sys.path.insert(0,str(ROOT))
from tools import items_assets as assets,items_presentation_bindings as bindings,native_items as native

def candidate():
 recipe=bindings.read(bindings.IR);recipe['layouts']['info']['anchor']=[.75,1];recipe['platform']['hint_offset']=[-2,-1]
 return recipe

def fixtures(output):
 baseline=bindings.read(ROOT/'content/native-items.json');native.verify_sources(baseline,ROOT)
 original=native.encode(native.lower(baseline,root=ROOT));assert original==(ROOT/'romfs/data/opening.encitems').read_bytes()
 require_candidate=bindings.read(output/'bindings.json');assert require_candidate==candidate()
 with patch.object(bindings,'IR',output/'bindings.json'):
  ex,definitions,raw=assets.reviewed_extract();reference=bindings.read(ROOT/'reports/items-menu-source/native-layout.json')
  changed=assets.export_ir(ex,definitions,raw,reference,baseline['resources'],baseline['dependencies'],write=False)
  native.verify_sources(changed,ROOT);blob=native.encode(native.lower(changed,root=ROOT));native.parse_pack(blob)
 assert blob!=original
 assert changed['definitions']==baseline['definitions']and changed['initial_inventory']==baseline['initial_inventory']and changed['clips']==baseline['clips']and changed['sounds']==baseline['sounds']
 bad=bytearray(original);bad[-1]^=1
 return {'baseline.encitems':original,'adapter.encitems':blob,'bad-crc.encitems':bytes(bad)}

def main():
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--prepare-only',action='store_true');parser.add_argument('--output',type=Path,default=ROOT/'build/items-binding-fixtures');args=parser.parse_args();output=args.output.resolve()
 if ROOT/'build'not in output.parents:parser.error('fixtures must be inside checkout build directory')
 if args.prepare_only:
  output.mkdir(parents=True,exist_ok=True);(output/'bindings.json').write_text(json.dumps(candidate(),indent=2)+'\n',encoding='utf-8')
 for name,blob in fixtures(output).items():
  if args.prepare_only:(output/name).write_bytes(blob)
  elif(output/name).read_bytes()!=blob:raise ValueError('Stale Items binding fixture '+name)
 print('Prepared source Items binding fixtures'if args.prepare_only else'Source Items binding fixtures reproduced exactly (read-only)')
if __name__=='__main__':main()
