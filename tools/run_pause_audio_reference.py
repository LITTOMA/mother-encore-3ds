"""Unchanged Player pause/unpause plus run playback, using bounded adapters."""
import argparse,hashlib,json,shutil,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import run_footstep_reference as r
from tools.reference_movement import extract
from tools.extract_battle_entry import Extractor,require,PIN
from tools.run_scene_reference import invoke
CASES=[['run',True],['pause',False],['resume',False],['run',True],['tap',True],['pause',False],['physics',False],['resume',False],['pause',True],['resume',False],['run',True],['pause',False],['outside',False],['resume',False],['inside',False],['resume',False]]
EXTRA='''const TELEPORTING=1
const AreaRoom=preload("res://area-room.gd")
signal paused
signal unpaused
var _tp_take_off_timer_done=false
var _tp_take_off_timer=Timer.new()
var _tp_crouch_timer=Timer.new()
var _tap_run=false
var _crouch=false
var _walk=false
var _input_vector=Vector2.ZERO
func _ready():
    add_child(_tp_take_off_timer)
    add_child(_tp_crouch_timer)
    var audio=AudioStreamPlayer.new()
    audio.name="AudioStreamPlayer"
    add_child(audio)
    var timer=Timer.new()
    timer.name="MiscTimer"
    add_child(timer)
func _anim_play_pause(_value,_idle=true):pass
func _set_collision_masks(_value):pass
func _start_landing():assert(false)
'''
PROBE='''extends SceneTree
func _init():call_deferred("run")
func run():
    var file=File.new()
    assert(file.open("res://input.json",File.READ)==OK)
    var config=JSON.parse(file.get_as_text()).result
    file.close()
    var manager=load("res://manager.gd").new()
    var actor=load("res://actor.gd").new()
    var world=load("res://world.gd").new()
    var area=load("res://area-room.gd").new()
    var other=Node2D.new()
    world.currentScene=area
    actor.global=world
    actor.audioManager=manager
    actor.run_sound=config.default
    manager.names=config.names
    get_root().add_child(manager)
    get_root().add_child(world)
    world.add_child(area)
    world.add_child(other)
    get_root().add_child(actor)
    var stream=load(config.sound)
    assert(stream is AudioStreamMP3 and stream.loop and stream.loop_offset==0)
    manager.current.stream=stream
    var rows=[]
    for action in config.cases:
        manager.events=[]
        match action[0]:
            "run":actor._set_running(action[1])
            "pause":actor.pause(action[1])
            "resume":actor.unpause()
            "tap":actor._tap_run=action[1]
            "physics":actor._move_state(1.0/60)
            "outside":world.currentScene=other
            "inside":world.currentScene=area
        rows.append({"action":action,"tap":actor._tap_run,"running":actor._running,"playing":manager.current.playing,"paused":actor._paused,"events":manager.events.duplicate(true)})
    assert(file.open("res://../reference.json",File.WRITE)==OK)
    file.store_string(JSON.print({"schema":1,"godot":Engine.get_version_info(),"rows":rows},"  "))
    file.close()
    actor.free()
    manager.free()
    world.free()
    quit()
'''
def source():
    m,actor,_,base=r.source();ex=Extractor(ROOT);path='Scripts/Main/party/Player.gd';raw=ex.data(path);methods,hashes=extract(raw,hashlib.sha256(raw).hexdigest(),['pause','unpause'])
    ui=ex.data('Scripts/global/uiManager.gd');_,ui_hashes=extract(ui,hashlib.sha256(ui).hexdigest(),['open_dialogue_box','open_commands_menu','close_commands_menu'])
    _,physics_hashes=extract(raw,hashlib.sha256(raw).hexdigest(),['_physics_process'])
    raw_actor=ex.data('Scripts/Main/actor.gd');_,actor_hashes=extract(raw_actor,hashlib.sha256(raw_actor).hexdigest(),['init'])
    raw_npc=ex.data('Scripts/Main/npc.gd');_,npc_hashes=extract(raw_npc,hashlib.sha256(raw_npc).hexdigest(),['interact'])
    actor+='\n'+EXTRA+'\n'+methods
    proof=dict(commit=PIN,whole_gameplay_approved=False,sources={**base['sources'],**ex.sources},methods=hashes,ui_methods=ui_hashes,physics_methods=physics_hashes,actor_methods=actor_hashes,npc_methods=npc_hashes,base_run_review_sha256=hashlib.sha256((ROOT/'compatibility/reviews/run-loop-v0410.json').read_bytes()).hexdigest(),actor_sha256=hashlib.sha256(actor.encode()).hexdigest(),probe_sha256=hashlib.sha256(PROBE.encode()).hexdigest(),scope='Actual Player pause/unpause and _set_running/paused MOVE; same run-SFX state adapter and real source MP3, injected timer/animation/collision/party services and AreaRoom type; uiManager/NPC entry gates and Player/Actor physics ownership reviewed, not executed')
    return m,actor,proof
