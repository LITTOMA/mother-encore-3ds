"""Persistent loop-player state across source MusicChanger scene lifetimes."""
from pathlib import Path
import argparse,hashlib,json,math,shutil,sys
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import run_music_player_reference as choice,podunk_music,audio_asset,field_scene_music
from tools.extract_battle_entry import Extractor,require
from tools.run_scene_reference import invoke
CASES=[
 dict(name='podunk_to_mayor',actions=[['scene',1],['enter','Music/MusicArea'],['scene',2],['enter','MusicArea'],['finish'],['flush']]),
 dict(name='rapid_podunk_mayor_podunk',actions=[['scene',1],['enter','Music/MusicArea'],['scene',2],['enter','MusicArea'],['scene',1],['enter','Music/MusicArea'],['finish'],['flush']]),
 dict(name='stopped_latest_reused_across_banks',actions=[['scene',1],['enter','Music/MusicArea'],['settle'],['stop','Music/MusicArea',0],['scene',2],['enter','MusicArea']]),
 dict(name='shared_song_and_cancelled_exit',actions=[['scene',1],['enter','Music/MusicArea'],['enter','Music/MusicArea3'],['exit','Music/MusicArea'],['idle'],['exit','Music/MusicArea3'],['enter','Music/MusicArea3'],['idle'],['scene',2],['enter','MusicArea']]),
 dict(name='door_live_array_skip',actions=[['scene',1],['enter','Music/MusicArea'],['enter','Music/MusicArea3'],['enter','Music/MusicArea6'],['door',1],['scene',2],['enter','MusicArea']]),
 dict(name='global_external_latest_same_song',actions=[['scene',2],['enter','MusicArea'],['external',1,True,0],['scene',1],['enter','Music/MusicArea']]),
 dict(name='global_external_latest_stopped',actions=[['scene',2],['enter','MusicArea'],['external',1,False,0],['scene',1],['enter','Music/MusicArea']]),
 dict(name='ui_and_collision_callbacks',actions=[['scene',1],['context',False,False,False,True],['enter','Music/MusicArea'],['context',True,True,False,True],['enter','Music/MusicArea'],['context',True,False,True,True],['enter','Music/MusicArea'],['context',True,False,False,True],['enter','Music/MusicArea'],['context',True,True,False,True],['exit','Music/MusicArea'],['idle'],['context',True,False,False,False],['exit','Music/MusicArea'],['idle'],['context',True,False,True,True],['exit','Music/MusicArea'],['idle']]),
 dict(name='source_appear_disappear_flags',actions=[['scene',1],['enter','Music/MusicArea2'],['flag','canary_melody',True],['enter','Music/MusicArea2'],['enter','Music/MusicArea10'],['flag','zoo_freed',True],['enter','Music/MusicArea10'],['enter','Music/MusicArea11']]),
 dict(name='all_source_areas_exit_before_rebind',actions=[['scene',1],['enter','Music/MusicArea'],['enter','Music/MusicArea3'],['enter','Music/MusicArea5'],['enter','Music/MusicArea6'],['scene',2],['enter','MusicArea'],['finish'],['flush']]),
]
PROBE='''extends SceneTree
var identities={}
var next_identity=0
var tree_exits=[]
func _init():call_deferred("run")
func identity(player):
    if not is_instance_valid(player):return -1
    var key=player.get_instance_id()
    if not identities.has(key):
        identities[key]=next_identity
        next_identity+=1
    return identities[key]
func record_exit(path):tree_exits.append(path)
func snapshot(manager,areas):
    var voices=[]
    for player in manager.get_audio_player_list():
        voices.append({"id":identity(player),"stream":player.stream.resource_path if player.stream else "","playing":player.playing,"gain":player.volume_db,"queued":player.is_queued_for_deletion()})
    var registered=[]
    for changer in manager.musicChangers:registered.append(changer.get_meta("path"))
    var states=[]
    for path in areas:
        var area=areas[path]
        states.append({"path":path,"inside":area.player_inside,"attached":identity(area.attached_player),"registered":area in manager.musicChangers})
    return {"voices":voices,"registered":registered,"areas":states,"exits":tree_exits.duplicate()}
func make_area(config,manager,context,ui,flags,owner):
    var area=load("res://changer.gd").new()
    area.set_meta("path",config.path)
    area.global=context
    area.audioManager=manager
    area.uiManager=ui
    area.globaldata=flags
    for field in ["music","loop","volume_db","fadein_length","fadeout_length","disabled","appear_flag","disappear_flag"]:area.set(field,config[field])
    var shape=CollisionShape2D.new()
    var rectangle=RectangleShape2D.new()
    rectangle.extents=Vector2(1,1)
    shape.shape=rectangle
    area.add_child(shape)
    area.connect("tree_exiting",area,"_on_MusicArea_tree_exiting")
    area.connect("tree_exiting",self,"record_exit",[config.path])
    owner.add_child(area)
    return area
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
        tree_exits=[]
        var owner=Node.new()
        get_root().add_child(owner)
        var manager=load("res://manager.gd").new()
        manager.audioManager=manager
        var tween=Tween.new()
        tween.name="Tween"
        manager.add_child(tween)
        var players=Node.new()
        players.name="AudioPlayers"
        manager.add_child(players)
        owner.add_child(manager)
        var context=load("res://context.gd").new()
        var player=load("res://player.gd").new()
        context.player=player
        var ui=load("res://ui.gd").new()
        var flags=load("res://flags.gd").new()
        var other=Node2D.new()
        for child in [context,player,ui,flags,other]:owner.add_child(child)
        var body=player
        var scene=null
        var areas={}
        var views=[snapshot(manager,areas)]
        for action in test.actions:
            tree_exits=[]
            match action[0]:
                "scene":
                    if scene:scene.free()
                    areas={}
                    scene=Node.new()
                    owner.add_child(scene)
                    for binding in config.scenes[int(action[1])-1]:areas[binding.path]=make_area(binding,manager,context,ui,flags,scene)
                "enter":areas[action[1]]._on_Area2D_body_entered(body)
                "exit":areas[action[1]]._on_MusicArea_body_exited(body)
                "stop":areas[action[1]].stop_music(action[2])
                "door":
                    for changer in manager.musicChangers:changer.stop_music(action[1])
                "flag":flags.flags[action[1]]=action[2]
                "context":
                    body=player if action[1] else other
                    ui.cutscene=action[2]
                    ui.battle=action[3]
                    player.collisions=action[4]
                "external":
                    manager.add_audio_player()
                    var index=manager.get_latest_audio_player_index()
                    manager.play_music("",config.imports[int(action[1])-1].source.get_file(),index)
                    manager.set_audio_player_volume(index,action[3])
                    tween.set_active(false)
                    yield(self,"idle_frame")
                    yield(self,"idle_frame")
                    if not action[2]:manager.stop_audio_player(index)
                "settle", "flush":
                    yield(self,"idle_frame")
                    yield(self,"idle_frame")
                "idle":yield(self,"idle_frame")
                "finish":
                    tween.set_active(true)
                    yield(tween,"tween_all_completed")
            tween.set_active(false)
            views.append(snapshot(manager,areas))
        rows.append({"case":test,"views":views})
        scene.free()
        owner.free()
    var output=File.new()
    assert(output.open("res://../reference.json",File.WRITE)==OK)
    output.store_string(JSON.print({"schema":1,"godot":Engine.get_version_info(),"rows":rows},"  "))
    output.close()
    quit()
'''
def source():
 files,base=choice.source();ex=Extractor(ROOT);podunk=json.loads((ROOT/'content/podunk-music.json').read_bytes());podunk_music.checked(podunk,ex.upstream);_,mayor,_,_=field_scene_music.source()
 scenes=[];imports=[]
 for metadata in [podunk,mayor]:
  tracks={t['stable_id']:t for t in metadata['tracks']};scenes.append([])
  for track in tracks.values():
   path=track['source_path'];ex.data(path[6:]);loop,offset=audio_asset.import_settings(ex.data(path[6:]+'.import'),path);require(loop,'Scene graph only admits checked loop streams');imports.append(dict(source=path,loop=loop,offset=offset))
  for row in metadata['regions']:
   scenes[-1].append(dict(path=row['source_path'],music='',loop=tracks[row['track_id']]['source_path'].split('/')[-1],volume_db=row['volume_db'],fadein_length=row['fadein_seconds'],fadeout_length=row['fadeout_seconds'],disabled=row['disabled'],appear_flag=row['appear_flag'],disappear_flag=row['disappear_flag']))
 files['probe.gd']=PROBE;files['input.json']=json.dumps(dict(imports=imports,scenes=scenes,cases=CASES));files['ui.gd']='extends Node\nvar cutscene=false\nvar battle=false\nfunc is_in_cutscene():return cutscene\nfunc is_in_battle():return battle\n';files['player.gd']='extends Node2D\nvar collisions=true\nfunc has_collisions():return collisions\n'
 proof=dict(source_commit=base['source_commit'],whole_gameplay_approved=False,sources={**base['sources'],**ex.sources},manager_methods=base['manager_methods'],flag_methods=base['flag_methods'],imports=imports,scenes=scenes,parameters=base['parameters'],cases=CASES,generated_sha256={k:hashlib.sha256(v.encode()).hexdigest()for k,v in files.items()},mapped_implementation={'scene_callbacks_and_persistent_players':'runtime/music_scene_graph.cpp','players':'runtime/music_players.cpp'},scope='Original MusicChanger callbacks and selected AudioManager methods over checked Podunk/Mayor loop bindings. Explicit player/UI/flag adapters, geometry-free manual callbacks, frozen Tweens, real ordered tree_exiting signals and event-driven shared completion; no full scene geometry, non-loop/diegetic support, physical bank routing, NDSP or live integration claim.')
 return files,proof
