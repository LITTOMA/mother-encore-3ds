#!/usr/bin/env python3
"""Prepare an isolated exact-source Godot probe; compile its real results to C++ fixtures."""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import re
import sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from tools.upstream import read_json, write_json, safe_path
REVIEW=ROOT/'compatibility/reviews/progression-v0410.json'

def extract(raw: bytes,review: dict)->tuple[str,dict]:
    if review.get('schema')!=1 or review.get('game_version')!='0.4.1.0' or review.get('level_cap')!=30:
        raise ValueError('Unreviewed progression schema/version/cap')
    if hashlib.sha256(raw).hexdigest()!=review['source_sha256']:
        raise ValueError('Progression source changed; semantic review required')
    text=raw.decode('utf-8');lines=text.splitlines(keepends=True)
    constants=[line for line in lines if re.fullmatch(r'const LEVEL_CAP := 30\r?\n?',line)]
    if len(constants)!=1:raise ValueError('Audited LEVEL_CAP constant missing/ambiguous')
    blocks=[];hashes={}
    symbols=['_level_to_exp','_exp_to_level']
    if review.get('symbols')!=symbols:raise ValueError('Unknown progression symbol scope')
    for symbol in symbols:
        indices=[i for i,line in enumerate(lines) if re.match(r'^static func '+re.escape(symbol)+r'\(',line)]
        if len(indices)!=1:raise ValueError('Missing/ambiguous audited symbol: '+symbol)
        start=indices[0];end=start+1
        while end<len(lines) and (lines[end].startswith('\t') or not lines[end].strip()):end+=1
        block=''.join(lines[start:end]).rstrip('\r\n')+'\n'
        if end==start+1 or '\t' not in block:raise ValueError('Missing audited function body')
        blocks.append(block);hashes[symbol]=hashlib.sha256(block.encode()).hexdigest()
    # No source rewriting: only the class wrapper is replaced to remove unrelated
    # inventory/globaldata dependencies. The functions are byte-identical text.
    return 'extends Reference\n'+constants[0]+ '\n'+ '\n'.join(blocks),hashes

def prepare(source_root: Path,out: Path,review: dict)->None:
    from tools.upstream import git
    if git(source_root,'rev-parse','HEAD')!=review['commit'] or git(source_root,'status','--porcelain'):
        raise ValueError('Reference requires pristine reviewed upstream commit')
    code,hashes=extract(safe_path(source_root,review['source']).read_bytes(),review)
    out.mkdir(parents=True,exist_ok=True)
    (out/'project.godot').write_text('config_version=4\n[application]\nconfig/name="Encore progression reference"\n[logging]\nfile_logging/enable_logging=false\n',encoding='utf-8')
    (out/'rules.gd').write_bytes(code.encode('utf-8'))
    metadata={'schema':1,'commit':review['commit'],'game_version':review['game_version'],
              'source':review['source'],'source_sha256':review['source_sha256'],'symbol_sha256':hashes,
              'scope':'isolated unchanged functions; not full game reference'}
    write_json(out/'metadata.json',metadata)
    (out/'probe.gd').write_text('''extends SceneTree
func _init():
    var args = OS.get_cmdline_args()
    var output = ""
    for i in range(args.size() - 1):
        if args[i] == "--encore-out": output = args[i + 1]
    if output == "":
        printerr("Missing --encore-out")
        quit(2)
        return
    var rules = load("res://rules.gd")
    if rules == null:
        quit(1)
        return
    var metadata_file = File.new()
    if metadata_file.open("res://metadata.json", File.READ) != OK:
        quit(1)
        return
    var parsed = JSON.parse(metadata_file.get_as_text())
    metadata_file.close()
    if parsed.error != OK:
        quit(1)
        return
    var document = parsed.result
    document["godot"] = Engine.get_version_info()
    document["level_cases"] = []
    for level in range(1, 61):
        document.level_cases.append([level, rules._level_to_exp(level)])
    document.level_cases.append([2147483647, rules._level_to_exp(2147483647)])
    document["xp_levels"] = []
    for xp in range(0, rules._level_to_exp(30) + 2):
        document.xp_levels.append(rules._exp_to_level(xp))
    document["extra_xp_cases"] = []
    for xp in [-2147483648, -1, 2147483647]:
        document.extra_xp_cases.append([xp, rules._exp_to_level(xp)])
    var result_file = File.new()
    if result_file.open(output, File.WRITE) != OK:
        quit(1)
        return
    result_file.store_string(JSON.print(document, "  ") + "\\n")
    result_file.close()
    print("Original progression functions: ", document.xp_levels.size(), " exhaustive XP values, ", document.level_cases.size(), " levels and 3 signed boundaries.")
    quit(0)
''',encoding='utf-8')
    print('Prepared unchanged audited functions; execute probe.gd in official Godot 3.6.2.')

