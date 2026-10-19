"""Execute source run-loop calls and step callbacks in native Godot.

Actual pinned MP3s/import settings are loaded; named-SFX playback is an injected
state/event service. No sound is output and no movement/area physics is run.
"""
import argparse,hashlib,json,shutil,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.extract_battle_entry import Extractor,require,PIN
from tools.reference_movement import extract
from tools.run_scene_reference import invoke
from tools.field_footsteps import source as step_source
# run(enabled,paused,MOVE,climbing), area(index,enter,player), enable(index,value),
# actual source paused/door _move_state branches.
CASES=[[0,1,0,1,0],[0,1,0,1,0],[1,1,1,1,0],[0,1,0,1,0],[0,0,0,1,0],[1,1,0,1,0],[0,1,1,1,0],[0,1,0,0,0],[0,1,0,1,1],[0,1,0,1,0],[2,1,0,0,0],[1,1,1,1,0],[2,1,1,0,0],[1,1,1,0,0],[1,1,1,1,0],[3,0,0,0,0],[0,1,1,1,0],[0,1,0,1,0],[1,1,0,1,0],[0,0,1,1,0],[0,0,0,1,0],[1,0,0,1,0],[1,0,1,1,0],[0,1,0,1,1],[0,1,0,1,0],[4,0,0,0,0],[0,0,0,1,0],[0,1,0,1,0],[0,0,0,1,0]]
PROBE='''extends SceneTree
func _init():call_deferred("run")
func read_json(path):
    var file=File.new()
    assert(file.open(path,File.READ)==OK)
    var data=JSON.parse(file.get_as_text()).result
    file.close()
    return data
func run():
    var config=read_json("res://input.json")
    var manager=load("res://manager.gd").new()
    var actor=load("res://actor.gd").new()
    var world=load("res://world.gd").new()
    var other=load("res://party.gd").new()
    world.player=actor
    actor.global=world
    actor.audioManager=manager
    actor.run_sound=config.default
    manager.names=config.names
    var sounds=[]
    for path in config.sounds:
        var stream=load(path)
        assert(stream is AudioStreamMP3 and stream.loop and stream.loop_offset==0)
        sounds.append(stream)
    manager.current.stream=sounds[config.names.find(config.default)]
    var areas=[]
    for row in config.areas:
        var area=load("res://step.gd").new()
        area.global=world
        area.entering_sound=row.enter
        area.exiting_sound=row.exit
        area.enter_shadow_effect=row.enter_shadow
        area.exit_shadow_effect=row.exit_shadow
        area.enabled=row.enabled
        areas.append(area)
    var rows=[]
    for action in config.cases:
        manager.events=[]
        match int(action[0]):
            0:
                actor._paused=bool(action[2])
                actor._state=actor.MOVE if bool(action[3]) else 1
                actor._climbing=bool(action[4])
                actor._set_running(bool(action[1]))
            1:
                var body=actor if bool(action[3]) else other
                if bool(action[2]):areas[int(action[1])]._on_Stepping_Sounds_body_entered(body)
                else:areas[int(action[1])]._on_Stepping_Sounds_body_exited(body)
            2:
                if bool(action[2]):areas[int(action[1])].enable()
                else:areas[int(action[1])].disable()
            3,4:
                actor._paused=int(action[0])==3
                world.entering_door=int(action[0])==4
                actor._move_state(1.0/60)
                world.entering_door=false
        rows.append({"action":action,"wanted":config.names.find(actor.run_sound),"registered":manager.names.find(manager.current.stream.resource_path.get_file().get_basename()),"playing":manager.current.playing,"running":actor._running,"events":manager.events.duplicate(true)})
    var file=File.new()
    assert(file.open("res://../reference.json",File.WRITE)==OK)
    file.store_string(JSON.print({"schema":1,"godot":Engine.get_version_info(),"rows":rows},"  "))
    file.close()
    for area in areas:area.free()
    actor.free()
    manager.free()
    world.free()
    other.free()
    quit()
'''
MANAGER='''extends Node
class Sfx:
    extends Reference
    var stream
    var playing=false
    var owner
    func play():
        playing=true
        owner.events.append([1,owner.names.find(stream.resource_path.get_file().get_basename())])
    func stop():
        playing=false
        owner.events.append([3,owner.names.find(stream.resource_path.get_file().get_basename())])
var current=Sfx.new()
var events=[]
var names=[]
func _init():current.owner=self
func get_sfx(_slot):return current
func play_sfx(stream,_slot):
    current.stream=stream
    current.playing=true
    events.append([2,names.find(stream.resource_path.get_file().get_basename())])
'''