def validate_result(doc,proof=None):
 if proof is None:proof=source()[1]
 require(type(doc)is dict and set(doc)=={'schema','godot','rows'}and type(doc['schema'])is int and doc['schema']==1 and type(doc['godot'])is dict and doc['godot'].get('string')=='3.6.2-stable (official)'and type(doc['rows'])is list and len(doc['rows'])==len(CASES),'Music scene result schema/count')
 paths={p['source']for p in proof['imports']}|{''};area_paths={r['path']for scene in proof['scenes']for r in scene}
 for row,case in zip(doc['rows'],CASES):
  require(type(row)is dict and set(row)=={'case','views'}and type(row['case'])is dict and json.dumps(row['case'],sort_keys=True)==json.dumps(case,sort_keys=True)and type(row['views'])is list and len(row['views'])==len(case['actions'])+1,'Music scene case/order')
  for view in row['views']:
   require(type(view)is dict and set(view)=={'voices','areas','registered','exits'}and type(view['voices'])is list and 1<=len(view['voices'])<=16 and type(view['areas'])is list and len(view['areas'])<=64,'Music scene view shape')
   ids=[]
   for voice in view['voices']:
    require(type(voice)is dict and set(voice)=={'id','stream','playing','gain','queued'}and type(voice['id'])is int and 0<=voice['id']<64 and voice['id']not in ids and type(voice['stream'])is str and voice['stream']in paths and type(voice['playing'])is bool and (not voice['playing']or voice['stream'])and type(voice['queued'])is bool and type(voice['gain'])in(int,float)and math.isfinite(voice['gain'])and -120<=voice['gain']<=24,'Music scene voice shape');ids.append(voice['id'])
   names=[]
   for area in view['areas']:
    require(type(area)is dict and set(area)=={'path','inside','attached','registered'}and type(area['path'])is str and area['path']in area_paths and area['path']not in names and type(area['inside'])is bool and type(area['registered'])is bool and type(area['attached'])is int and area['attached']in ids+[-1],'Music scene area state');names.append(area['path'])
   require(type(view['registered'])is list and len(set(view['registered']))==len(view['registered'])and all(type(v)is str and v in names for v in view['registered'])and all(a['registered']==(a['path']in view['registered'])for a in view['areas']),'Music scene registration order/state')
   require(type(view['exits'])is list and len(view['exits'])<=64 and len(set(view['exits']))==len(view['exits'])and all(type(v)is str and v in area_paths for v in view['exits']),'Music scene tree-exit list')
 return doc
