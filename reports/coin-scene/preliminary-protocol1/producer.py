"""Observe all source barrel/coin factories with their real scripts/prefabs.

Objects/Bottom2 identity parents, voice/HUD and input delivery are explicit
providers. No source TileMap terrain, other NPCs or whole room is admitted.
"""
import argparse,hashlib,json,sys,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import coin_scene as c,run_coin_reference as coin,run_breakable_reference as barrel
from tools.godot_text import instantiate,Vec2,NodePathValue
from tools.run_scene_reference import invoke

PROBE='''extends Node
var config
var born=[]
var case_results=[]
var base_idle=0
var base_physics=0
var active_scene
func bits(value):
 var stream=StreamPeerBuffer.new()
 stream.put_float(value)
 stream.seek(0)
 return stream.get_u32()
func real64(value):
 var stream=StreamPeerBuffer.new()
 stream.put_double(value)
 return stream.data_array.hex_encode()
func vector(value):return [bits(value.x),bits(value.y)]
func _ready():call_deferred("run")
func on_node(node):
 if node.get_script()==preload("res://Scripts/Main/Coin.gd"):node.connect("ready",self,"coin_ready",[node])
func coin_ready(node):
 var identity=0
 for binding in config.bindings:
  if node.global_position==Vector2(binding.position[0],binding.position[1]):
   if identity!=0:
    get_tree().quit(11)
    return
   identity=binding.id
 if identity==0 or not node._drop_sfx is AudioStreamPlayer2D:
  get_tree().quit(12)
  return
 var drop=preload("res://drop.gd").new()
 drop.base=base_idle
 node.add_child(drop)
 node._drop_sfx=drop
 born.append({"ordinal":born.size(),"factory":identity,"physics":Engine.get_physics_frames()-base_physics,"idle":Engine.get_idle_frames()-base_idle,"position":vector(node.position),"speed":real64(node._speed),"direction":vector(node._direction),"air_time":real64(node._air_time),"sparkle":node.get_node("Sprite/Sparkles").frame})
func snapshot(frame,objects):
 var factories=[]
 for i in objects.size():
  var object=objects[i]
  var factory=object.coinSpawner
  factories.append({"id":config.bindings[i].id,"timer":real64(factory._timer),"remaining":factory._coins_spawned,"spawning":factory._spawning,"position":vector(factory.global_position),"parent":str(active_scene.get_path_to(object.get_parent())),"flag":object._get_flag_status(),"body_disabled":object.get_node("StaticBody2D/CollisionShape2D").disabled})
 return {"frame":frame,"physics":Engine.get_physics_frames()-base_physics,"idle":Engine.get_idle_frames()-base_idle,"factories":factories,"born":born.size()}
func run():
 var file=File.new()
 if file.open("res://input.json",File.READ)!=OK:
  get_tree().quit(2)
  return
 config=JSON.parse(file.get_as_text()).result
 file.close()
 for mode in ["forward","reverse","flagged"]:
  var scene=preload("res://scene.gd").new()
  scene.name=config.name
  active_scene=scene
  global.currentScene=scene
  global.rumble=[]
  global.paused=false
  globaldata.flags={}
  globaldata.object_flags={}
  globaldata.cash=43
  audioManager.calls=[]
  uiManager._cash.opens=0
  for parent_name in config.parents:
   var parent=Node2D.new()
   parent.name=parent_name
   scene.add_child(parent)
  if mode=="flagged":globaldata.object_flags[config.owners[0].flag]=true
  var objects=[]
  for owner_config in config.owners:
   var object=load("res://"+config.barrel).instance()
   for key in owner_config.properties:
    var value=owner_config.properties[key]
    if typeof(value)==TYPE_DICTIONARY:
     if value.kind=="vector":value=Vector2(value.value[0],value.value[1])
     elif value.kind=="path":value=NodePath(value.value)
     else:
      get_tree().quit(13)
      return
    object.set(key,value)
   object.name=owner_config.name
   var original=object.get_node("AudioStreamPlayer")
   if not original is AudioStreamPlayer2D:
    get_tree().quit(14)
    return
   object.remove_child(original)
   original.free()
   var voice=preload("res://voice.gd").new()
   voice.name="AudioStreamPlayer"
   object.add_child(voice)
   scene.get_node(owner_config.parent).add_child(object)
   objects.append(object)
  get_tree().connect("node_added",self,"on_node")
  add_child(scene)
  yield(get_tree(),"idle_frame")
  seed(int(config.seed))
  base_idle=Engine.get_idle_frames()
  base_physics=Engine.get_physics_frames()
  global.base=base_idle
  born=[]
  var rows=[]
  rows.append(snapshot(-1,objects))
  var hit_order=objects.duplicate()
  if mode=="reverse":hit_order.invert()
  var emitter=Node2D.new()
  scene.add_child(emitter)
  for object in hit_order:
   object.get_node("AudioStreamPlayer").base=base_idle
   emitter.global_position=object.global_position
   object._on_Hurtbox_area_entered(emitter)
  rows.append(snapshot(0,objects))
  for frame in range(1,41):
   yield(get_tree(),"idle_frame")
   rows.append(snapshot(frame,objects))
  case_results.append({"mode":mode,"rows":rows,"births":born.duplicate(true),"rng_tail":randi(),"cash":globaldata.cash,"cash_opens":uiManager._cash.opens,"collect_audio":audioManager.calls.duplicate(true)})
  get_tree().disconnect("node_added",self,"on_node")
  scene.free()
  global.currentScene=null
  active_scene=null
  yield(get_tree(),"idle_frame")
 if file.open("res://../reference.json",File.WRITE)!=OK:
  get_tree().quit(3)
  return
 file.store_string(JSON.print({"schema":1,"godot":Engine.get_version_info(),"cases":case_results}))
 file.close()
 get_tree().quit()
'''

