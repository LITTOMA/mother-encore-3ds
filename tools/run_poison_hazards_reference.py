"""Real Timer and selected original poison callbacks with explicit services."""
import argparse,hashlib,json,math,struct,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import catacombs_poison as poison,catacombs_steps as steps
from tools.reference_movement import extract
from tools.run_scene_reference import invoke
from tools.extract_battle_entry import require
TICKS=720
OPERATIONS={120:[['player_pause',True]],160:[['player_pause',False]],210:[['hole_pause',6,True]],260:[['hole_pause',6,False]],300:[['tree_pause',True]],330:[['tree_pause',False]],400:[['visible',False]],440:[['visible',True]],500:[['tree_pause',True]],550:[['tree_pause',False]]}
PROBE=r'''extends Node
var config
var player
var party
var outsider
var holes=[]
var areas=[]
var geyser
var rows=[]
var timeouts=[]
var tick=0
var initial=[]
func bits(value):
    var bytes=StreamPeerBuffer.new()
    bytes.big_endian=false
    bytes.put_float(value)
    bytes.seek(0)
    return bytes.get_u32()
func clocks():
    var result=[]
    for hole in holes:result.append([bits(hole.timer.time_left),bits(hole.timer.wait_time),hole.timer.paused])
    return result
func timeout(index):timeouts.append(index)
func _ready():
    pause_mode=Node.PAUSE_MODE_PROCESS
    var file=File.new()
    assert(file.open("res://input.json",File.READ)==OK)
    config=JSON.parse(file.get_as_text()).result
    file.close()
    player=load("res://party.gd").new()
    party=load("res://party.gd").new()
    outsider=Node2D.new()
    for body in [player,party,outsider]:add_child(body)
    global.player=player
    global.currentScene=self
    var objects=Node2D.new()
    objects.name="Objects"
    add_child(objects)
    var source_node=Node2D.new()
    source_node.name="Poison Geyser"
    var factory=PackedScene.new()
    assert(factory.pack(source_node)==OK)
    source_node.free()
    for index in config.holes.size():
        var binding=config.holes[index]
        var hole=load("res://hole.gd").new()
        hole.pause_mode=Node.PAUSE_MODE_STOP
        hole.interval=binding.interval
        hole.delay=binding.delay
        hole.position=Vector2(binding.position[0],binding.position[1])
        hole.poison=factory
        var timer=Timer.new()
        timer.name="Timer"
        timer.wait_time=8.0
        assert(timer.connect("timeout",hole,"_on_Timer_timeout")==OK)
        hole.add_child(timer)
        add_child(hole)
        assert(timer.connect("timeout",self,"timeout",[index])==OK)
        assert(timer.process_mode==Timer.TIMER_PROCESS_IDLE and !timer.one_shot and !timer.autostart)
        holes.append(hole)
    for binding in config.areas:
        var area=load("res://"+config.area_scene).instance()
        area.collision_layer=int(binding.layer)
        area.collision_mask=int(binding.mask)
        area.monitoring=bool(int(binding.bits)&1)
        area.monitorable=bool(int(binding.bits)&2)
        area.stepping_sound=NodePath("Step")
        var step=load("res://step.gd").new()
        step.name="Step"
        step.entering_sound=config.step.enter
        step.exiting_sound=config.step.exit
        step.enter_shadow_effect=config.step.enter_shadow
        step.exit_shadow_effect=config.step.exit_shadow
        step.enabled=config.step.enabled
        area.add_child(step)
        for role in 2:
            var shape=config.shapes[int(binding.first)+role]
            var polygon=area.get_node("CollisionPolygon2D" if role==0 else "StaticBody2D/CollisionPolygon2D")
            polygon.position=Vector2(shape.position[0],shape.position[1])
            var points=PoolVector2Array()
            for index in range(int(shape.first),int(shape.first+shape.count)):points.append(Vector2(config.points[index][0],config.points[index][1]))
            polygon.polygon=points
            if role==1:
                area.get_node("StaticBody2D").collision_layer=int(shape.layer)
                area.get_node("StaticBody2D").collision_mask=int(shape.mask)
        add_child(area)
        areas.append(area)
    geyser=load("res://geyser.gd").new()
    add_child(geyser)
    initial=clocks()
func _process(_delta):
    var before_pause=get_tree().paused
    var delta_bits=bits(get_process_delta_time())
    for action in config.timeline[tick]:
        match action[0]:
            "player_pause":
                player.set_observed_pause(action[1])
            "hole_pause":
                if action[2]:holes[int(action[1])].pause()
                else:holes[int(action[1])].unpause()
            "tree_pause":get_tree().paused=action[1]
            "visible":
                for hole in holes:hole.visible=action[1]
            _:assert(false)
    rows.append({"tick":tick,"actions":config.timeline[tick],"before_tree_pause":before_pause,"delta_bits":delta_bits,"clocks":clocks(),"timeouts":timeouts.duplicate()})
    timeouts.clear()
    tick+=1
    if tick==config.timeline.size():
        set_process(false)
        for hole in holes:hole.free()
        get_tree().paused=false
        call_deferred("finish")
func body_state(body):
    return [body._is_continuous_damage,body._attack_damage,body._damage_variance,body._party_member.statuses.duplicate(),body.shadow_mode]
func finish():
    yield(get_tree(),"idle_frame")
    yield(get_tree(),"idle_frame")
    var spawned=[]
    for child in get_node("Objects").get_children():spawned.append([bits(child.position.x),bits(child.position.y)])
    var contacts=[]
    for action in config.contacts:
        player._paused=action[4]
        var body=[player,party,outsider][int(action[2])]
        var entered=action[3]
        if int(action[0])==0:
            var area=areas[int(action[1])]
            if entered:area._on_Area2D_body_entered(body)
            else:area._on_Poison_body_exited(body)
        else:
            if entered:geyser._on_Poison_Geyser_body_entered(body)
            else:geyser._on_Poison_Geyser_body_exited(body)
        contacts.append({"action":action,"player":body_state(player),"party":body_state(party),"wanted":player.run_sound})
    var geometry=[]
    for area in areas:
        for path in ["CollisionPolygon2D","StaticBody2D/CollisionPolygon2D"]:
            var polygon=area.get_node(path)
            var vertices=[]
            for point in polygon.polygon:
                var world=polygon.to_global(point)
                vertices.append([bits(world.x),bits(world.y)])
            geometry.append(vertices)
    var file=File.new()
    assert(file.open("res://../reference.json",File.WRITE)==OK)
    file.store_string(JSON.print({"schema":1,"godot":Engine.get_version_info(),"initial":initial,"rows":rows,"spawned":spawned,"contacts":contacts,"geometry":geometry}))
    file.close()
    get_tree().quit()
'''

