"""Run unchanged reviewed NPC callbacks with native Godot SceneTreeTimers.

Inputs replace dialogue/area services at their explicit boundaries. This is a
timer/return-gate oracle, not the original NPC scene, renderer or whole game.
"""
import argparse,hashlib,json,shutil,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.extract_battle_entry import Extractor,require
from tools.reference_movement import extract
from tools.run_scene_reference import invoke

CASES=[
 dict(name='ordinary_stop',staring=False,wander=False,actions=[[0,'stop']]),
 dict(name='timer_during_new_interaction',staring=False,wander=False,actions=[[0,'stop'],[30,'begin'],[80,'stop']]),
 dict(name='overlapping_returns',staring=False,wander=False,actions=[[0,'stop'],[20,'begin'],[30,'stop'],[70,'face']]),
 dict(name='staring_stop',staring=True,wander=False,actions=[[0,'enter'],[0,'stop']]),
 dict(name='staring_exit',staring=True,wander=False,actions=[[0,'enter'],[10,'exit']]),
 dict(name='reenter_blocks_old_return',staring=True,wander=False,actions=[[0,'enter'],[10,'exit'],[40,'enter'],[100,'exit']]),
 dict(name='wander_exit_no_return',staring=True,wander=True,actions=[[0,'enter'],[10,'exit']]),
 dict(name='other_body_ignored',staring=True,wander=False,actions=[[0,'enter'],[10,'other_exit']]),
]
WRAPPER='''extends Node
var global
var talking=true
var mute=true
var _pause_for_interact=false
var _is_looking=false
var staring=false
var wander=false
var initial_dir=Vector2.DOWN
var _input_vector=Vector2.RIGHT
'''
PROBE='''extends SceneTree
class Driver:
    extends Node
    var runner
    func _physics_process(_delta):runner.physics_tick()
    func _process(_delta):runner.idle_tick()
var actors=[]
var cases=[]
var rows=[]
var frame=0
var output=""
var service
func _init():
    for arg in OS.get_cmdline_args():
        if arg.begins_with("--encore-out="):output=arg.trim_prefix("--encore-out=")
    assert(output!="")
    var file=File.new()
    assert(file.open("res://cases.json",File.READ)==OK)
    cases=JSON.parse(file.get_as_text()).result
    file.close()
    service=load("res://global.gd").new()
    get_root().add_child(service)
    for c in cases:
        var actor=load("res://npc.gd").new()
        actor.global=service
        actor.staring=c.staring
        actor.wander=c.wander
        get_root().add_child(actor)
        actors.append(actor)
    var driver=Driver.new()
    driver.runner=self
    get_root().add_child(driver)
func physics_tick():
    for i in cases.size():
        var actor=actors[i]
        for action in cases[i].actions:
            if action[0]!=frame:continue
            match action[1]:
                "stop":actor.stop_interaction()
                "begin":
                    actor._pause_for_interact=true
                    actor._input_vector=Vector2.RIGHT
                "face":
                    actor._input_vector=Vector2.RIGHT
                "enter":actor._on_ViewArea_body_entered(service.get_player())
                "exit":actor._on_ViewArea_body_exited(service.get_player())
                "other_exit":actor._on_ViewArea_body_exited(get_root())
                _:assert(false)
func idle_tick():
    create_timer(0).connect("timeout",self,"capture")
func capture():
    for i in actors.size():
        var a=actors[i]
        rows.append([frame,i,a._input_vector.x,a._input_vector.y,a._is_looking,a._pause_for_interact,a.talking,a.mute])
    frame+=1
    if frame==180:
        var file=File.new()
        assert(file.open(output,File.WRITE)==OK)
        file.store_string(JSON.print({"schema":1,"godot":Engine.get_version_info(),"cases":cases,"rows":rows},"  "))
        file.close()
        quit()
'''


