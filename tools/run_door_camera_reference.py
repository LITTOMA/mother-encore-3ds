"""Original non-unit Shaker direction slice; no whole door, camera tween or scene."""
import argparse,hashlib,json,re,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.extract_battle_entry import Extractor,require
from tools.run_camera_shaker_reference import PROBE
from tools.run_scene_reference import invoke
def run(out,engine):
 out=Path(out).resolve();engine=Path(engine).resolve();require(out.is_relative_to((ROOT/'build').resolve())and out!=ROOT/'build'and not out.exists()and engine.is_file(),'Fresh door camera output and actual engine required')
 ex=Extractor(ROOT);shaker=ex.text('Scripts/misc/Shaker.gd');camera=ex.text('Scripts/Main/Camera2D.gd');door=ex.text('Scripts/Main/Openable Door.gd')
 calls=re.findall(r'shake_camera\(([0-9.]+),\s*([0-9.]+),\s*Vector2\(([0-9.+-]+),\s*([0-9.+-]+)\)\)',door);require(len(calls)==1,'Door camera call scope');magnitude,duration,x,y=map(float,calls[0]);direction=[x,y]
 step=re.findall(r'const SHAKE_STEP_TIME := ([0-9.]+)',camera);weight=re.findall(r'lerp_weight := ([0-9.]+), diminish := true\)',camera);require(len(step)==len(weight)==1,'Door source camera defaults');interval=float(step[0]);weight=float(weight[0])
 house=json.loads((ROOT/'content/native-house.json').read_bytes());require(house['openable_doors']and all(d['ram_strength']==magnitude and d['ram_duration']==duration and d['ram_direction']==direction for d in house['openable_doors']),'House compiled ram policy differs from actual source')
 def case(name,mag,d,vec):return dict(name=name,seed=123,actions=[dict(tick=0,magnitude=mag,duration=d,direction=vec,interval=interval,weight=weight,diminish=True)],deltas=[1/60]*60)
 config=[case('source_openable_door',magnitude,duration,direction),case('nonunit_diagonal_fixture',magnitude,duration,[x,-1]),case('larger_weighted_fixture',4,.4,direction)]
 require(shaker.count('rand_range(-1, 1)')==2,'Door shaker RNG call-site scope');adapted=shaker.replace('class_name Shaker','class_name Shaker\nvar runner').replace('rand_range(-1, 1)','runner.random_range(-1, 1)')
 project=out/'project';project.mkdir(parents=True);(project/'.owned-door-camera-reference').write_text('source-door-camera-reference-v1',encoding='utf8')
 for name,text in [('shaker.gd',adapted),('probe.gd',PROBE),('target.gd','extends Node2D\nvar shake_value=Vector2.ZERO\n')]: (project/name).write_text(text,encoding='utf8')
 (project/'input.json').write_text(json.dumps(config,indent=2)+'\n',encoding='utf8');(project/'project.godot').write_text('config_version=4\n_global_script_classes=[{"base":"Node","class":"Shaker","language":"GDScript","path":"res://shaker.gd"}]\n_global_script_class_icons={"Shaker":""}\n[application]\nconfig/name="Source nonunit door camera"\n[rendering]\nquality/driver/driver_name="GLES2"\n[logging]\nfile_logging/enable_logging=false\n',encoding='utf8')
 invoke(engine,project,['--debug','--script',str(project/'probe.gd')],out/'engine.log');observed=json.loads((project/'observations.json').read_bytes());v=observed['engine'];require((v['major'],v['minor'],v['patch'],v['hash'])==(3,6,2,'3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8'),'Door camera engine version')
 require(len(observed['cases'])==3,'Door camera case coverage');lines=['# Source door direction plus declared numeric Shaker parameter fixtures; native binary32 physics delta.']
 for test,actual in zip(config,observed['cases']):
  a=test['actions'][0];require(actual['name']==test['name']and len(actual['rows'])==60,'Door camera observations missing')
  lines.append('case '+' '.join(map(str,[test['name'],a['magnitude'],a['duration'],*a['direction'],a['interval'],a['weight'],int(a['diminish'])])))
  for delta,row in zip(test['deltas'],actual['rows']):require(row['raw_count']==0 and row['calls']==[],'Nonzero direction unexpectedly consumes source RNG');lines.append('frame '+' '.join(map(str,[repr(delta),*row['before']])))
 (out/'states.tsv').write_text('\n'.join(lines)+'\n',encoding='utf8');(out/'input.json').write_bytes((project/'input.json').read_bytes());(out/'observations.json').write_bytes((project/'observations.json').read_bytes())
 proof=dict(commit=ex.lock['commit'],sources=ex.sources,scope=__doc__,source_call=dict(magnitude=magnitude,duration=duration,direction=direction),house_ir_sha256=hashlib.sha256((ROOT/'content/native-house.json').read_bytes()).hexdigest(),rows=180,cases=3,fixture_scope='First case from actual door call; remaining two explicitly synthetic parameters on unchanged original Shaker. No full door state, Camera2D wrapper, audible output, viewport or hardware claim.',engine_sha256=hashlib.sha256(engine.read_bytes()).hexdigest(),producer_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),shared_probe_sha256=hashlib.sha256(PROBE.encode()).hexdigest(),outputs={p.name:hashlib.sha256(p.read_bytes()).hexdigest()for p in [out/'states.tsv',out/'observations.json',out/'input.json']})
 (out/'receipt.json').write_text(json.dumps(proof,indent=2)+'\n',encoding='utf8');print('Original scaled door Shaker:180 observations; real(2,0)door request and2declared numeric fixtures; zero random consumption')
 return proof
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',type=Path,required=True);p.add_argument('--godot',type=Path,required=True);a=p.parse_args();run(a.out,a.godot)
