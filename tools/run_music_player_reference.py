"""Original loop-only music player choices; no global bank lifetime claim."""
from pathlib import Path
import argparse,hashlib,json,math,re,shutil,sys
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import field_scene_music as music,podunk_music as podunk,audio_asset
from tools.podunk_field import Scene,Sources,load_recipe,owner_of
from tools.extract_battle_entry import Extractor,PIN,require
from tools.reference_movement import extract
from tools.run_scene_reference import invoke
CASES=[
 dict(name='empty_first',seed=[]),
 dict(name='different_latest_playing',seed=[['podunk',True,'podunk']]),
 dict(name='same_latest_playing',seed=[['mayor',True,'mayor']]),
 dict(name='different_latest_stopped',seed=[['mayor',True,'mayor'],['podunk',False,'podunk']]),
 dict(name='same_latest_stopped',seed=[['podunk',True,'podunk'],['mayor',False,'mayor']]),
 dict(name='different_registered_oldest_match',seed=[['mayor',False,'mayor'],['podunk',True,'podunk']],front='podunk'),
 dict(name='same_song_pending_fade',seed=[],prior='mayor'),
 dict(name='same_song_pending_fade_changed_gain',seed=[],prior='mayor',change_gain='podunk'),
 dict(name='same_latest_silent_playing',seed=[['mayor',True,'silence']]),
 dict(name='different_latest_silent_playing',seed=[['podunk',True,'silence']]),
 dict(name='declared_front_song_without_matching_stream',seed=[],front='mayor',replace_front='podunk'),
]
PROBE='''extends SceneTree
var identities={}
var next_identity=0
func _init():call_deferred("run")
func identity(player):
    if not is_instance_valid(player):return -1
    var key=player.get_instance_id()
    if not identities.has(key):
        identities[key]=next_identity
        next_identity+=1
    return identities[key]
func snapshot(manager,area):
    var voices=[]
    for player in manager.get_audio_player_list():
        voices.append({"id":identity(player),"stream":player.stream.resource_path if player.stream else "","playing":player.playing,"gain":player.volume_db})
    var registered=[]
    for changer in manager.musicChangers:registered.append(changer.get_meta("label"))
    return {"voices":voices,"registered":registered,"attached":identity(area.attached_player) if area else -1,"runtime":manager._tween.get_runtime()}
func area(config,label,manager,context,ui,flags,owner):
    var result=load("res://changer.gd").new()
    result.set_meta("label",label)
    result.global=context
    result.audioManager=manager
    result.uiManager=ui
    result.globaldata=flags
    for field in ["music","loop","volume_db","fadein_length","fadeout_length","disabled","appear_flag","disappear_flag"]:result.set(field,config[field])
    var shape=CollisionShape2D.new()
    var rectangle=RectangleShape2D.new()
    rectangle.extents=Vector2(1,1)
    shape.shape=rectangle
    result.add_child(shape)
    result.connect("tree_exiting",result,"_on_MusicArea_tree_exiting")
    owner.add_child(result)
    return result
func run():
    var file=File.new()
    var opened=file.open("res://input.json",File.READ)
    assert(opened==OK)
    var config=JSON.parse(file.get_as_text()).result
    file.close()
    # Fresh importer resources receive only the checked original loop settings;
    # hold strong references so unchanged load() calls reuse these resources.
    var streams=[]
    var imported=[]
    for settings in config.imports:
        var stream=load(settings.source)
        assert(stream is AudioStreamMP3 or stream is AudioStreamOGGVorbis)
        stream.loop=settings.loop
        stream.loop_offset=settings.offset
        assert(stream.loop==settings.loop and abs(stream.loop_offset-settings.offset)<0.00001)
        streams.append(stream)
        imported.append({"source":stream.resource_path,"loop":stream.loop,"offset":stream.loop_offset})
    var rows=[]
    for test in config.cases:
        identities={}
        next_identity=0
        var owner=Node.new()
        get_root().add_child(owner)
        var manager=load("res://manager.gd").new()
        manager.audioManager=manager
        var tween=Tween.new()
        tween.name="Tween"
        manager.add_child(tween)
        for label in ["AudioPlayers","Sfx"]:
            var child=Node.new()
            child.name=label
            manager.add_child(child)
        owner.add_child(manager)
        var context=load("res://context.gd").new()
        var player=load("res://player.gd").new()
        context.player=player
        var ui=load("res://ui.gd").new()
        var flags=load("res://flags.gd").new()
        for child in [context,player,ui,flags]:owner.add_child(child)
        var areas=Node.new()
        owner.add_child(areas)
        for index in test.seed.size():
            if index>0:manager.add_audio_player()
            var spec=test.seed[index]
            manager.play_music("",config.bindings[spec[0]].loop,index)
            manager.set_audio_player_volume(index,config.parameters.silence_db if spec[2]=="silence" else config.bindings[spec[2]].volume_db)
            if not spec[1]:manager.stop_audio_player(index)
        if test.has("front"):
            var first=area(config.bindings[test.front],"front",manager,context,ui,flags,areas)
            first._on_Area2D_body_entered(player)
            if test.has("replace_front"):manager.play_music("",config.bindings[test.replace_front].loop,manager.get_audio_player_index(first.attached_player))
        if test.has("prior"):
            var prior=area(config.bindings[test.prior],"prior",manager,context,ui,flags,areas)
            prior._on_Area2D_body_entered(player)
            prior.stop_music(prior.fadeout_length)
            prior.free()
            if test.has("change_gain"):manager.set_audio_player_volume(manager.get_latest_audio_player_index(),config.bindings[test.change_gain].volume_db)
        manager._tween.set_active(false)
        var before=snapshot(manager,null)
        var target=area(config.bindings.mayor,"target",manager,context,ui,flags,areas)
        target._on_Area2D_body_entered(player)
        manager._tween.set_active(false)
        var after=snapshot(manager,target)
        var half=after.duplicate(true)
        if test.has("prior"):
            manager._tween.seek(manager._tween.get_runtime()/2)
            half=snapshot(manager,target)
        rows.append({"case":test,"before":before,"after":after,"half":half})
        areas.free()
        owner.free()
    var output=File.new()
    var opened_output=output.open("res://../reference.json",File.WRITE)
    assert(opened_output==OK)
    output.store_string(JSON.print({"schema":1,"godot":Engine.get_version_info(),"rows":rows,"imports":imported,"parameters":config.parameters},"  "))
    output.close()
    quit()
'''
def source():
 ex=Extractor(ROOT);raw=ex.data('Scripts/global/audioManager.gd').decode();start=raw.index('var _sound_effects := {')
 names=['_ready','_get_audio_player_count','add_audio_player','get_audio_player','get_audio_player_from_song','get_audio_player_index','get_audio_player_list','_remove_audio_player_obj','_add_at_zero','get_latest_audio_player_index','play_music','music_fadeout','music_fadeout_obj','music_fadein','_music_fadein_obj','music_fadeto','set_audio_player_volume','_set_audio_player_volume_obj','stop_audio_player','_stop_audio_player_obj','_remove_all_unplaying','_remove_player_after_tween']
 methods,original=extract(raw.encode(),hashlib.sha256(raw.encode()).hexdigest(),names)
 manager=raw[:start]+'var audioManager\nvar musicChangers := []\n'+methods
 _,wrapped=extract(manager.encode(),hashlib.sha256(manager.encode()).hexdigest(),names);require(original==wrapped,'Candidate manager method changed')
 _,metadata,_,source_hashes=music.source();p=json.loads((ROOT/'content/podunk-music.json').read_bytes());podunk.checked(p,ex.upstream);primary=next(v for v in p['regions']if v['source_path']=='Music/MusicArea')
 bindings={}
 for label,data,row in [('podunk',p,primary),('mayor',metadata,metadata['regions'][0])]:
  track=next(v for v in data['tracks']if v['stable_id']==row['track_id']);require(not row['appear_flag']and not row['disappear_flag']and not row['disabled'],'Only enabled unconditional source areas in candidate')
  bindings[label]=dict(music='',loop=track['source_path'].split('/')[-1],volume_db=row['volume_db'],fadein_length=row['fadein_seconds'],fadeout_length=row['fadeout_seconds'],disabled=row['disabled'],appear_flag=row['appear_flag'],disappear_flag=row['disappear_flag'])
 scene=Scene(Sources(),load_recipe(ROOT/'content/mayor-field.json'));node=scene.by_path['MusicArea'];changer=owner_of(node,'script').sub_resource(node.props['script'])['script/source'];changer=changer.replace('extends Node2D','extends Node2D\nvar global\nvar globaldata\nvar uiManager\nvar audioManager',1)
 flags_raw=ex.data('Scripts/global/globalData.gd');flags,flag_hashes=extract(flags_raw,hashlib.sha256(flags_raw).hexdigest(),['check_appear_disappear_flags'])
 imports=[]
 for binding in bindings.values():
  path='res://Audio/Music/'+binding['loop'];ex.data(path[6:]);loop,offset=audio_asset.import_settings(ex.data(path[6:]+'.import'),path);imports.append(dict(source=path,loop=loop,offset=offset))
 parameters=dict(silence_db=metadata['silence_db'],fade_to_seconds=metadata['fade_to_seconds'],requested_gain_db=bindings['mayor']['volume_db'],fadein_seconds=bindings['mayor']['fadein_length'],fadeout_seconds=bindings['mayor']['fadeout_length'])
 files={'manager.gd':manager,'changer.gd':changer,'context.gd':'extends Node\nvar player\nfunc get_player():return player\n','player.gd':'extends Node2D\nfunc has_collisions():return true\n','ui.gd':'extends Node\nfunc is_in_cutscene():return false\nfunc is_in_battle():return false\n','flags.gd':'extends Node\nvar flags={}\n'+flags,'probe.gd':PROBE,'input.json':json.dumps(dict(cases=CASES,bindings=bindings,imports=imports,parameters=parameters)),'project.godot':'config_version=4\n[application]\nconfig/name="Candidate original music-player identities"\n[logging]\nfile_logging/enable_logging=false\n[rendering]\nquality/driver/driver_name="GLES2"\n'}
 proof=dict(source_commit=PIN,whole_gameplay_approved=False,sources={**source_hashes,**p['sources'],**ex.sources},manager_methods=original,flag_methods=flag_hashes,bindings=bindings,parameters=parameters,imports=imports,cases=CASES,generated_sha256={k:hashlib.sha256(v.encode()).hexdigest()for k,v in files.items()},mapped_implementation={'loop_only_player_choice':'runtime/music_player_choice.cpp:choose_music_loop','volume_property':'runtime/music_player_choice.cpp:sample_music_tween'},scope='Loop-only non-diegetic source MusicChanger choices over ordered AudioPlayers nodes, including stopped nodes and existing tweens; unchanged selected manager method closure; explicit singleton, player/UI/flags, area tree adapters, original loop/offset Resource cache settings, frozen Tween process and manual half-runtime seek; no full scene, geometry, bank ownership, NDSP, perceptual audio or global-runtime integration claim')
 return files,proof

