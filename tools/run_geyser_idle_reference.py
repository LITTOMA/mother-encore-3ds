"""Actual original Geyser idle clock with player/tree pauses and UI changes."""
import argparse,hashlib,json,math,struct,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import run_geyser_reference as manual,catacombs_geyser as g
from tools.extract_battle_entry import Extractor,require
from tools.run_scene_reference import invoke
OPERATIONS={24:[['player_pause',True]],42:[['player_pause',False]],70:[['tree_pause',True]],95:[['tree_pause',False]],105:[['visible',False]],125:[['visible',True]],130:[['ui',1]],150:[['ui',2]],165:[['ui',3]],180:[['ui',0]]}
PROBE=r'''extends Node
var actor
var player
var rows=[]
var finished=[]
var tick=0
var initial={}
var operations={}
var rumble_path
var named_slot
var active=false
func _ready():
    pause_mode=Node.PAUSE_MODE_PROCESS
    process_priority=100
    set_process(false)
    call_deferred("setup")
func bits(value):
    var bytes=StreamPeerBuffer.new()
    bytes.put_float(value)
    bytes.seek(0)
    return bytes.get_u32()
func done(name):finished.append(name)
func snapshot():
    var anim=actor.get_node("AnimationPlayer")
    var audio=actor.get_node("AudioStreamPlayer2D")
    var result={"time":bits(anim.current_animation_position),"frame":actor.get_node("Sprite").frame,"monitoring":actor.monitoring,"disabled":actor.get_node("CollisionShape2D").disabled,"active":anim.playback_active,"paused":audio.stream_paused,"gain":bits(audio.volume_db),"queued":actor.is_queued_for_deletion(),"finished":finished.duplicate(),"commands":audio.commands.duplicate(true),"global_calls":audioManager.commands.duplicate(true),"on_screen":actor.get_node("VisibilityNotifier2D").is_on_screen(),"ui":[uiManager.battle,uiManager.cutscene,uiManager.game_over]}
    audio.commands.clear()
    audioManager.commands.clear()
    return result
func setup():
    print("observer setup starting")
    var file=File.new()
    assert(file.open("res://input.json",File.READ)==OK)
    var config=JSON.parse(file.get_as_text()).result
    file.close()
    operations=config.operations
    rumble_path=config.rumble
    named_slot=config.slot
    player=load("res://party.gd").new()
    add_child(player)
    global.player=player
    actor=load("res://geyser.tscn").instance()
    actor.pause_mode=Node.PAUSE_MODE_STOP
    actor.position=Vector2(100,100)
    add_child(actor)
    var anim=actor.get_node("AnimationPlayer")
    assert(anim.connect("animation_finished",self,"done")==OK)
    actor.pause()
    yield(get_tree(),"idle_frame")
    yield(get_tree(),"idle_frame")
    player.set_observed_pause(false)
    initial=snapshot()
    print("observer setup ready:",initial)
    active=true
    set_process(true)
func _process(delta):
    if !active:return
    var row={"tick":tick,"delta":bits(delta),"tree_paused":get_tree().paused,"before":snapshot(),"actions":operations.get(str(tick),[])}
    if tick%50==0:print("observer tick:",tick," time:",row.before.time," paused:",row.tree_paused)
    # Record after real AnimationPlayer internal and source script process.
    # Actions affect the next idle frame;MessageQueue still sees CURRENT UI.
    for action in row.actions:
        match action[0]:
            "player_pause":player.set_observed_pause(action[1])
            "tree_pause":get_tree().paused=action[1]
            "visible":actor.visible=action[1]
            "ui":
                uiManager.battle=action[1]==1
                uiManager.cutscene=action[1]==2
                uiManager.game_over=action[1]==3
            _:assert(false)
    rows.append(row)
    call_deferred("after",tick)
    tick+=1
    if actor.is_queued_for_deletion():
        active=false
        set_process(false)
    if tick>300:
        active=false
        set_process(false)
func after(index):
    rows[index].after=snapshot()
    if !active:call_deferred("finish")
func finish():
    get_tree().paused=false
    var lifecycle=[]
    for old in audioManager.get_node("Sfx").get_children():old.free()
    get_tree().paused=true
    var node=audioManager.play_sfx(load(rumble_path),named_slot)
    lifecycle.append(sound_state(node,"born_paused"))
    yield(get_tree(),"idle_frame")
    yield(get_tree(),"idle_frame")
    lifecycle.append(sound_state(node,"settled_paused"))
    get_tree().paused=false
    lifecycle.append(sound_state(node,"unpause"))
    get_tree().paused=true
    lifecycle.append(sound_state(node,"pause_existing"))
    node.free()
    node=audioManager.play_sfx(load(rumble_path),named_slot)
    lifecycle.append(sound_state(node,"reborn_paused"))
    get_tree().paused=false
    lifecycle.append(sound_state(node,"unpause_reborn"))
    node.pause_mode=Node.PAUSE_MODE_PROCESS
    get_tree().paused=true
    lifecycle.append(sound_state(node,"processable_paused"))
    get_tree().paused=false
    lifecycle.append(sound_state(node,"unpause_processable"))
    var file=File.new()
    assert(file.open("res://../reference.json",File.WRITE)==OK)
    file.store_string(JSON.print({"schema":1,"godot":Engine.get_version_info(),"initial":initial,"rows":rows,"tree_lifecycle":lifecycle},"  "))
    file.close()
    get_tree().quit()
func sound_state(node,phase):
    return {"phase":phase,"paused":node.stream_paused,"playing":node.playing,"can_process":node.can_process(),"manager_can_process":audioManager.can_process(),"mode":node.pause_mode,"bus":node.bus,"source":node.stream.resource_path}
'''
def source():
 files,copied,proof=manual.source();_,model,_=g.source();files['probe.gd']=PROBE;files['input.json']=json.dumps(dict(operations=OPERATIONS,rumble='res://'+model['sounds'][2]['source'],slot=model['slot']));proof=dict(source=proof['source'],generated_sha256={k:hashlib.sha256(v.encode()).hexdigest()for k,v in files.items()},copied_source=proof['copied_source'],adapters=proof['adapters'][:3]+['Actual idle AnimationPlayer,source _process and pause/unpause signals;explicit observer PROCESS parent,actor STOP retains source inherited game pause behavior','Observer process priority100 records after original internal animation/source script priority0;actions follow snapshots,deferred capture follows original queued methods before delete','Actor held through two visibility settle frames using source pause;source unpause begins actual idle clock at time0;no manual advance and no pixel/DSP/Character/scene claim','Actual source named ordinary AudioStreamPlayer creation/recreation under paused tree,existing-node notifications and explicit PROCESS override;not inferred from Audio2D/player flags']);return files,copied,proof
