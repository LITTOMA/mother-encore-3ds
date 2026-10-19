"""Native unchanged MOVE call-site capture with explicit service adapters.

The run-SFX playing getter records a conditional-stop instruction and returns
false; _set_running records its call and bool assignment. No audible playback
or full Player/party claim. Separate run-loop reference covers those operations.
"""
import argparse,hashlib,json,math,shutil,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.extract_battle_entry import Extractor,require,PIN
from tools.reference_movement import extract,cases
from tools.run_scene_reference import invoke
MANAGER='''extends Node
class Sfx:
    extends Reference
    var owner
    var playing=false setget ,capture_stop_guard
    func capture_stop_guard():
        owner.body.audio_calls.append(3)
        return false
    func stop():assert(false)
var current=Sfx.new()
var body
func _init():current.owner=self
func get_sfx(slot):
    assert(slot=="run")
    return current
'''
def source():
    ex=Extractor(ROOT);path='Scripts/Main/party/Player.gd';raw=ex.data(path);code,hashes=extract(raw,hashlib.sha256(raw).hexdigest(),['_move','_movement','_move_state'])
    review=json.loads((ROOT/'compatibility/reviews/movement-v0410.json').read_bytes());require(review['sources'][path]==hashlib.sha256(raw).hexdigest()and all(hashes[k]==review['function_sha256'][path][k]for k in ('_move','_movement')),'Prior movement review changed')
    wrapper=(ROOT/'tools/godot_exporter/movement_wrapper.gd').read_text(encoding='utf-8');old='func _set_running(value): _running = value';require(wrapper.count(old)==1,'Movement capture wrapper changed')
    wrapper=wrapper.replace(old,'func _set_running(value):\n    audio_calls.append(2 if value else 1)\n    _running = value')
    wrapper+='\nvar audioManager\nvar global\nvar audio_calls=[]\nvar _state=0\nvar source_input=Vector2.ZERO\nfunc _check_event_collider():pass\nfunc _controls():_input_vector=source_input\n'
    probe=(ROOT/'tools/godot_exporter/movement_probe.gd').read_text(encoding='utf-8');old='    if !input.paused and !input.entering_door: body._movement(delta)';require(probe.count(old)==1,'Movement driver capture changed')
    probe=probe.replace(old,'    body.audio_calls=[]\n    audioManager.body=body\n    body.source_input=body._input_vector\n    global.entering_door=input.entering_door\n    body._move_state(delta)')
    probe=probe.replace('{"input": input, "state": state}','{"input": input, "state": state,"calls":body.audio_calls.duplicate()}')
    probe=probe.replace('extends SceneTree','extends SceneTree\nvar audioManager\nvar global',1)
    probe=probe.replace('func _init():','func _init():\n    audioManager=load("res://audio.gd").new()\n    global=load("res://global.gd").new()\n    get_root().add_child(audioManager)\n    get_root().add_child(global)',1)
    probe=probe.replace('    get_root().add_child(body)','    body.audioManager=audioManager\n    body.global=global\n    get_root().add_child(body)',1)
    probe=probe[:probe.index('func finish():')]+'''func finish():
    var file=File.new()
    assert(file.open(output,File.WRITE)==OK)
    file.store_string(JSON.print(result,"  ")+"\\n")
    file.close()
    quit()
'''
    proof=dict(commit=PIN,whole_gameplay_approved=False,sources=ex.sources,methods=hashes,base_movement_review_sha256=hashlib.sha256((ROOT/'compatibility/reviews/movement-v0410.json').read_bytes()).hexdigest(),wrapper_sha256=hashlib.sha256(wrapper.encode()).hexdigest(),probe_sha256=hashlib.sha256(probe.encode()).hexdigest(),manager_sha256=hashlib.sha256(MANAGER.encode()).hexdigest(),scope='Original _move/_movement/_move_state; MOVE only, no climbing/knockback/teleport/party motion; source _set_running and SFX playing guard are call-capture adapters, not actual audio playback')
    return wrapper+'\n'+code,probe,proof