def validate_result(doc,proof=None):
 if proof is None:proof=source()[1]
 require(type(doc)is dict and set(doc)=={'schema','godot','rows','imports','parameters'}and type(doc['schema'])is int and doc['schema']==1 and type(doc['godot'])is dict and doc['godot'].get('string')=='3.6.2-stable (official)'and type(doc['rows'])is list and len(doc['rows'])==len(CASES),'Music choice result schema/count')
 require(type(doc['parameters'])is dict and doc['parameters']==proof['parameters']and all(type(v)in(int,float)and math.isfinite(v)for v in doc['parameters'].values()),'Music choice source tuning changed')
 require(type(doc['imports'])is list and len(doc['imports'])==len(proof['imports']),'Music choice imported stream count')
 paths={v['source']for v in proof['imports']}
 for actual,expected in zip(doc['imports'],proof['imports']):
  require(type(actual)is dict and set(actual)=={'source','loop','offset'}and actual['source']==expected['source']and type(actual['loop'])is bool and actual['loop']==expected['loop']and type(actual['offset'])in(int,float)and math.isfinite(actual['offset'])and abs(actual['offset']-expected['offset'])<0.00001,'Music choice importer settings changed')
 for row,case in zip(doc['rows'],CASES):
  require(type(row)is dict and set(row)=={'case','before','after','half'}and row['case']==case and type(row['case'])is dict and all(type(v)is list and len(v)==3 and type(v[0])is str and type(v[1])is bool and type(v[2])is str for v in row['case']['seed']),'Music choice case/order')
  for label in ['before','after','half']:
   view=row[label];require(type(view)is dict and set(view)=={'voices','registered','attached','runtime'}and type(view['voices'])is list and 1<=len(view['voices'])<=16 and type(view['attached'])is int and type(view['runtime'])in(int,float)and math.isfinite(view['runtime'])and 0<=view['runtime']<=60,'Music choice snapshot shape')
   ids=[]
   for voice in view['voices']:
    require(type(voice)is dict and set(voice)=={'id','stream','playing','gain'}and type(voice['id'])is int and 0<=voice['id']<16 and voice['id']not in ids and type(voice['stream'])is str and voice['stream']in paths|{''}and type(voice['playing'])is bool and (not voice['playing']or voice['stream'])and type(voice['gain'])in(int,float)and math.isfinite(voice['gain'])and -120<=voice['gain']<=24,'Music choice voice identity/state');ids.append(voice['id'])
   require(view['attached']in ids+[-1]and type(view['registered'])is list and all(type(v)is str and v in ['front','target']for v in view['registered'])and len(set(view['registered']))==len(view['registered']),'Music choice registration/source attachment')
  expected_before=['front']if 'front'in case else[]
  require(row['before']['registered']==expected_before and row['after']['registered']==expected_before+['target']and row['half']['registered']==row['after']['registered'],'Music choice registration chronology changed')
  require(row['before']['attached']==-1 and row['after']['attached']>=0 and row['half']['attached']==row['after']['attached']and row['after']['registered'][-1:] == ['target'],'Music choice source attachment order')
 oldest=doc['rows'][5]['before']['voices'];require(len(oldest)==2 and oldest[0]['stream']==proof['imports'][1]['source']and not oldest[0]['playing']and oldest[1]['stream']==proof['imports'][0]['source']and oldest[1]['playing'],'Oldest-song fixture lost its source nodes')
 return doc

