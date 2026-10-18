"""Original start-zero track/loop and finished signal callback semantics."""
from pathlib import Path
import sys,json,hashlib,shutil,argparse,math
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import run_music_player_reference as base,audio_asset
from tools.extract_battle_entry import Extractor,require
from tools.run_scene_reference import invoke
CASES=[
 {'name':'one_shot','actions':[['play',0,1,0],['settle'],['emit',0],['flush']]},
 {'name':'intro_loop','actions':[['play',0,1,2],['settle'],['emit',0],['flush']]},
 {'name':'saved_index_missing','actions':[['add'],['play',1,1,2],['settle'],['remove',0],['flush'],['emit',0],['flush']]},
 {'name':'saved_index_now_other_node','actions':[['add'],['play',1,1,2],['settle'],['remove',0],['flush'],['add'],['emit',0],['flush']]},
 {'name':'stop_disconnects_loop','actions':[['play',0,1,2],['settle'],['stop',0],['emit',0],['flush']]},
 {'name':'stop_after_queued_signal','actions':[['play',0,1,2],['settle'],['emit',0],['stop',0],['flush']]},
 {'name':'replace_after_queued_signal','actions':[['play',0,1,2],['settle'],['emit',0],['play',0,3,3],['flush']]},
 {'name':'one_shot_then_loop_keeps_remove','actions':[['play',0,1,0],['play',0,3,3],['settle'],['emit',0],['flush']]},
 {'name':'queued_loop_then_remove','actions':[['play',0,1,2],['settle'],['emit',0],['remove',0],['flush']]},
 {'name':'nonbuiltin_loop_callback','actions':[['play',0,3,3],['settle'],['emit',0],['flush']]},
]
PROBE="""extends SceneTree
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
        var loop=""
        var index=-1
        var remove=false
        for connection in player.get_signal_connection_list("finished"):
            assert(connection.target==manager)
            if connection.method=="play_music":
                assert(connection.binds.size()==4 and connection.binds[0]=="" and connection.binds[3]==0)
                loop="res://Audio/Music/"+connection.binds[1]
                index=connection.binds[2]
            elif connection.method=="_remove_audio_player_obj":remove=true
            else:assert(false)
        voices.append({"id":identity(player),"stream":player.stream.resource_path if player.stream else "","playing":player.playing,"queued":player.is_queued_for_deletion(),"gain":player.volume_db,"loop":loop,"index":index,"remove":remove})
    return voices
func run():
    var input=File.new()
    assert(input.open("res://input.json",File.READ)==OK)
    var config=JSON.parse(input.get_as_text()).result
    input.close()
    var streams=[]
    var imported=[]
    for settings in config.imports:
        var stream=load(settings.source)
        stream.loop=settings.loop
        stream.loop_offset=settings.offset
        assert(stream.loop==settings.loop and abs(stream.loop_offset-settings.offset)<0.00001)
        streams.append(stream)
        imported.append({"source":stream.resource_path,"loop":stream.loop,"offset":stream.loop_offset})
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
        for action in test.actions:
            match action[0]:
                "play":
                    var track=config.imports[int(action[2])-1].source.trim_prefix("res://Audio/Music/")
                    var loop=config.imports[int(action[3])-1].source.trim_prefix("res://Audio/Music/") if action[3] else ""
                    manager.play_music(track,loop,int(action[1]))
                "add":manager.add_audio_player()
                "remove":manager._remove_audio_player_obj(manager.get_audio_player(int(action[1])))
                "stop":manager.stop_audio_player(int(action[1]))
                "emit":
                    var player=manager.get_audio_player(int(action[1]))
                    player.playing=false
                    player.emit_signal("finished")
                "settle", "flush":
                    yield(self,"idle_frame")
                    yield(create_timer(0.1),"timeout")
                    yield(self,"idle_frame")
            views.append(snapshot(manager))
        rows.append({"case":test,"views":views})
        manager.free()
    var output=File.new()
    assert(output.open("res://../reference.json",File.WRITE)==OK)
    output.store_string(JSON.print({"schema":1,"godot":Engine.get_version_info(),"rows":rows,"imports":imported},"  "))
    output.close()
    quit()
"""
def source():
 files,proof=base.source();ex=Extractor(ROOT);recipe=json.loads((ROOT/'content/native-audio.json').read_bytes());imports=[]
 for stable in [1001,34,4107189403]:
  row=next(a for a in recipe['assets']if a['stable_id']==stable);path=row['source_path'];raw=ex.data(path[6:]);settings=ex.data(path[6:]+'.import');require(hashlib.sha256(raw).hexdigest()==row['source_sha256']and hashlib.sha256(settings).hexdigest()==row['import_sha256'],'Reviewed original audio bytes changed');loop,offset=audio_asset.import_settings(settings,path);imports.append(dict(source=path,loop=loop,offset=offset))
 files={k:v for k,v in files.items()if k in ['manager.gd','project.godot']};files['probe.gd']=PROBE;files['input.json']=json.dumps(dict(imports=imports,cases=CASES));proof=dict(source_commit=proof['source_commit'],whole_gameplay_approved=False,sources={**proof['sources'],**ex.sources},manager_methods=proof['manager_methods'],parameters=proof['parameters'],cases=CASES,imports=imports,generated_sha256={k:hashlib.sha256(v.encode()).hexdigest()for k,v in files.items()},mapped_implementation={'start_zero_track_and_signal_binding':'runtime/music_players.cpp:play_track','finished_signal':'runtime/music_players.cpp:signal_finished','source_deferred_child_index':'runtime/music_players.cpp:dispatch_finished'},scope='Synthetic finished signals after explicitly setting playing=false; unchanged selected original manager methods and real signal bindings, MessageQueue/deferred callback and tree_exited behavior. Real AudioStreamPlayer resources receive checked original import settings and settle 0.1 seconds before observation. No natural end timing, audible output, nonzero seek, complete manager or live game integration claim.');return files,proof

