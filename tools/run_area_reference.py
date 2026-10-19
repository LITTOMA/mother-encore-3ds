"""Native source AreaRoom Ready and map-position probe; injected services only."""
import argparse,hashlib,json,math,re,shutil,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.extract_battle_entry import Extractor,require,PIN
from tools.reference_movement import extract
from tools.run_scene_reference import invoke
from tools.field_area import source as area_source,policy
PROBE='''extends SceneTree
func _init():call_deferred("run")
func run():
    var results=[]
    var positions=[Vector2(8,-63),Vector2(8,-519),Vector2(-216,-600),Vector2.ZERO]
    for sub in [false,true]:
        for old in [false,true]:
            for pos in positions:
                var area=load("res://area.gd").new()
                var data=load("res://data.gd").new()
                var world=load("res://world.gd").new()
                var screen=load("res://screen.gd").new()
                var player=Node2D.new()
                world.player=player
                world.currentScene=area
                player.position=pos
                area.global=world
                area.globaldata=data
                area._is_sub_area=sub
                data.flags["visited_podunk"]=old
                screen.global=world
                data.global=world
                world.connect("flags_updated",world,"record_flag_signal")
                area._ready()
                var point=screen._get_player_position()+screen._get_map_size()/2
                results.append({"sub":sub,"old":old,"position":[pos.x,pos.y],"map":[point.x,point.y],"visited":data.flags["visited_podunk"],"signals":world.flag_signals,"npc_party":world.partyNpcs.size()})
                area.free()
                data.free()
                world.free()
                screen.free()
                player.free()
    var file=File.new()
    assert(file.open("res://../reference.json",File.WRITE)==OK)
    file.store_string(JSON.print({"schema":1,"godot":Engine.get_version_info(),"results":results},"  "))
    file.close()
    quit()
'''

def source():
    p=policy();a,sources=area_source();ex=Extractor(ROOT);raw=ex.data(p['script']);methods,hashes=extract(raw,hashlib.sha256(raw).hexdigest(),['_ready','get_player_map_offset','is_sub_area','_update_visit_flags','_update_flying_man_status'])
    code=raw.decode();table=re.search(r'^const REGION_VISIT_FLAGS := (\{.*?^\})',code,re.M|re.S);require(table,'Area visit table missing')
    map_raw=ex.data(p['map_screen']);map_methods,map_hashes=extract(map_raw,hashlib.sha256(map_raw).hexdigest(),['_get_player_position','_is_in_sub_area'])
    setter_raw=ex.data(p['global_data']);setter,setter_hashes=extract(setter_raw,hashlib.sha256(setter_raw).hexdigest(),['set_flag']);sources[p['global_data']]=ex.sources[p['global_data']]
    proof=dict(flag_setter=setter_hashes,commit=PIN,whole_scene_approved=False,sources=sources,area_methods=hashes,map_methods=map_hashes,visit_table_sha256=hashlib.sha256(table[0].encode()).hexdigest())
    area='extends Node2D\nvar global\nvar globaldata\nvar _region_name:='+json.dumps(a['region'])+'\nvar _player_map_offset:=Vector2(%s,%s)\nvar _is_sub_area:=false\n'%tuple(a['map_offset'])+table[0]+'\n'+methods
    screen='extends Node\nconst AreaRoom=preload("res://area.gd")\nconst DungeonAreaRoom=preload("res://dungeon.gd")\nconst DungeonMapRoomManager=preload("res://dungeonmap.gd")\nvar global\nvar _loaded_map\nfunc _is_current_scene():return true\nfunc _get_map_size():return Vector2(768,512)\n'+map_methods
    return area,screen,setter,proof

def validate_result(r):
    require(type(r)is dict and set(r)=={'schema','godot','results'}and type(r['schema'])is int and r['schema']==1 and type(r['godot'])is dict and r['godot'].get('string')=='3.6.2-stable (official)','Area reference result version/schema')
    require(type(r['results'])is list and len(r['results'])==16,'Area reference result count')
    positions=[[8,-63],[8,-519],[-216,-600],[0,0]]
    for i,row in enumerate(r['results']):
        require(type(row)is dict and set(row)==set('sub old position map visited signals npc_party'.split()),'Area reference row fields')
        require(type(row['sub'])is bool and row['sub']==bool(i//8)and type(row['old'])is bool and row['old']==bool((i//4)%2)and type(row['position'])is list and len(row['position'])==2 and all(type(x)in(int,float)and math.isfinite(x)for x in row['position'])and row['position']==positions[i%4],'Area reference case/order')
        require(type(row['map'])is list and len(row['map'])==2 and all(type(x)in(int,float)and math.isfinite(x)for x in row['map'])and type(row['visited'])is bool and type(row['signals'])is int and type(row['npc_party'])is int,'Area reference result types')
    return r

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--godot',type=Path,required=True);ap.add_argument('--out',type=Path,required=True);a=ap.parse_args();out=a.out.resolve();require(out.is_relative_to(ROOT/'build')and out!=ROOT/'build'and not out.exists(),'Fresh Area reference path required')
    area,screen,setter,proof=source();review=json.loads((ROOT/'compatibility/reviews/field-area-v0410.json').read_bytes());require(type(review)is dict and review.get('whole_scene_approved')is False and review==proof,'Area semantic review changed')
    out.mkdir(parents=True);project=out/'temporary';project.mkdir();(project/'.owned-area-reference').write_text('source-area-v1')
    files={'area.gd':area,'screen.gd':screen,'world.gd':'extends Node\nsignal flags_updated\nvar flag_signals=0\nfunc record_flag_signal():flag_signals+=1\nvar currentScene\nvar player\nvar partyNpcs=[]\nfunc get_player():return player\n','data.gd':'extends Node\nvar global\nvar flags={"visited_podunk":false,"flying_man_in_party":false}\nvar characters={"flyingman":"flyingman"}\n'+setter,'dungeon.gd':'extends Node2D\n','dungeonmap.gd':'extends Node\n','probe.gd':PROBE,'project.godot':'config_version=4\n[application]\nconfig/name="Source AreaRoom Ready and map position"\n[logging]\nfile_logging/enable_logging=false\n'}
    try:
        for name,value in files.items():(project/name).write_bytes(value.encode())
        invoke(a.godot.resolve(),project,['--video-driver','GLES2','--script',str(project/'probe.gd')],out/'godot.log');result=validate_result(json.loads((out/'reference.json').read_bytes()))
        receipt=dict(source=proof,engine_sha256=hashlib.sha256(a.godot.read_bytes()).hexdigest(),producer_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),generated_sha256={name:hashlib.sha256((project/name).read_bytes()).hexdigest()for name in files},adapters=['Inject original Mayor area property values and non-Magicant empty NPC party','Manual isolated Ready invocation; unchanged source globalData.set_flag emits native flags_updated to an injected counter, no complete signal graph','Inject current-map identity and 768x512 map dimensions; record absolute map location','Dungeon source branch is retained but not executed; class stubs only'],result_sha256=hashlib.sha256((out/'reference.json').read_bytes()).hexdigest(),scope='Sixteen actual source Ready/map results; no scene, collision, GPU, audible or hardware pass')
        (out/'receipt.json').write_bytes((json.dumps(receipt,indent=2)+'\n').encode());print('AreaRoom source reference:',len(result['results']),'native results')
    finally:
        if project.resolve().is_relative_to(out)and(project/'.owned-area-reference').read_text()=='source-area-v1':shutil.rmtree(project)
if __name__=='__main__':main()
