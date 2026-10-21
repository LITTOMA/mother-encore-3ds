"""Original Small Key callbacks driven by native Area2D/player-polygon contacts.

Player identity/pause/motion, UI/flag observers and audio occupancy/completion
are explicit services. Source shapes and scripts are checked; no terrain,
rendering, exact physics-frame latency, audible output or complete map claim.
"""
import argparse,hashlib,json,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import dungeon_key_media as d,field_swing
from tools.godot_text import Document,Vec2
from tools.run_dungeon_keys_reference import GLOBAL
from tools.run_dungeon_key_clocks_reference import AUDIO
from tools.run_battle_victory_reference import function
from tools.run_scene_reference import invoke

PROBE='''extends Node
var config
var targets=[]
var rows=[]
var entries=[]
var notices=0
var initials=[]
func _ready():call_deferred("run")
func notice():notices+=1
func describe(node):
    if not is_instance_valid(node):return {"alive":false}
    return {"alive":true,"queued":node.is_queued_for_deletion(),"visible":node.visible,"audio":node.get_node("AudioStreamPlayer").playing,"disabled":node.get_node("Area2D/CollisionShape2D").disabled}
func entered(body,owner):entries.append({"owner":owner,"player":body==global.player})
func body_shape(body):
    var shape=CollisionPolygon2D.new()
    shape.name="CollisionShape2D"
    var points=PoolVector2Array()
    for value in config.player.points:points.append(Vector2(value[0],value[1]))
    shape.polygon=points
    shape.position=Vector2(config.player.position[0],config.player.position[1])
    body.add_child(shape)
func place(body,owner):body.global_position=Vector2(owner.center[0]-config.player.center[0],owner.center[1]-config.player.center[1])
func make_keys():
    var parent=TileMap.new()
    parent.name=config.parent
    parent.cell_size=Vector2(config.cell_size[0],config.cell_size[1])
    global.currentScene.add_child(parent)
    targets.clear()
    for definition in config.keys:
        var target=Sprite.new()
        target.name=definition.name
        target.hframes=definition.hframes
        target.position=Vector2(definition.position[0],definition.position[1])
        target.set_script(load("res://"+config.key_script))
        target.emit_flag_updated_signal=definition.emit_flags
        var area=Area2D.new()
        area.name="Area2D"
        area.collision_layer=definition.layer
        area.collision_mask=definition.mask
        area.monitoring=bool(int(definition.bits)&2)
        area.monitorable=bool(int(definition.bits)&4)
        var shape=CollisionShape2D.new()
        shape.name="CollisionShape2D"
        shape.position=Vector2(definition.center[0]-definition.position[0],definition.center[1]-definition.position[1])
        var rectangle=RectangleShape2D.new()
        rectangle.extents=Vector2(definition.extents[0],definition.extents[1])
        shape.shape=rectangle
        shape.disabled=bool(int(definition.bits)&1)
        shape.one_way_collision_margin=definition.margin
        area.add_child(shape)
        target.add_child(area)
        var audio=load("res://audio.gd").new()
        audio.name="AudioStreamPlayer"
        target.add_child(audio)
        assert(area.connect("body_entered",target,"_on_Area2D_body_entered")==OK)
        assert(audio.connect("finished",target,"_on_AudioStreamPlayer_finished")==OK)
        assert(area.connect("body_entered",self,"entered",[definition.owner])==OK)
        parent.add_child(target)
        targets.append(target)
func settle():
    for _i in 4:yield(get_tree(),"physics_frame")
    yield(get_tree(),"idle_frame")
func capture(ci,pi):
    var states=[]
    var flags=[]
    var writes=[]
    for i in config.keys.size():
        states.append(describe(targets[i]))
        flags.append(globaldata.object_flags.get(config.keys[i].flag,false))
        if is_instance_valid(targets[i]):
            var audio=targets[i].get_node("AudioStreamPlayer")
            for value in audio.writes:writes.append({"owner":config.keys[i].owner,"playing":value})
            audio.writes.clear()
    rows.append({"case":ci,"phase":pi,"count":uiManager.get_key_count(),"states":states,"flags":flags,"entries":entries.duplicate(true),"notices":notices,"ui":uiManager.events.duplicate(true),"audio_writes":writes})
    entries.clear()
    notices=0
    uiManager.events.clear()
func run():
    var file=File.new()
    assert(file.open("res://input.json",File.READ)==OK)
    config=JSON.parse(file.get_as_text()).result
    file.close()
    global.connect("flags_updated",self,"notice")
    body_shape(global.player)
    var other=KinematicBody2D.new()
    other.name="OtherBody"
    body_shape(other)
    global.add_child(other)
    for ci in config.cases.size():
        var test=config.cases[ci]
        global.currentScene.queue_free()
        yield(get_tree(),"idle_frame")
        yield(get_tree(),"idle_frame")
        global.currentScene=global.SceneService.new()
        global.currentScene.name="Catacombs"
        global.add_child(global.currentScene)
        globaldata.object_flags.clear()
        globaldata.flags.clear()
        globaldata.keys={"Catacombs":0}
        entries.clear()
        notices=0
        uiManager.events.clear()
        global.player.position=Vector2(10000,10000)
        other.position=Vector2(10000,10000)
        global.player.collision_layer=config.player.layer
        global.player.collision_mask=config.player.mask
        other.collision_layer=config.player.layer
        other.collision_mask=config.player.mask
        global.player.get_node("CollisionShape2D").disabled=false
        global.player.paused=test.initial_paused
        # Flush the old body's physical transform while the new scene is empty.
        # Otherwise a newly registered Area can observe the previous case's pose.
        yield(settle(),"completed")
        if test.saved:globaldata.object_flags[config.keys[test.target].flag]=true
        if test.initial_inside:place(global.player,config.keys[test.target])
        make_keys()
        yield(settle(),"completed")
        initials.append({"case":ci,"count":uiManager.get_key_count()})
        for pi in test.phases.size():
            var phase=test.phases[pi]
            global.player.paused=phase.paused
            global.player.collision_layer=phase.layer
            global.player.collision_mask=phase.mask
            global.player.get_node("CollisionShape2D").disabled=not phase.enabled
            global.player.position=Vector2(10000,10000)
            other.position=Vector2(10000,10000)
            if phase.inside:place(other if phase.other else global.player,config.keys[test.target])
            if phase.finish and is_instance_valid(targets[test.target]):targets[test.target].get_node("AudioStreamPlayer").finish()
            yield(settle(),"completed")
            capture(ci,pi)
    global.currentScene.queue_free()
    other.queue_free()
    yield(get_tree(),"idle_frame")
    yield(get_tree(),"idle_frame")
    assert(file.open("res://../contacts.json",File.WRITE)==OK)
    file.store_string(JSON.print({"engine":Engine.get_version_info(),"rows":rows,"initials":initials},"  "))
    file.close()
    print("KEY_CONTACT_REFERENCE_COMPLETE")
    get_tree().quit()
'''

