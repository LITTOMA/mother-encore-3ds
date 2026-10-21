"""Real source named-SFX methods and AudioStreamPlayer defaults on eight MP3s.

Selected Player calls only;Dummy output,manual inherited run_sound and flags.
No full AudioManager ready/settings,area physics,movement or audible claim.
"""
import argparse,hashlib,json,math,re,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import catacombs_step_audio as audio
from tools.extract_battle_entry import Extractor,require
from tools.reference_movement import extract
from tools.run_scene_reference import invoke
PROBE=r'''extends SceneTree
var rows=[]
func _init():call_deferred("run")
func capture(index,phase,actor,manager,original):
    var node=manager.get_sfx("run")
    var stream=node.stream
    rows.append({"index":index,"phase":phase,"running":actor._running,"playing":node.playing,"same_node":node==original,"children":manager.get_node("Sfx").get_child_count(),"name":node.name,"source":stream.resource_path,"bus":node.bus,"gain_db":node.volume_db,"pitch":node.pitch_scale,"autoplay":node.autoplay,"stream_paused":node.stream_paused,"loop":stream.loop,"loop_offset":stream.loop_offset,"length":stream.get_length()})
func settle(node,expected):
    # AudioServer mixes independently of idle frames. Observe a bounded stable
    # result instead of assuming two arbitrarily short idle frames drain stop.
    var stable=0
    for _i in 200:
        yield(create_timer(0.01),"timeout")
        stable=stable+1 if node.playing==expected else 0
        if stable==3:return
    assert(false)
func run():
    var file=File.new()
    assert(file.open("res://input.json",File.READ)==OK)
    var config=JSON.parse(file.get_as_text()).result
    file.close()
    var manager=load("res://manager.gd").new()
    var container=Node.new()
    container.name="Sfx"
    manager.add_child(container)
    get_root().add_child(manager)
    var actor=load("res://actor.gd").new()
    actor.audioManager=manager
    get_root().add_child(actor)
    var original
    for i in config.sounds.size():
        var stream=load(config.sounds[i].source)
        assert(stream is AudioStreamMP3)
        actor.run_sound=config.sounds[i].name
        actor._running=false
        var node=manager.add_sfx(stream,"run")
        if original==null:original=node
        yield(settle(node,false),"completed")
        capture(i,"initial",actor,manager,original)
        actor._set_running(true)
        yield(settle(node,true),"completed")
        capture(i,"play",actor,manager,original)
        actor._set_running(true)
        yield(settle(node,true),"completed")
        capture(i,"repeat",actor,manager,original)
        actor._set_running(false)
        yield(settle(node,false),"completed")
        capture(i,"stop",actor,manager,original)
    var buses=[]
    for i in AudioServer.bus_count:buses.append({"name":AudioServer.get_bus_name(i),"gain_db":AudioServer.get_bus_volume_db(i),"mute":AudioServer.is_bus_mute(i)})
    assert(file.open("res://../reference.json",File.WRITE)==OK)
    file.store_string(JSON.print({"schema":1,"godot":Engine.get_version_info(),"rows":rows,"buses":buses},"  "))
    file.close()
    actor.queue_free()
    manager.queue_free()
    yield(self,"idle_frame")
    yield(self,"idle_frame")
    quit()
'''
def source():
 p,m,_,manager,proof=audio.source();ex=Extractor(ROOT);raw=ex.data('Scripts/Main/party/Player.gd');methods,hashes=extract(raw,hashlib.sha256(raw).hexdigest(),['_set_running']);actor='extends Node\nconst MOVE=0\nvar _state=MOVE\nvar _running=false\nvar _paused=false\nvar _climbing=false\nvar run_sound=""\nvar audioManager\n'+methods
 player=raw.decode('utf8');require('enum {\n\tMOVE,'in player and 'var _state := MOVE'in player,'Stepping source MOVE adapter changed')
 files={'actor.gd':actor,'manager.gd':'extends Node\n'+manager,'probe.gd':PROBE,'input.json':json.dumps(dict(sounds=m['sounds'])),'project.godot':'config_version=4\n[application]\nconfig/name="Original named run SFX"\n[audio]\ndefault_bus_layout="res://'+p['bus_source']+'"\n'};copied=[p['bus_source']]+[name for name in proof['source']['sources']if name.startswith('Audio/Sound effects/Footsteps/')]
 proof=dict(source=proof,player_methods=hashes,generated_sha256={n:hashlib.sha256(v.encode()).hexdigest()for n,v in files.items()},copied_source={n:hashlib.sha256(ex.data(n)).hexdigest()for n in copied},adapters=['Original add_sfx/play_sfx/get_sfx and Player _set_running methods only;not full AudioManager ready/global settings','Real named AudioStreamPlayer reuse and original eight MP3/imports;manual inherited run_sound and MOVE flags','Dummy audio driver;bounded 10ms timer observations require three stable playing states because AudioServer mixes independently;no audible output,exact stop latency or playback-position comparison','Original default bus layout;no fake Master/SFX bus or environment cache']);return files,copied,proof
