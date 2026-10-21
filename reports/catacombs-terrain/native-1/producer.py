"""Private diagnostic: native TileSet metadata and unmodified serialized layers.

No game/root script, AI, navigation solver, scene admission or resource export.
"""
import hashlib,json,re,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT))
from tools.extract_battle_entry import Extractor,require
from tools.godot_text import Document
from tools.run_scene_reference import invoke
PROBE='''extends Node
func vec(v):return [v.x,v.y]
func rect(v):return [v.position.x,v.position.y,v.size.x,v.size.y]
func xf(v):return [v.x.x,v.x.y,v.y.x,v.y.y,v.origin.x,v.origin.y]
func points(p):
    var out=[]
    for v in p:out.append(vec(v))
    return out
func _ready():call_deferred("run")
func run():
    var terrain=load("res://terrain-source.tscn").instance()
    add_child(terrain)
    for _i in range(4):yield(get_tree(),"physics_frame")
    var ts=load("res://Tilesets/Catacombs.tres")
    var script=ts.get_script()
    var script_info={"present":script!=null}
    if script:
        script_info={"present":true,"class":script.get_class(),"source_code":script.source_code,"has_source":script.has_source_code(),"can_instance":script.can_instance(),"base":script.get_instance_base_type(),"methods":script.get_script_method_list(),"properties":script.get_script_property_list(),"signals":script.get_script_signal_list()}
    var ids=ts.get_tiles_ids()
    ids.sort()
    var tiles=[]
    for id in ids:
        var navigation=ts.tile_get_navigation_polygon(id)
        var nav=null
        if navigation:
            var polys=[]
            for j in navigation.get_polygon_count():polys.append(Array(navigation.get_polygon(j)))
            nav={"vertices":points(navigation.get_vertices()),"polygons":polys,"offset":vec(ts.tile_get_navigation_polygon_offset(id))}
        var shapes=[]
        for row in ts.tile_get_shapes(id):
            var shape=row.shape
            var geometry={"class":shape.get_class()}
            if shape is RectangleShape2D:geometry["extents"]=vec(shape.extents)
            elif shape is ConvexPolygonShape2D:geometry["points"]=points(shape.points)
            elif shape is ConcavePolygonShape2D:geometry["segments"]=points(shape.segments)
            else:
                push_error("Unreviewed native tile shape "+shape.get_class())
                get_tree().quit(4)
                return
            shapes.append({"autotile_coord":vec(row.autotile_coord),"one_way":row.one_way,"margin":row.one_way_margin,"transform":xf(row.shape_transform),"shape":geometry})
        var texture=ts.tile_get_texture(id)
        var info={"path":texture.resource_path,"class":texture.get_class(),"size":[texture.get_width(),texture.get_height()]}
        if texture is AnimatedTexture:
            var frames=[]
            for j in texture.frames:frames.append({"path":texture.get_frame_texture(j).resource_path,"delay":texture.get_frame_delay(j)})
            info["animation"]={"fps":texture.fps,"pause":texture.pause,"oneshot":texture.oneshot,"frames":frames}
        tiles.append({"id":id,"name":ts.tile_get_name(id),"region":rect(ts.tile_get_region(id)),"offset":vec(ts.tile_get_texture_offset(id)),"z":ts.tile_get_z_index(id),"mode":ts.tile_get_tile_mode(id),"shapes":shapes,"navigation":nav,"texture":info})
    var layers=[]
    for map in terrain.get_children():
        var cells=[]
        for p in map.get_used_cells():
            var id=map.get_cell(int(p.x),int(p.y))
            cells.append([int(p.x),int(p.y),id,map.is_cell_x_flipped(int(p.x),int(p.y)),map.is_cell_y_flipped(int(p.x),int(p.y)),map.is_cell_transposed(int(p.x),int(p.y)),int(map.get_cell_autotile_coord(int(p.x),int(p.y)).x),int(map.get_cell_autotile_coord(int(p.x),int(p.y)).y),ids.has(id)])
        cells.sort_custom(self,"cell_before")
        layers.append({"name":map.name,"used_rect":rect(map.get_used_rect()),"position":vec(map.position),"cell_size":vec(map.cell_size),"origin":map.cell_tile_origin,"ysort":map.cell_y_sort,"layer":map.collision_layer,"mask":map.collision_mask,"cells":cells})
    var f=File.new()
    if f.open("res://../terrain.json",File.WRITE)!=OK:
        get_tree().quit(2)
        return
    f.store_string(JSON.print({"engine":Engine.get_version_info(),"script":script_info,"tiles":tiles,"layers":layers},"  "))
    f.close()
    print("TERRAIN_DIAGNOSTIC_NATIVE_COMPLETE layers=",layers.size()," tiles=",tiles.size())
    get_tree().quit(0)
func cell_before(a,b):return a[1]<b[1] or (a[1]==b[1] and a[0]<b[0])
'''
def run(out):
 out=Path(out).resolve();require(out.is_relative_to(ROOT/'build')and not out.exists(),'Fresh diagnostic project required');project=out/'project';project.mkdir(parents=True);(project/'.owned-terrain-diagnostic').write_text('source-terrain-diagnostic-v1',encoding='ascii');(out/'producer.py').write_bytes(Path(__file__).read_bytes())
 ex=Extractor(ROOT);engine=ROOT/'build/godot-reference/Godot_v3.6.2-stable_win64.exe';require(hashlib.sha256(engine.read_bytes()).hexdigest()=='e748acc92ee5205dd3cc1754d4422870f6a68ae6e00a16ff88bd3438c957adfd','Pinned native engine changed')
 todo=['Tilesets/Catacombs.tres'];copied={}
 while todo:
  name=todo.pop()
  if name in copied:continue
  raw=ex.data(name);copied[name]={'sha256':hashlib.sha256(raw).hexdigest(),'bytes':len(raw)}
  if name.endswith('.tres'):todo.extend(p for p,t in Document(name,raw.decode('utf-8')).ext.values())
  elif name.endswith('.png'):
   dest=project/name;dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(raw)
  else:raise ValueError('Unknown terrain diagnostic dependency '+name)
 (project/'project.godot').write_text('config_version=4\n[application]\nconfig/name="Original terrain image import"\n',encoding='utf-8');(out/'import-project.godot').write_bytes((project/'project.godot').read_bytes())
 invoke(engine,project,['--video-driver','GLES2','--debug','--editor','--quit'],out/'import.log')
 for name in copied:
  if name.endswith('.tres'):
   dest=project/name;dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(ex.data(name))
 source='Maps/podunk/Catacombs.tscn';raw=ex.data(source);copied[source]={'sha256':hashlib.sha256(raw).hexdigest(),'bytes':len(raw)};text=raw.decode('utf-8');doc=Document(source,text);chosen=[n for n in doc.nodes if n.attrs.get('type')=='TileMap'];require(len(chosen)==5 and all(n.attrs['parent']=='.'and n.props['tile_set'].id==1 for n in chosen),'Unreviewed terrain node scope')
 blocks=[]
 for header,body in re.findall(r'^\[node ([^\n]+)\]\r?\n(.*?)(?=^\[|\Z)',text,re.M|re.S):
  if 'type="TileMap"'in header:blocks.append('[node '+header+']\n'+body)
 require(len(blocks)==len(chosen),'Original terrain block extraction mismatch')
 scene='[gd_scene load_steps=2 format=2]\n[ext_resource path="res://Tilesets/Catacombs.tres" type="TileSet" id=1]\n[node name="Terrain" type="Node2D"]\n\n'+'\n'.join(blocks)
 (project/'terrain-source.tscn').write_text(scene,encoding='utf-8');(project/'probe.gd').write_text(PROBE,encoding='utf-8');(project/'probe.tscn').write_text('[gd_scene load_steps=2 format=2]\n[ext_resource path="res://probe.gd" type="Script" id=1]\n[node name="Probe" type="Node"]\nscript=ExtResource( 1 )\n',encoding='utf-8');(project/'project.godot').write_text('config_version=4\n[application]\nconfig/name="Original terrain diagnostic"\nrun/main_scene="res://probe.tscn"\n[display]\nwindow/size/width=320\nwindow/size/height=180\n',encoding='utf-8')
 for name in ['probe.gd','probe.tscn','terrain-source.tscn','project.godot']:(out/name).write_bytes((project/name).read_bytes())
 (out/'source.json').write_text(json.dumps({'scope':'Original TileSet/textures and five verbatim serialized TileMap node blocks;isolated observer root,no game/root scripts or actor/AI/navigation execution. No map export or formal admission.','sources':copied,'api_reference':['https://docs.godotengine.org/en/3.6/classes/class_tileset.html','https://docs.godotengine.org/en/3.6/classes/class_script.html'],'engine_sha256':hashlib.sha256(engine.read_bytes()).hexdigest()},indent=2)+'\n',encoding='utf-8')
 invoke(engine,project,['--video-driver','GLES2'],out/'engine.log');print('Actual native diagnostic complete;review raw result before defining a supported terrain adapter.')
if __name__=='__main__':run(sys.argv[1])
