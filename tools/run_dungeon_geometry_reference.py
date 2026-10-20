"""Native source-prefab sprite/shape/default observation, not a full scene.

Uses the previously checked source script graph services. Parent TileMaps have
their actual source cell size/sort policy; terrain tiles are not instantiated.
No player contact, terrain collision, audible output or hardware claim.
"""
import argparse,hashlib,json,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import run_dungeon_mechanism_timeline_reference as base,dungeon_switches as d
from tools.run_battle_victory_reference import function
from tools.run_scene_reference import invoke

GEOMETRY='''func vec(value):return ["%.17f"%value.x,"%.17f"%value.y]
func rectangle(value):return {"position":vec(value.position),"size":vec(value.size)}
func shapes(target,node,result):
    if node is CollisionPolygon2D:assert(false)
    if node is CollisionShape2D:
        assert(node.shape is RectangleShape2D)
        var parent=node.get_parent()
        assert(parent is Area2D or parent is KinematicBody2D)
        var row={"path":str(target.get_path_to(node)),"kind":parent.get_class(),"layer":parent.collision_layer,"mask":parent.collision_mask,"disabled":node.disabled,"one_way":node.one_way_collision,"one_way_margin":node.one_way_collision_margin,"extents":vec(node.shape.extents),"body_position":vec(parent.position),"shape_position":vec(node.position),"local_center":vec(target.global_transform.affine_inverse()*node.global_transform.origin),"global_center":vec(node.global_transform.origin),"corners":[]}
        if parent is Area2D:
            row.monitoring=parent.monitoring
            row.monitorable=parent.monitorable
        else:
            var margin=parent.get("collision/safe_margin")
            assert(typeof(margin)==TYPE_REAL)
            row.safe_margin=margin
        for corner in [Vector2(-1,-1),Vector2(1,-1),Vector2(1,1),Vector2(-1,1)]:row.corners.append(vec(node.global_transform*(corner*node.shape.extents)))
        result.append(row)
    for child in node.get_children():shapes(target,child,result)
func observe():
    var records=[]
    for row in config.nodes:
        var target=scene.get_node(row.path)
        var sprite=target.get_node("Sprite" if row.kind=="plate" else "main")
        var records_for_shape=[]
        shapes(target,target,records_for_shape)
        records.append({"path":row.path,"family":row.kind,"position":vec(target.position),"global_position":vec(target.global_position),"parent_type":target.get_parent().get_class(),"parent_sort":target.get_parent().cell_y_sort,"sprite":{"position":vec(sprite.position),"offset":vec(sprite.offset),"global_position":vec(sprite.global_position),"rect":rectangle(sprite.get_rect()),"centered":sprite.centered,"flip_h":sprite.flip_h,"flip_v":sprite.flip_v,"visible":sprite.visible,"tree_visible":sprite.is_visible_in_tree(),"frame":sprite.frame,"hframes":sprite.hframes,"vframes":sprite.vframes,"texture_size":vec(sprite.texture.get_size()),"z_index":sprite.z_index,"z_relative":sprite.z_as_relative},"shapes":records_for_shape})
    return records
func defaults():
    var sprite=Sprite.new()
    var area=Area2D.new()
    var body=KinematicBody2D.new()
    var shape=CollisionShape2D.new()
    var tile=TileMap.new()
    var result={"sprite":{"centered":sprite.centered,"flip_h":sprite.flip_h,"flip_v":sprite.flip_v,"visible":sprite.visible,"offset":vec(sprite.offset),"z_index":sprite.z_index,"z_relative":sprite.z_as_relative},"area":{"layer":area.collision_layer,"mask":area.collision_mask,"monitoring":area.monitoring,"monitorable":area.monitorable},"body":{"layer":body.collision_layer,"mask":body.collision_mask,"safe_margin":body.get("collision/safe_margin")},"shape":{"disabled":shape.disabled,"one_way":shape.one_way_collision,"one_way_margin":shape.one_way_collision_margin},"tile":{"cell_y_sort":tile.cell_y_sort}}
    sprite.free()
    area.free()
    body.free()
    shape.free()
    tile.free()
    return result
'''