def player_source():
 review,ex=field_swing.read();name=review['scene'];d.require(name=='Nodes/Reusables/Player.tscn','Key contact player source selector');doc=Document(name,ex.text(name));root=doc.nodes[0];shape=next(n for n in doc.nodes if n.attrs.get('name')=='CollisionShape2D'and n.attrs.get('parent')=='.')
 d.require(root.attrs['type']=='KinematicBody2D'and set(root.props)=={'use_parent_material','position','collision_mask','script'}and shape.attrs['type']=='CollisionPolygon2D'and set(shape.props)=={'position','polygon'},'Key contact player body/shape source changed');points=shape.props['polygon'];position=shape.props['position'];d.require(type(position)is Vec2 and type(points)is list and len(points)==8 and all(type(v)is Vec2 for v in points),'Key contact polygon source')
 center=[d.real((min(getattr(p,a)for p in points)+max(getattr(p,a)for p in points))*.5+getattr(position,a))for a in['x','y']]
 return dict(layer=1,mask=root.props['collision_mask'],position=d.vector(position),points=[d.vector(p)for p in points],center=center),dict(path=name,sha256=hashlib.sha256(ex.data(name)).hexdigest(),scope='Native KinematicBody2D default layer1; original root mask and all8 polygon vertices+local origin. Movement/pause script is an explicit service.')

