"""Observe original pause/unpause methods across motion and Bat transitions.

The eight source animation branches and methods are unchanged. The fixture
supplies party services, an AreaRoom type guard, collision-mask/event-query
boundaries and a MiscTimer. It does not run scene physics, party timers,
damage, teleportation, audible audio or the full original game.
"""
import argparse,hashlib,json,re,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import field_swing,field_swing_entry,run_swing_entry_reference as entry
from tools.run_scene_reference import invoke

PROBE='''extends SceneTree
var rows=[]
var pauses=0
var unpauses=0
var topology_paths={}
func _init():call_deferred("run")
func on_pause():pauses+=1
func on_unpause():unpauses+=1
func capture(target,tree,id,op):
    target.get_node("SpecialAnimations/NintenBat")._process(0)
    var playback=tree.get("parameters/playback")
    var path=[]
    for name in playback.get_travel_path():path.append(name)
    rows.append({"case":id,"op":op,"node":playback.get_current_node(),"path":path,"time":"%.17f" % playback.get_current_play_position(),"state":target._state,"paused":target._paused,"paused_node":target.get_paused_anim_state(),"main_frame":target.get_node("Position/main").frame,"main":target.get_node("Position/main").visible,"special":target.get_node("SpecialAnimations").visible,"frame":target.get_node("SpecialAnimations").frame,"hit":not target.get_node("HitboxPivot/BatHitbox/CollisionShape2D").disabled,"pivot":"%.17f" % target.get_node("HitboxPivot").rotation_degrees,"behind":target.get_node("SpecialAnimations/NintenBat").show_behind_parent,"selected":target._current_skill_action,"starts":get_root().get_node("global").rumble.size(),"stream_paused":target.get_node("AudioStreamPlayer").stream_paused,"stream_playing":target.get_node("AudioStreamPlayer").playing,"timer_paused":target.get_node("MiscTimer").paused,"collision_calls":target._fixture_collision_calls.duplicate(),"event_queries":target._fixture_event_queries,"pauses":pauses,"unpauses":unpauses})
func press(target):
    var event=InputEventJoypadButton.new()
    event.button_index=1
    event.pressed=true
    target._input(event)
func run():
    yield(self,"idle_frame")
    var f=File.new()
    assert(f.open("res://input.json",File.READ)==OK)
    var config=JSON.parse(f.get_as_text()).result
    f.close()
    var topology_results=[]
    for scene in ["topology.tscn","topology-reduced.tscn"]:
        var result={}
        for state in config.states:
            for destination in ["Idle","FaintedIdle"]:
                print("Pause topology: ",scene," ",state," -> ",destination)
                var holder=load("res://"+scene).instance()
                get_root().add_child(holder)
                var graph=holder.get_node("AnimationTree")
                graph.process_mode=AnimationTree.ANIMATION_PROCESS_MANUAL
                graph.active=true
                graph.advance(0)
                var state_player=graph.get("parameters/playback")
                state_player.start(state)
                graph.advance(0)
                assert(state_player.get_current_node()==state)
                state_player.travel(destination)
                graph.advance(0)
                var path=[]
                for name in state_player.get_travel_path():path.append(name)
                if state_player.get_current_node()!=state:path.push_front(state_player.get_current_node())
                result[state+"->"+destination]=path
                graph.active=false
                holder.free()
        topology_results.append(result)
    topology_paths=topology_results
    print("Pause topology comparison recorded")
    var id=config.offset
    for definition in config.cases:
        print("Pause case: ",id," ",definition.state," steps ",definition.steps)
        pauses=0
        unpauses=0
        get_root().get_node("global").rumble.clear()
        var target=load("res://probe.tscn").instance()
        get_root().add_child(target)
        target.connect("paused",self,"on_pause")
        target.connect("unpaused",self,"on_unpause")
        target._party_member.incapacitated=definition.incapacitated
        target._direction=Vector2(definition.direction[0],definition.direction[1])
        var tree=target.get_node("AnimationTree")
        tree.process_mode=AnimationTree.ANIMATION_PROCESS_MANUAL
        tree.active=true
        for param in config.parameters:tree.set(param,target._direction)
        tree.advance(0)
        var playback=tree.get("parameters/playback")
        playback.start(definition.state)
        tree.advance(0)
        tree.advance(.04)
        capture(target,tree,id,0)
        press(target)
        capture(target,tree,id,1)
        for i in range(definition.steps):
            tree.advance(1.0/60.0)
            capture(target,tree,id,2)
            yield(self,"idle_frame")
            capture(target,tree,id,3)
        target.pause(false,definition.start_idle)
        capture(target,tree,id,4)
        press(target)
        capture(target,tree,id,5)
        for i in range(8):
            tree.advance(1.0/60.0)
            capture(target,tree,id,2)
            yield(self,"idle_frame")
            capture(target,tree,id,3)
        target.unpause()
        capture(target,tree,id,6)
        press(target)
        capture(target,tree,id,7)
        for i in range(45):
            tree.advance(1.0/60.0)
            capture(target,tree,id,2)
            yield(self,"idle_frame")
            capture(target,tree,id,3)
        tree.active=false
        target.free()
        yield(self,"idle_frame")
        id+=1
    assert(f.open("res://native.json",File.WRITE)==OK)
    f.store_string(JSON.print({"engine":Engine.get_version_info(),"paths":topology_paths,"rows":rows}))
    f.close()
    quit()
'''

