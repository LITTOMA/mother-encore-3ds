"""Original seven Animation resources and unchanged key/lock methods.

Native Sprite and AnimationPlayer clocks run in source order. Player identity,
UI, flag storage, body/prompt properties and audio occupancy are explicit
services; there is no physics world, audible stream, complete scene or save.
Source signal errors are individually checked, never globally ignored.
"""
import argparse,hashlib,json,os,re,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import dungeon_key_clocks as d,dungeon_keys
from tools.godot_text import Document
from tools.run_dungeon_keys_reference import GLOBAL
from tools.run_battle_victory_reference import function
from tools.run_scene_reference import invoke

AUDIO='''extends Node
signal finished
var playing=false setget change
var writes=[]
func change(value):
    playing=value
    writes.append(value)
func finish():
    playing=false
    emit_signal("finished")
'''
STATE='''extends Node
var disabled=false
var enabled=true
'''
PROBE='''extends Node
var config
var target
var root_anim
var child_anim
var rows=[]
var completions=[]
var all_completed=[]
func _ready():call_deferred("run")
func profile_by_id(id):
    for value in config.profiles:
        if value.id==id:return value
    assert(false)
func completed(name,profile):
    var event={"profile":profile,"name":name}
    completions.append(event)
    all_completed.append(event)
func service(parent,name,script):
    var node=load(script).new()
    node.name=name
    parent.add_child(node)
    return node
func animate(parent,profile):
    var node=AnimationPlayer.new()
    node.name="AnimationPlayer"
    node.playback_process_mode=AnimationPlayer.ANIMATION_PROCESS_MANUAL
    for clip in profile.clips:
        node.add_animation(clip.name,load("res://clips/"+str(clip.id)+".tres"))
    parent.add_child(node)
    for connection in profile.connections:
        if connection.from=="AnimationPlayer":assert(node.connect(connection.signal,parent,connection.method)==OK)
    return node
func make(profile):
    var node=Sprite.new()
    node.name=profile.node_name
    node.set_script(load("res://"+profile.script))
    if profile.kind!=3:
        node.is_object_flag=true
        node.emit_flag_updated_signal=true
    var sprite=Sprite.new()
    sprite.name="Sprite"
    sprite.hframes=profile.frames
    sprite.frame=profile.frame
    sprite.position=Vector2(profile.position[0],profile.position[1])
    sprite.visible=bool(int(profile.mask)&1)
    node.add_child(sprite)
    if profile.kind==1:
        var area=Node.new()
        area.name="Area2D"
        node.add_child(area)
        service(area,"CollisionShape2D","res://state.gd")
    elif profile.kind==2:
        var body=Node.new()
        body.name="StaticBody2D"
        node.add_child(body)
        service(body,"CollisionShape2D","res://state.gd")
        var interact=Node.new()
        interact.name="interact"
        node.add_child(interact)
        service(interact,"ButtonPrompt","res://state.gd")
        var child=make(profile_by_id(profile.child))
        child.name="Lock"
        node.add_child(child)
    else:
        node.anim_name="Open"
        node.anim_player=NodePath("AnimationPlayer")
    if profile.kind!=2:
        var audio=service(node,"AudioStreamPlayer","res://audio.gd")
        if profile.kind==1:assert(audio.connect("finished",node,"_on_AudioStreamPlayer_finished")==OK)
    animate(node,profile)
    return node
func pose(node,profile):
    if not is_instance_valid(node):return {"alive":false}
    var sprite=node.get_node("Sprite")
    var anim=node.get_node("AnimationPlayer")
    var p={"alive":true,"queued":node.is_queued_for_deletion(),"frame":sprite.frame,"visible":sprite.visible,"root_visible":node.visible,"position":["%.17f"%sprite.position.x,"%.17f"%sprite.position.y],"playing":anim.is_playing(),"assigned":anim.assigned_animation,"time":"%.17f"%anim.current_animation_position if anim.assigned_animation!="" else "0"}
    if profile.kind!=2:
        var audio=node.get_node("AudioStreamPlayer")
        p.audio=audio.playing
        p.audio_writes=audio.writes.duplicate()
        audio.writes.clear()
    if profile.kind==1:p.disabled=node.get_node("Area2D/CollisionShape2D").disabled
    elif profile.kind==2:
        p.disabled=node.get_node("StaticBody2D/CollisionShape2D").disabled
        p.prompt=node.get_node("interact/ButtonPrompt").enabled
        p.opened=node.opened
        p.child=pose(node.get_node("Lock"),profile_by_id(profile.child))
    return p
func capture(case_index,op_index,op,profile):
    rows.append({"case":case_index,"index":op_index,"op":op,"pose":pose(target,profile),"completed":completions.duplicate(true),"counter":uiManager.get_key_count(),"flags":globaldata.object_flags.duplicate(true),"ui":uiManager.events.duplicate(true)})
    completions.clear()
    uiManager.events.clear()
func run():
    var file=File.new()
    assert(file.open("res://input.json",File.READ)==OK)
    config=JSON.parse(file.get_as_text()).result
    file.close()
    for ci in config.cases.size():
        var test=config.cases[ci]
        var profile=profile_by_id(test.profile)
        globaldata.object_flags={}
        if test.get("saved",false):globaldata.object_flags[test.flag]=true
        globaldata.keys["Catacombs"]=1
        target=make(profile)
        global.currentScene.add_child(target)
        root_anim=target.get_node("AnimationPlayer")
        root_anim.connect("animation_finished",self,"completed",[profile.id])
        child_anim=target.get_node("Lock/AnimationPlayer") if profile.kind==2 else null
        if child_anim!=null:child_anim.connect("animation_finished",self,"completed",[profile.child])
        for i in test.actions.size():
            var action=test.actions[i]
            if action.op=="play":root_anim.play(action.name)
            elif action.op=="interact":target.interact()
            elif action.op=="pickup":target._on_Area2D_body_entered(global.player)
            elif action.op=="finish_audio":target.get_node("AudioStreamPlayer").finish()
            elif action.op=="noop":pass
            elif action.op=="advance":
                if child_anim!=null and child_anim.is_playing():child_anim.advance(action.delta)
                if root_anim.is_playing():root_anim.advance(action.delta)
            elif action.op=="flush":
                yield(get_tree(),"idle_frame")
                yield(get_tree(),"idle_frame")
            else:assert(false)
            capture(ci,i,action.op,profile)
        if is_instance_valid(target):target.queue_free()
        yield(get_tree(),"idle_frame")
        yield(get_tree(),"idle_frame")
    # Verify the normal native scheduler independently: a stopped player must
    # not keep rewriting properties, unlike a forced public advance() call.
    var child_profile
    for p in config.profiles:
        if p.kind==3:child_profile=p
    var auto=make(child_profile)
    global.currentScene.add_child(auto)
    var auto_anim=auto.get_node("AnimationPlayer")
    auto_anim.connect("animation_finished",self,"completed",[child_profile.id])
    auto_anim.playback_process_mode=AnimationPlayer.ANIMATION_PROCESS_IDLE
    auto_anim.play("Open")
    auto_anim.advance(1.5)
    var auto_audio=auto.get_node("AudioStreamPlayer")
    var finishing_writes=auto_audio.writes.duplicate()
    auto_audio.writes.clear()
    var stopped_before=not auto_anim.is_playing()
    for _i in range(6):yield(get_tree(),"idle_frame")
    var scheduler={"stopped_before":stopped_before,"stopped_after":not auto_anim.is_playing(),"finishing_writes":finishing_writes,"six_idle_writes":auto_audio.writes.duplicate()}
    auto.free()
    assert(file.open("res://../observations.json",File.WRITE)==OK)
    file.store_string(JSON.print({"engine":Engine.get_version_info(),"rows":rows,"completions":all_completed,"automatic_scheduler":scheduler},"  "))
    file.close()
    print("KEY_CLOCK_SOURCE_REFERENCE_COMPLETE")
    get_tree().quit(17)
'''

