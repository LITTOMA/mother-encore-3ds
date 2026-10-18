"""Execute unchanged original pause/resume methods with real Godot players/Tween."""
from pathlib import Path
import argparse,hashlib,json,math,shutil,sys
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import run_music_lifetime_reference as base
from tools.extract_battle_entry import Extractor,require
from tools.reference_movement import extract
from tools.run_scene_reference import invoke
CASES=[
 dict(name='empty_pause_new_player',actions=[['pause'],['add'],['play',1,1],['settle'],['resume']]),
 dict(name='playing_pause_new_battle',actions=[['play',0,1],['settle'],['pause'],['add'],['play',1,2],['settle'],['resume']]),
 dict(name='replacement_keeps_pause',actions=[['play',0,1],['settle'],['pause'],['play',0,2],['settle'],['resume']]),
 dict(name='stop_and_replay_keeps_pause',actions=[['play',0,1],['settle'],['pause'],['stop',0],['play',0,2],['settle'],['resume']]),
 dict(name='remove_recreates_unpaused',actions=[['pause'],['remove',0],['flush'],['play',0,1],['settle']]),
 dict(name='pause_finishes_fadeout',actions=[['play',0,1],['settle'],['fadeout',0,.2],['pause'],['flush'],['resume']]),
 dict(name='pause_finishes_fadeto',actions=[['play',0,1],['settle'],['fadeto',0,-12,.2],['pause'],['flush'],['resume']]),
 dict(name='pause_finishes_fadein',actions=[['play',0,1],['settle'],['fadein',0,6,.2],['pause'],['flush'],['resume']]),
 dict(name='pause_finishes_all_durations',actions=[['play',0,1],['add'],['play',1,2],['settle'],['fadeout',0,.1],['fadeto',1,6,.4],['pause'],['flush'],['resume']]),
 dict(name='stopped_peer_cleanup_only_with_active_tween',actions=[['play',0,1],['add'],['play',1,2],['settle'],['stop',1],['pause'],['resume'],['fadeto',0,-6,.3],['pause'],['flush']]),
 dict(name='resumed_tween_does_not_restart',actions=[['play',0,1],['settle'],['fadeto',0,-12,.2],['pause'],['volume',0,3],['resume'],['settle']]),
 dict(name='paused_fadeout_new_player_tween',actions=[['play',0,1],['settle'],['fadeout',0,.4],['pause'],['add'],['play',1,2],['fadein',1,6,.2],['half'],['finish'],['flush']]),
 dict(name='paused_fadeto_peer_tween',actions=[['play',0,1],['settle'],['fadeto',0,-12,.4],['pause'],['add'],['play',1,2],['fadein',1,6,.2],['half'],['finish'],['flush']]),
 dict(name='paused_fade_replaced_waiter',actions=[['play',0,1],['settle'],['fadeout',0,.2],['pause'],['fadein',0,6,.2],['half'],['finish'],['flush']]),
 dict(name='idempotent_pause_resume',actions=[['play',0,1],['settle'],['pause'],['pause'],['resume'],['resume']]),
]
PROBE='''extends SceneTree
var identities={}
var next_identity=0
func _init():call_deferred("run")
func identity(player):
    var key=player.get_instance_id()
    if not identities.has(key):
        identities[key]=next_identity
        next_identity+=1
    return identities[key]
func snapshot(manager):
    var voices=[]
    for player in manager.get_audio_player_list():
        voices.append({"id":identity(player),"stream":player.stream.resource_path if player.stream else "","playing":player.playing,"gain":player.volume_db,"paused":player.stream_paused,"queued":player.is_queued_for_deletion()})
    return voices
func run():
    var input=File.new()
    assert(input.open("res://input.json",File.READ)==OK)
    var config=JSON.parse(input.get_as_text()).result
    input.close()
    var streams=[]
    for settings in config.imports:
        var stream=load(settings.source)
        stream.loop=settings.loop
        stream.loop_offset=settings.offset
        streams.append(stream)
    var rows=[]
    for test in config.cases:
        identities={}
        next_identity=0
        var manager=load("res://manager.gd").new()
        manager.audioManager=manager
        var tween=Tween.new()
        tween.name="Tween"
        manager.add_child(tween)
        var players=Node.new()
        players.name="AudioPlayers"
        manager.add_child(players)
        get_root().add_child(manager)
        var views=[snapshot(manager)]
        var active=false
        for action in test.actions:
            tween.set_active(active)
            match action[0]:
                "pause":manager.pause_all_music()
                "resume":manager.resume_all_music()
                "add":manager.add_audio_player()
                "play":manager.play_music("",config.imports[int(action[2])-1].source.get_file(),int(action[1]))
                "stop":manager.stop_audio_player(int(action[1]))
                "volume":manager.set_audio_player_volume(int(action[1]),action[2])
                "remove":manager._remove_audio_player_obj(manager.get_audio_player(int(action[1])))
                "fadeout":manager.music_fadeout(int(action[1]),action[2])
                "fadeto":manager.music_fadeto(int(action[1]),action[2],action[3])
                "fadein":manager.music_fadein(int(action[1]),action[2],action[3])
                "half":tween.seek(tween.get_runtime()/2)
                "finish":
                    yield(tween,"tween_all_completed")
                "settle", "flush":
                    tween.set_active(false)
                    yield(self,"idle_frame")
                    yield(create_timer(.1),"timeout")
                    yield(self,"idle_frame")
            if not action[0] in ["settle","flush"]:active=tween.is_active()
            tween.set_active(false)
            views.append(snapshot(manager))
        rows.append({"case":test,"views":views})
        manager.free()
    var output=File.new()
    assert(output.open("res://../reference.json",File.WRITE)==OK)
    output.store_string(JSON.print({"schema":1,"godot":Engine.get_version_info(),"rows":rows},"  "))
    output.close()
    quit()
'''
def source():
 files,proof=base.source();ex=Extractor(ROOT);raw=ex.data('Scripts/global/audioManager.gd')
 methods,hashes=extract(raw,hashlib.sha256(raw).hexdigest(),['pause_all_music','resume_all_music'])
 files['manager.gd']+=methods;original={**proof['manager_methods'],**hashes}
 _,wrapped=extract(files['manager.gd'].encode(),hashlib.sha256(files['manager.gd'].encode()).hexdigest(),list(original))
 require(original==wrapped,'Original pause method closure changed')
 files['probe.gd']=PROBE;files['input.json']=json.dumps(dict(imports=proof['imports'],cases=CASES))
 proof=dict(source_commit=proof['source_commit'],whole_gameplay_approved=False,sources=proof['sources'],manager_methods=original,imports=proof['imports'],parameters=proof['parameters'],cases=CASES,generated_sha256={k:hashlib.sha256(v.encode()).hexdigest()for k,v in files.items()},mapped_implementation={'pause_snapshot_and_tween_seek':'runtime/music_players.cpp:pause_all','resume_existing_nodes':'runtime/music_players.cpp:resume_all','per_node_transport':'platform/ctr/music_region_player.cpp'},scope='Selected unchanged original audioManager methods; real stream_paused properties, Tween.seek/stop_all and deferred deletion. Tween active state restored only around explicit calls, frozen between observations; settled real AudioStreamPlayer resources with original import settings. No source-time audio position, bus filtering, nonzero seek, audible output or integrated game claim.')
 return files,proof
