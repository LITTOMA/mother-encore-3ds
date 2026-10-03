#!/usr/bin/env python3
"""Run unmodified scalar growth/learning methods in official Godot 3.6.2."""
import argparse,hashlib,json,os,re,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'tools'))
from extract_battle_entry import Extractor,require

def method(source,name):
 m=re.findall(r'^(?:static )?func '+re.escape(name)+r'\([^\n]*\n.*?(?=^(?:static )?func |\Z)',source,re.M|re.S);require(len(m)==1,'Missing method '+name);return m[0]
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--godot',type=Path,required=True);ap.add_argument('--work',type=Path,default=ROOT/'build/doll-growth-reference');ap.add_argument('--reports',type=Path,default=ROOT/'reports/doll-round/growth-reference');a=ap.parse_args();a.work.mkdir(parents=True,exist_ok=True);a.reports.mkdir(parents=True,exist_ok=True)
 ex=Extractor(ROOT);pm=ex.text('Scripts/global/PartyMember.gd');ch=ex.text('Scripts/global/Character.gd');entry=json.loads((ROOT/'content/native-battle.json').read_text());doll=json.loads((ROOT/'content/doll-round.json').read_text())
 data={'base':entry['party']['base_stats'],'boost':{k:entry['party']['effective_stats'][k]-v for k,v in entry['party']['base_stats'].items()},'skills':{k:ex.yaml('Data/BattleSkills/'+k+'.yaml')for k in['telepathy','strike','splitShot']},'learned':entry['party']['learned_skills']}
 (a.work/'fixture.json').write_text(json.dumps(data))
 code='extends Node\nsignal stat_changed(stat,value,max_value)\n'
 for key in['HP','PP','MAXHP','MAXPP','OFFENSE','DEFENSE','SPEED','IQ','GUTS']:code+='const '+key+'="'+key.lower()+'"\nvar _'+key.lower()+':int=0\n'
 for name in['LEVEL_CAP','NINTEN','ANA','LLOYD','TEDDY','PIPPI']:
  code+=re.search(r'^const '+name+r' := [^\n]+',pm,re.M)[0]+'\n'
 for name,close in [('PLAYER_STAT_TARGET_TABLE','}'),('PLAYER_LEARN_SKILL_TABLE_FLAGS','}'),('PLAYER_LEARN_SKILL_TABLE','}'),('SKILLS_ORDER',']'),('WEAPONS','}')]:
  code+=re.search(r'^const '+name+r' (?:\:=|: Dictionary =) [\s\S]*?^'+re.escape(close),pm,re.M)[0]+'\n'
 code+='''var _level:int=1
var _exp:int=3
var _learned_skills=[]
var globaldata
func get_name():return NINTEN
func get_max_hp():return _maxhp
func get_max_pp():return _maxpp
func has_item_activated_by_name(_name):
 assert(false)
 return false
'''
 names=['_level_to_exp','_exp_to_level','_set_exp','_update_level_from_exp','_get_stat_for_level','_learn_new_skills','_is_skill_usable','add_skill','_sort_skills']
 for name in names:code+='\n'+method(pm,name)
 for name in['get_base_stat','set_stat','set_hp','set_pp']:code+='\n'+method(ch,name)
 (a.work/'character.gd').write_text(code)
 (a.work/'service.gd').write_text('extends Reference\nvar skills={}\nvar flags={}\nfunc get_battle_skill(id):return skills.get(id,{})\n')
 probe='''extends SceneTree
func _init():
 var file=File.new()
 assert(file.open("res://fixture.json",File.READ)==OK)
 var fixture=JSON.parse(file.get_as_text()).result
 file.close()
 var c=load("res://character.gd").new()
 get_root().add_child(c)
 var service=load("res://service.gd").new()
 service.skills=fixture.skills
 c.globaldata=service
 c._learned_skills=fixture.learned.duplicate()
 for stat in fixture.base:c.set("_"+stat,int(fixture.base[stat]))
 c._hp=51
 c._pp=26
 seed(8473)
 var expected=str(randi())
 seed(8473)
 var changed={}
 var learned=[]
 c._set_exp(11,true,changed,learned)
 var actual=str(randi())
 assert(actual==expected)
 var stats={}
 for stat in fixture.base:stats[stat]=c.get_base_stat(stat)+int(fixture.boost[stat])
 var result={"engine":Engine.get_version_info(),"experience":c._exp,"level":c._level,"hp":c._hp,"pp":c._pp,"effective_stats":stats,"changed_stats":changed,"learned":learned,"all_skills":c._learned_skills,"expected_next_randi":expected,"actual_next_randi":actual}
 var output=""
 for arg in OS.get_cmdline_args():
  if arg.begins_with("--out="):output=arg.substr(6)
 assert(file.open(output,File.WRITE)==OK)
 file.store_string(JSON.print(result,"  "))
 file.close()
 print("DOLL_GROWTH_REFERENCE_COMPLETE")
 quit()
'''
 (a.work/'probe.gd').write_text(probe);(a.work/'project.godot').write_text('config_version=4\n[application]\nconfig/name="Doll source growth reference"\n[logging]\nfile_logging/enable_logging=false\n')
 command=[str(a.godot.resolve()),'--path',str(a.work.resolve()),'--script',str((a.work/'probe.gd').resolve()),'--out='+str((a.reports/'reference.json').resolve())]
 env=dict(os.environ)
 for key in['XDG_DATA_HOME','XDG_CONFIG_HOME','XDG_CACHE_HOME']:
  directory=a.work/key.lower();directory.mkdir(exist_ok=True);env[key]=str(directory.resolve())
 with(a.reports/'godot.txt').open('wb')as log:run=subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,timeout=25,env=env)
 log=(a.reports/'godot.txt').read_text();require(run.returncode==0 and 'SCRIPT ERROR'not in log and 'ERROR:'not in log and 'DOLL_GROWTH_REFERENCE_COMPLETE'in log,'Growth reference failed: '+log[-1500:])
 r=json.loads((a.reports/'reference.json').read_text());require(r['engine']['string']=='3.6.2-stable (official)','Unexpected engine');require(r['level']==2 and r['experience']==11 and r['hp']==54 and r['pp']==27 and r['learned']==['telepathy'],'Unexpected source growth')
 require([r['effective_stats'][k]for k in data['base']]==[g['after']for g in doll['growth']],'Native source differs from binary growth')
 receipt={'commit':native_pin(ex),'source_sha256':ex.sources,'godot_sha256':hashlib.sha256(a.godot.read_bytes()).hexdigest(),'methods':names+['Character.get_base_stat','Character.set_stat','Character.set_hp','Character.set_pp'],'adapters':['source data through JSON; fixture initial EXP3 HP51 PP26','Character name and no-status max HP/PP getters supplied','equipped BaseballCap boost applied separately to output effective defense','unreachable weapon activation rejects'],'native_rng_unchanged':r['actual_next_randi']==r['expected_next_randi']}
 (a.reports/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n');print('Official Godot source growth/learning matches Doll v3 data; no random draws')
def native_pin(ex):return ex.lock['commit']
if __name__=='__main__':main()