RUN='''func run():
    read_config()
    globaldata._init_flags()
    make_scene()
    var result={"engine":Engine.get_version_info(),"defaults":defaults(),"records":observe()}
    var file=File.new()
    assert(file.open("res://geometry.json",File.WRITE)==OK)
    file.store_string(JSON.print(result,"  "))
    file.close()
    scene.queue_free()
    yield(get_tree(),"idle_frame")
    yield(get_tree(),"idle_frame")
    print("DUNGEON_SOURCE_GEOMETRY_REFERENCE_COMPLETE")
    get_tree().quit()
'''

def run(out,engine):
 out=Path(out).resolve();engine=Path(engine).resolve()
 d.require(out.is_relative_to((ROOT/'build').resolve())and out!=ROOT/'build'and not out.exists()and engine.is_file(),'Fresh geometry reference output and actual engine required')
 # Reuse the exact source/service project producer, retaining its independent
 # timeline output as a bootstrap check. The original producer is not edited.
 timeline=base.run(out,engine);project=out/'project';(project/'.owned-mechanism-geometry-reference').write_text('source-mechanism-geometry-v1',encoding='utf8')
 config=json.loads((project/'input.json').read_bytes());review,ex=d.read();source=d.Sources(ROOT);nodes={n.path:n for n in d.instantiate(source.doc,review['paths']['scene'])};layers={}
 for row in config['nodes']:
  path=row['path'].rsplit('/',1)[0];n=nodes[path];d.require(n.type=='TileMap','Geometry parent is not the reviewed TileMap')
  for k in('position','scale','rotation','transform','rotation_degrees'):d.require(k not in n.props,'Unreviewed source parent transform')
  size=n.props['cell_size'];layers[path]=dict(cell_size=[size.x,size.y],sort=n.props.get('cell_y_sort',False))
 config['geometry_layers']=layers;(project/'input.json').write_text(json.dumps(config,indent=2)+'\n',encoding='utf8')
 probe=base.probe();parent=function(probe,'parent_for');d.require(parent.count('var parent=Node2D.new()')==1,'Geometry factory scope changed')
 replacement=parent.replace('var parent=Node2D.new()','var parent=TileMap.new()\n    var policy=config.geometry_layers[parent_path]\n    parent.cell_size=Vector2(policy.cell_size[0],policy.cell_size[1])\n    parent.cell_y_sort=policy.sort')
 probe=probe.replace(parent,replacement);old=function(probe,'run');probe=probe.replace(old,RUN);probe+='\n'+GEOMETRY
 (project/'probe.gd').write_text(probe,encoding='utf8');invoke(engine,project,['--debug'],out/'geometry-engine.log')
 observed=json.loads((project/'geometry.json').read_bytes());version=observed['engine'];d.require(tuple(version[k]for k in('major','minor','patch','hash'))==(3,6,2,'3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8'),'Geometry reference engine changed')
 d.require([v['path']for v in observed['records']]==[v['path']for v in config['nodes']]and sum(len(v['shapes'])for v in observed['records'])==17,'Geometry owner/shape coverage changed')
 d.require((project/'probe.gd').read_text(encoding='utf8')==probe,'Geometry adapter changed during runtime')
 (out/'geometry.json').write_bytes((project/'geometry.json').read_bytes());(out/'geometry-input.json').write_bytes((project/'input.json').read_bytes())
 proof=dict(commit=d.PIN,scope=__doc__,timeline_bootstrap_receipt_sha256=hashlib.sha256((out/'receipt.json').read_bytes()).hexdigest(),bootstrap_producer_sha256=hashlib.sha256(Path(base.__file__).read_bytes()).hexdigest(),producer_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),engine_sha256=hashlib.sha256(engine.read_bytes()).hexdigest(),sources={**timeline['reference_dependencies'],**source.digests()},owners=14,shapes=17,parents=layers,adapter_sha256=hashlib.sha256((project/'probe.gd').read_bytes()).hexdigest(),geometry_sha256=hashlib.sha256((out/'geometry.json').read_bytes()).hexdigest())
 (out/'geometry-receipt.json').write_text(json.dumps(proof,indent=2)+'\n',encoding='utf8');print('Original geometry:14 owners/17 native rectangle shapes and constructor defaults; real parent TileMap sort; no terrain/contact/admission')
 return proof

if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',type=Path,required=True);p.add_argument('--godot',type=Path,required=True);a=p.parse_args();run(a.out,a.godot)
