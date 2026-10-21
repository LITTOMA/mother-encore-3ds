"""Original Geyser/AnimationPlayer methods,real MP3s and deferred calls.

Explicit player/UI services and command observers;Dummy output,no full scene.
"""
import argparse,hashlib,json,math,struct,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import catacombs_geyser as g
from tools.reference_movement import extract
from tools.extract_battle_entry import Extractor,require
from tools.run_scene_reference import invoke
CASES=[dict(name='fine',deltas=[1/60]*181),dict(name='coarse',deltas=[1.2,1.1,.7]),dict(name='end_crossing',deltas=[3.0]),dict(name='zeros',deltas=[0.,.8,.066667,0.,.2,.000003,.06666,.93334,.4,.5,.1]),dict(name='paused_manual',deltas=[.5,.6,0.,.1,1.,.8],pause={0:True,3:False}),dict(name='battle_before_flush',deltas=[1.2,1.,.8],gate={0:1}),dict(name='cutscene_before_flush',deltas=[1.2,1.,.8],gate={0:2}),dict(name='gameover_before_flush',deltas=[1.2,1.,.8],gate={0:3}),dict(name='offscreen',deltas=[1.2,1.,.8],offscreen=True),dict(name='named_playing',deltas=[1.2,1.,.8],voice=3),dict(name='named_idle',deltas=[1.2,1.,.8],voice=2)]
AUDIO=r'''extends AudioStreamPlayer2D
var commands=[]
func bits(value):
    var bytes=StreamPeerBuffer.new()
    bytes.put_float(value)
    bytes.seek(0)
    return bytes.get_u32()
func set_stream(value):
    commands.append(["stream",value.resource_path])
    .set_stream(value)
func play(value=0.0):
    commands.append(["play",bits(value)])
    .play(value)
func stop():
    commands.append(["stop"])
    .stop()
'''
PROBE=r'''extends Node
var rows=[]
var finished=[]
func _ready():call_deferred("run")
func bits(value):
    var bytes=StreamPeerBuffer.new()
    bytes.put_float(value)
    bytes.seek(0)
    return bytes.get_u32()
func done(name):finished.append(name)
func capture(actor,case_index,index,phase,delta):
    var anim=actor.get_node("AnimationPlayer")
    var audio=actor.get_node("AudioStreamPlayer2D")
    rows.append({"case":case_index,"index":index,"phase":phase,"delta":bits(delta),"time":bits(anim.current_animation_position),"frame":actor.get_node("Sprite").frame,"monitoring":actor.monitoring,"disabled":actor.get_node("CollisionShape2D").disabled,"active":anim.playback_active,"paused":audio.stream_paused,"gain":bits(audio.volume_db),"commands":audio.commands.duplicate(true),"global_calls":audioManager.commands.duplicate(true),"finished":finished.duplicate(),"queued":actor.is_queued_for_deletion(),"on_screen":actor.get_node("VisibilityNotifier2D").is_on_screen(),"ui":[uiManager.battle,uiManager.cutscene,uiManager.game_over]})
    audio.commands.clear()
    audioManager.commands.clear()
func stable(node,expected):
    var count=0
    for _i in 200:
        yield(get_tree().create_timer(.01),"timeout")
        count=count+1 if node.playing==expected else 0
        if count==3:return
    assert(false)
func run():
    var file=File.new()
    assert(file.open("res://input.json",File.READ)==OK)
    var config=JSON.parse(file.get_as_text()).result
    file.close()
    var player=load("res://party.gd").new()
    add_child(player)
    global.player=player
    var meta={}
    for path in config.sounds:
        var stream=load(path)
        meta[path]={"length":bits(stream.get_length()),"loop":stream.loop,"offset":bits(stream.loop_offset)}
    var geometry={}
    for case_index in config.cases.size():
        var spec=config.cases[case_index]
        for old in audioManager.get_node("Sfx").get_children():old.free()
        audioManager.commands.clear()
        uiManager.battle=false
        uiManager.cutscene=false
        uiManager.game_over=false
        if spec.get("voice",1)!=1:
            var named=audioManager.add_sfx(load(config.sounds[2]),"geyser")
            if spec.voice==3:named.play()
            yield(stable(named,spec.voice==3),"completed")
        finished=[]
        var actor=load("res://geyser.tscn").instance()
        actor.position=Vector2(-1000,-1000) if spec.get("offscreen",false) else Vector2(100,100)
        add_child(actor)
        var anim=actor.get_node("AnimationPlayer")
        anim.playback_process_mode=AnimationPlayer.ANIMATION_PROCESS_MANUAL
        actor.set_process(false)
        assert(anim.connect("animation_finished",self,"done")==OK)
        yield(get_tree(),"idle_frame")
        yield(get_tree(),"idle_frame")
        if case_index==0:
            var sprite=actor.get_node("Sprite")
            var audio=actor.get_node("AudioStreamPlayer2D")
            var shape=actor.get_node("CollisionShape2D")
            var rect=actor.get_node("VisibilityNotifier2D").rect
            geometry={"centered":sprite.centered,"size":[bits(sprite.texture.get_width()/float(sprite.hframes)),bits(sprite.texture.get_height())],"sprite":[bits(sprite.position.x),bits(sprite.position.y),bits(sprite.offset.x),bits(sprite.offset.y)],"shape":[bits(shape.position.x),bits(shape.position.y),bits(shape.rotation),bits(shape.shape.extents.x),bits(shape.shape.extents.y)],"audio":[bits(audio.volume_db),bits(audio.pitch_scale),bits(audio.max_distance),bits(audio.attenuation),bits(audio.panning_strength)],"notify":[bits(rect.position.x),bits(rect.position.y),bits(rect.size.x),bits(rect.size.y)]}
        capture(actor,case_index,-1,"ready",0.0)
        for index in spec.deltas.size():
            var paused=spec.get("pause",{}).get(str(index),null)
            if paused!=null:player.set_observed_pause(paused)
            anim.advance(spec.deltas[index])
            var gate=spec.get("gate",{}).get(str(index),0)
            uiManager.battle=gate==1
            uiManager.cutscene=gate==2
            uiManager.game_over=gate==3
            actor._process(0.0)
            capture(actor,case_index,index,"before_deferred",spec.deltas[index])
            # Even queued_free source methods remain in the MessageQueue until
            # flush;an observer deferred after them runs before delete flush.
            call_deferred("capture_deferred",actor,case_index,index,spec.deltas[index])
            yield(get_tree(),"idle_frame")
            if !is_instance_valid(actor):break
        if is_instance_valid(actor):actor.queue_free()
        yield(get_tree(),"idle_frame")
    assert(file.open("res://../reference.json",File.WRITE)==OK)
    var cases=[]
    for spec in config.cases:cases.append({"name":spec.name,"steps":spec.deltas.size()})
    file.store_string(JSON.print({"schema":1,"godot":Engine.get_version_info(),"meta":meta,"geometry":geometry,"cases":cases,"rows":rows},"  "))
    file.close()
    get_tree().quit()
func capture_deferred(actor,case_index,index,delta):
    capture(actor,case_index,index,"after_deferred",delta)
'''
def source():
 p,m,proof=g.source();ex=Extractor(ROOT);n=p['paths'];script=ex.text(n['script']).replace('func _process(delta):','func _process(delta):\n\tvar _observer_delta=delta').replace('func _on_AnimationPlayer_animation_finished(anim_name):','func _on_AnimationPlayer_animation_finished(anim_name):\n\tvar _observer_name=anim_name');manager=extract(ex.data(n['manager']),p['sources'][n['manager']],['add_sfx','play_sfx','get_sfx'])[0];manager=manager.replace('\tvar sfx_node: AudioStreamPlayer = add_sfx(stream, node_name)','\tcommands.append([stream.resource_path,node_name])\n\tvar sfx_node: AudioStreamPlayer = add_sfx(stream, node_name)')
 scene=ex.text(n['scene']).replace('load_steps=8','load_steps=9').replace('[sub_resource type="RectangleShape2D" id=1]','[ext_resource path="res://audio.gd" type="Script" id=5]\n\n[sub_resource type="RectangleShape2D" id=1]').replace('max_distance = 180.0','script = ExtResource( 5 )\nmax_distance = 180.0')
 classes=[dict(base='KinematicBody2D',**{'class':'PartyObject'},language='GDScript',path='res://party.gd'),dict(base='Node',**{'class':'Status'},language='GDScript',path='res://status.gd')]
 project='config_version=4\n_global_script_classes='+json.dumps(classes)+'\n_global_script_class_icons={"PartyObject":"","Status":""}\n[application]\nconfig/name="Original Geyser timeline"\nrun/main_scene="res://probe.tscn"\n[autoload]\nglobal="*res://global.gd"\nuiManager="*res://ui.gd"\naudioManager="*res://manager.gd"\n[audio]\ndefault_bus_layout="res://'+n['bus']+'"\n'
 files={'project.godot':project,'party.gd':'extends KinematicBody2D\nclass_name PartyObject\nsignal paused\nsignal unpaused\nfunc set_observed_pause(value):\n\tif value:emit_signal("paused")\n\telse:emit_signal("unpaused")\nfunc is_paused():return false\nfunc start_continuous_damage(_a,_b,_c,_d):pass\nfunc stop_continuous_damage():pass\n','status.gd':'extends Node\nclass_name Status\nconst AILMENT_POISONED="poisoned"\n','global.gd':'extends Node\nvar player\nfunc get_player():return player\n','ui.gd':'extends Node\nvar battle=false\nvar cutscene=false\nvar game_over=false\nfunc is_in_battle():return battle\nfunc is_in_cutscene():return cutscene\nfunc is_game_over():return game_over\n','manager.gd':'extends Node\nvar commands=[]\nfunc _ready():\n\tvar node=Node.new()\n\tnode.name="Sfx"\n\tadd_child(node)\n'+manager,n['script']:script,'audio.gd':AUDIO,'geyser.tscn':scene,'probe.gd':PROBE,'probe.tscn':'[gd_scene load_steps=2 format=2]\n[ext_resource path="res://probe.gd" type="Script" id=1]\n[node name="Probe" type="Node"]\nscript=ExtResource(1)\n','input.json':json.dumps(dict(cases=CASES,sounds=['res://'+a['source']for a in m['sounds']]))}
 copied=[n['bus'],n['texture'],n['texture_import']]+[x for s in p['sounds']for x in[s['source'],s['import_source']]]
 receipt=dict(source=proof,generated_sha256={k:hashlib.sha256(v.encode()).hexdigest()for k,v in files.items()},copied_source={k:hashlib.sha256(ex.data(k)).hexdigest()for k in copied},adapters=['Complete original Geyser script with trailing observer-only reads of two unused parameters;original assignments/guards preserved,no diagnostic suppression','Original scene and real PNG/three MP3s/original import flags/bus;one Audio2D script records actual set_stream/play/stop calls and forwards native base methods','Original named SFX methods with one entry observer append;real named AudioStreamPlayer and stable playing/idle preconditions,not a synthetic gate result','Manual AnimationPlayer process mode only for deterministic manual advance;real deferred method queue and actual VisibilityNotifier;no automatic idle equivalence claim','Player/UI are explicit services;PartyObject callbacks are not invoked,no HP/Character/physics claim','UI changed after animation scheduling and before MessageQueue flush;queued-free actor observed before actual deferred delete','Dummy audio output;no audible/spatial-mix/natural decoder phase or hardware claim']);return files,copied,receipt