def cases():
 def phase(name,**kw):p=dict(name=name,inside=True,other=False,paused=False,enabled=True,layer=1,mask=4353,finish=False);p.update(kw);return p
 def case(name,phases,**kw):p=dict(name=name,target=0,initial_inside=False,initial_paused=False,saved=False,phases=phases);p.update(kw);return p
 out=[case('enter_stay_leave_reenter_finish',[phase('enter'),phase('stay'),phase('leave',inside=False),phase('reenter'),phase('finish',finish=True)]),case('paused_entry_consumed',[phase('enter_paused',paused=True),phase('unpause_stay'),phase('leave',inside=False),phase('reenter'),phase('finish',finish=True)]),case('other_body_ignored',[phase('other',other=True),phase('player')])]
 for layer,mask in[(0,4353),(1,0),(2,4353),(0,0),(2,0)]:out.append(case('filter_%s_%s'%(layer,mask),[phase('enter',layer=layer,mask=mask)]))
 out.append(case('disabled_body',[phase('disabled_inside',enabled=False),phase('enable_inside'),phase('stay')]))
 for target in range(3):out.append(case('initial_overlap_'+str(target),[phase('inside')],target=target,initial_inside=True))
 out.append(case('saved_initial_overlap',[phase('inside')],initial_inside=True,saved=True));out.append(case('initial_paused_overlap',[phase('unpause_stay'),phase('leave',inside=False),phase('reenter')],initial_inside=True,initial_paused=True))
 return out

def validate_observed(observed,config):
 d.require(type(observed)is dict and set(observed)=={'engine','rows','initials'}and observed['engine']['string']=='3.6.2-stable (official)','Key contact original engine/output fields')
 d.require([(v['case'],v['phase'])for v in observed['rows']]==[(i,j)for i,c in enumerate(config['cases'])for j in range(len(c['phases']))],'Key contact original phase coverage')
 d.require(observed['initials']==[dict(case=i,count=int(c['initial_inside']and not c['initial_paused']and not c['saved']))for i,c in enumerate(config['cases'])],'Key contact declared initial physics state differs')
 for row in observed['rows']:
  d.require(set(row)=={'case','phase','count','states','flags','entries','notices','ui','audio_writes'}and len(row['states'])==len(row['flags'])==len(config['keys'])and type(row['count'])is int and row['count']>=0,'Key contact observed state/coverage')
  for state in row['states']:d.require(type(state)is dict and type(state.get('alive'))is bool and set(state)==({'alive','queued','visible','audio','disabled'}if state['alive']else{'alive'})and all(type(v)is bool for v in state.values()),'Key contact observed live state fields')
  d.require(all(type(v)is bool for v in row['flags']),'Key contact observed flag type')
 return observed

