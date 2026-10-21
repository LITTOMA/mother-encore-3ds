"""Run the original Geyser Sprite geometry in an isolated official engine.

This checks source frame rectangles and node transforms, not GPU pixels or a
playable Catacombs scene. No original checkout files are changed.
"""
import argparse,hashlib,json,math,struct,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import run_geyser_reference as manual,catacombs_geyser as timeline
from tools.extract_battle_entry import Extractor,require
from tools.run_scene_reference import invoke

POSITIONS=[[128.,64.],[-9.25,16.75],[0.,-8.]]
PROBE=r'''extends Node
func _ready():call_deferred("run")
func bits(value):
    var bytes=StreamPeerBuffer.new()
    bytes.put_float(value)
    bytes.seek(0)
    return bytes.get_u32()
func vec(value):return [bits(value.x),bits(value.y)]
func rect(value):return vec(value.position)+vec(value.size)
func run():
    var file=File.new()
    assert(file.open("res://input.json",File.READ)==OK)
    var config=JSON.parse(file.get_as_text()).result
    file.close()
    var player=load("res://party.gd").new()
    add_child(player)
    global.player=player
    var actor=load("res://geyser.tscn").instance()
    add_child(actor)
    actor.pause()
    var sprite=actor.get_node("Sprite")
    var defaults={"root":vec(actor.position),"sprite":vec(sprite.position),"offset":vec(sprite.offset),"scale":vec(sprite.scale),"rotation":bits(sprite.rotation),"centered":sprite.centered,"flip_h":sprite.flip_h,"flip_v":sprite.flip_v,"region":sprite.region_enabled,"visible":sprite.visible,"behind":sprite.show_behind_parent,"z":sprite.z_index,"color":[bits(sprite.modulate.r),bits(sprite.modulate.g),bits(sprite.modulate.b),bits(sprite.modulate.a)],"hframes":sprite.hframes,"vframes":sprite.vframes,"size":vec(sprite.texture.get_size()),"pixel_snap":ProjectSettings.get_setting("rendering/2d/snapping/use_gpu_pixel_snap")}
    var frames=[]
    for frame in sprite.hframes*sprite.vframes:
        sprite.frame=frame
        frames.append({"frame":sprite.frame,"coords":vec(sprite.frame_coords),"rect":rect(sprite.get_rect())})
    var positions=[]
    for point in config.positions:
        actor.position=Vector2(point[0],point[1])
        var local=sprite.get_rect()
        positions.append({"root":vec(actor.position),"global":vec(sprite.global_position),"top_left":vec(sprite.to_global(local.position)),"basis":vec(sprite.global_transform.x)+vec(sprite.global_transform.y)})
    assert(file.open("res://../reference.json",File.WRITE)==OK)
    file.store_string(JSON.print({"schema":1,"godot":Engine.get_version_info(),"defaults":defaults,"frames":frames,"positions":positions},"  "))
    file.close()
    actor.queue_free()
    get_tree().quit()
'''

def source():
 files,copied,proof=manual.source();_,model,_=timeline.source()
 files['project.godot']+='[rendering]\n2d/snapping/use_gpu_pixel_snap=true\n'
 files['probe.gd']=PROBE;files['input.json']=json.dumps(dict(positions=POSITIONS))
 proof=dict(source=proof['source'],generated_sha256={k:hashlib.sha256(v.encode()).hexdigest()for k,v in files.items()},copied_source=proof['copied_source'],adapters=proof['adapters'][:3]+['Actual original Sprite/get_rect/frame_coords/global transform;source pause freezes animation while observer enumerates32 frames and three explicit factory positions','Actual checked original rendering pixel-snap setting;geometry observations only,no GPU pixel/audible/physics/full scene claim'])
 require(model['frames']<=256,'Bounded Geyser Sprite frame count');return files,copied,proof