def cases(data,review):
 result=[]
 for c in data['clips']:
  p=next(p for p in review['profiles']if p['id']==c['profile']);name=next(k['name']for k in p['clips']if k['id']==c['id'])
  times={0.,c['duration']/2,c['duration']}
  for track in c['tracks']:
   if track['kind']==3:
    for a,b in zip(track['keys'],track['keys'][1:]):
     for fraction in [.125,.25,.5,.75,.875]:times.add(a['time']+(b['time']-a['time'])*fraction)
   for k in track['keys']:
    if k['time']>0:times.update([max(0.,k['time']-.000001),k['time'],min(c['duration'],k['time']+.000001)])
  actions=[dict(op='play',name=name,clip=c['id'])];previous=0.
  for time in sorted(times):actions.extend([dict(op='advance',delta=time-previous),dict(op='flush')]);previous=time
  result.append(dict(profile=p['id'],actions=actions,scope='Direct original AnimationPlayer clip service, not a normal pickup request'))
 # Source child process precedes its parent's AnimationPlayer. Deferred calls
 # are flushed only at explicit idle boundaries, including a large crossing.
 door=next(p for p in review['profiles']if p['kind']=='door')
 for deltas in [[1/60]*83,[1.5,1.5]]:
  actions=[dict(op='interact')]
  for delta in deltas:actions.extend([dict(op='advance',delta=delta),dict(op='flush')])
  result.append(dict(profile=door['id'],actions=actions,scope='Original interact->parent and child native clocks with explicit idle flush'))
 key=next(p for p in review['profiles']if p['kind']=='key')
 result.append(dict(profile=key['id'],actions=[dict(op=op)for op in('pickup','pickup','flush','finish_audio','flush')],scope='Original pickup/duplicate occupancy and external audio-completion service; no audible stream'))
 for p in [key,door]:
  kind=1 if p['kind']=='key'else 2;node=next(n for n in data['metadata']['nodes']if n['kind']==kind)
  actions=[dict(op='noop'),dict(op='flush')]if kind==1 else[dict(op='noop'),dict(op='advance',delta=0),dict(op='flush'),dict(op='advance',delta=1.1),dict(op='flush')]
  result.append(dict(profile=p['id'],saved=True,flag=node['flag'],actions=actions,scope='Explicit saved object-flag service input; no save codec or live LOAD claim'))
 return result

