"""Original loop-player deletion and shared Tween completion boundaries."""
from pathlib import Path
import argparse,hashlib,json,math,shutil,sys
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import run_music_player_reference as choice
from tools.extract_battle_entry import Extractor,require
from tools.reference_movement import extract
from tools.run_scene_reference import invoke
CASES=[
 dict(name='remove_last_recreates_empty',actions=[['remove',0],['flush']]),
 dict(name='remove_two_in_order',actions=[['play',0,1],['add'],['play',1,2],['remove',1],['flush'],['remove',0],['flush']]),
 dict(name='clear_does_not_recreate',actions=[['play',0,1],['clear'],['flush']]),
 dict(name='clear_then_add_before_deletion',actions=[['play',0,1],['clear'],['add'],['play',1,2],['flush']]),
 dict(name='queued_player_remains_addressable',actions=[['play',0,1],['remove',0],['play',0,2],['volume',0,6],['flush']]),
 dict(name='stop_retains_stream_and_tween',actions=[['play',0,2],['settle'],['volume',0,6],['fadeout',0,0.1],['stop',0],['finish'],['flush']]),
 dict(name='volume_write_retains_tween',actions=[['play',0,2],['volume',0,6],['fadeout',0,0.1],['volume',0,0],['finish'],['flush']]),
 dict(name='equal_fadeto_retains_fadeout',actions=[['play',0,2],['volume',0,6],['fadeout',0,0.1],['fadeto',0,6,0.2],['finish'],['flush']]),
 dict(name='changed_fadeto_replaces_fadeout',actions=[['play',0,2],['volume',0,6],['fadeout',0,0.1],['volume',0,0],['fadeto',0,6,0.2],['finish'],['flush']]),
 dict(name='fadeout_waits_for_other_player',actions=[['play',0,1],['add'],['play',1,2],['volume',1,-6],['fadeout',0,0.1],['fadeto',1,6,0.3],['first_complete'],['finish'],['flush']]),
 dict(name='stopped_other_player_is_cleaned',actions=[['play',0,1],['add'],['play',1,2],['settle'],['stop',1],['fadeout',0,0.1],['finish'],['flush']]),
 dict(name='remove_twice_and_clear',actions=[['remove',0],['remove',0],['clear'],['flush']]),
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
func freeze(tween):tween.set_active(false)
func snapshot(manager,waiters):
    var voices=[]
    for player in manager.get_audio_player_list():
        voices.append({"id":identity(player),"stream":player.stream.resource_path if player.stream else "","playing":player.playing,"gain":player.volume_db,"queued":player.is_queued_for_deletion()})
    var waiting=0
    for waiter in waiters:
        if waiter is GDScriptFunctionState and waiter.is_valid():waiting+=1
    return {"voices":voices,"waiting":waiting,"elapsed":manager._tween.tell()}
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
        var waiters=[]
        var views=[snapshot(manager,waiters)]
        for action in test.actions:
            match action[0]:
                "add":manager.add_audio_player()
                "play":manager.play_music("",config.imports[int(action[2])-1].source.get_file(),int(action[1]))
                "volume":manager.set_audio_player_volume(int(action[1]),action[2])
                "stop":manager.stop_audio_player(int(action[1]))
                "fadeout":waiters.append(manager.music_fadeout_obj(manager.get_audio_player(int(action[1])),action[2]))
                "fadeto":manager.music_fadeto(int(action[1]),action[2],action[3])
                "remove":manager._remove_audio_player_obj(manager.get_audio_player(int(action[1])))
                "clear":manager.clear_all_music()
                "flush", "settle":
                    yield(self,"idle_frame")
                    yield(self,"idle_frame")
                "first_complete":
                    tween.set_active(true)
                    yield(tween,"tween_completed")
                    call_deferred("freeze",tween)
                    yield(self,"idle_frame")
                "finish":
                    tween.set_active(true)
                    yield(tween,"tween_all_completed")
            tween.set_active(false)
            views.append(snapshot(manager,waiters))
        rows.append({"case":test,"views":views})
        manager.free()
    var output=File.new()
    assert(output.open("res://../reference.json",File.WRITE)==OK)
    output.store_string(JSON.print({"schema":1,"godot":Engine.get_version_info(),"rows":rows},"  "))
    output.close()
    quit()
'''
def source():
 files,base=choice.source();ex=Extractor(ROOT);raw=ex.data('Scripts/global/audioManager.gd')
 extra,hashes=extract(raw,hashlib.sha256(raw).hexdigest(),['clear_all_music'])
 files['manager.gd']+=extra
 original={**base['manager_methods'],**hashes}
 _,wrapped=extract(files['manager.gd'].encode(),hashlib.sha256(files['manager.gd'].encode()).hexdigest(),list(original))
 require(original==wrapped,'Original lifetime method closure changed')
 files['probe.gd']=PROBE;files['input.json']=json.dumps(dict(imports=base['imports'],cases=CASES))
 # No MusicChanger is instantiated in this manager-only lifetime probe.
 for name in ['changer.gd','context.gd','player.gd','ui.gd','flags.gd']:del files[name]
 proof=dict(source_commit=base['source_commit'],whole_gameplay_approved=False,sources=base['sources'],manager_methods=original,imports=base['imports'],parameters=base['parameters'],cases=CASES,generated_sha256={k:hashlib.sha256(v.encode()).hexdigest()for k,v in files.items()},mapped_implementation={'loop_node_state':'runtime/music_players.cpp','choice':'runtime/music_player_choice.cpp'},scope='Unchanged selected AudioManager methods with real AudioStreamPlayer and Tween nodes. Ordered queue_free/tree_exited deletion, stopped streams, volume writes, replaced/preserved tweens, and shared tween_all_completed cleanup; explicit idle deletion boundaries and event-driven tween completion. Stops are tested after playback has settled; same-frame backend start/stop scheduling is outside this property model. No full music manager, finished callbacks, bus/pause behavior, physical bank ownership, NDSP, perceptual audio or live scene integration claim.')
 return files,proof
def validate_result(doc,proof=None):
 if proof is None:proof=source()[1]
 require(type(doc)is dict and set(doc)=={'schema','godot','rows'}and type(doc['schema'])is int and doc['schema']==1 and type(doc['godot'])is dict and doc['godot'].get('string')=='3.6.2-stable (official)'and type(doc['rows'])is list and len(doc['rows'])==len(CASES),'Music lifetime result schema/count')
 paths={p['source']for p in proof['imports']}|{''}
 for row,case in zip(doc['rows'],CASES):
  require(type(row)is dict and set(row)=={'case','views'}and type(row['case'])is dict and json.dumps(row['case'],sort_keys=True)==json.dumps(case,sort_keys=True)and type(row['views'])is list and len(row['views'])==len(case['actions'])+1,'Music lifetime case/order')
  for view in row['views']:
   require(type(view)is dict and set(view)=={'voices','waiting','elapsed'}and type(view['elapsed'])in(int,float)and math.isfinite(view['elapsed'])and 0<=view['elapsed']<=61 and type(view['voices'])is list and len(view['voices'])<=16 and type(view['waiting'])is int and 0<=view['waiting']<=16,'Music lifetime snapshot shape')
   ids=[]
   for voice in view['voices']:
    require(type(voice)is dict and set(voice)=={'id','stream','playing','gain','queued'}and type(voice['id'])is int and 0<=voice['id']<64 and voice['id']not in ids and type(voice['stream'])is str and voice['stream']in paths and type(voice['playing'])is bool and (not voice['playing']or voice['stream'])and type(voice['queued'])is bool and type(voice['gain'])in(int,float)and math.isfinite(voice['gain'])and -120<=voice['gain']<=24,'Music lifetime voice shape');ids.append(voice['id'])
 return doc
def fixture(doc,proof=None):
 if proof is None:proof=source()[1]
 validate_result(doc,proof);ids={p['source']:i+1 for i,p in enumerate(proof['imports'])};ids['']=0
 lines=['# original loop-player lifecycle: snapshots after explicit commands']
 for path,index in ids.items():
  if index:lines.append('# stream '+str(index)+' '+path)
 p=proof['parameters'];lines.append('# policy '+str(p['silence_db'])+' '+str(p['fade_to_seconds'])+' '+str(doc['rows'][0]['views'][0]['voices'][0]['gain']))
 for index,row in enumerate(doc['rows']):
  lines.append('case '+str(index)+' '+row['case']['name'])
  for step,view in enumerate(row['views']):
   command=['initial']if step==0 else row['case']['actions'][step-1]
   lines.append(' '.join(str(v)for v in ['op',*command]))
   values=[len(view['voices']),view['waiting'],view['elapsed']]
   for voice in view['voices']:values.extend([voice['id'],ids[voice['stream']],int(voice['playing']),voice['gain'],int(voice['queued'])])
   lines.append('state '+' '.join(str(v)for v in values))
 return '\n'.join(lines)+'\n'
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--out',type=Path,required=True);ap.add_argument('--godot',type=Path,default=ROOT/'build/godot-reference/Godot_v3.6.2-stable_win64.exe');args=ap.parse_args();out=args.out.resolve();require(out.is_relative_to(ROOT/'build')and out!=ROOT/'build'and not out.exists(),'Fresh owned music lifetime reference required')
 files,proof=source();require(proof==json.loads((ROOT/'compatibility/reviews/music-player-lifetime-v0410.json').read_bytes()),'Music lifetime source review changed')
 out.mkdir();project=out/'temporary';project.mkdir();(project/'.owned-music-lifetime').write_bytes(b'native-music-lifetime-v1')
 try:
  for name,value in files.items():(project/name).write_bytes(value.encode())
  ex=Extractor(ROOT)
  for imported in proof['imports']:
   name=imported['source'][6:];target=project/name;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(ex.data(name))
  engine=args.godot.resolve();invoke(engine,project,['--debug','--editor','--quit'],out/'import.log');invoke(engine,project,['--debug','--script',str(project/'probe.gd')],out/'godot.log')
  doc=validate_result(json.loads((out/'reference.json').read_bytes()),proof);(out/'lifetime.tsv').write_bytes(fixture(doc,proof).encode())
  receipt=dict(source=proof,producer_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),result_sha256=hashlib.sha256((out/'reference.json').read_bytes()).hexdigest(),engine_sha256=hashlib.sha256(engine.read_bytes()).hexdigest());(out/'receipt.json').write_bytes((json.dumps(receipt,indent=2)+'\n').encode());print('Original music lifetime cases:',len(doc['rows']))
 finally:
  if project.resolve().is_relative_to(out)and(project/'.owned-music-lifetime').read_bytes()==b'native-music-lifetime-v1':shutil.rmtree(project)
if __name__=='__main__':main()
