"""Original poison geometry/automatic contacts and real Character damage.

Selected Player identity,teleport and explicit damage/Flash clock services;
not ordinary movement,complete physics-frame timing,factory or formal scene.
"""
import argparse,hashlib,json,math,struct,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import run_poison_damage_reference as damage,catacombs_poison as poison,catacombs_steps as steps
from tools.run_dungeon_key_contact_reference import player_source
from tools.run_scene_reference import invoke
require=poison.require

PROBE=r'''var config
var areas=[]
var rows=[]
var contacts=[]
var selected
func notice(body,index,entered):contacts.append([index,entered,body==selected])
func settle():
    for _i in 4:yield(get_tree(),"physics_frame")
    yield(get_tree(),"idle_frame")
func describe(actor,body,ci,phase):
    var row=capture(actor,str(ci)+":"+phase)
    row.case=ci
    row.phase=phase
    row.position=[bits(body.position.x),bits(body.position.y)]
    row.layer=body.collision_layer
    row.mask=body.collision_mask
    row.enabled=!body.get_node("CollisionShape2D").disabled
    row.paused=actor._paused
    row.continuous=actor._is_continuous_damage
    row.damage=actor._attack_damage
    row.variance=actor._damage_variance
    row.run_sound=actor.run_sound
    row.shadow=actor.shadow_mode
    row.contacts=contacts.duplicate(true)
    row.overlaps=[]
    for area in areas:row.overlaps.append(area.overlaps_body(body))
    contacts.clear()
    rows.append(row)
func shape(body):
    var collider=CollisionPolygon2D.new()
    collider.name="CollisionShape2D"
    collider.position=Vector2(config.player.position[0],config.player.position[1])
    var points=PoolVector2Array()
    for point in config.player.points:points.append(Vector2(point[0],point[1]))
    collider.polygon=points
    body.add_child(collider)
    body.collision_layer=config.player.layer
    body.collision_mask=config.player.mask
    body.position=Vector2(20000,20000)
func geometry():
    var result=[]
    for i in areas.size():
        var shape=areas[i].get_node("CollisionPolygon2D")
        var parts=[]
        for owner in areas[i].get_shape_owners():
            if areas[i].shape_owner_get_owner(owner)!=shape:continue
            for j in areas[i].shape_owner_get_shape_count(owner):
                var polygon=areas[i].shape_owner_get_shape(owner,j)
                assert(polygon is ConvexPolygonShape2D)
                var points=[]
                for point in polygon.points:
                    var p=shape.global_transform.xform(point)
                    points.append([p.x,p.y])
                parts.append(points)
        result.append({"parts":parts,"layer":areas[i].collision_layer,"mask":areas[i].collision_mask,"monitoring":areas[i].monitoring,"monitorable":areas[i].monitorable})
    return result
func run():
    global.currentScene=self
    var file=File.new()
    assert(file.open("res://input.json",File.READ)==OK)
    config=JSON.parse(file.get_as_text()).result
    file.close()
    for binding in config.areas:
        var area=load("res://Nodes/Reusables/Overlap/Poison.tscn").instance()
        area.collision_layer=int(binding.layer)
        area.collision_mask=int(binding.mask)
        area.monitoring=bool(int(binding.bits)&1)
        area.monitorable=bool(int(binding.bits)&2)
        area.stepping_sound=NodePath("Step")
        var step=load("res://step.gd").new()
        step.name="Step"
        for key in ["entering_sound","exiting_sound","enter_shadow_effect","exit_shadow_effect","enabled"]:
            step.set(key,config.step[key])
        area.add_child(step)
        for role in 2:
            var description=config.shapes[int(binding.first)+role]
            var polygon=area.get_node("CollisionPolygon2D" if role==0 else "StaticBody2D/CollisionPolygon2D")
            polygon.position=Vector2(description.position[0],description.position[1])
            var points=PoolVector2Array()
            for index in range(int(description.first),int(description.first+description.count)):points.append(Vector2(config.points[index][0],config.points[index][1]))
            polygon.polygon=points
        area.connect("body_entered",self,"notice",[areas.size(),true])
        area.connect("body_exited",self,"notice",[areas.size(),false])
        add_child(area)
        areas.append(area)
    yield(settle(),"completed")
    var original_geometry=geometry()
    for ci in config.cases.size():
        var test=config.cases[ci]
        var actor=create_actor(int(config.hp),int(config.maxhp))
        actor.run_sound=config.initial_run
        actor.shadow_mode=config.initial_shadow
        shape(actor)
        var body=actor
        if test.other:
            body=KinematicBody2D.new()
            add_child(body)
            shape(body)
        selected=actor
        contacts.clear()
        seed(int(config.seed))
        yield(settle(),"completed")
        describe(actor,body,ci,"initial")
        body.collision_layer=int(test.layer)
        body.collision_mask=int(test.mask)
        body.get_node("CollisionShape2D").disabled=!test.enabled
        actor._paused=test.paused
        body.position=Vector2(test.point[0]-config.player.center[0],test.point[1]-config.player.center[1])
        yield(settle(),"completed")
        describe(actor,body,ci,"enter")
        actor._paused=false
        body.get_node("CollisionShape2D").disabled=false
        yield(settle(),"completed")
        describe(actor,body,ci,"stay")
        actor.physics_tail()
        if actor._flash_anim.is_playing():actor._flash_anim.advance(1.0/60.0)
        describe(actor,body,ci,"hit")
        body.position=Vector2(20000,20000)
        yield(settle(),"completed")
        describe(actor,body,ci,"leave")
        body.collision_layer=config.player.layer
        body.collision_mask=config.player.mask
        body.position=Vector2(test.point[0]-config.player.center[0],test.point[1]-config.player.center[1])
        yield(settle(),"completed")
        describe(actor,body,ci,"reenter")
        body.position=Vector2(20000,20000)
        yield(settle(),"completed")
        describe(actor,body,ci,"exit")
        rows[-1].rng_next=randi()
        if body!=actor:body.free()
        retire(actor)
        selected=null
    assert(file.open("res://../reference.json",File.WRITE)==OK)
    file.store_string(JSON.print({"schema":1,"godot":Engine.get_version_info(),"geometry":original_geometry,"rows":rows}))
    file.close()
    get_tree().quit()
'''