def validate_result(doc,proof=None):
 if proof is None:proof=source()[1]
 require(type(doc)is dict and set(doc)=={'schema','godot','rows'}and type(doc['schema'])is int and doc['schema']==1 and type(doc['godot'])is dict and doc['godot'].get('string')=='3.6.2-stable (official)'and type(doc['rows'])is list and len(doc['rows'])==len(CASES),'Music pause schema/count')
 paths={v['source']for v in proof['imports']}|{''}
 for row,case in zip(doc['rows'],CASES):
  require(type(row)is dict and set(row)=={'case','views'}and json.dumps(row['case'],sort_keys=True)==json.dumps(case,sort_keys=True)and type(row['views'])is list and len(row['views'])==len(case['actions'])+1,'Music pause case/order')
  for view in row['views']:
   require(type(view)is list and 1<=len(view)<=16,'Music pause voice count');ids=[]
   for v in view:
    require(type(v)is dict and set(v)=={'id','stream','playing','gain','paused','queued'}and type(v['id'])is int and 0<=v['id']<64 and v['id']not in ids and type(v['stream'])is str and v['stream']in paths and type(v['playing'])is bool and(not v['playing']or v['stream'])and type(v['paused'])is bool and type(v['queued'])is bool and type(v['gain'])in(int,float)and math.isfinite(v['gain'])and -120<=v['gain']<=24,'Music pause voice shape');ids.append(v['id'])
 return doc
