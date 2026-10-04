#!/usr/bin/env python3
"""Explicit build-only preparation; ordinary reproduction never writes files."""
import argparse,json,os,sys
from pathlib import Path
from unittest.mock import patch
ROOT=Path(os.environ.get('ENCORE_SOURCE_ROOT',Path(__file__).resolve().parents[1])).resolve()
sys.dont_write_bytecode=True;sys.path.insert(0,str(ROOT));sys.path.insert(0,str(ROOT/'tools'))
from tools import house_source_bindings as bindings,house_assets as assets,extract_house,native_house

def candidate():
 recipe=bindings.read(bindings.IR);recipe['presentation']['display_reference'][2]=.75;return recipe

def fixtures(output):
 baseline=bindings.read(ROOT/'content/native-house.json');presentation=bindings.read(ROOT/'content/native-house-presentation.json');native_house.verify_sources(baseline,presentation,ROOT)
 original=native_house.encode(native_house.lower(baseline,presentation,ROOT),baseline['commit'],baseline['schema']);assert original==(ROOT/'romfs/data/opening.enchouse').read_bytes()
 assert bindings.read(output/'bindings.json')==candidate()
 with patch.object(bindings,'IR',output/'bindings.json'):
  changed=extract_house.build(ROOT);assert changed==baseline
  receipt=bindings.read(ROOT/'content/asset-receipts/graphics/ui/house/source.json');adapter=assets.export_presentation(ROOT/'upstream/MOTHER-Encore',receipt['resources'],write=False)
  expected=dict(presentation);expected['parameters']=dict(presentation['parameters']);expected['parameters']['DisplayReference']=[320,180,.75,1];assert adapter==expected
  native_house.verify_sources(changed,adapter,ROOT);blob=native_house.encode(native_house.lower(changed,adapter,ROOT),changed['commit'],changed['schema']);native_house.parse_pack(blob)
 assert blob!=original
 bad=bytearray(original);bad[-1]^=1
 return {'baseline.enchouse':original,'adapter.enchouse':blob,'bad-crc.enchouse':bytes(bad)}

def main():
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--prepare-only',action='store_true');parser.add_argument('--output',type=Path,default=ROOT/'build/house-binding-fixtures');args=parser.parse_args();output=args.output.resolve()
 if ROOT/'build'not in output.parents:parser.error('fixtures must be inside checkout build directory')
 if args.prepare_only:output.mkdir(parents=True,exist_ok=True);(output/'bindings.json').write_text(json.dumps(candidate(),ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
 for name,blob in fixtures(output).items():
  if args.prepare_only:(output/name).write_bytes(blob)
  elif(output/name).read_bytes()!=blob:raise ValueError('Stale House binding fixture '+name)
 print('Prepared source House binding fixtures'if args.prepare_only else'Source House binding fixtures reproduced exactly (read-only)')
if __name__=='__main__':main()