def validate_result(r):
    require(type(r)is dict and set(r)=={'schema','godot','sequences'}and type(r['schema'])is int and r['schema']==1 and type(r['godot'])is dict and r['godot'].get('string')=='3.6.2-stable (official)','MOVE audio reference schema/engine')
    require(type(r['sequences'])is list and len(r['sequences'])==len(cases()),'MOVE audio reference sequences')
    state_keys=set('x y dx dy vx vy speed crouch tap_run running substantial walking toggle animation moved'.split())
    for sequence,case in zip(r['sequences'],cases()):
        require(type(sequence)is dict and set(sequence)=={'name','initial','frames'}and sequence['name']==case['name']and sequence['initial']==case['position']and type(sequence['frames'])is list and len(sequence['frames'])==len(case['steps']),'MOVE audio reference sequence identity/order')
        for row,inp in zip(sequence['frames'],case['steps']):
            require(type(row)is dict and set(row)=={'input','state','calls'}and row['input']==inp and all(type(row['input'][k])is type(inp[k])for k in inp),'MOVE audio reference input')
            state=row['state'];require(type(state)is dict and set(state)==state_keys,'MOVE audio reference state')
            require(all(type(state[k])in(int,float)and math.isfinite(state[k])for k in ('x','y','dx','dy','vx','vy','speed'))and all(type(state[k])is bool for k in ('crouch','tap_run','running','substantial','walking','toggle'))and state['animation']in('Idle','Walk','Run','Crouch')and type(state['moved'])is int and state['moved']>=0,'MOVE audio reference state domain')
            require(type(row['calls'])is list and len(row['calls'])<=3 and all(type(v)is int and v in(1,2,3)for v in row['calls']),'MOVE audio call domain')
    return r

def fixture(r):
    validate_result(r);lines=['# sequence frame x y toggle paused door calls; source stop guard is a conditional instruction']
    for i,seq in enumerate(r['sequences']):
        for j,row in enumerate(seq['frames']):
            inp=row['input'];lines.append('\t'.join(str(v)for v in [i,j,inp['x'],inp['y'],int(inp['toggle']),int(inp['paused']),int(inp['entering_door']),','.join(str(v)for v in row['calls'])or '-']))
    return '\n'.join(lines)+'\n'

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--godot',type=Path,required=True);ap.add_argument('--out',type=Path,required=True);o=ap.parse_args();out=o.out.resolve();require(out.is_relative_to(ROOT/'build')and out!=ROOT/'build'and not out.exists(),'Fresh owned MOVE reference required');actor,probe,proof=source();require(proof==json.loads((ROOT/'compatibility/reviews/move-audio-v0410.json').read_bytes()),'MOVE audio source review changed');out.mkdir(parents=True);project=out/'temporary';project.mkdir();(project/'.owned-move-audio').write_bytes(b'move-audio-v1')
    files={'movement.gd':actor,'probe.gd':probe,'audio.gd':MANAGER,'global.gd':'extends Node\nvar partyObjects=[]\nvar party=[null]\nvar entering_door=false\n','cases.json':json.dumps(cases()),'metadata.json':json.dumps(dict(schema=1)),'project.godot':'''config_version=4
[application]
config/name="Source MOVE audio call capture"
[logging]
file_logging/enable_logging=false
[input]
ui_toggle={"deadzone":0.5,"events":[]}
ui_accept={"deadzone":0.5,"events":[]}
[physics]
common/physics_fps=60
[rendering]
quality/driver/driver_name="GLES2"
'''}
    try:
        for name,value in files.items():(project/name).write_bytes(value.encode())
        invoke(o.godot.resolve(),project,['--editor','--quit'],out/'import.log')
        invoke(o.godot.resolve(),project,['--script',str(project/'probe.gd'),'--encore-out='+str(out/'reference.json')],out/'godot.log');r=validate_result(json.loads((out/'reference.json').read_bytes()));(out/'calls.tsv').write_bytes(fixture(r).encode())
        receipt=dict(source=proof,engine_sha256=hashlib.sha256(o.godot.read_bytes()).hexdigest(),producer_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),generated_sha256={name:hashlib.sha256((project/name).read_bytes()).hexdigest()for name in files},result_sha256=hashlib.sha256((out/'reference.json').read_bytes()).hexdigest(),scope='Real engine free-motion and input physics edges, conditional audio-call capture only; existing wrapper excludes collisions and non-motion services, not a full Player/scene/audio pass')
        (out/'receipt.json').write_bytes((json.dumps(receipt,indent=2)+'\n').encode());print('MOVE audio:',len(r['sequences']),'sequences',sum(len(s['frames'])for s in r['sequences']),'frames')
    finally:
        if project.resolve().is_relative_to(out)and(project/'.owned-move-audio').read_bytes()==b'move-audio-v1':shutil.rmtree(project)
if __name__=='__main__':main()