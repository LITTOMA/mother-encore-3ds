#!/usr/bin/env python3
"""Pillow action/RNG oracle using the existing native source harness.

Only fixture battler/weighted pool changes; inherited adapters remain explicit.
Ordinary victory is observed at its boundary; reward/world continuation is covered separately.
"""
import argparse,json,os,subprocess,sys,hashlib,re
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT));sys.path.insert(0,str(ROOT/'tools'))
from tools import run_battle_round_reference as base
from extract_battle_entry import Extractor,require

def main():
 ap=argparse.ArgumentParser();ap.add_argument('--godot',type=Path,required=True);ap.add_argument('--probe',type=Path,required=True);ap.add_argument('--work',type=Path,default=ROOT/'build/battle-round-reference/pillow-actions');ap.add_argument('--reports',type=Path,default=ROOT/'reports/pillow-battle/action-reference');a=ap.parse_args();require(not a.work.exists(),'Use a fresh oracle work directory');a.reports.mkdir(parents=True,exist_ok=True)
 metadata=base.prepare(a.work);ex=Extractor(ROOT);enemy=ex.yaml('Data/Battlers/pillow.yaml');fixtures=json.loads((a.work/'fixtures.json').read_text());fixtures['lamp']=enemy;(a.work/'fixtures.json').write_text(json.dumps(fixtures))
 sprite_path=a.work/'sprite.gd';sprite=sprite_path.read_text();source=ex.text('Scripts/UI/Battle/BattleSpriteParty.gd');source_line=next(line.strip()for line in source.splitlines()if 'hitAnim = "hit" + var2str(int(round(rand_range' in line)
 hit_count=json.loads((ROOT/'content/pillow-round.json').read_text())['presentation']['parameters']['PartyHit'][3]
 replacement='func bounce_up_hit(_amount):\n\tvar _hits='+str(int(hit_count))+'\n\tvar '+source_line.replace('rand_range(1, _hits)','runner.draw_range("BattleSpriteParty.bounce_up_hit", 1, _hits)')+'\n\trunner.event("native_hit_choice", {"animation":hitAnim})\n'
 sprite=re.sub(r'func bounce_up_hit\(_amount\):\n.*?(?=func )',replacement,sprite,flags=re.S);sprite_path.write_text(sprite)
 metadata['scope']='Eight Pillow first rounds using unmodified extracted BattleSystem damage/action methods and real native weighted choice/global RNG';metadata['sources'].update(ex.sources);metadata['fixture_alias']='Historical harness slot named lamp contains exact Pillow source data; ordinary victory adapter records only the source win boundary';(a.work/'metadata.json').write_text(json.dumps(metadata,indent=2))
 probe=base.PROBE[:base.PROBE.index('func run():')]+'''func run():
 var picked=[]
 var has_float=false
 for s in range(10000):
  seed(s)
  var choice=rand_range(0.0,6.0)
  var critical=randi()%100+1<=5
  if !critical and (picked.size()<8 or (choice>5 and !has_float)):
   picked.append(s)
   if choice>5:has_float=true
  if picked.size()>=8 and has_float:break
 for s in picked:yield(run_case("pillow_"+str(s),s),"completed")
 var f=File.new()
 assert(f.open(output,File.WRITE)==OK)
 f.store_string(JSON.print({"schema":1,"engine":Engine.get_version_info(),"metadata":metadata,"cases":cases},"  "))
 f.close()
 print("PILLOW_ACTION_REFERENCE_COMPLETE")
 quit()
'''
 (a.work/'probe.gd').write_text(probe)
 env=dict(os.environ)
 for key in['XDG_DATA_HOME','XDG_CONFIG_HOME','XDG_CACHE_HOME']:
  d=a.work/key.lower();d.mkdir();env[key]=str(d.resolve())
 command=[str(a.godot.resolve()),'--path',str(a.work.resolve()),'--fixed-fps','60','--script',str((a.work/'probe.gd').resolve()),'--encore-out='+str((a.reports/'reference.json').resolve())]
 with(a.reports/'godot.txt').open('wb')as log:run=subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,timeout=40,env=env)
 log=(a.reports/'godot.txt').read_text();require(run.returncode==0 and 'SCRIPT ERROR'not in log and 'ERROR:'not in log and 'PILLOW_ACTION_REFERENCE_COMPLETE'in log,'Pillow native action reference failed: '+log[-1200:])
 result=json.loads((a.reports/'reference.json').read_text());require(result['engine']['string']=='3.6.2-stable (official)','Unexpected Godot engine');comparisons=[]
 for case in result['cases']:
  require(case['boundary']in['next_menu','win'],'Unexpected Pillow action boundary')
  native=json.loads(subprocess.check_output([str(a.probe.resolve()),str(ROOT/'romfs/data/pillow-entry.encround'),str(ROOT/'romfs/data/pillow-entry.encbattle'),str(case['seed'])],text=True))
  expected={'boundary':case['boundary'],'player_hp':case['hp']['ninten'],'enemy_hp':case['hp']['lamp'],'turn':case['turn'],'raw_draw_count':case['raw_draw_count'],'next_randi':case['next_randi'],'decisions':[{'actor':0 if e['user']=='ninten'else 1,'target':0 if e['target']=='ninten'else 1,'damage':e['damage'],'smash':e['smash']}for e in case['events']if e['event']=='damage']}
  require(native==expected,'Pillow source/native action mismatch: '+str((case['seed'],native,expected)));comparisons.append({'seed':case['seed'],'actual':native,'match':True})
 (a.reports/'comparison.json').write_text(json.dumps(comparisons,indent=2)+'\n');(a.reports/'receipt.json').write_text(json.dumps({'commit':ex.lock['commit'],'godot_sha256':base.sha(a.godot),'tool_sha256':base.sha(__file__),'sources':metadata['sources'],'cases':len(comparisons),'limits':metadata['adapters']},indent=2)+'\n');print('Official Godot Pillow action/RNG comparison:',len(comparisons),'first rounds match')
if __name__=='__main__':main()