def source():
    p,m,step_proof=step_source();ex=Extractor(ROOT);raw=ex.data(p['player_script']);methods,hashes=extract(raw,hashlib.sha256(raw).hexdigest(),['_set_running','_move_state']);raw=ex.data(p['area_script']);callbacks,area_hashes=extract(raw,hashlib.sha256(raw).hexdigest(),['_on_Stepping_Sounds_body_entered','_on_Stepping_Sounds_body_exited','set_player_run_sound','enable','disable']);parent=ex.text('Scripts/Main/party/party_object.gd');require('var _climbing := false'in parent,'Source run-loop climbing default changed')
    player=ex.text(p['player_script']);require('var _state := MOVE'in player and 'var _running := false'in player and 'enum {\n\tMOVE,'in player,'Source run-loop initial state changed')
    actor='extends "res://party.gd"\nconst MOVE=0\nvar _state=MOVE\nvar _running=false\nvar _paused=false\nvar _climbing=false\nvar run_sound=""\nvar audioManager\nvar global\nfunc _check_event_collider():pass\nfunc _controls():pass\nfunc _movement(_delta):assert(false)\n'+methods
    area='extends Area2D\nconst PartyObject=preload("res://party.gd")\nvar global\nvar entering_sound=""\nvar exiting_sound=""\nvar enter_shadow_effect=""\nvar exit_shadow_effect=""\nvar enabled=true\n'+callbacks
    proof=dict(commit=PIN,whole_gameplay_approved=False,sources=dict(sorted({**step_proof['sources'],**ex.sources}.items())),player_methods=hashes,area_methods=area_hashes,scope='Original _set_running and paused/door _move_state branches; original step callbacks; manual calls, no input, movement, physics or real SFX manager')
    return m,actor,area,proof

def validate_result(r):
    require(type(r)is dict and set(r)=={'schema','godot','rows'}and type(r['schema'])is int and r['schema']==1 and type(r['godot'])is dict and r['godot'].get('string')=='3.6.2-stable (official)','Run-loop reference result version/schema')
    require(type(r['rows'])is list and len(r['rows'])==len(CASES),'Run-loop reference row count')
    for i,row in enumerate(r['rows']):
        require(type(row)is dict and set(row)==set('action wanted registered playing running events'.split())and row['action']==CASES[i]and all(type(x)is int for x in row['action']),'Run-loop reference case/order')
        require(all(type(row[k])is int and 0<=row[k]<2 for k in ('wanted','registered'))and all(type(row[k])is bool for k in ('playing','running')),'Run-loop reference state domain')
        require(type(row['events'])is list and len(row['events'])<=1 and all(type(e)is list and len(e)==2 and type(e[0])is int and 1<=e[0]<=3 and type(e[1])is int and 0<=e[1]<2 for e in row['events']),'Run-loop reference event domain')
    return r

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--godot',type=Path,required=True);ap.add_argument('--out',type=Path,required=True);o=ap.parse_args();out=o.out.resolve();require(out.is_relative_to(ROOT/'build')and out!=ROOT/'build'and not out.exists(),'Fresh owned run-loop reference directory required');m,actor,area,proof=source();review=json.loads((ROOT/'compatibility/reviews/run-loop-v0410.json').read_bytes());require(type(review)is dict and review.get('whole_gameplay_approved')is False and review==proof,'Run-loop source review changed')
    out.mkdir(parents=True);project=out/'temporary';project.mkdir();(project/'.owned-run-loop-reference').write_text('native-run-loop-v1')
    files={'actor.gd':actor,'step.gd':area,'manager.gd':MANAGER,'world.gd':'extends Node\nvar player\nvar entering_door=false\nfunc get_player():return player\n','party.gd':'extends Node2D\nvar shadow=""\nfunc set_shadow(effect):shadow=effect\n','probe.gd':PROBE,'input.json':json.dumps(dict(default=m['default'],names=[s['name']for s in m['sounds']],sounds=[s['source']for s in m['sounds']],areas=m['areas'],cases=CASES)),'project.godot':'config_version=4\n[application]\nconfig/name="Source run-loop callbacks"\n[logging]\nfile_logging/enable_logging=false\n[rendering]\nquality/driver/driver_name="GLES2"\n'}
    try:
        for name,value in files.items():(project/name).write_bytes(value.encode())
        ex=Extractor(ROOT)
        for sound in m['sounds']:
            for name in (sound['source'][6:],):
                data=ex.data(name);target=project/name;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(data)
        invoke(o.godot.resolve(),project,['--editor','--quit'],out/'import.log')
        invoke(o.godot.resolve(),project,['--script',str(project/'probe.gd')],out/'godot.log');result=validate_result(json.loads((out/'reference.json').read_bytes()))
        receipt=dict(source=proof,producer_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),engine_sha256=hashlib.sha256(o.godot.read_bytes()).hexdigest(),generated_sha256={name:hashlib.sha256((project/name).read_bytes()).hexdigest()for name in files},copied_original_media=ex.sources,result_sha256=hashlib.sha256((out/'reference.json').read_bytes()).hexdigest(),adapters=['Manual calls use unchanged Player and stepping functions','Player initial MOVE/running/climbing state and original step-area exports are injected','Ordinary movement body is an assert-false stub; only paused/door _move_state branches execute','Actual pinned MP3 files are freshly imported; native loop=true/offset=0 are checked against original source settings; SFX stream identity/playing/events are injected, no audible output','PartyObject supplies only type identity and idempotent shadow setter; no shadow-rendering claim'],scope='29 native source call outcomes; no movement/area-physics, whole manager, scene or hardware pass')
        (out/'receipt.json').write_bytes((json.dumps(receipt,indent=2)+'\n').encode());print('Source run-loop:',len(result['rows']),'native state/event outcomes')
    finally:
        if project.resolve().is_relative_to(out)and(project/'.owned-run-loop-reference').read_text()=='native-run-loop-v1':shutil.rmtree(project)
if __name__=='__main__':main()