PHASES=['initial','enter','stay','hit','leave','reenter','exit']
EXTRA=set('case phase position layer mask enabled paused continuous damage variance run_sound shadow contacts overlaps'.split())
def source():
 files,copied,base=damage.source();_,model,proof=poison.source();_,sm,_=steps.source();player,player_proof=player_source();config=json.loads(files['input.json']);prefix=files['probe.gd'].split('func run():')[0];require(prefix!=files['probe.gd'],'Contact observer prefix missing');files['probe.gd']=prefix+PROBE
 target={a['step']for a in model['areas']};require(len(target)==1,'Contact source shared step profile');selected=next(a for a in sm['areas']if a['id']in target)
 step=dict(entering_sound=selected['enter'],exiting_sound=selected['exit'],enter_shadow_effect=selected['enter_shadow'],exit_shadow_effect=selected['exit_shadow'],enabled=selected['enabled']);geometry=[];cases=[]
 for index,area in enumerate(model['areas']):
  shape=model['shapes'][area['first']];points=[[poison.f.f32(v[k]+shape['position'][k])for k in [0,1]]for v in model['points'][shape['first']:shape['first']+shape['count']]];parts=steps.geometry.decompose_polygon_in_convex(points);require(parts,'Original trigger decomposition empty');parts=[[[x,y]for x,y in part]for part in parts];geometry.append(parts);point=[poison.f.f32(sum(v[k]for v in parts[0])/len(parts[0]))for k in [0,1]]
  cases.append(dict(name='area_'+str(index),area=index,point=point,other=False,layer=player['layer'],mask=player['mask'],enabled=True,paused=False))
 for name,change in [('paused_entry',dict(paused=True)),('other_body',dict(other=True)),('mask_zero',dict(mask=0)),('disabled_shape',dict(enabled=False))]:case=dict(cases[0]);case.update(name=name,**change);cases.append(case)
 config.update(player=player,step=step,cases=cases);config.pop('actions');files['input.json']=json.dumps(config)
 proof=dict(damage=base,poison=proof,player=player_proof,trigger_parts=geometry,generated_sha256={n:hashlib.sha256(v.encode()).hexdigest()for n,v in files.items()},copied={n:hashlib.sha256(v).hexdigest()for n,v in copied.items()},scope='Original11 poison areas/polygons,actual KinematicBody2D source8-vertex footprint and automatic Area2D callbacks,full original Character/Status and selected Player damage;explicit teleport/pause and one manual damage/Flash tick per case,run/Shadow/UI/graphics/audio holders. Stable settled contact phases only,no ordinary movement/exact callback-frame/factory/formal room/hardware admission.')
 return files,copied,proof

def canonical(points):
 steps.convex(points);points=[tuple(p)for p in points];return min(tuple(points[n:]+points[:n])for n in range(len(points)))

