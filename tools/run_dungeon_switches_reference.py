"""Original switch/control methods with explicit animation/camera/player services.

Source script copies are exact. AnimationPlayer uses named request clips;
source timelines, collision, audible output and full-map gameplay are excluded.
Camera completion is explicitly released, permitting asynchronous order checks.
"""
import argparse,hashlib,json,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import dungeon_switches as d
from tools.run_battle_victory_reference import function
from tools.run_scene_reference import invoke

GLOBAL='''extends Node
signal flags_updated
class CameraService:
    extends Node
    signal release
    var events=[]
    func complete():emit_signal("release")
    func move_camera(position,duration):
        events.append({"kind":"move","x":position.x,"y":position.y,"duration":duration})
        yield(self,"release")
    func return_camera(duration):
        events.append({"kind":"return","duration":duration})
        yield(self,"release")
    func shake_camera(a,b,c,d,e,f):events.append({"kind":"shake","arguments":[a,b,c.x,c.y,d,e,f]})
class PlayerService:
    extends Node
    var paused=false
    var events=[]
    var camera
    func is_paused():return paused
    func pause():
        paused=true
        events.append("pause")
    func unpause():
        paused=false
        events.append("unpause")
var currentScene
var currentCamera
var player
func _ready():
    player=PlayerService.new()
    add_child(player)
    currentCamera=CameraService.new()
    add_child(currentCamera)
    player.camera=currentCamera
func get_player():return player
func broadcast_flags_updated():emit_signal("flags_updated")
'''
SCENE='''extends Node2D
signal area_left(region_changed)
signal synced_switches_changed(emitter,value,silent)
var can_switch=true
func get_switches_state():return false
func leave(changed):emit_signal("area_left",changed)
func notify_synced(emitter,value,silent):emit_signal("synced_switches_changed",emitter,value,silent)
'''
UI='''extends Node
var events=[]
var cutscene=false
func set_cutscene(value):
    cutscene=value
    events.append(value)
'''
AUDIO='''extends Node
var events=[]
func music_fadeout(value):events.append({"kind":"fade","value":value})
func play_sfx(stream,channel):events.append({"kind":"sfx","source":stream.resource_path,"channel":channel})
'''
MUSIC='''extends Node
func play_music():audioManager.events.append({"kind":"resume"})
'''
PROBE='''extends Node
var config
var rows=[]
var scene
var switches={}
var objects={}
func _ready():call_deferred("run")
func read_config():
    var file=File.new()
    assert(file.open("res://input.json",File.READ)==OK)
    config=JSON.parse(file.get_as_text()).result
    file.close()
func parent_for(path):
    var parent_path=path.get_base_dir()
    if scene.has_node(parent_path):return scene.get_node(parent_path)
    var parent=Node2D.new()
    parent.name=parent_path
    scene.add_child(parent)
    return parent
func animator(target,names):
    var player=AnimationPlayer.new()
    player.name="AnimationPlayer"
    player.playback_process_mode=AnimationPlayer.ANIMATION_PROCESS_MANUAL
    for name in names:
        var animation=Animation.new()
        animation.length=0.01 # Request service only; not a source timeline.
        player.add_animation(name,animation)
    target.add_child(player)
func make_scene():
    scene=load("res://scene.gd").new()
    scene.name="Catacombs"
    global.currentScene=scene
    var music=Node.new()
    music.name="Music"
    scene.add_child(music)
    var area=load("res://music.gd").new()
    area.name="MusicArea"
    music.add_child(area)
    switches.clear()
    objects.clear()
    # Keep the actual source child order. Registration in _enter_tree runs
    # before controlled objects' _ready, including later siblings.
    for row in config.nodes:
        var value
        var target
        if row.kind=="lever" or row.kind=="plate":
            value=config.switches[row.path]
            target=load("res://"+config.paths.switch_script).new()
            target.name=row.path.get_file()
            target.position=Vector2(value.x,value.y)
            target.flag=value.flag if not (int(value.bits)&1) else ""
            target.is_object_flag=bool(int(value.bits)&1)
            target.emit_flag_updated_signal=bool(int(value.bits)&2)
            target.reset_when_leaving_area=bool(int(value.bits)&4)
            target.reset_when_leaving_region=bool(int(value.bits)&8)
            target._is_one_way=bool(int(value.bits)&16)
            target._pause_when_changed=bool(int(value.bits)&32)
            target._focus_objects_when_changed=bool(int(value.bits)&64)
            target._camera_delay=value.camera
            target._controlled_objects=[]
            for reference in config.references[row.path]:target._controlled_objects.append(NodePath(reference))
            animator(target,["TurnOn","TurnOff"])
            var audio=AudioStreamPlayer.new()
            audio.name="AudioStreamPlayer"
            var sample=AudioStreamSample.new()
            sample.mix_rate=8000
            var pcm=PoolByteArray()
            pcm.resize(80000) # Explicit ten-second service occupancy, no audio equivalence.
            sample.data=pcm
            audio.stream=sample
            target.add_child(audio)
            switches[row.path]=target
        else:
            value=config.objects[row.path]
            target=load("res://"+config.paths["gate_script" if row.kind=="gate" else "wall_script"]).new()
            target.name=row.path.get_file()
            target.position=Vector2(value.x,value.y)
            target._initially_on=bool(int(value.bits)&1)
            target._move_cam_when_changed=bool(int(value.bits)&2)
            target._move_cam_offset=Vector2(value.offset_x,value.offset_y)
            target._update_order=int(value.order)
            animator(target,["On","Off","StayOn","StayOff"] if row.kind=="gate" else ["On","StayOn","StayOff"])
            objects[row.path]=target
        parent_for(row.path).add_child(target)
    switches[config.main].connect("switch_hit",scene,"_on_main_switch_hit")
    objects[config.wall].connect("state_changed",scene,"_on_wall_door_state_changed")
    add_child(scene)
func complete_animations():
    for target in objects.values():target.get_node("AnimationPlayer").advance(0.02)
    for target in switches.values():target.get_node("AnimationPlayer").advance(0.02)
func capture(action):
    var switch_rows=[]
    var object_rows=[]
    for row in config.data.switches:
        var target=switches[row.path]
        switch_rows.append({"path":row.path,"on":target.is_on(),"disabled":target._is_disabled,"ready":target._is_ready_to_interact,"animation":target.get_node("AnimationPlayer").assigned_animation,"audio":target.get_node("AudioStreamPlayer").playing})
    for row in config.data.objects:
        var target=objects[row.path]
        object_rows.append({"path":row.path,"on":target._get_state(),"animation":target.get_node("AnimationPlayer").assigned_animation,"order":target.get_update_order(),"registrations":target._switches.size()})
    rows.append({"action":action,"switches":switch_rows,"objects":object_rows,"paused":global.player.paused,"cutscene":uiManager.cutscene,"player_events":global.player.events.duplicate(),"ui_events":uiManager.events.duplicate(),"audio_events":audioManager.events.duplicate(true),"camera_events":global.currentCamera.events.duplicate(true)})
    global.player.events.clear()
    uiManager.events.clear()
    audioManager.events.clear()
    global.currentCamera.events.clear()
func run():
    read_config()
    globaldata._init_flags()
    make_scene()
    capture("ready_timer_pending")
    switches["Bottom2/switch1"].interact()
    capture("early_interact_ignored")
    complete_animations()
    yield(get_tree().create_timer(0.25),"timeout")
    capture("ready_timer_complete")
    global.player.paused=true
    switches["Bottom2/switch1"].interact()
    capture("paused_interact_ignored")
    global.player.paused=false
    scene.can_switch=false
    switches["Bottom2/switch1"].interact()
    capture("scene_switch_denied")
    scene.can_switch=true
    switches["Bottom2/switch1"].interact()
    capture("four_gates_requested")
    complete_animations()
    capture("four_gates_complete")
    var other=Node.new()
    switches["Bottom2/switch1"]._on_hit(other)
    complete_animations()
    capture("hit_toggles_back")
    switches["Bottom2/Plate1"]._on_player_entered(other)
    other.free()
    capture("nonplayer_plate_ignored")
    switches["Bottom2/Plate1"]._on_player_entered(global.player)
    capture("plate_turns_on_once")
    switches["Bottom2/Plate1"]._on_player_entered(global.player)
    capture("plate_repeat_ignored")
    complete_animations()
    capture("plate_gate_complete")
    switches["Bottom2/switch2"].interact()
    capture("second_focus_pending")
    complete_animations()
    capture("second_focus_animations_complete")
    global.currentCamera.complete()
    yield(get_tree(),"idle_frame")
    yield(get_tree(),"idle_frame")
    capture("second_focus_return_pending")
    global.currentCamera.complete()
    capture("second_focus_complete")
    switches["Bottom2/Plate4"]._on_player_entered(global.player)
    complete_animations()
    capture("second_plate_on")
    switches["Bottom2/Plate4"]._on_player_entered(global.player)
    capture("second_plate_repeat_ignored")
    switches[config.main].interact()
    capture("main_switch_camera_pending")
    complete_animations()
    capture("wall_animation_complete_camera_pending")
    yield(get_tree().create_timer(0.55),"timeout")
    capture("wall_callback_delay_complete")
    global.currentCamera.complete()
    yield(get_tree(),"idle_frame")
    yield(get_tree(),"idle_frame")
    capture("camera_return_pending")
    global.currentCamera.complete()
    capture("main_control_complete")
    switches[config.main].interact()
    capture("main_repeat_ignored")
    switches["Bottom2/switch1"].interact()
    complete_animations()
    capture("reset_switch_on_before_leave")
    scene.leave(false)
    capture("area_reset_flag_only")
    scene.queue_free()
    yield(get_tree(),"idle_frame")
    yield(get_tree(),"idle_frame")
    make_scene()
    capture("saved_flags_ready_timer_pending")
    complete_animations()
    yield(get_tree().create_timer(0.25),"timeout")
    capture("saved_flags_ready_timer_complete")
    scene.queue_free()
    yield(get_tree(),"idle_frame")
    yield(get_tree(),"idle_frame")
    var file=File.new()
    assert(file.open("res://observations.json",File.WRITE)==OK)
    file.store_string(JSON.print({"engine":Engine.get_version_info(),"rows":rows},"  "))
    file.close()
    get_tree().quit()
'''

