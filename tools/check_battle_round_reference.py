#!/usr/bin/env python3
"""Compare complete native round values and random continuation to Godot oracle.
The C++ probe uses explicitly limited instant-HP and one-idle media adapters;
this check makes no claim about real presentation timing or GPU rendering.
"""
import argparse,json,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--probe',type=Path,required=True);p.add_argument('--report',type=Path);args=p.parse_args()
 reference=json.loads((ROOT/'reports/battle-round-reference/final/reference.json').read_text())
 results=[]
 for case in reference['cases']:
  if case['name'].startswith('equal_speed_'):continue # separate source comparator fixtures, altered inputs
  got=json.loads(subprocess.check_output([str(args.probe.resolve()),str(ROOT/'romfs/data/opening.encround'),str(ROOT/'romfs/data/opening.encbattle'),str(case['seed'])],text=True))
  expected={'boundary':case['boundary'],'player_hp':case['hp']['ninten'],'enemy_hp':case['hp']['lamp'],'turn':case['turn'],'raw_draw_count':case['raw_draw_count'],'next_randi':case['next_randi']}
  assert all(got[k]==v for k,v in expected.items()),(case['name'],got,expected)
  native_damage=[{'actor':0 if e['user']=='ninten' else 1,'target':0 if e['target']=='ninten' else 1,'damage':e['damage'],'smash':e['smash']}for e in case['events']if e['event']=='damage']
  assert got['decisions']==native_damage,(case['name'],got['decisions'],native_damage)
  results.append({'name':case['name'],'matched':got})
 record={'scope':__doc__,'cases':results,'source_reference':str(ROOT/'reports/battle-round-reference/final/reference.json')}
 if args.report:args.report.parent.mkdir(parents=True,exist_ok=True);args.report.write_text(json.dumps(record,indent=2)+'\n')
 print(f'{len(results)} original round outcomes/action orders/RNG continuations matched')
if __name__=='__main__':main()
