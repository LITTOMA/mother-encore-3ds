"""Actual original Timer -> PackedScene -> deferred ready -> idle -> deletion.

Explicit player/UI/audio observers; no Character, full scene or physics claim.
"""
import argparse,hashlib,json,math,struct,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import run_geyser_reference as manual,catacombs_geyser as g,catacombs_poison as p
from tools.reference_movement import extract
from tools.extract_battle_entry import Extractor,require
from tools.run_scene_reference import invoke
TICKS=720
OPERATIONS={90:[['player_pause',False]],120:[['player_pause',True]],150:[['player_pause',False]],280:[['tree_pause',False]],320:[['tree_pause',True]],350:[['tree_pause',False]],400:[['ui',1]],430:[['ui',0]]}
PROBE=r'''extends Node
var player
var holes=[]
var actors={}
var events=[]
var rows=[]
var counter=0
var tick=0
var first_gate=false
var tree_gate=false
var finishing=false
var config
func bits(value):
    var bytes=StreamPeerBuffer.new()
    bytes.put_float(value)
    bytes.seek(0)
    return bytes.get_u32()
func register_created(actor):
    counter+=1
    actors[counter]=actor
    events.append(["created",counter])
    return counter
func observed_ready(actor):events.append(["ready",actor.observer_ticket,player.is_paused(),get_tree().paused])
func observed_finished(actor,name):events.append(["finished",actor.observer_ticket,name,actor.is_queued_for_deletion()])
func observed_exit(ticket):
    if !finishing:events.append(["exit",ticket])
    actors.erase(ticket)
func timeout(index):
    actors[counter].set_meta("hole",index)
    events.append(["timeout",index,counter])
    if !first_gate:
        first_gate=true
        player.set_observed_pause(true)
        events.append(["player_pause",true])
    elif tick>=220 and !tree_gate:
        tree_gate=true
        get_tree().paused=true
        events.append(["tree_pause",true])
func _ready():
    pause_mode=Node.PAUSE_MODE_PROCESS
    process_priority=100
    var file=File.new()
    assert(file.open("res://input.json",File.READ)==OK)
    config=JSON.parse(file.get_as_text()).result
    file.close()
    player=load("res://party.gd").new()
    add_child(player)
    global.player=player
    global.currentScene=self
    var objects=Node2D.new()
    objects.name="Objects"
    objects.pause_mode=Node.PAUSE_MODE_STOP
    add_child(objects)
    var factory=load("res://record_factory.gd").new()
    factory.scene=load("res://geyser.tscn")
    factory.observer=self
    for index in config.holes.size():
        var binding=config.holes[index]
        var hole=load("res://observed_hole.gd").new()
        hole.observer_hole=index
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
        holes.append(hole)
func snapshot():
    var result=[]
    var ids=actors.keys()
    ids.sort()
    for id in ids:
        var actor=actors[id]
        assert(is_instance_valid(actor))
        var anim=actor.get_node("AnimationPlayer")
        var audio=actor.get_node("AudioStreamPlayer2D")
        var assigned=anim.assigned_animation
        result.append({"ticket":id,"hole":actor.get_meta("hole"),"position":[bits(actor.position.x),bits(actor.position.y)],"inside":actor.is_inside_tree(),"ready":actor.observer_ready,"time":bits(anim.current_animation_position) if assigned!="" else 0,"assigned":assigned,"playing":anim.is_playing(),"frame":actor.get_node("Sprite").frame,"monitoring":actor.monitoring,"disabled":actor.get_node("CollisionShape2D").disabled,"active":anim.playback_active,"paused":audio.stream_paused,"gain":bits(audio.volume_db),"queued":actor.is_queued_for_deletion(),"commands":audio.commands.duplicate(true),"on_screen":actor.get_node("VisibilityNotifier2D").is_on_screen()})
        audio.commands.clear()
    var clocks=[]
    for hole in holes:clocks.append([bits(hole.timer.time_left),bits(hole.timer.wait_time),hole.timer.paused])
    var value={"actors":result,"clocks":clocks,"events":events.duplicate(true),"global_calls":audioManager.commands.duplicate(true),"player_paused":player.is_paused(),"tree_paused":get_tree().paused,"ui":[uiManager.battle,uiManager.cutscene,uiManager.game_over]}
    events.clear()
    audioManager.commands.clear()
    return value
func _process(delta):
    var row={"tick":tick,"delta":bits(delta),"before":snapshot(),"actions":config.operations.get(str(tick),[])}
    for action in row.actions:
        match action[0]:
            "player_pause":player.set_observed_pause(action[1])
            "tree_pause":get_tree().paused=action[1]
            "ui":
                uiManager.battle=action[1]==1
                uiManager.cutscene=action[1]==2
                uiManager.game_over=action[1]==3
            _:assert(false)
    rows.append(row)
    call_deferred("after",tick)
    tick+=1
    if tick==int(config.ticks):set_process(false)
func after(index):
    rows[index].after=snapshot()
    if tick==int(config.ticks):call_deferred("finish")
func finish():
    finishing=true
    get_tree().paused=false
    var file=File.new()
    assert(file.open("res://../reference.json",File.WRITE)==OK)
    file.store_string(JSON.print({"schema":1,"godot":Engine.get_version_info(),"rows":rows,"spawn_count":counter}))
    file.close()
    get_tree().quit()
'''