def source():
 recipe,model,proof=c.source();cp,cm,coin_proof=c.coins.source();bp,bm,break_proof=c.b.source();files,copied,coin_inputs=coin.source();bfiles,bcopied,break_inputs=barrel.source()
 for name,raw in bcopied.items():
  c.require(name not in copied or copied[name]==raw,'Coin scene overlapping original dependency changed');copied[name]=raw
 files['global.gd']=bfiles['global.gd'];files['scene.gd']=bfiles['scene.gd'];files['voice.gd']=bfiles['voice.gd'];files['flags.gd']=bfiles['flags.gd']+'\nvar cash=0\n'
 project=files['project.godot'];prefix='_global_script_classes=';line=next(v for v in project.splitlines()if v.startswith(prefix));classes=json.loads(line[len(prefix):]);classes.append(dict(base='Sprite',**{'class':'FlaggableObject'},language='GDScript',path='res://'+bp['paths']['flag_script']));files['project.godot']=project.replace(line,prefix+json.dumps(classes));s=c.f.Sources();nodes=instantiate(s.doc,bp['paths']['scene']);by={n.path:n for n in nodes};owners={o['id']:o for o in bm['owners']};definitions=[]
 allowed=set('script position visible contents Drop_item_if_hidden Drop_where New_parent appear_flag disappear_flag interact_content pause_on_break coins_spawned coin_spawner flag is_object_flag emit_flag_updated_signal reset_when_leaving_region reset_when_leaving_area'.split())
 for binding in model['bindings']:
  node=by[binding['owner_path']];c.require(set(node.props)<=allowed and node.scene==bp['paths']['barrel'],'Coin scene source owner overrides');properties={}
  for key,value in node.props.items():
   if key=='script':continue
   if type(value)is Vec2:value=dict(kind='vector',value=c.point(value))
   elif type(value)is NodePathValue:value=dict(kind='path',value=value.path)
   else:c.require(type(value)in[str,int,float,bool],'Coin scene unreviewed property type')
   properties[key]=value
  definitions.append(dict(name=node.name,parent=c.f.parent_path(node.path),flag=owners[binding['owner']]['flag'],properties=properties))
 config=dict(seed=1,name=bm['name'],parents=sorted({model['sink'],*[v['reparent']for v in model['bindings']]}),barrel=bp['paths']['barrel'],bindings=model['bindings'],owners=definitions);c.require(len({tuple(v['position'])for v in model['bindings']})==len(model['bindings']),'Coin scene native factory positions must be unique');files['input.json']=json.dumps(config);files['probe.gd']=PROBE
 # The original extracted methods already use tabs. Normalize generated
 # one-space-per-level observer code before the editor rewrites groups of four.
 for name,text in list(files.items()):
  if name.endswith('.gd'):files[name]='\n'.join('\t'*(len(line)-len(line.lstrip(' ')))+line.lstrip(' ')for line in text.split('\n'))
 proof=dict(source=proof,coin_inputs=coin_inputs,break_inputs=break_inputs,copied_sha256={k:hashlib.sha256(v).hexdigest()for k,v in copied.items()},generated_sha256={k:hashlib.sha256(v.encode()).hexdigest()for k,v in files.items()},scope=__doc__)
 return files,copied,proof

def main():
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--work',type=Path,required=True);parser.add_argument('--godot',type=Path,required=True);a=parser.parse_args();work=a.work.resolve();engine=a.godot.resolve();c.require(work.is_relative_to(ROOT/'build')and not work.exists(),'Fresh coin scene native directory')
 c.require(hashlib.sha256(engine.read_bytes()).hexdigest()=='e748acc92ee5205dd3cc1754d4422870f6a68ae6e00a16ff88bd3438c957adfd','Coin scene actual engine');files,copied,proof=source();work.mkdir();project=work/'project';project.mkdir();(project/'.owned-coin-scene-reference').write_bytes(b'coin-scene-reference-v1')
 for name,raw in copied.items():target=project/name;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(raw)
 def write():
  for name,text in files.items():target=project/name;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(text.encode())
 write();(work/'inputs.json').write_text(json.dumps(proof,indent=2)+'\n');start=time.monotonic();invoke(engine,project,['--video-driver','GLES2','--debug','--editor','--quit'],work/'editor.log');write();invoke(engine,project,['--video-driver','GLES2','--debug','--fixed-fps','60'],work/'godot.log');c.require(all((project/name).read_bytes()==raw for name,raw in copied.items())and all((project/name).read_bytes()==text.encode()for name,text in files.items()),'Coin scene executed input changed');raw=(work/'reference.json').read_bytes();doc=json.loads(raw,object_pairs_hook=c.unique);c.require(type(doc['schema'])is int and doc['schema']==1 and doc['godot']['hash']=='3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8'and len(doc['cases'])==3 and all(len(v['rows'])==42 for v in doc['cases']),'Coin scene actual observation coverage');proof.update(result_sha256=hashlib.sha256(raw).hexdigest(),engine_binary_sha256=hashlib.sha256(engine.read_bytes()).hexdigest(),producer_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),return_code=0,elapsed_seconds=time.monotonic()-start);(work/'receipt.json').write_text(json.dumps(proof,indent=2)+'\n');print('Original barrel/Coin composition',len(doc['cases']),[len(v['births'])for v in doc['cases']],proof['result_sha256'])

if __name__=='__main__':main()