def validate_result(doc,proof=None):
 if proof is None:proof=source()[1]
 require(type(doc)is dict and set(doc)=={'schema','godot','rows','imports'}and type(doc['schema'])is int and doc['schema']==1 and type(doc['godot'])is dict and doc['godot'].get('string')=='3.6.2-stable (official)'and type(doc['rows'])is list and len(doc['rows'])==len(CASES),'Music finished schema/count')
 require(type(doc['imports'])is list and len(doc['imports'])==len(proof['imports']),'Music finished imports')
 for actual,expected in zip(doc['imports'],proof['imports']):
  require(type(actual)is dict and set(actual)=={'source','loop','offset'}and actual['source']==expected['source']and type(actual['loop'])is bool and actual['loop']==expected['loop']and type(actual['offset'])in(int,float)and math.isfinite(actual['offset'])and abs(actual['offset']-expected['offset'])<.00001,'Music finished import settings changed')
 paths={p['source']for p in proof['imports']}|{''}
 for row,case in zip(doc['rows'],CASES):
  require(type(row)is dict and set(row)=={'case','views'}and type(row['case'])is dict and json.dumps(row['case'],sort_keys=True)==json.dumps(case,sort_keys=True)and type(row['views'])is list and len(row['views'])==len(case['actions'])+1,'Music finished case/order')
  for view in row['views']:
   require(type(view)is list and 1<=len(view)<=16,'Music finished snapshot shape');ids=[]
   for voice in view:
    require(type(voice)is dict and set(voice)=={'id','stream','playing','queued','gain','loop','index','remove'}and type(voice['id'])is int and 0<=voice['id']<64 and voice['id']not in ids and type(voice['stream'])is str and voice['stream']in paths and type(voice['loop'])is str and voice['loop']in paths and type(voice['playing'])is bool and (not voice['playing']or voice['stream'])and type(voice['queued'])is bool and type(voice['remove'])is bool and type(voice['gain'])in(int,float)and math.isfinite(voice['gain'])and -120<=voice['gain']<=24 and type(voice['index'])is int and (0<=voice['index']<16 if voice['loop']else voice['index']==-1),'Music finished player/callback shape');ids.append(voice['id'])
 return doc
def fixture(doc,proof=None):
 if proof is None:proof=source()[1]
 validate_result(doc,proof);ids={p['source']:i+1 for i,p in enumerate(proof['imports'])};ids['']=0;lines=['# original start-zero finished callbacks; explicit synthetic signals']
 for path,index in ids.items():
  if index:lines.append('# stream '+str(index)+' '+path)
 p=proof['parameters'];lines.append('# policy '+str(p['silence_db'])+' '+str(p['fade_to_seconds'])+' '+str(doc['rows'][0]['views'][0][0]['gain']))
 for index,row in enumerate(doc['rows']):
  lines.append('case '+str(index)+' '+row['case']['name'])
  for step,view in enumerate(row['views']):
   command=['initial']if step==0 else row['case']['actions'][step-1];lines.append('op '+' '.join(str(v)for v in command));values=[len(view)]
   for v in view:values.extend([v['id'],ids[v['stream']],int(v['playing']),int(v['queued']),v['gain'],ids[v['loop']],v['index'],int(v['remove'])])
   lines.append('state '+' '.join(str(v)for v in values))
 return '\n'.join(lines)+'\n'
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--out',type=Path,required=True);ap.add_argument('--godot',type=Path,default=ROOT/'build/godot-reference/Godot_v3.6.2-stable_win64.exe');a=ap.parse_args();out=a.out.resolve();require(out.is_relative_to(ROOT/'build')and out!=ROOT/'build'and not out.exists(),'Fresh owned music finished reference required');files,proof=source();require(proof==json.loads((ROOT/'compatibility/reviews/music-finished-v0410.json').read_bytes()),'Music finished source review changed');out.mkdir();project=out/'temporary';project.mkdir();(project/'.owned-finished').write_bytes(b'finished-reference-1')
 try:
  for name,value in files.items():(project/name).write_bytes(value.encode())
  ex=Extractor(ROOT)
  for settings in proof['imports']:
   name=settings['source'][6:];target=project/name;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(ex.data(name))
  engine=a.godot.resolve();invoke(engine,project,['--debug','--editor','--quit'],out/'import.log');invoke(engine,project,['--debug','--script',str(project/'probe.gd')],out/'godot.log');doc=validate_result(json.loads((out/'reference.json').read_bytes()),proof);(out/'finished.tsv').write_bytes(fixture(doc,proof).encode());receipt=dict(source=proof,producer_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),result_sha256=hashlib.sha256((out/'reference.json').read_bytes()).hexdigest(),engine_sha256=hashlib.sha256(engine.read_bytes()).hexdigest());(out/'receipt.json').write_bytes((json.dumps(receipt,indent=2)+'\n').encode());print('Original music finished sequences:',len(doc['rows']))
 finally:
  if project.resolve().is_relative_to(out)and(project/'.owned-finished').read_bytes()==b'finished-reference-1':shutil.rmtree(project)
if __name__=='__main__':main()
