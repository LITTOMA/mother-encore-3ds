"""Run source door-music registration mutation in native Godot.

Only the source prefix before door sound, and unchanged stop_music with null
attached players, are executed. This is not a fade/playback or scene oracle.
"""
import argparse,hashlib,json,shutil,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.extract_battle_entry import Extractor,PIN,require
from tools.reference_movement import extract
from tools.podunk_field import Scene,Sources,load_recipe,owner_of
from tools.run_scene_reference import invoke
PROBE='''extends SceneTree
func _init():
    var results=[]
    for size in range(7):
        for mode in range(3):
            var manager=load("res://manager.gd").new()
            var transition=load("res://transition.gd").new()
            var world=load("res://global.gd").new()
            var scene=Node.new()
            get_root().add_child(scene)
            world.currentScene=scene
            get_root().add_child(world)
            get_root().add_child(manager)
            transition.global=world
            transition.audioManager=manager
            get_root().add_child(transition)
            var door=load("res://door.gd").new()
            if mode==1:door.targetScene=scene.name
            if mode==2:door.fadeout_music_on_scene_change=false
            get_root().add_child(door)
            for index in size:
                var region=load("res://region.gd").new()
                region.set_meta("source_id",index)
                region.audioManager=manager
                get_root().add_child(region)
                manager.musicChangers.append(region)
            transition.start_door_transition(door,null)
            var remaining=[]
            for region in manager.musicChangers:remaining.append(region.get_meta("source_id"))
            results.append({"count":size,"mode":mode,"remaining":remaining})
    var f=File.new()
    assert(f.open("res://../reference.json",File.WRITE)==OK)
    f.store_string(JSON.print({"schema":1,"godot":Engine.get_version_info(),"results":results},"  "))
    f.close()
    quit()
'''

def source():
    ex=Extractor(ROOT);path='Scripts/global/SceneTransition.gd';raw=ex.data(path);functions,hashes=extract(raw,hashlib.sha256(raw).hexdigest(),['start_door_transition']);prefix=functions[:functions.index('\n\tif door.sound')]
    prefix=prefix.replace('door: Door, player: PartyMemberPlayer','door, _player').replace('\r\n','\n')
    s=Scene(Sources(),load_recipe(ROOT/'content/mayor-field.json'));n=s.by_path['MusicArea'];script=owner_of(n,'script').sub_resource(n.props['script'])['script/source'];stop,stop_hashes=extract(script.encode(),hashlib.sha256(script.encode()).hexdigest(),['stop_music'])
    changer=ex.data('Nodes/Overworld/MusicChanger.tscn')
    proof=dict(commit=PIN,whole_handler_approved=False,source_sha256=ex.sources,methods_sha256=hashes,changer_script_sha256=hashlib.sha256(script.encode()).hexdigest(),stop_method_sha256=stop_hashes,prefix_sha256=hashlib.sha256(prefix.encode()).hexdigest())
    return prefix,stop,proof


def validate_review(review,proof):
    require(type(review)is dict and review.get('whole_handler_approved')is False and review==proof,'Door music semantic source review changed')


def validate_result(result):
    require(type(result)is dict and set(result)=={'schema','godot','results'}and type(result['schema'])is int and result['schema']==1 and type(result['godot'])is dict and result['godot'].get('string')=='3.6.2-stable (official)','Door music result schema/version')
    require(type(result['results'])is list and len(result['results'])==21,'Door music result count')
    for index,row in enumerate(result['results']):
        require(type(row)is dict and set(row)=={'count','mode','remaining'}and type(row['count'])is int and type(row['mode'])is int and row['count']==index//3 and row['mode']==index%3,'Door music result domain/order')
        require(type(row['remaining'])is list and all(type(v)is int and 0<=v<row['count']for v in row['remaining'])and row['remaining']==sorted(set(row['remaining'])),'Door music result identities')
    return result


def main():
    p=argparse.ArgumentParser();p.add_argument('--godot',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args();out=a.out.resolve()
    require(out.is_relative_to(ROOT/'build')and out!=ROOT/'build'and not out.exists(),'Fresh owned door music reference path required')
    prefix,stop,proof=source();review=json.loads((ROOT/'compatibility/reviews/door-music-stop-v0410.json').read_bytes());validate_review(review,proof)
    out.mkdir(parents=True);project=out/'temporary';project.mkdir();(project/'.owned-door-music-reference').write_text('native-source-loop-v1')
    try:
        files={'door.gd':'extends Node\nvar targetScene="Destination"\nvar fadeout_music_on_scene_change=true\nvar fadeout_music_length=0.8\n','manager.gd':'extends Node\nvar musicChangers=[]\n','global.gd':'extends Node\nvar currentScene\n','transition.gd':'extends Node\nvar global\nvar audioManager\n'+prefix+'\n','region.gd':'extends Node\nvar audioManager\nvar attached_player=null\nvar music=""\nvar loop=""\nvar diegetic=false\n'+stop,'project.godot':'config_version=4\n[application]\nconfig/name="Source door music array semantics"\n[logging]\nfile_logging/enable_logging=false\n','probe.gd':PROBE}
        for name,value in files.items():(project/name).write_bytes(value.encode())
        invoke(a.godot.resolve(),project,['--script',str(project/'probe.gd')],out/'godot.log')
        result=validate_result(json.loads((out/'reference.json').read_bytes()))
        receipt=dict(source=proof,engine_sha256=hashlib.sha256(a.godot.read_bytes()).hexdigest(),producer_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),generated_sha256={name:hashlib.sha256((project/name).read_bytes()).hexdigest()for name in files},adapters=['Door/player signature type annotations removed; unused player injected null; LF normalization','Global/currentScene and manager references injected as Nodes','Unattached source regions isolate registration mutation; metadata supplies stable test identity'],scope='Source array-loop registration outcomes only; no fade/playback or scene/hardware claim',result_sha256=hashlib.sha256((out/'reference.json').read_bytes()).hexdigest())
        (out/'receipt.json').write_bytes((json.dumps(receipt,indent=2)+'\n').encode());print('Door music reference:',len(result['results']),'native source outcomes')
    finally:
        if project.resolve().is_relative_to(out)and(project/'.owned-door-music-reference').read_text()=='native-source-loop-v1':shutil.rmtree(project)
if __name__=='__main__':main()
