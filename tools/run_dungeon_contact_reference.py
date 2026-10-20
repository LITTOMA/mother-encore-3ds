"""Native mechanism Area2D entry callbacks with source player collision shapes.

Original mechanism scripts/prefabs run unchanged (apart from the previously
declared wall audio observation property). Player movement, camera, timers and
input are explicit services. This measures settled contact outcomes, not full
map collision, exact physics-frame latency, audible output or hardware.
"""
import argparse,hashlib,json,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import dungeon_switches as d,field_swing
from tools import run_dungeon_mechanism_timeline_reference as base
from tools.run_battle_victory_reference import function
from tools.run_scene_reference import invoke
from tools.godot_text import Document,Vec2

EXTRA='''var contact_events=[]
func observed(value,path):
    contact_events.append({"kind":value,"path":path})
func area_entered(other,path):observed("area_entered",path)
func body_entered(other,path):observed("player_entered" if other==global.player else "other_entered",path)
func hit(path):observed("switch_hit",path)
func body_shape(body):
    var shape=CollisionPolygon2D.new()
    var points=PoolVector2Array()
    for point in config.contact_player.points:points.append(Vector2(point[0],point[1]))
    shape.polygon=points
    shape.position=Vector2(config.contact_player.position[0],config.contact_player.position[1])
    body.add_child(shape)
func make_player_shapes():
    global.player.collision_layer=config.contact_player.layer
    global.player.collision_mask=config.contact_player.mask
    body_shape(global.player)
    var p=config.contact_swing
    var pivot=Node2D.new()
    pivot.name="ProbePivot"
    pivot.position=Vector2(p[4],p[5])
    pivot.rotation=p[6]
    global.player.add_child(pivot)
    var area=load("res://"+config.contact_hitbox).instance()
    area.name="Bat"
    area.position=Vector2(p[7],p[8])
    area.rotation=p[9]
    pivot.add_child(area)
    var shape=area.get_node("CollisionShape2D")
    var capsule=CapsuleShape2D.new()
    capsule.radius=p[13]
    capsule.height=p[14]
    shape.shape=capsule
    shape.position=Vector2(p[10],p[11])
    shape.rotation=p[12]
    shape.disabled=true
    var other=KinematicBody2D.new()
    other.name="ProbeOtherBody"
    other.collision_layer=config.contact_player.layer
    other.collision_mask=config.contact_player.mask
    body_shape(other)
    global.add_child(other)
func capture_contact(case_name,phase):
    var states=[]
    for definition in config.nodes:
        if switches.has(definition.path):
            var target=switches[definition.path]
            states.append({"path":definition.path,"on":target.is_on(),"ready":target._is_ready_to_interact,"disabled":target._is_disabled})
    rows.append({"case":case_name,"phase":phase,"events":contact_events.duplicate(true),"states":states,"paused":global.player.paused,"denied":audioManager.events.duplicate(true)})
    contact_events.clear()
    audioManager.events.clear()
func settle():
    for _i in 4:yield(get_tree(),"physics_frame")
    yield(get_tree(),"idle_frame")
'''
RUN='''func run():
    read_config()
    make_player_shapes()
    var bat=global.player.get_node("ProbePivot/Bat")
    var bat_shape=bat.get_node("CollisionShape2D")
    var other=global.get_node("ProbeOtherBody")
    for test in config.contact_cases:
        globaldata._init_flags()
        globaldata.object_flags.clear()
        global.player.paused=false
        global.player.position=Vector2(10000,10000)
        other.position=Vector2(10000,10000)
        global.player.collision_layer=config.contact_player.layer
        global.player.collision_mask=config.contact_player.mask
        bat.collision_layer=2
        bat.collision_mask=2
        bat.monitorable=true
        bat_shape.disabled=true
        make_scene()
        scene.can_switch=test.can_switch
        for definition in config.nodes:
            if not switches.has(definition.path):continue
            var target=switches[definition.path]
            target.connect("switch_hit",self,"hit",[definition.path])
            var area=target.get_node("interact" if definition.kind=="lever" else "Area2D")
            if definition.kind=="lever":area.connect("area_entered",self,"area_entered",[definition.path])
            else:area.connect("body_entered",self,"body_entered",[definition.path])
        if test.wait_ready:yield(get_tree().create_timer(0.25),"timeout")
        contact_events.clear()
        audioManager.events.clear()
        var target=scene.get_node(test.target)
        var monitor=target.get_node("interact/CollisionShape2D2" if test.kind=="bat" else "Area2D/CollisionShape2D")
        for phase in test.phases:
            global.player.paused=phase.paused
            global.player.position=Vector2(10000,10000)
            other.position=Vector2(10000,10000)
            bat_shape.disabled=not phase.enabled
            bat.collision_layer=phase.layer
            bat.collision_mask=phase.mask
            bat.monitorable=phase.monitorable
            global.player.collision_layer=phase.body_layer
            global.player.collision_mask=phase.body_mask
            if phase.inside:
                if test.kind=="bat":global.player.global_position+=monitor.global_position-bat_shape.global_position
                elif phase.other:other.global_position=monitor.global_position-Vector2(-0.5,6)
                else:global.player.global_position=monitor.global_position-Vector2(-0.5,6)
            if phase.delay>0:yield(get_tree().create_timer(phase.delay),"timeout")
            yield(settle(),"completed")
            capture_contact(test.name,phase.name)
        scene.queue_free()
        yield(get_tree(),"idle_frame")
        yield(get_tree(),"idle_frame")
    var file=File.new()
    assert(file.open("res://contacts.json",File.WRITE)==OK)
    file.store_string(JSON.print({"engine":Engine.get_version_info(),"rows":rows},"  "))
    file.close()
    print("DUNGEON_CONTACT_REFERENCE_COMPLETE")
    get_tree().quit()
'''