def source():
 files,copied,proof=manual.source();recipe,model,poison=p.source();ex=Extractor(ROOT);path=recipe['paths']['hole_script'];hole=extract(ex.data(path),recipe['sources'][path],list(recipe['symbols']['hole_script']))[0]
 files['hole.gd']='extends Sprite\nvar interval\nvar delay\nvar poison\nonready var timer=$Timer\n'+hole
 files['observed_hole.gd']='''extends "res://hole.gd"
var observer_hole=0
func _on_Timer_timeout():
    ._on_Timer_timeout()
    global.currentScene.timeout(observer_hole)
'''
 original=g.source()[0]['paths']['script'];needle='res://'+original;require(files['geyser.tscn'].count(needle)==1,'Factory exact root script binding');files['geyser.tscn']=files['geyser.tscn'].replace(needle,'res://observed_geyser.gd')
 files['observed_geyser.gd']='extends '+json.dumps(needle)+'''
var observer_ticket=0
var observer_ready=false
func _ready():
    observer_ready=true
    global.currentScene.observed_ready(self)
func _on_AnimationPlayer_animation_finished(anim_name):
    ._on_AnimationPlayer_animation_finished(anim_name)
    global.currentScene.observed_finished(self,anim_name)
func _exit_tree():
    if is_instance_valid(global.currentScene):global.currentScene.observed_exit(observer_ticket)
'''
 files['record_factory.gd']='''extends Reference
var scene
var observer
func instance():
    var node=scene.instance()
    node.observer_ticket=observer.register_created(node)
    return node
'''
 files['global.gd']='extends Node\nvar player\nvar currentScene\nfunc get_player():return player\n'
 files['party.gd']=files['party.gd'].replace('signal unpaused\n','signal unpaused\nvar observed_paused=false\n').replace('func set_observed_pause(value):\n','func set_observed_pause(value):\n\tobserved_paused=value\n').replace('func is_paused():return false','func is_paused():return observed_paused')
 files['probe.gd']=PROBE;files['input.json']=json.dumps(dict(holes=model['holes'],operations=OPERATIONS,ticks=TICKS,pause_after_first_timeout=True,pause_tree_after_tick220_timeout=True));files['project.godot']+='\n[physics]\ncommon/physics_fps=60\ncommon/physics_jitter_fix=0.0\n'
 proof=dict(geyser=proof['source'],poison=poison,generated_sha256={k:hashlib.sha256(v.encode()).hexdigest()for k,v in files.items()},copied_source=proof['copied_source'],scope=['Complete original Geyser script/scene,actual Timer/PackedScene/idle AnimationPlayer/deferred add_child/queue_free;hole selected original methods','Factory holder forwards actual PackedScene.instance then records the completed allocation;inherited Geyser wrapper records Ready after automatic base Ready and explicit finished handler after base handler','Inherited hole handler calls original handler before observer;no reliance on ordering of a second Timer signal connection','Two source callback observation gates emit player pause or pause tree after timeout instance construction but before deferred ready','Original source hole-local position and spawn offset,37 source holes;no scene transforms invented','Explicit Player pause/UI/command audio holders;no Character/contact delivery/full root/hardware;original sound methods and real MP3/PNG/import parameters retained','Observer PROCESS priority100 reads after source internal/script phase;after snapshot follows actual MessageQueue,delete exit events may be visible at next process phase']);return files,copied,proof

