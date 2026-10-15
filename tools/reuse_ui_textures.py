#!/usr/bin/env python3
"""Retain genuine UI texture outputs across reviewed metadata-only extensions.

Requires an exact Git baseline, its original IR/producer/receipt, identical
conversion function AST, unchanged source art recipes and verified output bytes.
Never converts textures, creates a compiler identity or edits pack bytes.
"""
import argparse,ast,hashlib,importlib,json,re,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))

def require(condition,message):
    if not condition:raise ValueError(message)

def sha(raw):return hashlib.sha256(raw).hexdigest()

def conversion(code):
    functions=[n for n in ast.parse(code).body if isinstance(n,ast.FunctionDef) and n.name=='compile_assets']
    require(len(functions)==1,'Texture conversion function missing/ambiguous')
    return ast.dump(functions[0],include_attributes=False)

def candidate(old_ir,old_producer,old_receipt,new_ir,new_producer,new_ir_bytes):
    previous=json.loads(old_ir)
    require(old_receipt['ir_sha256']==sha(old_ir) and old_receipt['producer_sha256']==sha(old_producer),
            'Original producer/IR receipt mismatch')
    require(old_receipt['commit']==previous['commit']==new_ir['commit'] and old_receipt['schema']==1 and
            re.fullmatch('[0-9a-f]{64}',old_receipt['tex3ds_sha256']) is not None,'Original compiler/pin receipt missing')
    require(previous['resources']==new_ir['resources'] and all(new_ir['sources'].get(p)==h for p,h in previous['sources'].items()),
            'Texture source recipe changed')
    require(conversion(old_producer)==conversion(new_producer),'Texture conversion changed')
    return dict(old_receipt,ir_sha256=sha(new_ir_bytes),producer_sha256=sha(new_producer))

def main():
    parser=argparse.ArgumentParser();parser.add_argument('producer',choices=['storage_assets','item_details']);parser.add_argument('--base',required=True);a=parser.parse_args()
    require(re.fullmatch('[0-9a-f]{40}',a.base) is not None,'Use the exact reviewed baseline commit')
    module=importlib.import_module('tools.'+a.producer);ir=module.load(ROOT)
    source='tools/'+a.producer+'.py';receipt=ROOT/module.RECEIPT
    old=lambda p:subprocess.check_output(['git','show',a.base+':'+p],cwd=ROOT)
    value=candidate(old(module.IR),old(source),json.loads(old(module.RECEIPT)),ir,(ROOT/source).read_bytes(),(ROOT/module.IR).read_bytes())
    previous=receipt.read_bytes()
    try:
        module.write_json(receipt,value)
        if a.producer=='item_details':module.verify_receipt(ir)
        else:module.verify_receipt()
    except Exception:
        receipt.write_bytes(previous);raise
    out=ROOT/'build/basement-presents/texture-reuse';out.mkdir(parents=True,exist_ok=True)
    module.write_json(out/(a.producer+'.json'),dict(baseline=a.base,producer=source,original_receipt_sha256=sha(old(module.RECEIPT)),
        receipt_sha256=sha(receipt.read_bytes()),conversion_ast_unchanged=True,source_recipes_unchanged=True,outputs_verified=True))
    print('Retained genuine',a.producer,'textures: identical conversion/source recipes and verified output bytes')
    return 0
if __name__=='__main__':raise SystemExit(main())