def prepare(work):
 recipe=field_swing_entry.read();proof,params=entry.prepare(work,recipe);p,ex=field_swing.read()
 source=ex.text(p['script']);party_path='Scripts/Main/party/party_object.gd';party=ex.text(party_path)
 def method(text,name,path):
  m=re.search(r'^func '+re.escape(name)+r'\(.*?(?=^func |^static func |\Z)',text,re.M|re.S)
  if not m:raise ValueError('Missing original pause method: '+name)
  proof.setdefault('pause_methods',{})[path+'::'+name]=hashlib.sha256(m[0].encode()).hexdigest();return m[0]
 methods=''.join(method(source,n,p['script'])for n in('pause','unpause'))+''.join(method(party,n,party_path)for n in('_anim_play_pause','set_idle','set_anim_state','get_paused_anim_state'))
 constant=re.search(r'^const LOOPED_ANIMS := (.+)$',party,re.M)
 if not constant:raise ValueError('Original pausable animation list missing')
 looped=json.loads(constant[1]);known={r['name']for r in recipe['states']}
 if type(looped)is not list or not looped or len(set(looped))!=len(looped)or any(n not in known for n in looped):raise ValueError('Unreviewed pausable animation scope')
 wrapper='''
signal paused
signal unpaused
var _paused_anim_state=""
var _tp_take_off_timer_done=false
var _tp_take_off_timer=null
var _input_vector=Vector2.ZERO
var _fixture_collision_calls=[]
var _fixture_event_queries=0
onready var _anim_player=$AnimationPlayer
func _set_collision_masks(enabled):_fixture_collision_calls.append(enabled)
func _check_event_collider():_fixture_event_queries+=1
func _start_landing():assert(false)
'''
 player=work/'player.gd';player.write_text(player.read_text(encoding='utf8')+'\nconst LOOPED_ANIMS = '+json.dumps(looped)+'\n'+wrapper+methods,encoding='utf8')
 (work/'probe.tscn').write_text((work/'probe.tscn').read_text(encoding='utf8')+'\n[node name="MiscTimer" type="Timer" parent="."]\n',encoding='utf8')
 raw=(work/'probe.tscn').read_text(encoding='utf8');doc=entry.Document('probe.tscn',raw);root_id=next(n for n in doc.nodes if n.attrs['name']=='AnimationTree').props['tree_root'].id
 pattern=r'^\[sub_resource [^\n]+id='+str(root_id)+r'\]\n.*?(?=^\[|\Z)';subset=re.search(pattern,raw,re.M|re.S);full=(work/'topology.tscn').read_text(encoding='utf8')
 if not subset or len(re.findall(pattern,full,re.M|re.S))!=1:raise ValueError('Pause source topology root missing')
 (work/'topology-reduced.tscn').write_text(re.sub(pattern,lambda m:subset[0],full,flags=re.M|re.S),encoding='utf8')
 (work/'area_room.gd').write_text('class_name AreaRoom\nextends Node\n',encoding='utf8')
 global_file=work/'global.gd';global_file.write_text(global_file.read_text(encoding='utf8')+'''var currentScene
var party_calls=[]
func _ready():
    currentScene=load("res://area_room.gd").new()
    add_child(currentScene)
func party_call(method):
    assert(method in ["pause_flash_anim","stop_creating_afterimage","pause_timers","resume_flash_anim","resume_timers"])
    party_calls.append(method)
''',encoding='utf8')
 proof.update(sources={**proof['sources'],**ex.sources},scope=__doc__,looped_animations=looped)
 return proof,params

