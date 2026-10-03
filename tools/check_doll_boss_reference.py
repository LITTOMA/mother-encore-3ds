#!/usr/bin/env python3
"""Compare native host boss callbacks/RNG against the retained Godot oracle."""
import argparse,json,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def main():
 p=argparse.ArgumentParser();p.add_argument('--probe',type=Path,required=True);p.add_argument('--report',type=Path,default=ROOT/'reports/doll-round/boss-reference/comparison.json');a=p.parse_args()
 source=json.loads((ROOT/'reports/doll-round/boss-reference/reference.json').read_text());expected={e['event']:e['frame']for e in source['events']if e['event']!='flash_position'};expected['next_randi']=source['next_randi']
 actual=json.loads(subprocess.check_output([str(a.probe.resolve()),str(ROOT/'romfs/data/doll-entry.encround'),str(ROOT/'romfs/data/doll-entry.encbattle')],text=True));a.report.parent.mkdir(parents=True,exist_ok=True);a.report.write_text(json.dumps(dict(expected=expected,actual=actual,match=actual==expected),indent=2)+'\n')
 if actual!=expected:raise SystemExit('Boss native/source callback or RNG mismatch: '+str((expected,actual)))
 print('Doll boss AnimationPlayer/Shaker: exact native callback frames and RNG match')
if __name__=='__main__':main()
