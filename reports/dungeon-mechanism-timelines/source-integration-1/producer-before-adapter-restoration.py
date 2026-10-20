"""Original switch graph scripts combined with their native source animations.

Manual animation callbacks use original node order. Native timers are frozen
only during the manual animation interval, then released normally. Camera
completion is an explicit service. No full map, contact, audibility or hardware.
"""
import argparse,hashlib,json,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import dungeon_switches as d,run_dungeon_switches_reference as s
from tools.run_battle_victory_reference import function
from tools.run_scene_reference import invoke
from tools.godot_text import Document
from tools.audio_asset import import_settings
from tools.run_dungeon_wall_timeline_reference import AUDIO

EXTRA='''signal flushed
var animators=[]
var timeline_events=[]
func event(value):timeline_events.append(value)
func completed(clip,path):timeline_events.append({"kind":"finished","path":path,"clip":clip})
func barrier():emit_signal("flushed")
func setup(target,path):
    var player=target.get_node("AnimationPlayer")
    player.playback_process_mode=AnimationPlayer.ANIMATION_PROCESS_MANUAL
    assert(player.method_call_mode==AnimationPlayer.ANIMATION_METHOD_CALL_DEFERRED)
    player.connect("animation_finished",self,"completed",[path])
    animators.append(player)
    var audio=target.get_node_or_null("AudioStreamPlayer")
    if audio and audio.stream:
        var policy=config.audio[audio.stream.resource_path]
        if audio.stream is AudioStreamMP3:
            audio.stream.loop=policy.loop
            audio.stream.loop_offset=policy.offset
    if target.name==config.wall.get_file():
        audio.set_script(load("res://logged-audio.gd"))
        audio.runner=self
func advance_frames(count,label):
    for index in count:
        for player in animators:
            if player.is_playing():player.advance(1.0/60)
        call_deferred("barrier")
        yield(self,"flushed")
        capture(label+"/"+str(index))
'''

CAPTURE='''func capture(action):
    var nodes=[]
    for definition in config.nodes:
        var target=switches[definition.path] if switches.has(definition.path) else objects[definition.path]
        var player=target.get_node("AnimationPlayer")
        var sprite=target.get_node("Sprite" if definition.kind=="plate" else "main")
        var body=target.get_node_or_null("collision/CollisionShape2D") if definition.kind!="plate" else null
        nodes.append({"path":definition.path,"frame":sprite.frame,"body":not body.disabled if body else false,"assigned":player.assigned_animation,"playing":player.is_playing(),"position":"%.17f"%player.current_animation_position,"on":target.is_on() if switches.has(definition.path) else target._get_state()})
    rows.append({"action":action,"nodes":nodes,"paused":global.player.paused,"cutscene":uiManager.cutscene,"player_events":global.player.events.duplicate(),"ui_events":uiManager.events.duplicate(),"audio_events":audioManager.events.duplicate(true),"camera_events":global.currentCamera.events.duplicate(true),"timeline_events":timeline_events.duplicate(true)})
    global.player.events.clear()
    uiManager.events.clear()
    audioManager.events.clear()
    global.currentCamera.events.clear()
    timeline_events.clear()
'''

RUN='''func run():
    read_config()
    globaldata._init_flags()
    make_scene()
    capture("ready")
    yield(advance_frames(2,"initial"),"completed")
    yield(get_tree().create_timer(0.25),"timeout")
    capture("ready_timers_complete")
    Engine.time_scale=0
    switches[config.main].interact()
    capture("main_interact")
    yield(advance_frames(10,"focus_pending"),"completed")
    global.currentCamera.complete()
    capture("focus_complete")
    yield(advance_frames(145,"opening"),"completed")
    capture("wall_timer_pending")
    Engine.time_scale=1
    yield(get_tree().create_timer(0.6),"timeout")
    capture("wall_timer_complete_before_camera_return")
    global.currentCamera.complete()
    capture("camera_return_complete")
    switches[config.main].interact()
    capture("one_way_repeat")
    switches["Bottom2/switch1"].interact()
    capture("other_switch_on")
    Engine.time_scale=0
    yield(advance_frames(65,"other_switch_animations"),"completed")
    scene.leave(false)
    capture("leave_resets_flag_only")
    Engine.time_scale=1
    scene.queue_free()
    yield(get_tree(),"idle_frame")
    yield(get_tree(),"idle_frame")
    var file=File.new()
    assert(file.open("res://observations.json",File.WRITE)==OK)
    file.store_string(JSON.print({"engine":Engine.get_version_info(),"rows":rows},"  "))
    file.close()
    print("DUNGEON_MECHANISM_TIMELINE_REFERENCE_COMPLETE")
    get_tree().quit()
'''

def probe():
 make=function(s.PROBE,'make_scene')
 replacements=[('target=load("res://"+config.paths.switch_script).new()','target=load("res://"+config.paths[row.kind]).instance()'),('target=load("res://"+config.paths["gate_script" if row.kind=="gate" else "wall_script"]).new()','target=load("res://"+config.paths[row.kind]).instance()')]
 for before,after in replacements:d.require(make.count(before)==1,'Combined source factory scope changed');make=make.replace(before,after)
 start=make.index('            animator(target,["TurnOn","TurnOff"])');end=make.index('            switches[row.path]=target',start)
 make=make[:start]+'            setup(target,row.path)\n'+make[end:]
 before='            animator(target,["On","Off","StayOn","StayOff"] if row.kind=="gate" else ["On","StayOn","StayOff"])';d.require(make.count(before)==1,'Combined object animator scope changed');make=make.replace(before,'            setup(target,row.path)')
 return 'extends Node\nvar config\nvar rows=[]\nvar scene\nvar switches={}\nvar objects={}\nfunc _ready():call_deferred("run")\n'+function(s.PROBE,'read_config')+'\n'+function(s.PROBE,'parent_for')+'\n'+EXTRA+'\n'+make+'\n'+CAPTURE+'\n'+RUN

