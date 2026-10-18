"""Native stepping-area contact outcomes on source Player body geometry.

Teleport fixtures settle for four physics frames. This measures stable region
selection and one-way mask filtering, not boundary-frame or full-map physics.
"""
import argparse,hashlib,json,math,shutil,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import run_footstep_reference as run,podunk_field as f
from tools.godot_text import instantiate,Vec2
from tools.extract_battle_entry import require,PIN
from tools.run_scene_reference import invoke
PROBE='''extends SceneTree
class Driver:
    extends Node
    var runner
    func _physics_process(_delta):runner.tick()
var config
var player
var other
var world
var areas=[]
var rows=[]
var index=0
var frame=0
func _init():call_deferred("setup")
func body():
    var result=load("res://party.gd").new()
    result.collision_layer=config.player.layer
    result.collision_mask=config.player.mask
    result.run_sound=config.default
    result.shadow=config.shadow
    result.position=Vector2(80,80)
    var shape=CollisionPolygon2D.new()
    var points=PoolVector2Array()
    for point in config.player.points:points.append(Vector2(point[0],point[1]))
    shape.polygon=points
    result.add_child(shape)
    get_root().add_child(result)
    return result
func setup():
    var file=File.new()
    assert(file.open("res://input.json",File.READ)==OK)
    config=JSON.parse(file.get_as_text()).result
    file.close()
    player=body()
    other=body()
    world=load("res://world.gd").new()
    world.player=player
    get_root().add_child(world)
    for record in config.areas:
        var area=load("res://step.gd").new()
        area.global=world
        area.entering_sound=record.enter
        area.exiting_sound=record.exit
        area.enter_shadow_effect=record.enter_shadow
        area.exit_shadow_effect=record.exit_shadow
        area.enabled=record.enabled
        area.position=Vector2(record.center[0],record.center[1])
        area.collision_layer=record.layer
        area.collision_mask=record.mask
        area.monitorable=false
        var shape=CollisionShape2D.new()
        var rectangle=RectangleShape2D.new()
        rectangle.extents=Vector2(record.extents[0],record.extents[1])
        shape.shape=rectangle
        area.add_child(shape)
        for connection in config.connections:area.connect(connection.signal,area,connection.method)
        world.add_child(area)
        areas.append(area)
    var driver=Driver.new()
    driver.runner=self
    get_root().add_child(driver)
func tick():
    if frame==0:
        var step=config.cases[index]
        player.position=Vector2(step.player[0],step.player[1])
        other.position=Vector2(step.other[0],step.other[1])
        player.collision_mask=step.mask
        for i in areas.size():
            if step.enabled[i]:areas[i].enable()
            else:areas[i].disable()
    frame+=1
    if frame<5:return
    var inside=[]
    for area in areas:inside.append(area.get_overlapping_bodies().has(player))
    rows.append({"case":config.cases[index],"wanted":player.run_sound,"inside":inside,"shadow":player.shadow})
    index+=1
    frame=0
    if index<config.cases.size():return
    var file=File.new()
    assert(file.open("res://../reference.json",File.WRITE)==OK)
    file.store_string(JSON.print({"schema":1,"godot":Engine.get_version_info(),"rows":rows},"  "))
    file.close()
    player.free()
    other.free()
    world.free()
    quit()
'''
def source():
    m,_,area,base=run.source();p=f.load_recipe(ROOT/'content/mayor-field.json');sources=f.Sources();nodes=instantiate(sources.doc,p['player']['scene']);root=nodes[0];shape=next(n for n in nodes if n.path=='CollisionShape2D')
    require(root.type=='KinematicBody2D'and shape.type=='CollisionPolygon2D'and not shape.props.get('disabled',False),'Step contact source body kind/shape')
    layer=root.props.get('collision_layer',1);mask=root.props.get('collision_mask',1);require(layer==p['player']['collision_layer']and mask==p['player']['collision_mask'],'Step contact source Player filter')
    root_transform=f.local_transform(root);require(root_transform.xx==root_transform.yy==1 and root_transform.xy==root_transform.yx==0 and f.parent_path(shape.path)=='.','Step contact source Player basis/shape parent')
    raw=shape.props['polygon'];require(type(raw)is list and 3<=len(raw)<=16 and all(isinstance(v,Vec2)for v in raw),'Step contact source polygon')
    # Inputs name the persistent Player origin, not its scene file's initial
    # parent-space position. Keep the collider's own source translation.
    points=[[v.x,v.y]for v in [f.local_transform(shape).apply(v)for v in raw]]
    room_ir=ROOT/'content/native-opening.json';room=json.loads(room_ir.read_bytes());scene=room['sections']['Scene'][0];hull=room['sections']['Vec2'][scene['actor_hull_first']:scene['actor_hull_first']+scene['actor_hull_count']]
    require(points==[[v['x'],v['y']]for v in hull],'Source contact polygon differs from compiled shared-core hull')
    connections=sources.doc('Nodes/Reusables/stepping sounds.tscn').connections
    require(connections==[dict(signal='body_entered',**{'from':'.','to':'.'},method='_on_Stepping_Sounds_body_entered'),dict(signal='body_exited',**{'from':'.','to':'.'},method='_on_Stepping_Sounds_body_exited')],'Step source signal bindings changed')
    outside=[80,80];wood=m['areas'][0]['center'];carpet=m['areas'][1]['center'];enabled=[True,True]
    def case(player=outside,other=outside,enabled=enabled,mask=mask):return dict(player=list(player),other=list(other),enabled=list(enabled),mask=mask)
    cases=[case(),case(wood),case(wood),case(),case(carpet),case(carpet),case(),case(enabled=[True,False]),case(carpet,enabled=[True,False]),case(carpet),case(carpet),case(),case(carpet),case(carpet,mask=0),case(mask=0),case(carpet,mask=0),case(),case(other=carpet),case(other=wood),case()]
    geometry=dict(layer=layer,mask=mask,points=points)
    proof=dict(commit=PIN,whole_gameplay_approved=False,sources={**base['sources'],**sources.digests()},base_run_review_sha256=hashlib.sha256((ROOT/'compatibility/reviews/run-loop-v0410.json').read_bytes()).hexdigest(),area_sha256=hashlib.sha256(area.encode()).hexdigest(),probe_sha256=hashlib.sha256(PROBE.encode()).hexdigest(),geometry=geometry,root_scene_position=[root_transform.ox,root_transform.oy],compiled_hull_ir_sha256=hashlib.sha256(room_ir.read_bytes()).hexdigest(),connections=connections,cases=cases,scope='Player-local source polygon (collider translation retained; teleports override root scene position), source step callbacks/signals/rectangles in native Area2D/KinematicBody2D; teleport inputs settle four subsequent physics ticks; injected PartyObject identity/shadow setter and global player pointer; no full map, movement, precise boundary timing, shader or audio claim')
    return m,area,geometry,cases,connections,proof