def source():
 p,m,proof=poison.source();s=poison.f.Sources();n=p['paths'];_,step,_=steps.source();targets={a['step']for a in m['areas']};require(len(targets)==1,'Poison native selected source step ownership');selected=next(a for a in step['areas']if a['id']in targets)
 def methods(key):
  raw=s.data(n[key]);return extract(raw,hashlib.sha256(raw).hexdigest(),list(p['symbols'][key]))[0]
 party='''extends KinematicBody2D
class_name PartyObject
signal paused
signal unpaused
var _paused=false
var _is_continuous_damage=false
var _attack_damage=0
var _damage_variance=0
var shadow_mode="shadow"
var run_sound="wood"
class Member:
    var statuses=[]
    func add_status(status):statuses.append(status)
var _party_member=Member.new()
func is_paused():return _paused
func set_shadow(mode):shadow_mode=mode
func set_observed_pause(value):
    _paused=value
    if value:emit_signal("paused")
    else:emit_signal("unpaused")
'''+methods('party_script').replace('func stop_continuous_damage():','\tvar _observer_direction = hit_direct\n\nfunc stop_continuous_damage():')
 hole='extends Sprite\nvar interval\nvar delay\nvar poison\nonready var timer=$Timer\n'+methods('hole_script')
 classes=[dict(base='KinematicBody2D',**{'class':'PartyObject'},language='GDScript',path='res://party.gd'),dict(base='Node',**{'class':'Status'},language='GDScript',path='res://status.gd')]
 project='config_version=4\n_global_script_classes='+json.dumps(classes)+'\n_global_script_class_icons={"PartyObject":"","Status":""}\n[application]\nconfig/name="Original poison callbacks and Timer"\nrun/main_scene="res://probe.tscn"\n[autoload]\nglobal="*res://global.gd"\n[physics]\ncommon/physics_fps=60\ncommon/physics_jitter_fix=0.0\n'
 contacts=[]
 for index in range(len(m['areas'])):
  contacts += [[0,index,0,True,False],[0,index,1,True,False],[0,index,2,True,False],[0,index,0,False,True],[0,index,1,False,True],[0,index,2,False,True],[0,index,0,True,True],[0,index,1,True,True]]
 contacts += [[0,0,0,True,False],[0,1,0,True,False],[0,0,0,False,False],[0,1,0,False,False],[1,0,0,True,False],[1,0,1,True,False],[1,0,2,True,False],[1,0,0,False,True],[1,0,1,False,True],[1,0,0,True,True],[1,0,1,True,True]]
 inputs=dict(holes=m['holes'],areas=m['areas'],shapes=m['shapes'],points=m['points'],area_scene=n['area'],step=selected,timeline=[OPERATIONS.get(i,[])for i in range(TICKS)],contacts=contacts)
 files={'project.godot':project,'global.gd':'extends Node\nvar player\nvar currentScene\nfunc get_player():return player\n','status.gd':'extends Node\nclass_name Status\nconst AILMENT_POISONED="'+m['status']+'"\n','party.gd':party,'hole.gd':hole,'geyser.gd':'extends Area2D\n'+methods('geyser_script'),'step.gd':s.text(n['step_script']),n['area']:s.text(n['area']),n['area_script']:s.text(n['area_script']),'probe.gd':PROBE,'probe.tscn':'[gd_scene load_steps=2 format=2]\n[ext_resource path="res://probe.gd" type="Script" id=1]\n[node name="Probe" type="Node"]\nscript=ExtResource(1)\n','input.json':json.dumps(inputs)}
 receipt=dict(source=proof,generated_sha256={name:hashlib.sha256(value.encode()).hexdigest()for name,value in files.items()},adapters=['Verbatim selected original hole/contact methods and complete original poison Area script/prefab;not full root/player/Character implementation','Actual Timer nodes/defaults,source interval/delay,ordinary fixed60idle;explicit tree-pause and player signal services','PackedScene Node2D factory retains source deferred add_child/position;geyser animation/audio/physics not instantiated','Exact source raw polygons and flattened identity-parent origins;manual body callback delivery,not full physics contacts','PartyObject classifier retains original continuous-damage assignments;one trailing observer-only local reads the otherwise unused hit_direct argument,without modifying assignments or suppressing compiler diagnostics','Character.add_status and set_shadow are explicit request/state recorders;no HP/status effects or elapsed Shadow time;pause signals are emitted by the declared player service','Actions occur after the internal Timer phase;source-ordered timeout signals and final settled deferred children are observed separately','Hide does not disable Timer;pause player signal differs from tree pause;old wait rearm is observed rather than rescheduled by observer'],ticks=TICKS,contacts=len(contacts));return files,receipt

