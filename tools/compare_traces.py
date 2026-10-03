#!/usr/bin/env python3
"""Compare exact semantic JSONL traces. Never rewrites the expected/reference trace."""
import argparse
import json
from pathlib import Path
import sys

def load(path):
    records=[]
    for number,line in enumerate(path.read_text(encoding='utf-8').splitlines(),1):
        if line.strip():records.append((number,json.loads(line)))
    return records

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('reference',type=Path);p.add_argument('candidate',type=Path);a=p.parse_args()
    try:
        r,c=load(a.reference),load(a.candidate)
        for index,((rn,rv),(cn,cv)) in enumerate(zip(r,c)):
            if rv!=cv:
                print(f'MISMATCH record {index}; reference line {rn}, candidate line {cn}')
                for key in sorted(set(rv)|set(cv)):
                    if rv.get(key)!=cv.get(key):print(f'  {key}: {rv.get(key)!r} != {cv.get(key)!r}')
                return 1
        if len(r)!=len(c):print(f'LENGTH MISMATCH: {len(r)} != {len(c)}');return 1
        print(f'PASS: {len(r)} exact semantic records');return 0
    except (OSError,ValueError) as e:print(e,file=sys.stderr);return 2
if __name__=='__main__':raise SystemExit(main())