def run(out,engine):
 out=Path(out).resolve();engine=Path(engine).resolve();d.require(out.is_relative_to((ROOT/'build').resolve())and out!=ROOT/'build'and not out.exists()and engine.is_file(),'Fresh mechanism timeline output and actual engine required')
 review,ex=d.read();data,proof=d.extract();p=review['paths'];project=out/'project';project.mkdir(parents=True);(project/'.owned-mechanism-timeline-reference').write_text('source-mechanism-timeline-v1',encoding='utf8');copied={};audio={}
 def copy(source):
  raw=ex.data(source);target=project/source;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(raw);copied[source]=hashlib.sha256(raw).hexdigest();return raw
 for key in ('flag_script','abstract_script','list_script','switch_script','object_script','two_state_script','gate_script','wall_script'):copy(p[key])
 copy('Scripts/Main/Switches/TwoStatesSyncedSwitch.gd');copy('Audio/Sound effects/M3/bump.wav')
 for key in ('lever','plate','gate','wall'):
  raw=copy(p[key]);doc=Document(p[key],raw.decode())
  for source,kind in doc.ext.values():
   if kind in ('Texture','AudioStream'):copy(source)
   if kind=='AudioStream':
    loop,offset=import_settings(ex.data(source+'.import'),'res://'+source);audio['res://'+source]=dict(loop=loop,offset=offset)
  if key=='wall':
   text=raw.decode();d.require(text.count('NodePath("AudioStreamPlayer:playing")')==2,'Wall requested-property adapter scope');(project/p[key]).write_text(text.replace('NodePath("AudioStreamPlayer:playing")','NodePath("AudioStreamPlayer:requested_playing")'),encoding='utf8')
 config=dict(data=data,paths=p,nodes=review['nodes'],switches={n['path']:n for n in data['switches']},objects={n['path']:n for n in data['objects']},audio=audio,main=next(n['path']for n in data['switches']if n['id']==data['main']),wall=next(n['path']for n in data['objects']if n['id']==data['wall']))
 sources=d.Sources(ROOT);by_path={n.path:n for n in d.instantiate(sources.doc,p['scene'])};config['references']={n['path']:[v.path for v in by_path[n['path']].props['_controlled_objects']]for n in data['switches']}
 (project/'input.json').write_text(json.dumps(config,indent=2)+'\n',encoding='utf8')
 methods='\n'.join(function(ex.text(p['globaldata_script']),n)for n in ('_init_flags','set_flag','set_object_flag'));scene_methods='\n'.join(function(ex.text(p['scene_script']),n)for n in ('_on_main_switch_hit','_on_wall_door_state_changed'))
 adapters={'global.gd':s.GLOBAL,'data.gd':'extends Node\nvar flags={}\nvar object_flags={}\n'+methods,'scene.gd':s.SCENE+scene_methods,'ui.gd':s.UI,'audio.gd':s.AUDIO,'music.gd':s.MUSIC,'probe.gd':probe(),'logged-audio.gd':AUDIO}
 for name,text in adapters.items():(project/name).write_text(text,encoding='utf8')
 (project/'probe.tscn').write_text('[gd_scene load_steps=2 format=2]\n[ext_resource path="res://probe.gd" type="Script" id=1]\n[node name="Probe" type="Node"]\nscript = ExtResource( 1 )\n',encoding='utf8')
 (project/'project.godot').write_text('config_version=4\n[application]\nconfig/name="Source mechanism timelines"\nrun/main_scene="res://probe.tscn"\n[autoload]\nglobal="*res://global.gd"\nglobaldata="*res://data.gd"\nuiManager="*res://ui.gd"\naudioManager="*res://audio.gd"\n[debug]\ngdscript/warnings/unused_argument=false\n',encoding='utf8')
 invoke(engine,project,['--debug','--editor','--quit'],out/'import.log');invoke(engine,project,['--debug'],out/'engine.log')
 obs=json.loads((project/'observations.json').read_bytes());v=obs['engine'];d.require((v['major'],v['minor'],v['patch'],v['hash'])==(3,6,2,'3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8'),'Combined reference engine changed')
 for source,digest in copied.items():
  if source!=p['wall']:d.require(hashlib.sha256((project/source).read_bytes()).hexdigest()==digest,'Combined source copy changed')
 (out/'observations.json').write_bytes((project/'observations.json').read_bytes());(out/'input.json').write_bytes((project/'input.json').read_bytes())
 proof.update(scope=__doc__,producer_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),engine_sha256=hashlib.sha256(engine.read_bytes()).hexdigest(),copied_source_sha256=copied,reference_dependencies=ex.sources,rows=len(obs['rows']),adapter_sha256={name:hashlib.sha256((project/name).read_bytes()).hexdigest()for name in adapters})
 (out/'receipt.json').write_text(json.dumps(proof,indent=2)+'\n',encoding='utf8');print('Combined source mechanism timelines:',len(obs['rows']),'observations; original scripts/native animations, explicit timer freeze/camera service, no map/audible/hardware claim')
 return proof

if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',type=Path,required=True);p.add_argument('--godot',type=Path,required=True);a=p.parse_args();run(a.out,a.godot)