def validate_result(doc,proof):
 require(type(doc)is dict and set(doc)=={'schema','godot','initial','rows','spawned','contacts','geometry'}and type(doc['schema'])is int and doc['schema']==1 and type(doc['godot'])is dict and doc['godot'].get('string')=='3.6.2-stable (official)','Poison native schema/engine');_,m,_=poison.source();files,current=source();_,step,_=steps.source();sound_names=[s['name']for s in step['sounds']];mode_names=[a['name']for a in step['shadows']['animations']];require(proof['generated_sha256']==current['generated_sha256'],'Poison native generated source changed')
 def clocks(value):
  require(type(value)is list and len(value)==len(m['holes']),'Poison native clock count')
  for v in value:require(type(v)is list and len(v)==3 and all(type(k)is int and 0<=k<0x7f800000 for k in v[:2])and type(v[2])is bool,'Poison native clock domain')
 clocks(doc['initial']);require(type(doc['rows'])is list and len(doc['rows'])==TICKS,'Poison native row count');inputs=json.loads(files['input.json']);requests=[]
 for index,row in enumerate(doc['rows']):
  require(type(row)is dict and set(row)=={'tick','actions','before_tree_pause','delta_bits','clocks','timeouts'}and type(row['tick'])is int and row['tick']==index and row['actions']==inputs['timeline'][index]and type(row['before_tree_pause'])is bool and type(row['delta_bits'])is int and row['delta_bits']==struct.unpack('<I',struct.pack('<f',1/60))[0],'Poison native clock/actions domain');clocks(row['clocks']);require(type(row['timeouts'])is list and len(row['timeouts'])<=len(m['holes'])and all(type(v)is int and 0<=v<len(m['holes'])for v in row['timeouts'])and len(set(row['timeouts']))==len(row['timeouts']),'Poison native timeout domain');requests+=row['timeouts']
 def bits(v):return struct.unpack('<I',struct.pack('<f',v))[0]
 expected=[[bits(poison.f.f32(h['position'][0]+m['spawn'][0])),bits(poison.f.f32(h['position'][1]+m['spawn'][1]))]for h in[m['holes'][i]for i in requests]];require(doc['spawned']==expected,'Poison actual deferred spawn count/order/positions differ')
 require(type(doc['contacts'])is list and len(doc['contacts'])==len(inputs['contacts']),'Poison native contact count')
 for row,action in zip(doc['contacts'],inputs['contacts']):
  require(type(row)is dict and set(row)=={'action','player','party','wanted'}and row['action']==action and row['wanted']in['wood']+sound_names,'Poison native contact identity')
  for key in['player','party']:
   v=row[key];require(type(v)is list and len(v)==5 and type(v[0])is bool and all(type(n)is int and 0<=n<=1000000 for n in v[1:3])and type(v[3])is list and len(v[3])<=256 and all(s==m['status']for s in v[3])and v[4]in mode_names,'Poison native damage/status/Shadow request domain')
 expected=[]
 for s in m['shapes']:expected.append([[bits(poison.f.f32(p[0]+s['position'][0])),bits(poison.f.f32(p[1]+s['position'][1]))]for p in m['points'][s['first']:s['first']+s['count']]])
 require(doc['geometry']==expected,'Poison native original polygon/global vertex geometry differs');return doc

