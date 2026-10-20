"""Wall frame/body, audio property writes and deferred method dispatch slice.

Native AnimationPlayer and AudioStreamPlayer run against the original resources.
Only playing-property writes are forwarded through a logging setter. The wall's
original _shake body targets a camera-call recorder. Controlled-object scripts,
scene flags, physical contact, camera execution and audibility are not tested.
"""
import argparse,hashlib,json,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.extract_battle_entry import Extractor,require
from tools.godot_text import Document
from tools.run_scene_reference import invoke
from tools.run_battle_victory_reference import function
from tools.prop_clips import f32
from tools.audio_asset import import_settings

AUDIO='''extends AudioStreamPlayer
var runner
var requested_playing=false setget requested
func requested(value):
    requested_playing=value
    runner.event({"kind":"audio","playing":value})
    playing=value
'''
SERVICE='''extends Node
var currentCamera=self
var runner
func shake_camera(magnitude,duration,direction,interval,weight,diminish):
    runner.event({"kind":"shake","magnitude":magnitude,"duration":duration,"direction":[direction.x,direction.y],"interval":interval,"weight":weight,"diminish":diminish})
'''
PROBE='''extends SceneTree
signal flushed
var target
var animator
var audio
var service
var events=[]
var rows=[]
var results=[]
func _init():call_deferred("run")
func event(value):events.append(value)
func completed(name):event({"kind":"finished","clip":name})
func flush_queue():emit_signal("flushed")
func capture(index,action):
    rows.append({"index":index,"op":action.op,"frame":target.get_node("main").frame,"body":not target.get_node("collision/CollisionShape2D").disabled,"playing_request":audio.requested_playing,"animation_playing":animator.is_playing(),"animation":animator.current_animation if animator.is_playing() else "","position":"%.17f"%animator.current_animation_position,"events":events.duplicate(true)})
    events=[]
func run():
    var file=File.new()
    assert(file.open("res://input.json",File.READ)==OK)
    var config=JSON.parse(file.get_as_text()).result
    file.close()
    assert(file.open("res://audio-policy.json",File.READ)==OK)
    var policy=JSON.parse(file.get_as_text()).result
    file.close()
    service=load("res://service.gd").new()
    service.runner=self
    get_root().add_child(service)
    for test in config:
        events=[]
        rows=[]
        target=load("res://wall.tscn").instance()
        target.global=service
        audio=target.get_node("AudioStreamPlayer")
        # Fresh private imports must not reuse an upstream .mp3str cache path.
        # Apply the reviewed source import options to the native resource.
        audio.stream.loop=policy.loop
        audio.stream.loop_offset=policy.offset
        audio.set_script(load("res://audio.gd"))
        audio.runner=self
        assert(audio.stream is AudioStreamMP3 and not audio.stream.loop)
        animator=target.get_node("AnimationPlayer")
        animator.playback_process_mode=AnimationPlayer.ANIMATION_PROCESS_MANUAL
        assert(animator.method_call_mode==AnimationPlayer.ANIMATION_METHOD_CALL_DEFERRED)
        animator.connect("animation_finished",self,"completed")
        get_root().add_child(target)
        for i in test.actions.size():
            var action=test.actions[i]
            if action.op=="play":animator.play(action.clip)
            elif action.op=="advance":animator.advance(action.delta)
            elif action.op=="flush":
                call_deferred("flush_queue")
                yield(self,"flushed")
            else:assert(false)
            capture(i,action)
        results.append({"name":test.name,"rows":rows})
        target.free()
    assert(file.open("res://observations.json",File.WRITE)==OK)
    file.store_string(JSON.print({"engine":Engine.get_version_info(),"cases":results},"  "))
    file.close()
    print("WALL_TIMELINE_REFERENCE_COMPLETE")
    quit()
'''

def actions():
 def play(name):return dict(op='play',clip=name)
 def advance(delta):return dict(op='advance',delta=delta)
 def flush():return dict(op='flush')
 on=[play('On'),advance(0),advance(0),flush()];previous=0
 for point in [f32(.498),f32(.498)+.000001,f32(1.245),f32(1.245)+.000001,f32(1.992),f32(2.08333)]:
  on.extend([advance(point-previous),flush()]);previous=point
 on.extend([play('StayOff'),advance(0),flush(),play('StayOn'),advance(0),flush(),play('RESET'),advance(0),flush()])
 normal=[play('On')]
 for _ in range(130):normal.extend([advance(1/60),flush()])
 return [dict(name='boundaries_and_persistent_audio_request',actions=on),dict(name='ordinary_steps',actions=normal),
  dict(name='large_step_before_deferred_calls',actions=[play('On'),advance(3),flush()]),
  dict(name='reset_before_pending_methods',actions=[play('On'),advance(3),play('RESET'),advance(0),flush()]),
  dict(name='restart_keeps_queued_methods',actions=[play('On'),advance(.6),play('On'),advance(.6),flush()]),
  dict(name='zero_and_constant_clips',actions=[play('RESET'),advance(0),play('StayOn'),advance(0),advance(.1),flush(),play('StayOff'),advance(0),advance(.1),flush()])]