def fixture(doc,proof=None):
 if proof is None:proof=source()[1]
 validate_result(doc,proof);paths={v['source']:i+1 for i,v in enumerate(proof['imports'])};paths['']=0
 lines=['# original music player choices; node IDs differ from playback generations']
 for name,index in paths.items():
  if index:lines.append('# stream '+str(index)+' '+name)
 p=proof['parameters'];lines.append('# policy '+' '.join(str(p[k])for k in ['silence_db','fade_to_seconds','requested_gain_db','fadein_seconds','fadeout_seconds']))
 def players(view):
  return [v for voice in view['voices']for v in [voice['id'],paths[voice['stream']],int(voice['playing']),voice['gain']]]
 for index,row in enumerate(doc['rows']):
  before=row['before'];after=row['after'];case=row['case'];first=not before['registered'];front=2 if first or case.get('front')=='mayor'else 1
  values=[index,int(first),front,len(before['voices']),*players(before),len(after['voices']),*players(after),after['attached'],before['runtime'],after['runtime'],int('prior'in case)]
  half=next(v['gain']for v in row['half']['voices']if v['id']==after['attached']);values.append(half);lines.append('\t'.join(str(v)for v in values))
 return '\n'.join(lines)+'\n'

def main():
 ap=argparse.ArgumentParser();ap.add_argument('--out',type=Path,required=True);ap.add_argument('--godot',type=Path,default=ROOT/'build/godot-reference/Godot_v3.6.2-stable_win64.exe');args=ap.parse_args();out=args.out.resolve();require(out.is_relative_to(ROOT/'build')and out!=ROOT/'build'and not out.exists(),'Fresh owned music choice reference required')
 files,proof=source();require(proof==json.loads((ROOT/'compatibility/reviews/music-player-choice-v0410.json').read_bytes()),'Music player choice source review changed')
 out.mkdir();project=out/'temporary';project.mkdir();(project/'.owned-music-players').write_bytes(b'native-music-players-v1')
 try:
  for name,value in files.items():(project/name).write_bytes(value.encode())
  ex=Extractor(ROOT)
  for imported in proof['imports']:
   name=imported['source'][6:];target=project/name;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(ex.data(name))
  engine=args.godot.resolve();invoke(engine,project,['--debug','--editor','--quit'],out/'import.log');invoke(engine,project,['--debug','--script',str(project/'probe.gd')],out/'godot.log')
  result=validate_result(json.loads((out/'reference.json').read_bytes()),proof);(out/'choices.tsv').write_bytes(fixture(result,proof).encode())
  receipt=dict(source=proof,producer_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),result_sha256=hashlib.sha256((out/'reference.json').read_bytes()).hexdigest(),engine_sha256=hashlib.sha256(engine.read_bytes()).hexdigest())
  (out/'receipt.json').write_bytes((json.dumps(receipt,indent=2)+'\n').encode());print('Original music player choices:',len(result['rows']))
 finally:
  if project.resolve().is_relative_to(out)and(project/'.owned-music-players').read_bytes()==b'native-music-players-v1':shutil.rmtree(project)
if __name__=='__main__':main()