def validate_result(doc):
    require(type(doc)is dict and set(doc)=={'schema','godot','rows'}and type(doc['schema'])is int and doc['schema']==1 and type(doc['godot'])is dict and doc['godot'].get('string')=='3.6.2-stable (official)'and type(doc['rows'])is list and len(doc['rows'])==len(CASES),'Pause audio result schema/count')
    for row,action in zip(doc['rows'],CASES):
        require(type(row)is dict and set(row)==set('action tap running playing paused events'.split())and row['action']==action and type(row['action'])is list and type(row['action'][1])is bool and all(type(row[k])is bool for k in ('tap','running','playing','paused')),'Pause audio row/state')
        require(type(row['events'])is list and len(row['events'])<=1 and all(type(v)is list and len(v)==2 and type(v[0])is int and v[0]in(1,2,3)and type(v[1])is int and v[1]==0 for v in row['events']),'Pause audio event')
    return doc
def fixture(doc):
    validate_result(doc);lines=['# action argument tap running playing paused events']
    for row in doc['rows']:
        values=[row['action'][0],int(row['action'][1]),*[int(row[k])for k in ['tap','running','playing','paused']],','.join(str(e[0])for e in row['events'])or '-'];lines.append('\t'.join(str(v)for v in values))
    return '\n'.join(lines)+'\n'
def main():
    ap=argparse.ArgumentParser();ap.add_argument('--godot',type=Path,required=True);ap.add_argument('--out',type=Path,required=True);o=ap.parse_args();out=o.out.resolve();require(out.is_relative_to(ROOT/'build')and out!=ROOT/'build'and not out.exists(),'Fresh pause reference required');m,actor,proof=source();require(proof==json.loads((ROOT/'compatibility/reviews/pause-audio-v0410.json').read_bytes()),'Pause source review changed');out.mkdir(parents=True);project=out/'temporary';project.mkdir();(project/'.owned-pause-audio').write_bytes(b'pause-audio-v1')
    world='extends Node\nvar currentScene\nvar entering_door=false\nfunc party_call(method):assert(method in ["pause_flash_anim","stop_creating_afterimage","pause_timers","resume_flash_anim","resume_timers"])\n'
    files={'actor.gd':actor,'party.gd':'extends Node2D\nvar shadow=""\nfunc set_shadow(value):shadow=value\n','manager.gd':r.MANAGER,'world.gd':world,'area-room.gd':'extends Node2D\n','probe.gd':PROBE,'input.json':json.dumps(dict(default=m['default'],names=[s['name']for s in m['sounds']],sound=m['sounds'][0]['source'],cases=CASES)),'project.godot':'config_version=4\n[application]\nconfig/name="Native pause audio"\n[logging]\nfile_logging/enable_logging=false\n[rendering]\nquality/driver/driver_name="GLES2"\n'}
    try:
        for name,value in files.items():(project/name).write_bytes(value.encode())
        ex=Extractor(ROOT);name=m['sounds'][0]['source'][6:];target=project/name;target.parent.mkdir(parents=True);target.write_bytes(ex.data(name))
        invoke(o.godot.resolve(),project,['--editor','--quit'],out/'import.log');invoke(o.godot.resolve(),project,['--script',str(project/'probe.gd')],out/'godot.log');doc=validate_result(json.loads((out/'reference.json').read_bytes()));(out/'calls.tsv').write_bytes(fixture(doc).encode())
        (out/'receipt.json').write_bytes((json.dumps(dict(source=proof,producer_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),engine_sha256=hashlib.sha256(o.godot.read_bytes()).hexdigest(),generated_sha256={name:hashlib.sha256((project/name).read_bytes()).hexdigest()for name in files},result_sha256=hashlib.sha256((out/'reference.json').read_bytes()).hexdigest(),scope=proof['scope']),indent=2)+'\n').encode());print('Pause audio:',len(doc['rows']),'native operations')
    finally:
        if project.resolve().is_relative_to(out)and(project/'.owned-pause-audio').read_bytes()==b'pause-audio-v1':shutil.rmtree(project)
if __name__=='__main__':main()