def fixture(doc,proof=None):
 if proof is None:proof=source()[1]
 validate_result(doc,proof);ids={p['source']:i+1 for i,p in enumerate(proof['imports'])};ids['']=0
 lines=['# original scene callbacks over persistent loop players']
 for path,index in ids.items():
  if index:lines.append('# stream '+str(index)+' '+path)
 p=proof['parameters'];lines.append('# policy '+str(p['silence_db'])+' '+str(p['fade_to_seconds'])+' '+str(doc['rows'][0]['views'][0]['voices'][0]['gain']))
 for index,row in enumerate(doc['rows']):
  lines.append('case '+str(index)+' '+row['case']['name'])
  for step,view in enumerate(row['views']):
   command=['initial']if step==0 else row['case']['actions'][step-1];lines.append('op '+' '.join(str(int(v))if type(v)is bool else str(v)for v in command))
   lines.append(('exits '+str(len(view['exits']))+' '+' '.join(view['exits'])).rstrip())
   values=[len(view['voices'])]
   for v in view['voices']:values.extend([v['id'],ids[v['stream']],int(v['playing']),v['gain'],int(v['queued'])])
   lines.append('voices '+' '.join(str(v)for v in values));lines.append(('registered '+str(len(view['registered']))+' '+' '.join(view['registered'])).rstrip())
   values=[len(view['areas'])]
   for a in view['areas']:values.extend([a['path'],int(a['inside']),int(a['registered']),a['attached']])
   lines.append('areas '+' '.join(str(v)for v in values))
 return '\n'.join(lines)+'\n'
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--out',type=Path,required=True);ap.add_argument('--godot',type=Path,default=ROOT/'build/godot-reference/Godot_v3.6.2-stable_win64.exe');args=ap.parse_args();out=args.out.resolve();require(out.is_relative_to(ROOT/'build')and out!=ROOT/'build'and not out.exists(),'Fresh owned music scene reference required')
 files,proof=source();require(proof==json.loads((ROOT/'compatibility/reviews/music-scene-graph-v0410.json').read_bytes()),'Music scene source review changed');out.mkdir();project=out/'temporary';project.mkdir();(project/'.owned-music-scene').write_bytes(b'native-music-scene-v1')
 try:
  for name,value in files.items():(project/name).write_bytes(value.encode())
  ex=Extractor(ROOT)
  for imported in proof['imports']:
   name=imported['source'][6:];target=project/name;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(ex.data(name))
  engine=args.godot.resolve();invoke(engine,project,['--debug','--editor','--quit'],out/'import.log');invoke(engine,project,['--debug','--script',str(project/'probe.gd')],out/'godot.log');doc=validate_result(json.loads((out/'reference.json').read_bytes()),proof);(out/'scenes.tsv').write_bytes(fixture(doc,proof).encode());receipt=dict(source=proof,producer_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),result_sha256=hashlib.sha256((out/'reference.json').read_bytes()).hexdigest(),engine_sha256=hashlib.sha256(engine.read_bytes()).hexdigest());(out/'receipt.json').write_bytes((json.dumps(receipt,indent=2)+'\n').encode());print('Original music scene sequences:',len(doc['rows']))
 finally:
  if project.resolve().is_relative_to(out)and(project/'.owned-music-scene').read_bytes()==b'native-music-scene-v1':shutil.rmtree(project)
if __name__=='__main__':main()
