"""Verify original animation restart resumes both pending state continuations.

Uses the same explicit method services as the28-row reference, not source
timelines, real physics, audible output or a playable dungeon.
"""
import argparse,hashlib,json,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import run_dungeon_switches_reference as reference
from tools.run_scene_reference import invoke
PROBE='''extends "res://base_probe.gd"
var history=[]
func changed(state,silent,path):history.append({"path":path,"state":state,"silent":silent})
func run():
    read_config()
    globaldata._init_flags()
    make_scene()
    for path in objects:objects[path].connect("state_changed",self,"changed",[path])
    complete_animations()
    yield(get_tree().create_timer(0.25),"timeout")
    history.clear()
    switches["Bottom2/switch1"].interact()
    var other=Node.new()
    switches["Bottom2/switch1"]._on_hit(other)
    other.free()
    capture("twice_requested_before_finish")
    complete_animations()
    capture("one_finish_resumes_both")
    var file=File.new()
    assert(file.open("res://restart.json",File.WRITE)==OK)
    file.store_string(JSON.print({"rows":rows,"history":history,"engine":Engine.get_version_info()},"  "))
    file.close()
    scene.queue_free()
    yield(get_tree(),"idle_frame")
    yield(get_tree(),"idle_frame")
    get_tree().quit()
'''
def run(out,engine):
 out=Path(out).resolve();engine=Path(engine).resolve();reference.run(out,engine);project=out/'project'
 (project/'probe.gd').rename(project/'base_probe.gd');(project/'probe.gd').write_text(PROBE,encoding='utf8')
 invoke(engine,project,['--debug'],out/'restart-engine.log');observed=json.loads((project/'restart.json').read_bytes());history=observed['history']
 reference.d.require(len(history)==8 and len(observed['rows'])==2,'Source restart did not resume8captured control states')
 data,proof=reference.d.extract();switch=next(n for n in data['switches']if n['path']=='Bottom2/switch1');targets={edge[1]for edge in data['edges'][switch['first']:switch['first']+switch['count']]}
 for obj in data['objects']:
  entries=[n for n in history if n['path']==obj['path']]
  expected=[not bool(obj['bits']&1),bool(obj['bits']&1)]if obj['id']in targets else []
  reference.d.require([n['state']for n in entries]==expected and all(n['silent']is False for n in entries),'Original restart state/connection order differs')
 for source,digest in json.loads((out/'receipt.json').read_bytes())['copied_scripts'].items():reference.d.require(hashlib.sha256((project/source).read_bytes()).hexdigest()==digest,'Original restart script modified')
 (out/'restart.json').write_bytes((project/'restart.json').read_bytes());(out/'restart-receipt.json').write_text(json.dumps(dict(scope=__doc__,source=proof,events=8,adapter_sha256=hashlib.sha256(PROBE.encode()).hexdigest()),indent=2)+'\n',encoding='utf8');print('Original restart: one animation finish resumes both pending state snapshots on each of4controlled objects,8events')
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',type=Path,required=True);p.add_argument('--godot',type=Path,required=True);a=p.parse_args();run(a.out,a.godot)