def validate_review(review,pin):
    require(isinstance(review,dict)and set(review)==set('schema commit whole_file_approved source source_sha256 symbols function_sha256 scope semantics'.split()),'NPC return review fields')
    require(type(review['schema'])is int and review['schema']==1 and review['commit']==pin and review['whole_file_approved']is False,'NPC return review pin/scope')
    require(review['source']=='Scripts/Main/npc.gd'and review['symbols']==['stop_interaction','_on_ViewArea_body_entered','_on_ViewArea_body_exited','return_to_init_dir'],'NPC return symbol domain')
    require(isinstance(review['function_sha256'],dict)and set(review['function_sha256'])==set(review['symbols']),'NPC return method identities')


def validate_result(result):
    require(isinstance(result,dict)and set(result)=={'schema','godot','cases','rows'}and type(result['schema'])is int and result['schema']==1,'NPC return result schema')
    require(isinstance(result['godot'],dict)and result['godot'].get('string')=='3.6.2-stable (official)'and result['cases']==CASES and len(result['rows'])==180*len(CASES),'Native reference domain/version mismatch')
    for index,row in enumerate(result['rows']):
        require(isinstance(row,list)and len(row)==8 and row[0]==index//len(CASES)and row[1]==index%len(CASES)and type(row[0])is int and type(row[1])is int,'NPC reference frame order')
        require(all(type(v)in(int,float)for v in row[2:4])and row[2:4]in ([1,0],[0,1])and all(type(v)is bool for v in row[4:]),'NPC reference state values')
    return result


def main():
    p=argparse.ArgumentParser();p.add_argument('--godot',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args();out=a.out.resolve()
    require(out.is_relative_to(ROOT/'build')and out!=ROOT/'build'and not out.exists(),'Fresh owned reference directory required')
    review=json.loads((ROOT/'compatibility/reviews/npc-return-v0410.json').read_bytes());ex=Extractor(ROOT)
    validate_review(review,ex.lock['commit'])
    functions,hashes=extract(ex.data(review['source']),review['source_sha256'],review['symbols']);require(hashes==review['function_sha256'],'Reviewed NPC functions changed')
    out.mkdir(parents=True);project=out/'temporary';project.mkdir();(project/'.owned-npc-reference').write_text('native-timers-v1')
    try:
        (project/'npc.gd').write_text(WRAPPER+'\n'+functions,encoding='utf8')
        (project/'global.gd').write_text('extends Node\nfunc get_player():return self\n',encoding='utf8')
        (project/'probe.gd').write_text(PROBE,encoding='utf8')
        (project/'cases.json').write_text(json.dumps(CASES),encoding='utf8')
        (project/'project.godot').write_text('config_version=4\n[application]\nconfig/name="NPC source timer oracle"\n[logging]\nfile_logging/enable_logging=false\n[physics]\ncommon/physics_fps=60\n',encoding='utf8')
        generated={f.name:hashlib.sha256(f.read_bytes()).hexdigest()for f in project.iterdir()if f.is_file()}
        invoke(a.godot.resolve(),project,['--fixed-fps','60','--script',str(project/'probe.gd'),'--encore-out='+str(out/'reference.json')],out/'godot.log')
        result=validate_result(json.loads((out/'reference.json').read_bytes()))
        transitions={c['name']:[row[0]for row in result['rows']if row[1]==i and row[2:4]==[0,1]and(row[0]==0 or result['rows'][(row[0]-1)*len(CASES)+i][2:4]!=[0,1])]for i,c in enumerate(CASES)}
        receipt=dict(source_commit=review['commit'],scope=review['scope'],adapters=["global.get_player identity injected as a Node peer","UI begin supplies interaction pause/facing inputs","Sprite/wander physics excluded; callbacks and SceneTreeTimers unchanged"],engine_sha256=hashlib.sha256(a.godot.read_bytes()).hexdigest(),source_sha256=ex.sources,methods_sha256=hashes,tool_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),generated_sha256=generated,reference_sha256=hashlib.sha256((out/'reference.json').read_bytes()).hexdigest(),return_frames=transitions,gameplay=False,hardware=False)
        (out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf8');print(json.dumps(transitions),flush=True)
    finally:
        if project.is_relative_to(out)and(project/'.owned-npc-reference').is_file():shutil.rmtree(project)


if __name__=='__main__':main()