def source_player():
 swing=field_swing.source();review,ex=d.read();name='Nodes/Reusables/Player.tscn';doc=Document(name,ex.text(name));root=doc.nodes[0]
 shape=next(n for n in doc.nodes if n.attrs.get('name')=='CollisionShape2D'and n.attrs.get('parent')=='.')
 d.require(root.attrs['type']=='KinematicBody2D'and shape.attrs['type']=='CollisionPolygon2D'and set(shape.props)=={'position','polygon'},'Unreviewed contact player shape')
 points=shape.props['polygon'];pos=shape.props['position']
 d.require(type(points)is list and len(points)==8 and all(type(v)is Vec2 for v in points)and type(pos)is Vec2,'Contact player polygon changed')
 d.require(set(root.props)=={'use_parent_material','position','collision_mask','script'}and root.props['collision_mask']==4353,'Unreviewed contact player root')
 return dict(layer=1,mask=root.props['collision_mask'],position=[pos.x,pos.y],points=[[p.x,p.y]for p in points]),swing,ex

def cases():
 def phase(name,inside=True,**kw):return dict(name=name,inside=inside,paused=False,enabled=True,layer=2,mask=2,body_layer=1,body_mask=4353,monitorable=True,other=False,delay=0,**kw)
 def changed(name,**kw):p=phase(name);p.update(kw);return p
 def case(name,kind,phases,wait=True,can=True):return dict(name=name,kind=kind,target='Bottom2/switch1'if kind=='bat'else'Bottom2/Plate1',phases=phases,wait_ready=wait,can_switch=can)
 out=[]
 for kind in ('bat','body'):
  out.append(case(kind+'_enter_stay_reenter',kind,[phase('enter'),phase('stay'),changed('leave',inside=False),phase('reenter')]))
  out.append(case(kind+'_paused',kind,[changed('enter_paused',paused=True),phase('unpause_stay'),changed('leave',inside=False),phase('reenter')]))
  out.append(case(kind+'_not_ready',kind,[phase('early_enter'),changed('ready_stay',delay=0.25),changed('leave',inside=False),phase('reenter')],wait=False))
  out.append(case(kind+'_denied',kind,[phase('enter'),phase('stay'),changed('leave',inside=False),phase('reenter')],can=False))
 for layer in (0,1,2,64,128):out.append(case('bat_layer_'+str(layer),'bat',[changed('enter',layer=layer,mask=0)]))
 out.append(case('bat_reverse_mask','bat',[changed('enter',layer=0,mask=1)]))
 out.append(case('bat_unmonitorable','bat',[changed('enter',monitorable=False)]))
 out.append(case('bat_disabled','bat',[changed('enter_disabled',enabled=False),phase('enable_inside'),phase('stay')]))
 for layer,mask in ((0,4353),(1,0),(2,4353),(0,0),(2,0)):out.append(case('body_filter_%s_%s'%(layer,mask),'body',[changed('enter',body_layer=layer,body_mask=mask)]))
 out.append(case('other_body','body',[changed('other_enters',other=True),phase('player_enters')]))
 return out