def validate_result(doc,proof):
 require(type(doc)is dict and set(doc)=={'schema','godot','rows','buses'}and type(doc['schema'])is int and doc['schema']==1 and type(doc['godot'])is dict and doc['godot'].get('string')=='3.6.2-stable (official)','Native stepping audio schema/engine');p,m,_,_,_=audio.source();require(type(doc['rows'])is list and len(doc['rows'])==4*len(m['sounds']),'Native stepping audio coverage')
 for i,row in enumerate(doc['rows']):
  fields={'index','phase','running','playing','same_node','children','name','source','bus','gain_db','pitch','autoplay','stream_paused','loop','loop_offset','length'};require(type(row)is dict and set(row)==fields,'Native stepping audio row fields');phase=['initial','play','repeat','stop'][i%4];sound=m['sounds'][i//4];active=phase in['play','repeat']
  require(type(row['index'])is int and row['index']==i//4 and row['phase']==phase and type(row['running'])is bool and type(row['playing'])is bool and row['running']==row['playing']==active and row['same_node']is True and type(row['children'])is int and row['children']==1,'Native named-SFX lifecycle/identity differs')
  require(row['name']==m['slot']and row['source']==sound['source']and row['bus']==p['bus']and type(row['gain_db'])in(int,float)and row['gain_db']==p['gain_db']and type(row['pitch'])in(int,float)and row['pitch']==p['pitch']and row['autoplay']is False and row['stream_paused']is False and row['loop']is True and type(row['loop_offset'])in(int,float)and row['loop_offset']==0 and type(row['length'])in(int,float)and 0<row['length']<60,'Native run stream/defaults differ')
 bus_text=Extractor(ROOT).text(p['bus_source']);names={int(i):name for i,name in re.findall(r'^bus/([0-9]+)/name = "([^\"]+)"$',bus_text,re.M)};names[0]='Master';gains={int(i):float(value)for i,value in re.findall(r'^bus/([0-9]+)/volume_db = (-?[0-9.]+)$',bus_text,re.M)};mutes={int(i):value=='true'for i,value in re.findall(r'^bus/([0-9]+)/mute = (true|false)$',bus_text,re.M)}
 require(type(doc['buses'])is list and len(doc['buses'])==len(names)and set(names)==set(range(len(names))),'Native source bus coverage')
 for i,b in enumerate(doc['buses']):require(type(b)is dict and set(b)=={'name','gain_db','mute'}and b['name']==names[i]and type(b['mute'])is bool and b['mute']==mutes.get(i,False)and type(b['gain_db'])in(int,float)and math.isfinite(b['gain_db'])and abs(b['gain_db']-gains.get(i,0))<=1e-5,'Native original bus name/gain/mute differs')
 require(type(proof)is dict and type(proof.get('generated_sha256'))is dict and proof['generated_sha256'].get('probe.gd')==hashlib.sha256(PROBE.encode()).hexdigest(),'Native stepping audio producer mapping changed');return doc
def main():
 a=argparse.ArgumentParser(description=__doc__);a.add_argument('--out',type=Path,required=True);a.add_argument('--godot',type=Path,default=ROOT/'build/godot-reference/Godot_v3.6.2-stable_win64.exe');o=a.parse_args();out=o.out.resolve();engine=o.godot.resolve();require(out.is_relative_to(ROOT/'build')and out!=ROOT/'build'and not out.exists(),'Fresh owned stepping-audio native output under build required');require(hashlib.sha256(engine.read_bytes()).hexdigest()=='e748acc92ee5205dd3cc1754d4422870f6a68ae6e00a16ff88bd3438c957adfd','Native original engine SHA changed');files,copied,proof=source();out.mkdir(parents=True);project=out/'temporary';project.mkdir();(project/'.owned-step-audio-native').write_bytes(b'step-audio-native-v1');ex=Extractor(ROOT)
 for name in copied:
  if not name.endswith('.import'):
   target=project/name;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(ex.data(name))
 def write_files():
  for name,value in files.items():(project/name).write_bytes(value.encode())
 write_files();args=['--video-driver','GLES2','--audio-driver','Dummy','--debug'];invoke(engine,project,args+['--editor','--quit'],out/'fresh-import.log')
 # Restore the original import parameters after fresh import,then invalidate
 # only these newly created checksums so the original settings are reimported.
 for name in copied:
  if name.endswith('.import'):(project/name).write_bytes(ex.data(name))
 checksums=list((project/'.import').glob('*.md5'));require(len(checksums)==8,'Native stepping-audio importer coverage changed')
 for path in checksums:require(path.resolve().parent==(project/'.import').resolve()and not path.is_symlink(),'Native stepping import checksum escaped');path.unlink()
 invoke(engine,project,args+['--editor','--quit'],out/'source-settings-import.log');write_files();invoke(engine,project,args+['--script','res://probe.gd'],out/'godot.log');doc=validate_result(json.loads((out/'reference.json').read_bytes()),proof);proof.update(producer_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),engine_sha256=hashlib.sha256(engine.read_bytes()).hexdigest(),result_sha256=hashlib.sha256((out/'reference.json').read_bytes()).hexdigest(),native_defaults_verified=True);(out/'receipt.json').write_bytes((json.dumps(proof,indent=2)+'\n').encode());print('Native original run SFX:',len(doc['rows']),'stable node/stream observations;Dummy output,not full scene or audible/hardware claim')
if __name__=='__main__':main()