def validate_result(doc,proof):
 require(type(doc)is dict and set(doc)=={'schema','godot','initial','rows','tree_lifecycle'}and type(doc['schema'])is int and doc['schema']==1 and doc['godot'].get('string')=='3.6.2-stable (official)','Geyser idle schema/engine');_,_,current=source();require(proof['source']==current['source']and proof['generated_sha256']==current['generated_sha256'],'Geyser idle source/observer changed');require(type(doc['rows'])is list and 200<=len(doc['rows'])<=260,'Geyser idle finite lifecycle');fields=set('time frame monitoring disabled active paused gain queued finished commands global_calls on_screen ui'.split())
 _,model,_=g.source();paths={'res://'+s['source']for s in model['sounds']}
 def state(v):
  require(type(v)is dict and set(v)==fields and type(v['frame'])is int and 0<=v['frame']<model['frames']and type(v['time'])is int and 0<=v['time']<=struct.unpack('<I',struct.pack('<f',3.))[0]and all(type(v[k])is bool for k in['monitoring','disabled','active','paused','queued','on_screen'])and v['finished']in[[],['Spewing']],'Geyser idle state domain');require(type(v['gain'])is int and 0<=v['gain']<=0xffffffff and math.isfinite(struct.unpack('<f',struct.pack('<I',v['gain']))[0])and type(v['ui'])is list and len(v['ui'])==3 and all(type(k)is bool for k in v['ui']),'Geyser idle gain/UI domain');require(type(v['commands'])is list and len(v['commands'])<=2 and type(v['global_calls'])is list and len(v['global_calls'])<=1,'Geyser idle command capacities')
  for c in v['commands']:require(type(c)is list and(c==['stop']or len(c)==2 and(c[0]=='stream'and c[1]in paths or c[0]=='play'and type(c[1])is int and 0<=c[1]<=0x7f7fffff)),'Geyser idle unknown command/operand')
  for c in v['global_calls']:require(c==['res://'+model['sounds'][2]['source'],model['slot']],'Geyser idle unknown named rumble')
 state(doc['initial']);require(doc['initial']['time']==0 and not doc['initial']['commands']and not doc['initial']['global_calls'],'Geyser idle initial preparation')
 for index,row in enumerate(doc['rows']):
  require(type(row)is dict and set(row)=={'tick','delta','tree_paused','before','actions','after'}and type(row['tick'])is int and row['tick']==index and row['delta']==struct.unpack('<I',struct.pack('<f',1/60))[0]and type(row['tree_paused'])is bool and row['actions']==OPERATIONS.get(index,[]),'Geyser idle input/order');state(row['before']);state(row['after'])
 require(doc['rows'][-1]['before']['queued']and doc['rows'][-1]['after']['queued'],'Geyser actual queued retirement');phases=['born_paused','settled_paused','unpause','pause_existing','reborn_paused','unpause_reborn','processable_paused','unpause_processable'];require(type(doc['tree_lifecycle'])is list and len(doc['tree_lifecycle'])==len(phases),'Geyser named tree lifecycle coverage')
 for index,row in enumerate(doc['tree_lifecycle']):require(type(row)is dict and set(row)=={'phase','paused','playing','can_process','manager_can_process','mode','bus','source'}and row['phase']==phases[index]and all(type(row[k])is bool for k in['paused','playing','can_process','manager_can_process'])and type(row['mode'])is int and row['mode']==(2 if index>=6 else 0)and row['bus']==model['bus']and row['source']=='res://'+model['sounds'][2]['source'],'Geyser named native lifecycle identity/domain')
 return doc