def run(out,engine):
 out=Path(out).resolve();engine=Path(engine).resolve();d.require(out.is_relative_to((ROOT/'build').resolve())and out!=ROOT/'build'and not out.exists()and engine.is_file(),'Fresh owned contact reference required')
 player,swing,ex=source_player();bootstrap=base.run(out,engine);project=out/'project';(project/'.owned-mechanism-contact-reference').write_text('source-mechanism-contact-v1',encoding='utf8')
 config=json.loads((project/'input.json').read_bytes());hitbox='Nodes/Reusables/Overlap/Hitbox.tscn';target=project/hitbox;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(ex.data(hitbox))
 config.update(contact_player=player,contact_swing=swing['real'],contact_hitbox=hitbox,contact_cases=cases());(project/'input.json').write_text(json.dumps(config,indent=2)+'\n',encoding='utf8')
 glob=base.s.GLOBAL;d.require(glob.count('class PlayerService:\n    extends Node\n')==1,'Contact global adapter scope');glob=glob.replace('class PlayerService:\n    extends Node\n','class PlayerService:\n    extends KinematicBody2D\n');(project/'global.gd').write_text(glob,encoding='utf8')
 probe=base.probe();probe=probe.replace(function(probe,'run'),RUN)+'\n'+EXTRA;(project/'probe.gd').write_text(probe,encoding='utf8');invoke(engine,project,['--debug'],out/'contact-engine.log')
 observed=json.loads((project/'contacts.json').read_bytes());v=observed['engine'];d.require((v['major'],v['minor'],v['patch'],v['hash'])==(3,6,2,'3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8'),'Contact reference engine changed')
 d.require([(r['case'],r['phase'])for r in observed['rows']]==[(c['name'],p['name'])for c in config['contact_cases']for p in c['phases']],'Contact case coverage/order changed')
 for name,text in (('global.gd',glob),('probe.gd',probe)):d.require((project/name).read_text(encoding='utf8')==text,'Contact adapter changed during runtime')
 for name in ('contacts.json','input.json'):(out/('contact-input.json'if name=='input.json'else name)).write_bytes((project/name).read_bytes())
 sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
 proof=dict(commit=d.PIN,scope=__doc__,engine_sha256=sha(engine),producer_sha256=sha(Path(__file__)),bootstrap_sha256=sha(out/'receipt.json'),sources={**bootstrap['reference_dependencies'],**swing['sources'],**ex.sources},cases=len(config['contact_cases']),observations=len(observed['rows']),adapter_sha256={name:sha(project/name)for name in ('global.gd','probe.gd')},outputs={name:sha(out/name)for name in ('contacts.json','contact-input.json','contact-engine.log')})
 (out/'contact-receipt.json').write_text(json.dumps(proof,indent=2)+'\n',encoding='utf8');print('Source contacts:',proof['cases'],'cases;',proof['observations'],'settled observations; no exact-frame/full-map claim');return proof

if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',type=Path,required=True);p.add_argument('--godot',type=Path,required=True);a=p.parse_args();run(a.out,a.godot)