def validate_result(doc,proof):
 from tests.prepare_world_damage import append_observation
 files,_,current=source();require(type(proof)is dict and all(json.dumps(proof.get(k),sort_keys=True)==json.dumps(v,sort_keys=True)for k,v in current.items()),'Contact source inputs changed');config=json.loads(files['input.json']);_,dm,_=damage.damage.source();_,pm,_=poison.source();_,sm,_=steps.source()
 require(type(doc)is dict and set(doc)=={'schema','godot','geometry','rows'}and type(doc['schema'])is int and doc['schema']==1 and type(doc['godot'])is dict and doc['godot'].get('string')=='3.6.2-stable (official)','Contact engine/schema')
 require(type(doc['geometry'])is list and len(doc['geometry'])==len(pm['areas']),'Contact exact geometry coverage')
 for row,area,expected in zip(doc['geometry'],pm['areas'],current['trigger_parts']):
  require(type(row)is dict and set(row)=={'parts','layer','mask','monitoring','monitorable'}and type(row['layer'])is int and type(row['mask'])is int and row['layer']==area['layer']and row['mask']==area['mask']and type(row['monitoring'])is bool and type(row['monitorable'])is bool and row['monitoring']==bool(area['bits']&1)and row['monitorable']==bool(area['bits']&2),'Contact native area property mismatch')
  require(type(row['parts'])is list and sorted(canonical(v)for v in row['parts'])==sorted(canonical(v)for v in expected),'Contact native convex geometry mismatch')
 require(type(doc['rows'])is list and len(doc['rows'])==len(config['cases'])*len(PHASES),'Contact exact settled phase coverage');seen=set();names={v['name']for v in sm['sounds']};shadows={v['name']for v in sm['shadows']['animations']}
 for index,row in enumerate(doc['rows']):
  ci=index//len(PHASES);phase=PHASES[index%len(PHASES)];require(type(row)is dict and EXTRA<=set(row)and type(row['case'])is int and row['case']==ci and row['phase']==phase and row['label']==str(ci)+':'+phase,'Contact phase/source identity')
  require(all(type(row[k])is bool for k in ['enabled','paused','continuous'])and all(type(row[k])is int and 0<=row[k]<=0xfffff for k in ['layer','mask','damage','variance'])and row['run_sound']in names and row['shadow']in shadows and type(row['position'])is list and len(row['position'])==2 and all(type(v)is int and 0<=v<=0xffffffff and math.isfinite(struct.unpack('<f',struct.pack('<I',v))[0])for v in row['position']),'Contact body/state domain')
  require(type(row['overlaps'])is list and len(row['overlaps'])==len(pm['areas'])and all(type(v)is bool for v in row['overlaps'])and type(row['contacts'])is list and len(row['contacts'])<=len(pm['areas']),'Contact pair/event capacity')
  for event in row['contacts']:require(type(event)is list and len(event)==3 and type(event[0])is int and 0<=event[0]<len(pm['areas'])and all(type(v)is bool for v in event[1:]),'Contact event opcode/body domain')
  if phase=='exit':require(type(row.get('rng_next'))is int and 0<=row['rng_next']<=0xffffffff,'Contact final shared RNG')
  append_observation([],{k:v for k,v in row.items()if k not in EXTRA},dm,seen)
 return doc

def main():
 a=argparse.ArgumentParser(description=__doc__);a.add_argument('--out',type=Path,required=True);out=a.parse_args().out.resolve();require(out.is_relative_to(ROOT/'build')and out!=ROOT/'build'and not out.exists(),'Fresh owned contact output required');engine=ROOT/'build/godot-reference/Godot_v3.6.2-stable_win64.exe';require(hashlib.sha256(engine.read_bytes()).hexdigest()=='e748acc92ee5205dd3cc1754d4422870f6a68ae6e00a16ff88bd3438c957adfd','Original engine changed');files,copied,proof=source();out.mkdir();project=out/'temporary';project.mkdir();(project/'.owned-poison-contact').write_bytes(b'poison-contact-reference-v1')
 for name,raw in copied.items():target=project/name;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(raw)
 def write():
  for name,text in files.items():target=project/name;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(text.encode())
 write();(out/'inputs.json').write_text(json.dumps(proof,indent=2)+'\n');invoke(engine,project,['--video-driver','GLES2','--debug','--editor','--quit'],out/'editor.log');(out/'editor-project.godot').write_bytes((project/'project.godot').read_bytes());write();invoke(engine,project,['--video-driver','GLES2','--debug','--fixed-fps','60'],out/'godot.log');require(all(hashlib.sha256((project/n).read_bytes()).hexdigest()==h for n,h in proof['generated_sha256'].items())and all((project/n).read_bytes()==b for n,b in copied.items()),'Executed contact inputs changed');raw=(out/'reference.json').read_bytes();doc=validate_result(json.loads(raw),proof);proof.update(engine_sha256=hashlib.sha256(engine.read_bytes()).hexdigest(),producer_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),result_sha256=hashlib.sha256(raw).hexdigest());(out/'receipt.json').write_text(json.dumps(proof,indent=2)+'\n');print('Original automatic poison contacts:',len(doc['rows']),'settled states;not exact frame/full room/hardware')
if __name__=='__main__':main()