def validate_result(doc,proof):
 require(type(doc)is dict and set(doc)=={'schema','godot','rows','spawn_count'}and type(doc['schema'])is int and doc['schema']==1 and type(doc['godot'])is dict and doc['godot'].get('string')=='3.6.2-stable (official)','Geyser factory engine/schema');_,_,current=source();require(type(proof)is dict and set(proof)in[set(current),set(current)|{'producer_sha256','engine_sha256','result_sha256'}]and all(proof[k]==current[k]for k in current),'Geyser factory source/input changed');require(type(doc['rows'])is list and len(doc['rows'])==TICKS and type(doc['spawn_count'])is int and 0<doc['spawn_count']<=4096,'Geyser factory bounded coverage')
 _,poison,_=p.source();recipe,geyser,_=g.source();allocated=set();ready=set();finished=set();exited=set();origins={};gates=set();paths={'res://'+s['source']for s in geyser['sounds']};clips={c['name']for c in recipe['clips']};actor_fields=set('ticket hole position inside ready time assigned playing frame monitoring disabled active paused gain queued commands on_screen'.split())
 def integer(value,minimum=0,maximum=0xffffffff):return type(value)is int and minimum<=value<=maximum
 def word(value,nonnegative=False):return integer(value)and math.isfinite(struct.unpack('<f',struct.pack('<I',value))[0])and(not nonnegative or not value&0x80000000)
 def bits(value):return struct.unpack('<I',struct.pack('<f',value))[0]
 def event(value):
  require(type(value)is list and value and type(value[0])is str,'Geyser factory event type')
  name=value[0]
  if name=='created':
   require(len(value)==2 and integer(value[1],1,4096)and value[1]==len(allocated)+1,'Geyser factory allocation order');allocated.add(value[1])
  elif name=='timeout':
   require(len(value)==3 and integer(value[1],0,len(poison['holes'])-1)and integer(value[2],1,4096)and value[2]in allocated and value[2]not in origins,'Geyser factory timeout ownership');origins[value[2]]=value[1]
  elif name=='ready':
   require(len(value)==4 and integer(value[1],1,4096)and value[1]in origins and value[1]not in ready and all(type(v)is bool for v in value[2:]),'Geyser factory Ready ownership');ready.add(value[1])
  elif name=='finished':
   require(len(value)==4 and integer(value[1],1,4096)and value[1]in ready and value[1]not in finished and value[2]in clips and value[3]is True,'Geyser factory completion ownership');finished.add(value[1])
  elif name=='exit':
   require(len(value)==2 and integer(value[1],1,4096)and value[1]in finished and value[1]not in exited,'Geyser factory deferred delete ownership');exited.add(value[1])
  elif name in ['player_pause','tree_pause']:
   require(len(value)==2 and value[1]is True and name not in gates,'Geyser factory observation gate');gates.add(name)
  else:raise ValueError('Geyser factory unknown lifecycle event')
 # Validation deliberately precedes freezing a new observation; replay/model
 # assertions are separate and must never rewrite source output to fit C++.
 for index,row in enumerate(doc['rows']):
  require(type(row)is dict and set(row)=={'tick','delta','before','actions','after'}and row['tick']==index and type(row['tick'])is int and row['delta']==struct.unpack('<I',struct.pack('<f',1/60))[0]and json.dumps(row['actions'],sort_keys=True)==json.dumps(OPERATIONS.get(index,[]),sort_keys=True),'Geyser factory clock/action identity')
  for phase in ['before','after']:
   state=row[phase];require(type(state)is dict and set(state)=={'actors','clocks','events','global_calls','player_paused','tree_paused','ui'}and type(state['player_paused'])is bool and type(state['tree_paused'])is bool and type(state['actors'])is list and len(state['actors'])<=4096 and type(state['clocks'])is list and len(state['clocks'])==len(poison['holes'])and type(state['ui'])is list and len(state['ui'])==3 and all(type(v)is bool for v in state['ui']),'Geyser factory phase domain');require(type(state['events'])is list and len(state['events'])<=4096,'Geyser factory event capacity')
   for value in state['events']:event(value)
   for clock in state['clocks']:require(type(clock)is list and len(clock)==3 and all(word(v,True)for v in clock[:2])and clock[1]!=0 and type(clock[2])is bool,'Geyser factory Timer domain')
   ids=[]
   for actor in state['actors']:
    require(type(actor)is dict and set(actor)==actor_fields and integer(actor['ticket'],1,4096)and actor['ticket']in origins and actor['hole']==origins[actor['ticket']]and type(actor['hole'])is int,'Geyser factory actor identity');ticket=actor['ticket'];ids.append(ticket);hole=poison['holes'][actor['hole']];position=[bits(p.f.f32(hole['position'][n]+poison['spawn'][n]))for n in range(2)]
    require(actor['position']==position and type(actor['position'])is list and all(word(v)for v in actor['position'])and all(type(actor[k])is bool for k in ['inside','ready','playing','monitoring','disabled','active','paused','queued','on_screen'])and actor['inside']==actor['ready']==(ticket in ready)and actor['queued']==(ticket in finished),'Geyser factory actor source/lifecycle');require(word(actor['time'],True)and struct.unpack('<f',struct.pack('<I',actor['time']))[0]<=max(c['duration']for c in geyser['clips'])and actor['assigned']in clips|{''}and integer(actor['frame'],0,geyser['frames']-1)and word(actor['gain']),'Geyser factory animation/property domain');require(type(actor['commands'])is list and len(actor['commands'])<=2,'Geyser factory audio capacity')
    for command in actor['commands']:require(type(command)is list and(command==['stop']or len(command)==2 and(command[0]=='stream'and command[1]in paths or command[0]=='play'and word(command[1],True))),'Geyser factory audio command')
   require(ids==sorted(allocated-exited),'Geyser factory missing/duplicate actor');require(type(state['global_calls'])is list and len(state['global_calls'])<=1,'Geyser factory named command capacity')
   for command in state['global_calls']:require(command==['res://'+geyser['sounds'][2]['source'],geyser['slot']],'Geyser factory named command binding')
 require(len(allocated)==doc['spawn_count']and ready==allocated and finished and gates=={'player_pause','tree_pause'},'Geyser factory incomplete lifetime/gate coverage')
 return doc

