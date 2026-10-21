"""Run pinned original Coin/Sparkles with ordinary clocks, real circles and cash.

Party identity, HUD display and audio requests are explicit providers. Obstacles
and reposition commands are test inputs; this is not a full-room playthrough.
"""
from pathlib import Path
import argparse,hashlib,json,re,subprocess,sys,time,textwrap
r=Path(__file__).resolve().parents[1];sys.path.insert(0,str(r))
from tools import podunk_field as f
from tools.reference_movement import extract
from tools.run_dungeon_key_contact_reference import player_source
from tools.run_scene_reference import invoke
from tools import coins
from tools.coin_reference_protocol import validate
def source():
    name='observation';p,s=coins.read();_,source_model,source_proof=coins.source(directions=False);audit=dict(sources=p['sources']);copied={}
    for path,digest in audit['sources'].items():
     raw=s.data(path);assert hashlib.sha256(raw).hexdigest()==digest
     if path in ['project.godot','Scripts/global/uiManager.gd']or path.endswith('.import'):continue
     copied[path]=raw
    player,player_proof=player_source();cases=[dict(name='free',party=False,other=False,area=False,peek=False),dict(name='ready_peek',party=False,other=False,area=False,peek=True),dict(name='party_at_spawn',party=True,other=False,area=False,peek=False),dict(name='other_then_area',party=False,other=True,area=True,peek=False),dict(name='party_enter_after_land',party=True,other=False,area=False,peek=False,relocate=True,position=[10000,10000]),dict(name='party_at_landing_spot',party=True,other=False,area=False,peek=False,position=[-2,-12])]
    for case_name,obstacles in [('wall_left',[dict(kind='rectangle',center=[-5,-12],extents=[1,100])]),('wall_up',[dict(kind='rectangle',center=[0,-8],extents=[100,1])]),('slope',[dict(kind='convex',points=[[-12,-10],[0,-10],[-12,4]])]),('segment',[dict(kind='segment',points=[[-100,-8],[100,-8]])]),('two_walls',[dict(kind='rectangle',center=[-5,-12],extents=[1,100]),dict(kind='rectangle',center=[0,-8],extents=[100,1])])]:cases.append(dict(name=case_name,party=True,other=False,area=False,peek=False,relocate=True,position=[10000,10000],obstacles=obstacles))
    cases.append(dict(name='free_seed42',party=False,other=False,area=False,peek=False,seed=42))
    cases.append(dict(name='tight_corner',party=True,other=False,area=False,peek=False,relocate=True,position=[10000,10000],obstacles=[dict(kind='rectangle',center=[-4.5,-12],extents=[1,100]),dict(kind='rectangle',center=[0,-8],extents=[100,1])]))
    angle_count=re.findall(r'randi\(\) % (\d+)',s.text('Scripts/Main/Coin.gd'));assert len(angle_count)==1;config=dict(cases=cases,frames=120,player=player,seed=1,direction_count=int(angle_count[0]))
    config.update(factory_parent=source_model['factory_parent'],spawner_cases=[dict(name='three_then_two',amount=3,use_default=False,restart_frame=6,restart_amount=2,position=[3,7],seed=1,frames=10),dict(name='default_five',amount=0,use_default=True,restart_frame=0,restart_amount=0,position=[-5,9],seed=1,frames=10),dict(name='replace_active',amount=5,use_default=False,restart_frame=2,restart_amount=1,position=[3,7],seed=1,frames=10)])
    probe=r'''extends Node2D
    var rows=[]
    var contacts=[]
    var spawners=[]
    var factory_ready=[]
    var factory_drops=[]
    var factory_ordinals={}
    var factory_idle=0
    var factory_physics=0
    var contact_base=0
    var config
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
    func shape(body):
     var node=CollisionPolygon2D.new()
     node.name="SourceFoot"
     node.position=Vector2(config.player.position[0],config.player.position[1])
     var points=PoolVector2Array()
     for point in config.player.points:points.append(Vector2(point[0],point[1]))
     node.polygon=points
     body.add_child(node)
     body.collision_layer=config.player.layer
     body.collision_mask=config.player.mask
    func coin_state(coin):
     var animation=coin._animation_player
     return {"position":vector(coin.position),"speed":real64(coin._speed),"direction":vector(coin._direction),"air_time":real64(coin._air_time),"start_air_time":real64(coin._start_air_time),"grounded":coin._grounded,"physics":coin.is_physics_processing(),"body_disabled":coin.get_node("CollisionShape2D").disabled,"pickup_disabled":coin.collisions.disabled,"offset":vector(coin._sprite.offset),"sprite_frame":coin._sprite.frame,"sparkle_frame":coin.get_node("Sprite/Sparkles").frame,"glow":real64(coin._sprite.material.get_shader_param("glow_modifier")),"animation":animation.assigned_animation,"playing":animation.is_playing(),"time":bits(animation.current_animation_position),"queued":coin.is_queued_for_deletion()}
    func capture(ci,frame,coin,drop):
     var row={"case":ci,"frame":frame,"alive":is_instance_valid(coin),"cash":globaldata.cash,"cash_opens":uiManager._cash.opens,"collect_audio":audioManager.calls.duplicate(true),"drop_audio":drop.calls.duplicate(true),"physics_frame":Engine.get_physics_frames(),"idle_frame":Engine.get_idle_frames()}
     if row.alive:
      row.state=coin_state(coin)
     rows.append(row)
    func on_spawn_enter(coin):
     if coin.get_script()!=preload("res://Scripts/Main/Coin.gd"):
      get_tree().quit(10)
      return
     coin.connect("ready",self,"on_spawn_ready",[coin])
    func on_spawn_ready(coin):
     if !(coin._drop_sfx is AudioStreamPlayer2D):
      get_tree().quit(11)
      return
     var drop=load("res://drop.gd").new()
     drop.base=factory_idle
     coin._drop_sfx=drop
     factory_drops.append(drop)
     factory_ordinals[coin]=factory_ready.size()
     factory_ready.append({"ordinal":factory_ready.size(),"physics":Engine.get_physics_frames()-factory_physics,"idle":Engine.get_idle_frames()-factory_idle,"parent":coin.get_parent().name,"position":vector(coin.position),"speed":real64(coin._speed),"direction":vector(coin._direction),"air_time":real64(coin._air_time),"sparkle":coin.get_node("Sprite/Sparkles").frame})
    func capture_spawner(ci,frame,source):
     var live=[]
     for coin in global.currentScene.get_node(config.factory_parent).get_children():live.append({"ordinal":factory_ordinals[coin],"state":coin_state(coin)})
     spawners.append({"case":ci,"frame":frame,"timer":real64(source._timer),"spawning":source._spawning,"remaining":source._coins_spawned,"coins":factory_ready.duplicate(true),"cash":globaldata.cash,"cash_opens":uiManager._cash.opens,"collect_calls":audioManager.calls.size(),"live":live})
    func run_spawner(ci,definition):
     var scene=Node2D.new()
     scene.name="FactoryProviderWorld"
     var objects=Node2D.new()
     objects.name=config.factory_parent
     objects.connect("child_entered_tree",self,"on_spawn_enter")
     scene.add_child(objects)
     add_child(scene)
     global.currentScene=scene
     var source=load("res://Nodes/Overworld/Objects/CoinSpawner.tscn").instance()
     source.position=Vector2(definition.position[0],definition.position[1])
     scene.add_child(source)
     factory_ready=[]
     factory_drops=[]
     factory_ordinals={}
     factory_idle=Engine.get_idle_frames()
     factory_physics=Engine.get_physics_frames()
     globaldata.cash=29
     uiManager._cash.opens=0
     audioManager.calls=[]
     seed(int(definition.seed))
     if definition.use_default:source.spawn_coins()
     else:source.spawn_coins(int(definition.amount))
     capture_spawner(ci,0,source)
     for frame in range(1,int(definition.frames)+1):
      yield(get_tree(),"idle_frame")
      if frame==definition.restart_frame:source.spawn_coins(int(definition.restart_amount))
      capture_spawner(ci,frame,source)
     spawners[-1].rng_tail=randi()
     var settled=false
     for _tick in range(120):
      settled=true
      for coin in objects.get_children():
       if !coin._grounded or coin.is_physics_processing():settled=false
      if settled:break
      yield(get_tree(),"idle_frame")
     if !settled:
      get_tree().quit(12)
      return
     yield(get_tree(),"idle_frame")
     scene.free()
     global.currentScene=null
     for drop in factory_drops:drop.free()
     yield(get_tree(),"idle_frame")
    func on_body(body,ci):
     contacts.append({"case":ci,"frame":Engine.get_idle_frames()-contact_base,"kind":"body","identity":1 if body is PartyObject else 2})
    func on_area(area,ci):
     contacts.append({"case":ci,"frame":Engine.get_idle_frames()-contact_base,"kind":"area","identity":3,"layer":area.collision_layer})
    func run():
     var file=File.new()
     if file.open("res://input.json",File.READ)!=OK:
      get_tree().quit(2)
      return
     config=JSON.parse(file.get_as_text()).result
     file.close()
     var peeks=[]
     for ci in config.cases.size():
      var test=config.cases[ci]
      var scene=Node2D.new()
      scene.name="CoinProviderWorld"
      var objects=Node2D.new()
      objects.name="Objects"
      scene.add_child(objects)
      add_child(scene)
      global.currentScene=scene
      globaldata.cash=11
      uiManager._cash.opens=0
      audioManager.calls=[]
      for obstacle in test.get("obstacles",[]):
       var solid=StaticBody2D.new()
       solid.collision_layer=8192
       solid.collision_mask=0
       var shape_node=CollisionShape2D.new()
       if obstacle.kind=="rectangle":
        var shape=RectangleShape2D.new()
        shape.extents=Vector2(obstacle.extents[0],obstacle.extents[1])
        shape_node.shape=shape
        shape_node.position=Vector2(obstacle.center[0],obstacle.center[1])
       else:
        var points=PoolVector2Array()
        for point in obstacle.points:points.append(Vector2(point[0],point[1]))
        if obstacle.kind=="convex":
         var shape=ConvexPolygonShape2D.new()
         shape.points=points
         shape_node.shape=shape
        elif obstacle.kind=="segment":
         var shape=ConcavePolygonShape2D.new()
         shape.segments=points
         shape_node.shape=shape
        else:
         get_tree().quit(8)
         return
       solid.add_child(shape_node)
       objects.add_child(solid)
      var body=null
      if test.party or test.other:
       if test.party:body=load("res://party.gd").new()
       else:body=KinematicBody2D.new()
       shape(body)
       objects.add_child(body)
       if test.has("position"):body.position=Vector2(test.position[0],test.position[1])
      var emitter=Area2D.new()
      emitter.collision_layer=0
      emitter.collision_mask=0
      emitter.monitoring=false
      var collider=CollisionShape2D.new()
      var rectangle=RectangleShape2D.new()
      rectangle.extents=Vector2(1,1)
      collider.shape=rectangle
      emitter.add_child(collider)
      emitter.position=Vector2(10000,10000)
      objects.add_child(emitter)
      yield(get_tree(),"idle_frame")
      seed(int(test.get("seed",config.seed)))
      var coin=load("res://Nodes/Reusables/Coin.tscn").instance()
      objects.add_child(coin)
      if !(coin._sprite is Sprite) or !(coin._animation_player is AnimationPlayer) or !(coin._drop_sfx is AudioStreamPlayer2D):
       get_tree().quit(3)
       return
      var drop=load("res://drop.gd").new()
      coin._drop_sfx=drop
      if coin._drop_sfx!=drop:
       get_tree().quit(4)
       return
      drop.base=Engine.get_idle_frames()
      audioManager.base=drop.base
      contact_base=Engine.get_idle_frames()
      coin.get_node("Area2D").connect("body_entered",self,"on_body",[ci])
      coin.get_node("Area2D").connect("area_entered",self,"on_area",[ci])
      var base_physics=Engine.get_physics_frames()
      var base_idle=Engine.get_idle_frames()
      capture(ci,0,coin,drop)
      var peek=null
      if test.peek:peek=randi()
      peeks.append(peek)
      var duplicate_called=false
      for frame in range(1,int(config.frames)+1):
       yield(get_tree(),"idle_frame")
       if frame==60 and test.area and is_instance_valid(coin):
        emitter.position=coin.position
        emitter.collision_layer=2
       if frame==60 and test.get("relocate",false) and is_instance_valid(coin):body.position=coin.position
       if is_instance_valid(coin) and animation_collect(coin) and !duplicate_called:
        duplicate_called=true
        coin._collect()
       capture(ci,frame,coin,drop)
      rows[-1].rng_tail=randi()
      rows[-1].peek_after_ready=peeks[-1]
      rows[-1].duplicate_collect_called=duplicate_called
      for row in rows:
       if row.case==ci:
        row.physics_frame-=base_physics
        row.idle_frame-=base_idle
      scene.free()
      global.currentScene=null
      drop.free()
      yield(get_tree(),"idle_frame")
     for ci in config.spawner_cases.size():
      yield(run_spawner(ci,config.spawner_cases[ci]),"completed")
     if file.open("res://../OUTPUT_REFERENCE",File.WRITE)!=OK:
      get_tree().quit(5)
      return
     var directions=[]
     for angle in range(int(config.direction_count)):directions.append(vector(Vector2.RIGHT.rotated(angle)))
     file.store_string(JSON.print({"schema":4,"godot":Engine.get_version_info(),"directions":directions,"rows":rows,"contacts":contacts,"spawners":spawners}))
     file.close()
     get_tree().quit()
    func animation_collect(coin):return coin._animation_player.current_animation=="Collect"
    '''.replace('OUTPUT_REFERENCE',name+'/reference.json')
    probe="\n".join(line[4:]if i and line.startswith("    ")else line for i,line in enumerate(probe.split("\n")))
    raw=s.data('Scripts/global/uiManager.gd');cash,methods=extract(raw,hashlib.sha256(raw).hexdigest(),['add_cash','get_cash_box'])
    classes=[dict(base='KinematicBody2D',**{'class':'PartyObject'},language='GDScript',path='res://party.gd'),dict(base='Node2D',**{'class':'AbstractSpawner'},language='GDScript',path='res://Scripts/Main/Abstract Spawner.gd'),dict(base='AbstractSpawner',**{'class':'CoinSpawner'},language='GDScript',path='res://Scripts/Main/Coin Spawner.gd')]
    files={'project.godot':'config_version=4\n_global_script_classes='+json.dumps(classes)+'\n_global_script_class_icons={"PartyObject":"","AbstractSpawner":"","CoinSpawner":""}\n[application]\nconfig/name="Coin source providers"\nrun/main_scene="res://main.tscn"\n[autoload]\nglobal="*res://global.gd"\nglobaldata="*res://flags.gd"\nuiManager="*res://ui.gd"\naudioManager="*res://audio.gd"\n[logging]\nfile_logging/enable_logging=false\n[rendering]\nquality/driver/driver_name="GLES2"\n','main.tscn':'[gd_scene load_steps=2 format=2]\n[ext_resource path="res://probe.gd" type="Script" id=1]\n[node name="Reference" type="Node2D"]\nscript=ExtResource(1)\n','probe.gd':probe,'global.gd':'extends Node\nvar currentScene\n','flags.gd':'extends Node\nvar cash=0\n','party.gd':'class_name PartyObject\nextends KinematicBody2D\n','ui.gd':'extends Node\nvar _cash=preload("res://cash.gd").new()\nvar _phone_units=preload("res://cash.gd").new()\n'+cash+'\nfunc _exit_tree():\n _cash.free()\n _phone_units.free()\n','cash.gd':'extends CanvasLayer\nvar opens=0\nfunc open_and_close():opens+=1\n','audio.gd':'extends Node\nvar calls=[]\nvar base=0\nfunc play_sfx(stream,name):calls.append([Engine.get_idle_frames()-base,stream.resource_path,name])\n','drop.gd':'extends Reference\nvar calls=[]\nvar base=0\nfunc play():calls.append(Engine.get_idle_frames()-base)\n','input.json':json.dumps(config)}
    for path,text in list(files.items()):
     if path.endswith('.gd'):files[path]='\n'.join('\t'*(len(line)-len(line.lstrip(' ')))+line.lstrip(' ')for line in text.split('\n'))
    files['drop.gd']=files['drop.gd'].replace('extends Reference','extends Node')
    proof=dict(copied_sha256={k:hashlib.sha256(v).hexdigest()for k,v in copied.items()},generated_sha256={k:hashlib.sha256(v.encode()).hexdigest()for k,v in files.items()},methods=methods,player=player_proof,source=source_proof,scope='Actual Coin/Sparkles/prefabs/ordinary clocks/real Kinematic circle and source player footprint. UIcash selected original methods,party type and audio request holders explicit. After actual original Ready validates AudioStreamPlayer2D binding,only dynamic _drop_sfx field replaced with play-recorder;not production media/wholeRoom/GPU/hardware. Sprite/audio import params excluded from gameplay/pose scope.')
    proof['protocol_sha256']=hashlib.sha256((r/'tools/coin_reference_protocol.py').read_bytes()).hexdigest()
    return files,copied,proof

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--work',type=Path,required=True);ap.add_argument('--godot',type=Path,required=True);a=ap.parse_args();root=a.work.resolve();engine=a.godot.resolve();assert root.is_relative_to(r/'build')and not root.exists()and hashlib.sha256(engine.read_bytes()).hexdigest()=='e748acc92ee5205dd3cc1754d4422870f6a68ae6e00a16ff88bd3438c957adfd';root.mkdir(parents=True);project=root/'project';project.mkdir();(project/'.owned-coin-original').write_bytes(b'coin-original-v1');out=root/'observation';out.mkdir();files,copied,proof=source();config=json.loads(files['input.json'])
    for path,raw in copied.items():
     target=project/path;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(raw)
    def write():
     for path,text in files.items():target=project/path;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(text.encode())
    write();observer=out/'observer';observer.mkdir()
    for path,text in files.items():target=observer/path;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(text.encode())
    (out/'inputs.json').write_text(json.dumps(proof,indent=2)+'\n');start=time.monotonic();invoke(engine,project,['--video-driver','GLES2','--debug','--editor','--quit'],out/'editor.log');write();invoke(engine,project,['--video-driver','GLES2','--debug','--fixed-fps','60'],out/'godot.log');assert all((project/p).read_bytes()==raw for p,raw in copied.items())and all((project/p).read_bytes()==text.encode()for p,text in files.items());result=(out/'reference.json').read_bytes();validate(json.loads(result,object_pairs_hook=coins.unique),config);proof.update(engine_binary_sha256=hashlib.sha256(engine.read_bytes()).hexdigest(),return_code=0,result_sha256=hashlib.sha256(result).hexdigest(),elapsed_seconds=time.monotonic()-start,producer_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest());(out/'receipt.json').write_text(json.dumps(proof,indent=2)+'\n');print('Actual source coins',len(json.loads(result)['rows']),proof['result_sha256'])

if __name__=="__main__":main()
