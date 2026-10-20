"""Original Shaker math/RNG slice, manually stepped; not a scene or camera tween oracle."""
import argparse, hashlib, json, sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.extract_battle_entry import Extractor,require
from tools.run_scene_reference import invoke

PROBE='''extends SceneTree
var target
var shakers=[]
var mirror=RandomNumberGenerator.new()
var count=0
var calls=[]
var rows=[]
var result=[]
func _init():call_deferred("run")
func real32(value):
    var buffer=StreamPeerBuffer.new()
    buffer.put_float(value)
    buffer.seek(0)
    return buffer.get_float()
func random_range(low,high):
    var value=rand_range(low,high)
    var raw=[str(mirror.randi())]
    if raw[0]!="0":
        raw.append(str(mirror.randi()))
        raw.append(str(mirror.randi()))
    count+=raw.size()
    calls.append({"value":"%.17f"%value,"raw":raw})
    return value
func run():
    var file=File.new()
    assert(file.open("res://input.json",File.READ)==OK)
    var cases=JSON.parse(file.get_as_text()).result
    file.close()
    for test in cases:
        seed(test.seed)
        mirror.seed=int(test.seed)
        count=0
        calls=[]
        rows=[]
        shakers=[]
        target=load("res://target.gd").new()
        get_root().add_child(target)
        for tick in test.deltas.size():
            for action in test.actions:
                if action.tick==tick:
                    var shaker=load("res://shaker.gd").new(target,"shake_value")
                    shaker.set_physics_process(false)
                    shaker.runner=self
                    shaker.set_shake_direction(Vector2(action.direction[0],action.direction[1]))
                    shaker.set_shake_magnitude(action.magnitude)
                    shaker.set_shake_length(action.duration)
                    shaker.set_shake_interval(action.interval)
                    shaker.set_shake_weight(action.weight)
                    shaker.set_shake_diminish(action.diminish).start()
                    shakers.append(shaker)
            var before=target.shake_value
            for shaker in shakers:
                if shaker._shaked_object!=null:shaker._physics_process(real32(test.deltas[tick]))
            rows.append({"tick":tick,"before":[before.x,before.y],"after":[target.shake_value.x,target.shake_value.y],"raw_count":count,"state":str(mirror.state),"calls":calls.duplicate(true)})
            calls=[]
        var tail=[]
        for _i in range(3):
            var actual=str(randi())
            assert(actual==str(mirror.randi()))
            tail.append(actual)
        result.append({"name":test.name,"rows":rows,"tail":tail})
        target.free()
    assert(file.open("res://observations.json",File.WRITE)==OK)
    file.store_string(JSON.print({"engine":Engine.get_version_info(),"cases":result},"  "))
    file.close()
    print("CAMERA_SHAKER_REFERENCE_COMPLETE")
    quit()
'''

def cases():
 def action(tick=0,magnitude=5,duration=.5,direction=(0,0),interval=.02,weight=1,diminish=False):
  return dict(tick=tick,magnitude=magnitude,duration=duration,direction=list(direction),interval=interval,weight=weight,diminish=diminish)
 def case(name,actions,seed=123,deltas=None):return dict(name=name,seed=seed,actions=actions,deltas=[1/60]*100 if deltas is None else deltas)
 return [case('wall_half_second',[action()]),case('wall_one_second',[action(duration=1)]),
  case('wall_second_seed',[action()],seed=314159),
  case('random_diminish',[action(diminish=True,weight=.5)]),
  case('one_step_per_large_frame',[action(duration=1)],deltas=[.2]*60),
  case('zero_delta_and_threshold',[action(magnitude=.4,duration=.2,diminish=True)],deltas=[0,.01,.01,0,.03]*14),
  case('last_two_steps_still_draw',[action(duration=.04)],deltas=[.02]*15),
  case('overlap',[action(),action(tick=6,duration=.2,weight=.5)]),
  case('random_and_axis',[action(weight=.5),action(tick=3,duration=.2,direction=(1,0),weight=.5,diminish=True)]),
  case('diagonal_fixed',[action(direction=(1,1),diminish=True,weight=.5)])]