def definitions():
 cases=[]
 for state in('Idle','Run'):
  for direction in([0,1],[0,-1]):
   for steps in(0,1,10,23):cases.append(dict(state=state,direction=direction,steps=steps,start_idle=True,incapacitated=False))
 for state in('Walk','Crouch'):
  for steps in(0,10):cases.append(dict(state=state,direction=[1,0],steps=steps,start_idle=True,incapacitated=False))
 for state in('FaintedIdle','FaintedWalk'):
  for steps in(0,10):cases.append(dict(state=state,direction=[0,1],steps=steps,start_idle=True,incapacitated=True))
 for steps in(0,1,10,23):cases.append(dict(state='Run',direction=[-1,0],steps=steps,start_idle=False,incapacitated=False))
 return cases

def run(work,reports,engine):
 work=work.resolve();reports=reports.resolve();engine=engine.resolve()
 if work.exists()or reports.exists()or not work.is_relative_to(ROOT/'build')or not reports.is_relative_to(ROOT/'reports'):raise ValueError('Fresh owned pause reference directories required')
 work.mkdir(parents=True);reports.mkdir(parents=True);proof,params=prepare(work);cases=definitions();(work/'probe.gd').write_text(PROBE,encoding='utf8')
 (reports/'cases.json').write_text(json.dumps(cases,indent=2)+'\n',encoding='utf8');invoke(engine,work,['--editor','--quit'],reports/'import.log');actual=None
 for start in range(0,len(cases),8):
  config=dict(cases=cases[start:start+8],parameters=params,offset=start,states=proof['selected_states']);raw=json.dumps(config);(work/'input.json').write_text(raw,encoding='utf8');(reports/f'input-{start}.json').write_text(raw,encoding='utf8')
  invoke(engine,work,['--fixed-fps','60','--script',str(work/'probe.gd')],reports/f'godot-{start}.log');part=json.loads((work/'native.json').read_bytes())
  if len(part['rows'])!=sum(112+c['steps']*2 for c in config['cases'])or part['engine']['string']!='3.6.2-stable (official)':raise ValueError('Pause source engine/coverage differs')
  if len(part['paths'])!=2 or part['paths'][0]!=part['paths'][1]:raise ValueError('Pause reduced graph misses original travel paths')
  if actual is None:actual=part
  else:
   if actual['engine']!=part['engine']or actual['paths']!=part['paths']:raise ValueError('Pause source engine/topology changed')
   actual['rows'].extend(part['rows'])
 (reports/'reference.json').write_text(json.dumps(actual,separators=(',',':'))+'\n',encoding='utf8')
 stable=[{k:v for k,v in row.items()if k!='stream_playing'}for row in actual['rows']]
 (reports/'stable.json').write_text(json.dumps(stable,separators=(',',':'))+'\n',encoding='utf8')
 sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
 proof.update(engine=actual['engine'],engine_sha256=sha(engine),cases=len(cases),observations=len(actual['rows']),tools={p:sha(ROOT/p)for p in ['tools/run_swing_pause_reference.py','tools/run_swing_entry_reference.py','tools/run_swing_action_reference.py']},fixture_sources={p.relative_to(work).as_posix():sha(p)for p in work.iterdir()if p.suffix in('.gd','.tscn','.json','.godot')},outputs={p.name:sha(p)for p in reports.iterdir()if p.is_file()})
 (reports/'source.json').write_text(json.dumps(proof,indent=2)+'\n',encoding='utf8');print('Original pause source:',len(cases),'cases;',len(actual['rows']),'observations; audio playing outside stable projection')
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--work',type=Path,required=True);p.add_argument('--reports',type=Path,required=True);p.add_argument('--engine',type=Path,required=True);o=p.parse_args();run(o.work,o.reports,o.engine)