def validate_result(doc,proof):
 require(type(doc)is dict and set(doc)=={'schema','godot','meta','geometry','cases','rows'}and type(doc['schema'])is int and doc['schema']==1 and doc['godot'].get('string')=='3.6.2-stable (official)'and doc['cases']==[dict(name=c['name'],steps=len(c['deltas']))for c in CASES],'Geyser native schema/cases/engine');p,m,_=g.source();files,_,current=source();require(proof['generated_sha256']==current['generated_sha256']and proof['source']==current['source'],'Geyser native source/observer changed')
 require(doc['meta']=={'res://'+s['source']:dict(length=p['native_lengths'][s['source']],loop=False,offset=0)for s in m['sounds']},'Geyser native stream metadata changed');require(type(doc['rows'])is list and len(doc['rows'])==sum(1+2*len(c['deltas'])for c in CASES),'Geyser original full case/phase coverage')
 def bits(v):return struct.unpack('<I',struct.pack('<f',v))[0]
 geometry=dict(centered=m['centered'],size=list(map(bits,m['size'])),sprite=list(map(bits,m['sprite']+m['offset'])),shape=list(map(bits,m['shape']+[m['rotation']]+m['extents'])),audio=list(map(bits,[m['normal_gain'],m['pitch'],m['max_distance'],m['attenuation'],m['panning']])),notify=list(map(bits,m['notify'])))
 require(doc['geometry']==geometry,'Geyser native default/geometry profile differs')
 for case_index,c in enumerate(CASES):
  rows=[row for row in doc['rows']if row['case']==case_index];require(len(rows)==1+2*len(c['deltas'])and rows[0]['index']==-1 and rows[0]['phase']=='ready','Geyser native row identity')
  for j,row in enumerate(rows):
   require(set(row)==set('case index phase delta time frame monitoring disabled active paused gain commands global_calls finished queued on_screen ui'.split())and type(row['frame'])is int and 0<=row['frame']<m['frames']and type(row['time'])is int and 0<=row['time']<=0x7f7fffff,'Geyser native row domain');require(all(type(row[k])is bool for k in['monitoring','disabled','active','paused','queued','on_screen'])and type(row['ui'])is list and len(row['ui'])==3 and all(type(v)is bool for v in row['ui']),'Geyser native Boolean domain');require(row['finished']in[[],['Spewing']],'Geyser native completion');require(row['on_screen']==(not c.get('offscreen',False)),'Geyser actual visibility precondition differs')
   require(type(row['commands'])is list and len(row['commands'])<=2 and type(row['global_calls'])is list and len(row['global_calls'])<=1,'Geyser native command capacity')
   for command in row['commands']:require(type(command)is list and (command==['stop']or len(command)==2 and(command[0]=='stream'and command[1]in doc['meta']or command[0]=='play'and type(command[1])is int and 0<=command[1]<=0x7f7fffff)),'Geyser native command opcode/operand')
   for command in row['global_calls']:require(command==['res://'+m['sounds'][2]['source'],m['slot']],'Geyser native global command binding')
   if j:require(row['index']==(j-1)//2 and row['phase']==['before_deferred','after_deferred'][(j-1)%2]and row['delta']==struct.unpack('<I',struct.pack('<f',c['deltas'][(j-1)//2]))[0],'Geyser native order/delta')
 return doc
def main():
 a=argparse.ArgumentParser(description=__doc__);a.add_argument('--out',type=Path,required=True);o=a.parse_args();out=o.out.resolve();engine=ROOT/'build/godot-reference/Godot_v3.6.2-stable_win64.exe';require(out.is_relative_to(ROOT/'build')and out!=ROOT/'build'and not out.exists(),'Fresh Geyser native output under build required');require(hashlib.sha256(engine.read_bytes()).hexdigest()=='e748acc92ee5205dd3cc1754d4422870f6a68ae6e00a16ff88bd3438c957adfd','Geyser engine SHA');files,copied,proof=source();out.mkdir(parents=True);project=out/'temporary';project.mkdir();(project/'.owned-geyser-reference').write_bytes(b'geyser-reference-v1');ex=Extractor(ROOT)
 for name in copied:
  if not name.endswith('.import'):target=project/name;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(ex.data(name))
 def write():
  for name,value in files.items():target=project/name;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(value.encode())
 write();args=['--video-driver','GLES2','--debug'];invoke(engine,project,args+['--editor','--quit'],out/'fresh-import.log')
 for name in copied:
  if name.endswith('.import'):(project/name).write_bytes(ex.data(name))
 checks=list((project/'.import').glob('*.md5'));require(len(checks)==4,'Geyser fresh import coverage')
 for path in checks:require(path.resolve().parent==(project/'.import').resolve()and not path.is_symlink(),'Geyser fresh checksum escaped');path.unlink()
 invoke(engine,project,args+['--editor','--quit'],out/'source-import.log');(out/'editor-project.godot').write_bytes((project/'project.godot').read_bytes());write();invoke(engine,project,args+['--fixed-fps','60'],out/'godot.log')
 require(all(hashlib.sha256((project/n).read_bytes()).hexdigest()==h for n,h in proof['generated_sha256'].items()),'Geyser executed generated input changed');require(all(hashlib.sha256((project/n).read_bytes()).hexdigest()==h for n,h in proof['copied_source'].items()),'Geyser copied source changed');doc=validate_result(json.loads((out/'reference.json').read_bytes()),proof);proof.update(producer_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),engine_sha256=hashlib.sha256(engine.read_bytes()).hexdigest(),result_sha256=hashlib.sha256((out/'reference.json').read_bytes()).hexdigest());(out/'receipt.json').write_text(json.dumps(proof,indent=2)+'\n');print('Original Geyser:',len(CASES),'cases,',len(doc['rows']),'observations;not full scene/audio admission')
if __name__=='__main__':main()