def fixture(document: dict,review: dict)->str:
    for key in ('schema','commit','game_version','source_sha256'):
        if document.get(key)!=review.get(key):raise ValueError('Reference provenance mismatch: '+key)
    engine=document.get('godot',{})
    if [engine.get(k) for k in ('major','minor','patch','status')]!=[3,6,2,'stable']:
        raise ValueError('Reference requires Godot 3.6.2 stable')
    levels=document.get('level_cases',[]);xp=document.get('xp_levels',[]);extra=document.get('extra_xp_cases',[])
    if len(levels)!=61 or [row[0] for row in levels]!=list(range(1,61))+[2147483647]:
        raise ValueError('Incomplete reference level domain')
    if len(xp)!=20927 or any(type(v)!=int or not 1<=v<=30 for v in xp):
        raise ValueError('Incomplete/invalid exhaustive XP domain')
    if len(extra)!=3 or [row[0] for row in extra]!=[-2147483648,-1,2147483647]:
        raise ValueError('Incomplete signed XP boundaries')
    for row in levels+extra:
        if len(row)!=2 or any(type(v)!=int for v in row):raise ValueError('Invalid reference case')
    result='// GENERATED by tools/reference_progression.py from actual Godot output. Do not edit.\n'
    result+='// Source '+review['commit']+' / '+review['source_sha256']+'\n#pragma once\n#include <cstdint>\n'
    result+='namespace progression_reference {\nstruct Case { int32_t input; int32_t expected; };\n'
    for name,rows in [('levels',levels),('extra_xp',extra)]:
        result+='constexpr Case '+name+'[] = {\n'+''.join('    {'+str(a)+', '+str(b)+'},\n' for a,b in rows)+'};\n'
    result+='constexpr uint8_t xp_levels[] = {\n'
    for i in range(0,len(xp),80):result+='    '+','.join(str(v) for v in xp[i:i+80])+',\n'
    return result+'};\n}\n'

def main()->int:
    ap=argparse.ArgumentParser(description=__doc__);sub=ap.add_subparsers(dest='action',required=True)
    p=sub.add_parser('prepare');p.add_argument('--root',type=Path,default=ROOT/'upstream/MOTHER-Encore');p.add_argument('--out',type=Path,default=ROOT/'build/m1/progression-reference')
    p=sub.add_parser('fixture');p.add_argument('--reference',type=Path,required=True);p.add_argument('--out',type=Path,default=ROOT/'tests/fixtures/progression_v0410.hpp')
    args=ap.parse_args()
    try:
        review=read_json(REVIEW)
        if args.action=='prepare':prepare(args.root,args.out,review)
        else:
            result=fixture(read_json(args.reference),review);args.out.parent.mkdir(parents=True,exist_ok=True)
            args.out.write_text(result,encoding='utf-8');print('Generated exhaustive fixture from measured Godot results.')
        return 0
    except (OSError,ValueError,KeyError) as error:
        print(f'PROGRESSION REFERENCE ERROR: {error}',file=sys.stderr);return 1
if __name__=='__main__':raise SystemExit(main())