def fixture(doc,proof=None):
 if proof is None:proof=source()[1]
 validate_result(doc,proof);ids={v['source']:i+1 for i,v in enumerate(proof['imports'])};ids['']=0;lines=['# original pause/resume: frozen Tween between explicit commands']
 for path,index in ids.items():
  if index:lines.append('# stream '+str(index)+' '+path)
 p=proof['parameters'];lines.append('# policy '+str(p['silence_db'])+' '+str(p['fade_to_seconds'])+' '+str(doc['rows'][0]['views'][0][0]['gain']))
 for index,row in enumerate(doc['rows']):
  lines.append('case '+str(index)+' '+row['case']['name'])
  for step,view in enumerate(row['views']):
   op=['initial']if step==0 else row['case']['actions'][step-1];lines.append('op '+' '.join(str(v)for v in op));values=[len(view)]
   for v in view:values.extend([v['id'],ids[v['stream']],int(v['playing']),v['gain'],int(v['paused']),int(v['queued'])])
   lines.append('state '+' '.join(str(v)for v in values))
 return '\n'.join(lines)+'\n'
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--out',type=Path,required=True);ap.add_argument('--godot',type=Path,default=ROOT/'build/godot-reference/Godot_v3.6.2-stable_win64.exe');a=ap.parse_args();out=a.out.resolve();require(out.is_relative_to(ROOT/'build')and out!=ROOT/'build'and not out.exists(),'Fresh owned music pause reference required');files,proof=source();require(proof==json.loads((ROOT/'compatibility/reviews/music-pause-v0410.json').read_bytes()),'Music pause source review changed');out.mkdir();project=out/'temporary';project.mkdir();(project/'.owned-pause').write_bytes(b'music-pause-v1')
 try:
  for name,value in files.items():(project/name).write_bytes(value.encode())
  ex=Extractor(ROOT)
  for settings in proof['imports']:
   name=settings['source'][6:];target=project/name;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(ex.data(name))
  engine=a.godot.resolve();invoke(engine,project,['--debug','--editor','--quit'],out/'import.log');invoke(engine,project,['--debug','--script',str(project/'probe.gd')],out/'godot.log');doc=validate_result(json.loads((out/'reference.json').read_bytes()),proof);(out/'pause.tsv').write_bytes(fixture(doc,proof).encode());receipt=dict(source=proof,producer_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),result_sha256=hashlib.sha256((out/'reference.json').read_bytes()).hexdigest(),engine_sha256=hashlib.sha256(engine.read_bytes()).hexdigest());(out/'receipt.json').write_bytes((json.dumps(receipt,indent=2)+'\n').encode());print('Original music pause cases:',len(doc['rows']))
 finally:
  if project.resolve().is_relative_to(out)and(project/'.owned-pause').read_bytes()==b'music-pause-v1':shutil.rmtree(project)
if __name__=='__main__':main()