def run(out,engine):
 out=Path(out).resolve();engine=Path(engine).resolve()
 require(out.is_relative_to((ROOT/'build').resolve())and out!=ROOT/'build'and not out.exists()and engine.is_file(),'Fresh owned wall reference and actual engine required')
 ex=Extractor(ROOT);scene='Nodes/Overworld/switches/Wall Door.tscn';script='Scripts/misc/wall door.gd';raw=ex.text(scene);original=Document(scene,raw)
 require(original.ext[3]==(script,'Script')and len(original.ext)==3 and not original.connections,'Wall source script/resource/connection scope changed')
 require(raw.count('NodePath("AudioStreamPlayer:playing")')==2,'Wall audio property selector changed')
 changed=raw.replace('res://'+script,'res://wall_slice.gd').replace('NodePath("AudioStreamPlayer:playing")','NodePath("AudioStreamPlayer:requested_playing")')
 checked=Document(scene,changed);require(checked.nodes==original.nodes,'Wall node properties changed')
 for ident,(kind,props)in original.sub.items():
  actual=checked.sub[ident];expected=Document(scene,raw.replace('NodePath("AudioStreamPlayer:playing")','NodePath("AudioStreamPlayer:requested_playing")')).sub[ident]
  require(actual==expected,'Wall non-audio animation changed')
 project=out/'project';project.mkdir(parents=True);(project/'.owned-wall-timeline-reference').write_text('source-wall-timeline-reference-v1',encoding='utf8')
 (out/'source-wall.tscn').write_bytes(ex.data(scene));(out/'source-wall.gd').write_bytes(ex.data(script))
 (project/'wall.tscn').write_text(changed,encoding='utf8')
 bridge='extends Sprite\nvar global\nvar _move_cam_when_changed=false\n'+function(ex.text(script),'_shake')
 for name,text in [('wall_slice.gd',bridge),('audio.gd',AUDIO),('service.gd',SERVICE),('probe.gd',PROBE)]: (project/name).write_text(text,encoding='utf8')
 copied={}
 for path,kind in original.ext.values():
  if kind=='Script':continue
  require(kind in ('Texture','AudioStream'),'Wall unknown external resource')
  target=project/path;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(ex.data(path));copied[path]=hashlib.sha256(target.read_bytes()).hexdigest()
  if kind=='AudioStream':
   imported=ex.data(path+'.import');loop,offset=import_settings(imported,'res://'+path);require(loop is False and offset==0,'Wall sound loop policy changed')
   (out/'source-audio-import.txt').write_bytes(imported)
   (project/'audio-policy.json').write_text(json.dumps(dict(loop=loop,offset=offset))+'\n',encoding='utf8')
 config=actions();(project/'input.json').write_text(json.dumps(config,indent=2)+'\n',encoding='utf8')
 (project/'project.godot').write_text('config_version=4\n[application]\nconfig/name="Source wall timeline slice"\n[rendering]\nquality/driver/driver_name="GLES2"\n[logging]\nfile_logging/enable_logging=false\n',encoding='utf8')
 invoke(engine,project,['--debug','--editor','--quit'],out/'import.log');invoke(engine,project,['--debug','--script',str(project/'probe.gd')],out/'engine.log')
 observed=json.loads((project/'observations.json').read_bytes());v=observed['engine'];require((v['major'],v['minor'],v['patch'],v['hash'])==(3,6,2,'3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8'),'Wall timeline engine identity')
 require(len(observed['cases'])==len(config),'Wall timeline incomplete cases')
 for actual,test in zip(observed['cases'],config):
  require(actual['name']==test['name']and len(actual['rows'])==len(test['actions']),'Wall timeline action coverage')
  for i,(row,action)in enumerate(zip(actual['rows'],test['actions'])):require(row['index']==i and row['op']==action['op'],'Wall timeline action order')
 for path,digest in copied.items():require(hashlib.sha256((project/path).read_bytes()).hexdigest()==digest,'Wall source asset modified')
 (out/'observations.json').write_bytes((project/'observations.json').read_bytes());(out/'input.json').write_bytes((project/'input.json').read_bytes())
 proof=dict(commit=ex.lock['commit'],sources=ex.sources,scope=__doc__,whole_scripts_approved=False,cases=len(config),rows=sum(len(v['rows'])for v in observed['cases']),
  adapters=['Original wall scene keeps all subresources/node properties except the two declared playing-property selectors; original script attachment targets isolated _shake-only bridge.','Original _shake method body unchanged, global service only records camera call. No controlled base scripts run.','Fresh native MP3 import, then apply original checked loop=false/offset=0 parameters directly to native resource; do not copy upstream .mp3str cache. Logging requested_playing setter forwards every write to native playing. Records commands only, not PCM clock or audibility.','Native manual AnimationPlayer default DEFERRED method mode; flush barrier queued behind pending calls. All other scene callbacks excluded.'],
  engine_sha256=hashlib.sha256(engine.read_bytes()).hexdigest(),producer_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
  files={p.name:hashlib.sha256(p.read_bytes()).hexdigest()for p in [out/'observations.json',out/'input.json',out/'source-wall.tscn',out/'source-wall.gd',project/'wall.tscn',project/'wall_slice.gd',project/'audio.gd',project/'service.gd',project/'probe.gd']})
 (out/'receipt.json').write_text(json.dumps(proof,indent=2)+'\n',encoding='utf8');print('Original wall timeline:',proof['cases'],'cases,',proof['rows'],'observations; native property/audio writes and deferred method queue, not live scene admission')
 return proof
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',type=Path,required=True);p.add_argument('--godot',type=Path,required=True);a=p.parse_args();run(a.out,a.godot)