def validate_result(doc,proof):
 require(type(doc)is dict and set(doc)=={'schema','godot','defaults','frames','positions'}and type(doc['schema'])is int and doc['schema']==1 and type(doc['godot'])is dict and doc['godot'].get('string')=='3.6.2-stable (official)','Geyser graphics schema/engine');_,model,_=timeline.source();_,_,current=source();require(proof['source']==current['source']and proof['generated_sha256']==current['generated_sha256'],'Geyser graphics source/observer changed')
 bits=lambda v:struct.unpack('<I',struct.pack('<f',v))[0]
 def words(value,count):return type(value)is list and len(value)==count and all(type(v)is int and 0<=v<=0xffffffff and math.isfinite(struct.unpack('<f',struct.pack('<I',v))[0])for v in value)
 defaults=dict(root=list(map(bits,model['root'])),sprite=list(map(bits,model['sprite'])),offset=list(map(bits,model['offset'])),scale=[bits(1),bits(1)],rotation=bits(0),centered=model['centered'],flip_h=False,flip_v=False,region=False,visible=True,behind=False,z=0,color=[bits(1)]*4,hframes=model['hframes'],vframes=model['vframes'],size=list(map(bits,[model['hframes']*model['size'][0],model['vframes']*model['size'][1]])),pixel_snap=True)
 require(type(doc['defaults'])is dict and doc['defaults']==defaults,'Geyser graphics actual defaults differ')
 for key in ['root','sprite','offset','scale','size','color']:require(words(doc['defaults'][key],4 if key=='color'else 2),'Geyser graphics typed default words')
 require(type(doc['defaults']['rotation'])is int and all(type(doc['defaults'][key])is int for key in ['z','hframes','vframes'])and all(type(doc['defaults'][key])is bool for key in ['centered','flip_h','flip_v','region','visible','behind','pixel_snap']),'Geyser graphics typed defaults');require(type(doc['frames'])is list and len(doc['frames'])==model['frames']and type(doc['positions'])is list and len(doc['positions'])==len(POSITIONS),'Geyser graphics observation coverage')
 offset=[math.floor(model['offset'][axis]-(model['size'][axis]/2 if model['centered']else 0))for axis in range(2)]
 for index,row in enumerate(doc['frames']):require(type(row)is dict and set(row)=={'frame','coords','rect'}and type(row['frame'])is int and row['frame']==index and words(row['coords'],2)and words(row['rect'],4)and row['coords']==list(map(bits,[index%model['hframes'],index//model['hframes']]))and row['rect']==list(map(bits,offset+model['size'])),'Geyser graphics source frame rectangle')
 for point,row in zip(POSITIONS,doc['positions']):
  require(type(row)is dict and set(row)=={'root','global','top_left','basis'}and all(words(row[k],4 if k=='basis'else 2)for k in row),'Geyser graphics typed transform words')
  # Actual Node2D identity has a negative-zero off-diagonal. Preserve the raw
  # observation;IEEE +0/-0 are the same transform,not an upstream change.
  b=row['basis'];require(row['root']==list(map(bits,point))and row['global']==[bits(point[a]+model['sprite'][a])for a in range(2)]and row['top_left']==[bits(point[a]+model['sprite'][a]+offset[a])for a in range(2)]and b[0]==bits(1)and b[3]==bits(1)and b[1]in[0,0x80000000]and b[2]in[0,0x80000000],'Geyser graphics source global transform')
 return doc

def main():
 a=argparse.ArgumentParser(description=__doc__);a.add_argument('--out',type=Path,required=True);o=a.parse_args();out=o.out.resolve();engine=ROOT/'build/godot-reference/Godot_v3.6.2-stable_win64.exe';require(out.is_relative_to(ROOT/'build')and out!=ROOT/'build'and not out.exists(),'Fresh Geyser graphics output under build required');require(hashlib.sha256(engine.read_bytes()).hexdigest()=='e748acc92ee5205dd3cc1754d4422870f6a68ae6e00a16ff88bd3438c957adfd','Official engine bytes changed');files,copied,proof=source();out.mkdir(parents=True);project=out/'temporary';project.mkdir();(project/'.owned-geyser-graphics').write_bytes(b'geyser-graphics-reference-v1');ex=Extractor(ROOT)
 for name in copied:
  if not name.endswith('.import'):target=project/name;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(ex.data(name))
 def write():
  for name,value in files.items():target=project/name;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(value.encode())
 write();args=['--video-driver','GLES2','--debug'];invoke(engine,project,args+['--editor','--quit'],out/'fresh-import.log')
 for name in copied:
  if name.endswith('.import'):(project/name).write_bytes(ex.data(name))
 checks=list((project/'.import').glob('*.md5'));require(len(checks)==4,'Geyser graphics fresh import coverage')
 for path in checks:require(path.resolve().parent==(project/'.import').resolve()and not path.is_symlink(),'Geyser graphics import path escaped');path.unlink()
 invoke(engine,project,args+['--editor','--quit'],out/'source-import.log');(out/'editor-project.godot').write_bytes((project/'project.godot').read_bytes());write();invoke(engine,project,args+['--fixed-fps','60'],out/'godot.log')
 require(all(hashlib.sha256((project/n).read_bytes()).hexdigest()==h for n,h in proof['generated_sha256'].items()),'Geyser graphics executed input changed');require(all(hashlib.sha256((project/n).read_bytes()).hexdigest()==h for n,h in proof['copied_source'].items()),'Geyser graphics actual source changed');doc=validate_result(json.loads((out/'reference.json').read_bytes()),proof);proof.update(producer_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),engine_sha256=hashlib.sha256(engine.read_bytes()).hexdigest(),result_sha256=hashlib.sha256((out/'reference.json').read_bytes()).hexdigest());(out/'receipt.json').write_text(json.dumps(proof,indent=2)+'\n');print('Original Geyser Sprite:',len(doc['frames']),'frames,',len(doc['positions']),'positions;not GPU/full scene')
if __name__=='__main__':main()