def run(out,engine):
 out=Path(out).resolve();engine=Path(engine).resolve();d.require(out.is_relative_to(ROOT/'build')and not out.exists()and engine.is_file(),'Fresh key clock reference and real engine required')
 review,ex=d.read();data,blob,proof=d.extract();key_review,key_ex=dungeon_keys.read();paths=key_review['paths']
 project=out/'project';project.mkdir(parents=True);(project/'.owned-key-clock-reference').write_text('source-key-clock-reference-v1',encoding='utf8');(project/'clips').mkdir()
 copied={};config_profiles=[]
 for p,model in zip(review['profiles'],data['profiles']):
  raw=ex.text(p['scene']);doc=Document(p['scene'],raw);anim=next(n for n in doc.nodes if n.attrs.get('name')=='AnimationPlayer')
  for c in p['clips']:
   ref=anim.props['anims/'+c['name']];pattern=r'^\[sub_resource type="Animation" id='+str(ref.id)+r'\]\r?\n(.*?)(?=^\[sub_resource |^\[node |\Z)';match=re.search(pattern,raw,re.M|re.S);d.require(match is not None,'Key clock original animation block')
   (project/'clips'/f"{c['id']}.tres").write_text('[gd_resource type="Animation" format=2]\n\n[resource]\n'+match[1],encoding='utf8')
  root_name=next(n.attrs['name']for n in doc.nodes if n.attrs.get('parent')is None)
  node_name=next((n['path'].rsplit('/',1)[-1]for n in data['metadata']['nodes']if n['kind']==model['kind']),root_name)if model['kind']!=3 else root_name
  config_profiles.append(dict(id=p['id'],kind=model['kind'],frames=model['frames'],frame=model['frame'],position=model['position'],mask=model['mask'],child=model['child'],node_name=node_name,script=p['script'],clips=p['clips'],connections=doc.connections))
 for source in {p['script']for p in review['profiles']}|{paths['flag_script']}:
  raw=key_ex.data(source);dest=project/source;dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(raw);copied[source]=hashlib.sha256(raw).hexdigest()
 settings='config_version=4\n[application]\nconfig/name="Original key lock source clocks"\nrun/main_scene="res://probe.tscn"\n[autoload]\nglobal="*res://global.gd"\nglobaldata="*res://data.gd"\nuiManager="*res://ui.gd"\n[debug]\n'+''.join('gdscript/warnings/'+name+'=false\n'for name in review['warning_profile'])
 (project/'project.godot').write_text(settings,encoding='utf8');(project/'global.gd').write_text(GLOBAL,encoding='utf8');(project/'audio.gd').write_text(AUDIO,encoding='utf8');(project/'state.gd').write_text(STATE,encoding='utf8');(project/'probe.gd').write_text(PROBE,encoding='utf8')
 (project/'probe.tscn').write_text('[gd_scene load_steps=2 format=2]\n[ext_resource path="res://probe.gd" type="Script" id=1]\n[node name="Probe" type="Node"]\nscript = ExtResource( 1 )\n',encoding='utf8')
 methods='\n'.join(function(key_ex.text(paths['globaldata_script']),n)for n in('set_flag','set_object_flag'))
 (project/'data.gd').write_text('extends Node\nvar flags={}\nvar object_flags={}\nvar keys={"Catacombs":0}\n'+methods,encoding='utf8')
 methods='\n'.join(function(key_ex.text(paths['ui_script']),n)for n in('get_key_count','try_alter_key_count'))
 (project/'ui.gd').write_text('extends Node\nvar events=[]\nfunc update_key_indicator():events.append({"kind":"indicator","value":get_key_count()})\nfunc open_dialogue_box(path):events.append({"kind":"text","path":path})\n'+methods,encoding='utf8')
 config=dict(profiles=config_profiles,cases=cases(data,review));(project/'input.json').write_text(json.dumps(config,indent=2)+'\n',encoding='utf8')
 invoke(engine,project,['--debug','--editor','--quit'],out/'import.log')
 env=dict(os.environ)
 for var,name in [('APPDATA','user-roaming'),('LOCALAPPDATA','user-local'),('XDG_DATA_HOME','user-data'),('XDG_CONFIG_HOME','user-config')]:env[var]=str(project/name)
 with (out/'engine.log').open('wb')as log:process=subprocess.run([str(engine),'--path',str(project),'--no-window','--audio-driver','Dummy','--debug'],stdout=log,stderr=subprocess.STDOUT,env=env,timeout=55)
 output=(out/'engine.log').read_text('utf8');d.require(process.returncode==17 and 'KEY_CLOCK_SOURCE_REFERENCE_COMPLETE'in output,'Original key clock run did not complete; retain actual failure')
 observed=json.loads((out/'observations.json').read_text('utf8'));d.require(observed['engine']['string']=='3.6.2-stable (official)','Key clock wrong engine')
 d.require(observed['automatic_scheduler']==dict(stopped_before=True,stopped_after=True,finishing_writes=[True,False],six_idle_writes=[]),'Original stopped-clock automatic scheduler differs')
 errors=[line for line in output.splitlines()if 'ERROR:'in line];expected=[];by={p['id']:p for p in data['profiles']}
 for completed in observed['completions']:
  flags=by[completed['profile']]['diagnostics']
  if flags&1:expected.append('missing')
  if flags&2:expected.append('emit:'+completed['name'])
 actual=[]
 for line in errors:
  if "Error calling method from signal 'animation_finished'"in line and '_on_AnimationPlayer_animation_finished'in line and'Method not found'in line:actual.append('missing')
  else:
   match=re.search(r'Can.t emit non-existing signal "(Open|RESET)"',line);d.require(match is not None,'Unmapped original key clock diagnostic: '+line);actual.append('emit:'+match[1])
 d.require(sorted(actual)==sorted(expected)and 'WARNING:'not in output,'Original key clock diagnostic count/identity mismatch')
 for source,h in copied.items():d.require(hashlib.sha256((project/source).read_bytes()).hexdigest()==h,'Original key clock script changed')
 d.require(len(observed['rows'])==sum(len(c['actions'])for c in config['cases']),'Original key clock observation cardinality')
 (out/'input.json').write_bytes((project/'input.json').read_bytes());(out/'source.encklc').write_bytes(blob)
 proof.update(scope=__doc__,process_exit=process.returncode,engine_sha256=hashlib.sha256(engine.read_bytes()).hexdigest(),profiles=3,clips=7,rows=len(observed['rows']),source_scripts=copied,diagnostics=actual,probe_sha256=hashlib.sha256(PROBE.encode()).hexdigest(),explicit_services=['player identity','UI/flag storage','body/prompt properties','audio occupancy; no audible stream'],clock_dispatch='Child then parent only while is_playing, plus independent native IDLE-process stopped-clock assertion. Forced advance after stop is not normal scene processing.',animation_resources='Exact native original property/method tracks extracted without changing key times, values, easing, update or deferred method mode.',observations_sha256=hashlib.sha256((out/'observations.json').read_bytes()).hexdigest(),input_sha256=hashlib.sha256((out/'input.json').read_bytes()).hexdigest())
 (out/'receipt.json').write_text(json.dumps(proof,ensure_ascii=False,indent=2)+'\n',encoding='utf8');print('Original key clocks:',len(observed['rows']),'observations,',len(actual),'individually mapped original diagnostics')
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',type=Path,required=True);p.add_argument('--godot',type=Path,required=True);a=p.parse_args();run(a.out,a.godot)