def main():
 a=argparse.ArgumentParser(description=__doc__);a.add_argument('--out',type=Path,required=True);a.add_argument('--godot',type=Path,default=ROOT/'build/godot-reference/Godot_v3.6.2-stable_win64.exe');o=a.parse_args();out=o.out.resolve();engine=o.godot.resolve();require(out.is_relative_to(ROOT/'build')and out!=ROOT/'build'and not out.exists(),'Fresh poison original output under build required');require(hashlib.sha256(engine.read_bytes()).hexdigest()=='e748acc92ee5205dd3cc1754d4422870f6a68ae6e00a16ff88bd3438c957adfd','Poison original engine SHA');files,proof=source();out.mkdir();project=out/'temporary';project.mkdir();(project/'.owned-poison-reference').write_bytes(b'poison-reference-v1')
 for name,value in files.items():target=project/name;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(value.encode())
 invoke(engine,project,['--video-driver','GLES2','--debug','--editor','--quit'],out/'import.log')
 # The editor may serialize its class index into project.godot. Retain that
 # actual intermediate input,then restore every checked file before execution.
 (out/'editor-project.godot').write_bytes((project/'project.godot').read_bytes())
 for name,value in files.items():(project/name).write_bytes(value.encode())
 invoke(engine,project,['--video-driver','GLES2','--debug','--fixed-fps','60'],out/'godot.log');doc=validate_result(json.loads((out/'reference.json').read_bytes()),proof)
 for name,digest in proof['generated_sha256'].items():require(hashlib.sha256((project/name).read_bytes()).hexdigest()==digest,'Poison original executed input changed')
 proof.update(producer_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),engine_sha256=hashlib.sha256(engine.read_bytes()).hexdigest(),result_sha256=hashlib.sha256((out/'reference.json').read_bytes()).hexdigest());(out/'receipt.json').write_text(json.dumps(proof,indent=2)+'\n');print('Actual original poison:',len(doc['rows']),'Timer frames,',len(doc['contacts']),'manual callbacks,',len(doc['spawned']),'settled source deferred children;not full hazards/scene/hardware')
if __name__=='__main__':main()