def run(out,engine):
 out=Path(out).resolve();engine=Path(engine).resolve();d.require(out.is_relative_to(ROOT/'build')and not out.exists()and engine.is_file(),'Fresh key-contact project and real engine required');review,source=d.read();model,_,proof=d.source_model();clock=d.clocks.parse(model['clocks']);metadata={n['id']:n for n in clock['metadata']['nodes']};player,player_proof=player_source();profile=next(p for p in d.clocks.read()[0]['profiles']if p['kind']=='key');doc=Document(profile['scene'],source.text(profile['scene']));root=doc.nodes[0];key_nodes=[o for o in model['owners']if clock['metadata']['nodes'][model['owners'].index(o)]['kind']==1];d.require(len(key_nodes)==3,'Key-contact fixture target coverage')
 keys=[]
 for owner in key_nodes:
  shape=next(s for s in model['shapes']if s['owner']==owner['owner']and s['role']==1);d.require(owner['position']==[d.real(v)for v in owner['position']],'Key contact root transform');keys.append(dict(owner=owner['owner'],name=owner['path'].rsplit('/',1)[-1],flag=metadata[owner['owner']]['flag'],position=owner['position'],center=shape['center'],extents=shape['extents'],layer=shape['layer'],mask=shape['mask'],bits=shape['bits'],margin=shape['one_way_margin'],hframes=root.props['hframes'],emit_flags=root.props['emit_flag_updated_signal']))
 scene_nodes={n.path:n for n in d.instantiate(source.doc,'Maps/podunk/Catacombs.tscn')};parents={o['parent']for o in key_nodes};d.require(len(parents)==1,'Key contact source parent coverage');parent=next(iter(parents));parent_node=scene_nodes[parent];d.require(parent_node.type=='TileMap'and not any(k in parent_node.props for k in['position','scale','rotation']),'Key contact parent transform unsupported');config=dict(player=player,keys=keys,parent=parent,cell_size=d.vector(parent_node.props['cell_size']),key_script=profile['script'],cases=cases())
 project=out/'project';project.mkdir(parents=True);(project/'.owned-key-contact-reference').write_text('source-key-contact-reference-v1',encoding='utf8');copied={}
 for name in[profile['script'],'Scripts/Main/FlaggableObject.gd']:
  raw=source.data(name);target=project/name;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(raw);copied[name]=hashlib.sha256(raw).hexdigest()
 global_text=GLOBAL.replace('class PlayerService:\n    extends Node\n','class PlayerService:\n    extends KinematicBody2D\n');d.require(global_text!=GLOBAL,'Key contact explicit player body service');(project/'global.gd').write_text(global_text,encoding='utf8');(project/'audio.gd').write_text(AUDIO,encoding='utf8');(project/'probe.gd').write_text(PROBE,encoding='utf8');(project/'input.json').write_text(json.dumps(config,indent=2)+'\n',encoding='utf8')
 names=d.clocks.dungeon_keys.read()[0]['paths'];methods='\n'.join(function(source.text(names['globaldata_script']),n)for n in['set_flag','set_object_flag']);(project/'data.gd').write_text('extends Node\nvar flags={}\nvar object_flags={}\nvar keys={"Catacombs":0}\n'+methods,encoding='utf8');methods='\n'.join(function(source.text(names['ui_script']),n)for n in['get_key_count','try_alter_key_count']);(project/'ui.gd').write_text('extends Node\nvar events=[]\nfunc update_key_indicator():events.append(get_key_count())\n'+methods,encoding='utf8')
 (project/'probe.tscn').write_text('[gd_scene load_steps=2 format=2]\n[ext_resource path="res://probe.gd" type="Script" id=1]\n[node name="Probe" type="Node"]\nscript = ExtResource( 1 )\n',encoding='utf8');(project/'project.godot').write_text('config_version=4\n[application]\nconfig/name="Original source key contacts"\nrun/main_scene="res://probe.tscn"\n[autoload]\nglobal="*res://global.gd"\nglobaldata="*res://data.gd"\nuiManager="*res://ui.gd"\n[debug]\ngdscript/warnings/unused_argument=false\ngdscript/warnings/unused_signal=false\n',encoding='utf8')
 invoke(engine,project,['--debug','--editor','--quit'],out/'import.log');invoke(engine,project,['--debug'],out/'engine.log');observed=validate_observed(json.loads((out/'contacts.json').read_text('utf8')),config)
 for name,h in copied.items():d.require(hashlib.sha256((project/name).read_bytes()).hexdigest()==h,'Key contact original script changed')
 (out/'input.json').write_bytes((project/'input.json').read_bytes());proof.update(scope=__doc__,player=player_proof,copied_scripts=copied,engine_sha256=hashlib.sha256(engine.read_bytes()).hexdigest(),producer_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),cases=len(config['cases']),rows=len(observed['rows']),contacts_sha256=hashlib.sha256((out/'contacts.json').read_bytes()).hexdigest(),input_sha256=hashlib.sha256((out/'input.json').read_bytes()).hexdigest(),adapter_sha256={p.name:hashlib.sha256(p.read_bytes()).hexdigest()for p in project.glob('*.gd')});(out/'receipt.json').write_text(json.dumps(proof,indent=2)+'\n',encoding='utf8');print('Original native key contacts:',proof['cases'],'cases;',proof['rows'],'settled observations; explicit audio/UI/player services,not live map')
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',type=Path,required=True);p.add_argument('--godot',type=Path,required=True);a=p.parse_args();run(a.out,a.godot)