def prepare(out,engine):
 out=Path(out).resolve();engine=Path(engine).resolve()
 require(out.is_relative_to((ROOT/'build').resolve())and out!=ROOT/'build'and not out.exists()and engine.is_file(),'Fresh owned build output and actual engine required')
 ex=Extractor(ROOT);source=ex.text('Scripts/misc/Shaker.gd');camera=ex.text('Scripts/Main/Camera2D.gd');wall=ex.text('Scripts/misc/wall door.gd');scene=ex.text('Nodes/Overworld/switches/Wall Door.tscn')
 require('shake_camera(5, duration, Vector2.ZERO, 0.02, 1, false)'in wall,'Wall shake binding changed')
 require('"args": [ 0.5 ]'in scene and '"args": [ 1.0 ]'in scene,'Wall duration source changed')
 # Forward both call sites to the GLOBAL builtin, never RNG.randf_range.
 require(source.count('rand_range(-1, 1)')==2,'Shaker random call sites changed')
 adapted=source.replace('class_name Shaker','class_name Shaker\nvar runner').replace('rand_range(-1, 1)','runner.random_range(-1, 1)')
 project=out/'project';project.mkdir(parents=True);(project/'.owned-camera-shaker-reference').write_text('source-camera-shaker-reference-v1',encoding='utf8')
 (out/'source-shaker.gd').write_bytes(ex.data('Scripts/misc/Shaker.gd'))
 (project/'shaker.gd').write_text(adapted,encoding='utf8');(project/'target.gd').write_text('extends Node2D\nvar shake_value=Vector2.ZERO\n',encoding='utf8');(project/'probe.gd').write_text(PROBE,encoding='utf8')
 config=cases();(project/'input.json').write_text(json.dumps(config,indent=2)+'\n',encoding='utf8')
 (project/'project.godot').write_text('config_version=4\n_global_script_classes=[{"base":"Node","class":"Shaker","language":"GDScript","path":"res://shaker.gd"}]\n_global_script_class_icons={"Shaker":""}\n[application]\nconfig/name="Source camera shaker slice"\n[rendering]\nquality/driver/driver_name="GLES2"\n[logging]\nfile_logging/enable_logging=false\n',encoding='utf8')
 invoke(engine,project,['--debug','--script',str(project/'probe.gd')],out/'engine.log')
 observed=json.loads((project/'observations.json').read_bytes());version=observed['engine']
 require((version['major'],version['minor'],version['patch'],version['hash'])==(3,6,2,'3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8'),'Camera shaker engine changed')
 require(len(observed['cases'])==len(config),'Incomplete camera shaker cases')
 lines=['# Original manual Shaker slice; parent display samples before child writes; exact input deltas.']
 total=0;retry=0
 for test,actual in zip(config,observed['cases']):
  require(actual['name']==test['name']and len(actual['rows'])==len(test['deltas']),'Camera shaker case mismatch')
  lines.append('case '+test['name']+' '+str(test['seed']))
  for tick,(delta,row)in enumerate(zip(test['deltas'],actual['rows'])):
   require(row['tick']==tick and len(row['calls'])%2==0,'Camera shaker RNG order mismatch')
   for action in test['actions']:
    if action['tick']==tick:lines.append('shake '+' '.join(map(str,[action['magnitude'],action['duration'],*action['direction'],action['interval'],action['weight'],int(action['diminish'])])))
   lines.append('frame '+' '.join(map(str,[repr(delta),*row['before'],row['raw_count'],int(row['state'])&((1<<64)-1)])))
   # More than one direction attempt in a step is a real repeated-direction rejection.
   retry+=len(row['calls'])>2;total+=1
  lines.append('tail '+' '.join(actual['tail']))
 (out/'states.tsv').write_text('\n'.join(lines)+'\n',encoding='utf8');(out/'observations.json').write_bytes((project/'observations.json').read_bytes());(out/'input.json').write_bytes((project/'input.json').read_bytes())
 proof=dict(commit=ex.lock['commit'],sources=ex.sources,scope=__doc__,cases=len(config),rows=total,frames_with_multiple_direction_attempts=retry,
  source_unchanged_except='Two exact rand_range call sites forward to GLOBAL builtin with raw-stream mirror; add runner reference. All Shaker methods otherwise unchanged.',
  boundaries=['Explicit manual Shaker physics; input delta round-trips native binary32 as the engine callback; no automatic physics/timer dispatch','Node2D property adapter; no full Camera2D script, recovery tween or idle completion signal','No live scene, audio, viewport, release integration or hardware claim'],
  engine_sha256=hashlib.sha256(engine.read_bytes()).hexdigest(),tool_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
  files={p.name:hashlib.sha256(p.read_bytes()).hexdigest()for p in [out/'states.tsv',out/'observations.json',out/'input.json',out/'source-shaker.gd',project/'shaker.gd',project/'probe.gd']})
 (out/'receipt.json').write_text(json.dumps(proof,indent=2)+'\n',encoding='utf8');print('Original camera Shaker:',len(config),'cases,',total,'manual physics observations;',retry,'frames with repeat attempts')
 return proof
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',type=Path,required=True);p.add_argument('--godot',type=Path,required=True);a=p.parse_args();prepare(a.out,a.godot)