def main():
 a=argparse.ArgumentParser(description=__doc__);a.add_argument('--out',type=Path,required=True);o=a.parse_args();out=o.out.resolve();engine=ROOT/'build/godot-reference/Godot_v3.6.2-stable_win64.exe';require(out.is_relative_to(ROOT/'build')and out!=ROOT/'build'and not out.exists(),'Fresh factory reference output under build required');require(hashlib.sha256(engine.read_bytes()).hexdigest()=='e748acc92ee5205dd3cc1754d4422870f6a68ae6e00a16ff88bd3438c957adfd','Factory engine SHA');files,copied,proof=source();out.mkdir(parents=True);project=out/'temporary';project.mkdir();(project/'.owned-geyser-factory').write_bytes(b'geyser-factory-v1');ex=Extractor(ROOT);(out/'inputs.json').write_text(json.dumps(proof,indent=2)+'\n')
 for name in copied:
  if not name.endswith('.import'):target=project/name;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(ex.data(name))
 def write():
  for name,value in files.items():target=project/name;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(value.encode())
 write();args=['--video-driver','GLES2','--debug'];invoke(engine,project,args+['--editor','--quit'],out/'fresh-import.log')
 for name in copied:
  if name.endswith('.import'):(project/name).write_bytes(ex.data(name))
 checks=list((project/'.import').glob('*.md5'));require(len(checks)==4,'Factory fresh import coverage')
 for path in checks:require(path.resolve().parent==(project/'.import').resolve()and not path.is_symlink(),'Factory import checksum escaped');path.unlink()
 invoke(engine,project,args+['--editor','--quit'],out/'source-import.log');(out/'editor-project.godot').write_bytes((project/'project.godot').read_bytes());write();invoke(engine,project,args+['--fixed-fps','60'],out/'godot.log');require(all(hashlib.sha256((project/n).read_bytes()).hexdigest()==h for n,h in proof['generated_sha256'].items()),'Factory generated input changed');require(all(hashlib.sha256((project/n).read_bytes()).hexdigest()==h for n,h in proof['copied_source'].items()),'Factory copied source changed');doc=validate_result(json.loads((out/'reference.json').read_bytes()),proof);proof.update(producer_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),engine_sha256=hashlib.sha256(engine.read_bytes()).hexdigest(),result_sha256=hashlib.sha256((out/'reference.json').read_bytes()).hexdigest());(out/'receipt.json').write_text(json.dumps(proof,indent=2)+'\n');print('Original Geyser factory:',len(doc['rows']),'frames,',doc['spawn_count'],'actual instances;not full scene/hardware')
if __name__=='__main__':main()