def run(out,engine):
 out=Path(out).resolve();engine=Path(engine).resolve();d.require(out.is_relative_to(ROOT/'build')and not out.exists()and engine.is_file(),'Fresh switch reference directory and actual engine required')
 review,ex=d.read();data,proof=d.extract();p=review['paths'];project=out/'project';project.mkdir(parents=True);(project/'.owned-switch-reference').write_text('source-switch-method-reference-v1',encoding='utf8')
 copied={}
 for key in('flag_script','abstract_script','list_script','switch_script','object_script','two_state_script','gate_script','wall_script'):
  source=p[key];raw=ex.data(source);target=project/source;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(raw);copied[source]=hashlib.sha256(raw).hexdigest()
 # The source method signature refers to this class even when no synced switch
 # exists. Copy it byte-identically for type resolution, never instantiate it.
 source='Scripts/Main/Switches/TwoStatesSyncedSwitch.gd';raw=ex.data(source);target=project/source;target.write_bytes(raw);copied[source]=hashlib.sha256(raw).hexdigest()
 bump='Audio/Sound effects/M3/bump.wav';target=project/bump;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(ex.data(bump))
 d.require('gdscript/warnings/unused_argument=false'in ex.text('project.godot'),'Source warning profile changed')
 (project/'project.godot').write_text('config_version=4\n[application]\nconfig/name="Source switch methods"\nrun/main_scene="res://probe.tscn"\n[autoload]\nglobal="*res://global.gd"\nglobaldata="*res://data.gd"\nuiManager="*res://ui.gd"\naudioManager="*res://audio.gd"\n[debug]\ngdscript/warnings/unused_argument=false\n',encoding='utf8')
 methods='\n'.join(function(ex.text(p['globaldata_script']),n)for n in('_init_flags','set_flag','set_object_flag'))
 scene_methods='\n'.join(function(ex.text(p['scene_script']),n)for n in('_on_main_switch_hit','_on_wall_door_state_changed'))
 for name,text in dict(global_gd=GLOBAL,data_gd='extends Node\nvar flags={}\nvar object_flags={}\n'+methods,scene_gd=SCENE+scene_methods,ui_gd=UI,audio_gd=AUDIO,music_gd=MUSIC,probe_gd=PROBE).items():(project/name.replace('_gd','.gd')).write_text(text,encoding='utf8')
 (project/'probe.tscn').write_text('[gd_scene load_steps=2 format=2]\n[ext_resource path="res://probe.gd" type="Script" id=1]\n[node name="Probe" type="Node"]\nscript = ExtResource( 1 )\n',encoding='utf8')
 sources=d.Sources(ROOT);nodes=d.instantiate(sources.doc,p['scene']);by_path={n.path:n for n in nodes}
 config=dict(data=data,paths=p,nodes=review['nodes'],switches={n['path']:n for n in data['switches']},objects={n['path']:n for n in data['objects']},references={n['path']:[v.path for v in by_path[n['path']].props['_controlled_objects']]for n in data['switches']},main=next(n['path']for n in data['switches']if n['id']==data['main']),wall=next(n['path']for n in data['objects']if n['id']==data['wall']))
 (project/'input.json').write_text(json.dumps(config,indent=2)+'\n',encoding='utf8')
 invoke(engine,project,['--debug','--editor','--quit'],out/'import.log');invoke(engine,project,['--debug'],out/'engine.log')
 observations=json.loads((project/'observations.json').read_bytes());v=observations['engine'];d.require((v['major'],v['minor'],v['patch'],v['hash'])==(3,6,2,'3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8'),'Reference engine version changed')
 rows=observations['rows'];d.require(len(rows)==28 and rows[0]['action']=='ready_timer_pending'and rows[-1]['action']=='saved_flags_ready_timer_complete','Switch reference observations incomplete')
 stages={row['action']:row for row in rows};d.require(len(stages)==len(rows),'Duplicate source observation')
 for row in rows:
  d.require(len(row['switches'])==5 and len(row['objects'])==9 and all(n['registrations']==1 for n in row['objects']),'Source graph registration differs')
  values={n['path']:n['on']for n in row['switches']}
  for n in row['objects']:
   definition=config['objects'][n['path']];state=bool(definition['bits']&1)
   for switch in data['switches']:
    if [switch['id'],definition['id']]in data['edges']:state=state!=values[switch['path']]
   d.require(state==n['on'],'Source XOR graph state differs from metadata')
 d.require(all(not n['ready']for n in rows[0]['switches'])and all(n['ready']for n in stages['ready_timer_complete']['switches']),'Source ready-timer gate differs')
 d.require(stages['scene_switch_denied']['audio_events']==[dict(kind='sfx',source='res://'+bump,channel='switch')],'Source denied-switch sound differs')
 main=stages['main_switch_camera_pending'];d.require(main['player_events']==['pause','pause']and main['ui_events']==[True,True]and main['audio_events']==[dict(kind='fade',value=0)],'Source main hit/action callback order differs')
 d.require(stages['wall_callback_delay_complete']['player_events']==['unpause']and stages['wall_callback_delay_complete']['audio_events']==[dict(kind='resume')],'Source wall callback completion differs')
 reset=next(n for n in stages['area_reset_flag_only']['switches']if n['path']=='Bottom2/switch1');d.require(reset['on']is False and reset['animation']=='TurnOn','Source area-left resets only the flag, not current presentation')
 for source,digest in copied.items():d.require(hashlib.sha256((project/source).read_bytes()).hexdigest()==digest,'Original switch script was modified')
 (out/'observations.json').write_bytes((project/'observations.json').read_bytes());proof.update(scope=__doc__,engine_sha256=hashlib.sha256(engine.read_bytes()).hexdigest(),copied_scripts=copied,additional_reference_dependencies=dict(sorted(ex.sources.items())),rows=len(rows),adapter_sha256={p.name:hashlib.sha256(p.read_bytes()).hexdigest()for p in project.glob('*.gd')})
 (out/'receipt.json').write_text(json.dumps(proof,indent=2)+'\n',encoding='utf8');print('Original switch/control methods:',len(rows),'observations; source scripts exact, explicit animation/camera/player services')
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',type=Path,required=True);p.add_argument('--godot',type=Path,required=True);a=p.parse_args();run(a.out,a.godot)