def main():
 a=argparse.ArgumentParser(description=__doc__);a.add_argument('--out',type=Path,required=True);o=a.parse_args();out=o.out.resolve();engine=ROOT/'build/godot-reference/Godot_v3.6.2-stable_win64.exe';require(out.is_relative_to(ROOT/'build')and out!=ROOT/'build'and not out.exists(),'Fresh Geyser idle output under build required');require(hashlib.sha256(engine.read_bytes()).hexdigest()=='e748acc92ee5205dd3cc1754d4422870f6a68ae6e00a16ff88bd3438c957adfd','Geyser idle engine SHA');files,copied,proof=source();out.mkdir(parents=True);project=out/'temporary';project.mkdir();(project/'.owned-geyser-idle').write_bytes(b'geyser-idle-v1');ex=Extractor(ROOT)
 for name in copied:
  if not name.endswith('.import'):target=project/name;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(ex.data(name))
 def write():
  for name,value in files.items():target=project/name;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(value.encode())
 write();args=['--video-driver','GLES2','--debug'];invoke(engine,project,args+['--editor','--quit'],out/'fresh-import.log')
 for name in copied:
  if name.endswith('.import'):(project/name).write_bytes(ex.data(name))
 checks=list((project/'.import').glob('*.md5'));require(len(checks)==4,'Geyser idle fresh import coverage')
 for path in checks:require(path.resolve().parent==(project/'.import').resolve()and not path.is_symlink(),'Geyser idle import checksum escaped');path.unlink()
 invoke(engine,project,args+['--editor','--quit'],out/'source-import.log');(out/'editor-project.godot').write_bytes((project/'project.godot').read_bytes());write();invoke(engine,project,args+['--fixed-fps','60'],out/'godot.log');require(all(hashlib.sha256((project/n).read_bytes()).hexdigest()==h for n,h in proof['generated_sha256'].items()),'Geyser idle executed input changed');require(all(hashlib.sha256((project/n).read_bytes()).hexdigest()==h for n,h in proof['copied_source'].items()),'Geyser idle copied source changed');doc=validate_result(json.loads((out/'reference.json').read_bytes()),proof);proof.update(producer_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),engine_sha256=hashlib.sha256(engine.read_bytes()).hexdigest(),result_sha256=hashlib.sha256((out/'reference.json').read_bytes()).hexdigest());(out/'receipt.json').write_text(json.dumps(proof,indent=2)+'\n');print('Original Geyser actual idle:',len(doc['rows']),'frames;not spatial audio/GPU/full scene/hardware')
if __name__=='__main__':main()