def validate_result(doc,cases=None):
    if cases is None:cases=source()[3]
    require(type(doc)is dict and set(doc)=={'schema','godot','rows'}and type(doc['schema'])is int and doc['schema']==1 and type(doc['godot'])is dict and doc['godot'].get('string')=='3.6.2-stable (official)'and type(doc['rows'])is list and len(doc['rows'])==len(cases),'Step contact result schema/count')
    for row,case in zip(doc['rows'],cases):
        require(type(row)is dict and set(row)=={'case','wanted','inside','shadow'}and row['case']==case and type(row['case'])is dict and all(type(v)is bool for v in row['case']['enabled'])and type(row['case']['mask'])is int,'Step contact input/order')
        require(row['wanted']in('wood','stone')and type(row['wanted'])is str and row['shadow']=='shadow'and type(row['inside'])is list and len(row['inside'])==2 and all(type(v)is bool for v in row['inside']),'Step contact output domain')
    return doc
def fixture(doc):
    validate_result(doc);lines=['# case player_x player_y other_x other_y enabled0 enabled1 mask wanted inside0 inside1']
    for i,row in enumerate(doc['rows']):
        c=row['case'];lines.append('\t'.join(str(v)for v in [i,*c['player'],*c['other'],*[int(v)for v in c['enabled']],c['mask'],row['wanted'],*[int(v)for v in row['inside']]]))
    return '\n'.join(lines)+'\n'
def main():
    ap=argparse.ArgumentParser();ap.add_argument('--godot',type=Path,required=True);ap.add_argument('--out',type=Path,required=True);o=ap.parse_args();out=o.out.resolve();require(out.is_relative_to(ROOT/'build')and out!=ROOT/'build'and not out.exists(),'Fresh owned step contact probe required');m,area,geometry,cases,connections,proof=source();require(proof==json.loads((ROOT/'compatibility/reviews/step-contact-v0410.json').read_bytes()),'Step contact source review changed');out.mkdir(parents=True);project=out/'temporary';project.mkdir();(project/'.owned-step-contact').write_bytes(b'step-contact-v1')
    files={'step.gd':area,'party.gd':'extends KinematicBody2D\nvar run_sound=""\nvar shadow=""\nfunc set_shadow(value):shadow=value\n','world.gd':'extends Node\nvar player\nfunc get_player():return player\n','probe.gd':PROBE,'input.json':json.dumps(dict(player=geometry,areas=m['areas'],cases=cases,connections=connections,default=m['default'],shadow=m['shadow'])),'project.godot':'config_version=4\n[application]\nconfig/name="Native source step contacts"\n[logging]\nfile_logging/enable_logging=false\n[physics]\ncommon/physics_fps=60\n[rendering]\nquality/driver/driver_name="GLES2"\n'}
    try:
        for name,value in files.items():(project/name).write_bytes(value.encode())
        invoke(o.godot.resolve(),project,['--script',str(project/'probe.gd')],out/'godot.log');doc=validate_result(json.loads((out/'reference.json').read_bytes()),cases);(out/'contacts.tsv').write_bytes(fixture(doc).encode())
        (out/'receipt.json').write_bytes((json.dumps(dict(source=proof,producer_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),engine_sha256=hashlib.sha256(o.godot.read_bytes()).hexdigest(),generated_sha256={name:hashlib.sha256((project/name).read_bytes()).hexdigest()for name in files},result_sha256=hashlib.sha256((out/'reference.json').read_bytes()).hexdigest(),scope=proof['scope']),indent=2)+'\n').encode());print('Step contacts:',len(doc['rows']),'stable native outcomes')
    finally:
        if project.resolve().is_relative_to(out)and(project/'.owned-step-contact').read_bytes()==b'step-contact-v1':shutil.rmtree(project)
if __name__=='__main__':main()