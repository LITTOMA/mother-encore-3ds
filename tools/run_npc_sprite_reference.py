"""Native AnimationTree / unchanged stationary NPC physics reference.

UI and player services are explicit inputs. No original map, wandering, renderer
or full dialogue execution is claimed by this isolated reference.
"""
import argparse,hashlib,json,math,shutil,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.extract_battle_entry import Extractor,require
from tools.reference_movement import extract
from tools.run_scene_reference import invoke

SOURCES={'Scripts/Main/npc.gd':['_physics_process'],
         'Scripts/Main/character_sprite.gd':['_create_animations','_create_tree','travel','blend_position']}
CASES=[
 dict(name='four_idle_talk',animation='4dir',idle='Idle',actions=[[10,'talk'],[25,'silent']]),
 dict(name='four_look_phase',animation='4dir',idle='Idle',actions=[[3,'look'],[20,'right'],[35,'up'],[50,'leave']]),
 dict(name='four_mute',animation='4dir',idle='Idle',actions=[[5,'talk'],[12,'mute'],[35,'unmute'],[51,'empty']]),
 dict(name='four_direction_during_talk',animation='4dir',idle='Idle',actions=[[5,'talk'],[18,'right'],[31,'up'],[43,'silent']]),
 dict(name='eight_idle_talk',animation='PartyMember',idle='Idle',actions=[[10,'talk'],[25,'silent']]),
 dict(name='eight_shallow_look',animation='PartyMember',idle='Idle',actions=[[3,'look'],[20,'right'],[35,'up'],[50,'leave']]),
 dict(name='eight_diagonal_talk',animation='PartyMember',idle='Idle',actions=[[5,'talk'],[18,'diagonal'],[43,'silent']]),
 dict(name='four_office_walk_idle',animation='4dir',idle='Walk',actions=[]),
]
NPC='''extends KinematicBody2D
var global
var uiManager
var character_sprite
var wander=false
var new_pos=null
var _pause_for_interact=false
var talking=false
var mute=false
var _is_looking=false
var _player_nearby=false
var _input_vector=Vector2.DOWN
var velocity=Vector2.ZERO
var speed=64
var idle_animation="Idle"
var talk_idle_animation="Idle"
'''
SPRITE='''extends Sprite
var _anim_player
var _sprite_frame_path=NodePath(".:frame")
var _json_data
var dir=0
var _direction=Vector2.ZERO
var animationState
var animationTree
var _current_state=""
var _directional_tags=[]
var _all_tags=[]
'''
PROBE='''extends SceneTree
class Driver:
    extends Node
    var runner
    func _physics_process(_delta):runner.physics_tick()
    func _process(_delta):runner.idle_tick()
var actors=[]
var cases=[]
var rows=[]
var frame=0
var output=""
func _init():
    for arg in OS.get_cmdline_args():
        if arg.begins_with("--encore-out="):output=arg.trim_prefix("--encore-out=")
    assert(output!="")
    var f=File.new()
    assert(f.open("res://cases.json",File.READ)==OK)
    cases=JSON.parse(f.get_as_text()).result
    f.close()
    for c in cases:
        var a=load("res://npc.gd").new()
        a.global=load("res://global.gd").new()
        a.uiManager=load("res://ui.gd").new()
        get_root().add_child(a.global)
        get_root().add_child(a.uiManager)
        get_root().add_child(a)
        a.set_physics_process(false)
        a.global.talker=a
        a.idle_animation=c.idle
        a.talk_idle_animation=c.idle
        var s=load("res://sprite.gd").new()
        a.add_child(s)
        a.character_sprite=s
        assert(f.open("res://"+c.animation+".json",File.READ)==OK)
        s._json_data=JSON.parse(f.get_as_text()).result
        f.close()
        var image=Image.new()
        image.create(s._json_data.size[0],s._json_data.size[1],false,Image.FORMAT_RGBA8)
        var texture=ImageTexture.new()
        texture.create_from_image(image)
        s.texture=texture
        s._anim_player=AnimationPlayer.new()
        s._anim_player.name="AnimationPlayer"
        s.add_child(s._anim_player)
        s.animationTree=AnimationTree.new()
        s.add_child(s.animationTree)
        s._create_animations([["Talk","Idle",2]])
        s.travel(c.idle)
        actors.append(a)
    var driver=Driver.new()
    driver.runner=self
    get_root().add_child(driver)
func physics_tick():
    for i in cases.size():
        var a=actors[i]
        for action in cases[i].actions:
            if action[0]!=frame:continue
            match action[1]:
                "talk":a.talking=true
                "silent":a.talking=false
                "mute":a.mute=true
                "unmute":a.mute=false
                "empty":a.uiManager.empty=true
                "look":
                    a._is_looking=true
                    a.global.player.global_position=Vector2(0.5,1)
                "right":a._input_vector=Vector2.RIGHT
                "up":a._input_vector=Vector2.UP
                "diagonal":a._input_vector=Vector2(1,1).normalized()
                "leave":a._is_looking=false
                _:assert(false)
        a._physics_process(1.0/60)
func idle_tick():
    create_timer(0).connect("timeout",self,"capture")
func capture():
    for i in actors.size():
        var a=actors[i]
        var s=a.character_sprite
        rows.append([frame,i,s.frame,s.animationState.get_current_node(),s._direction.x,s._direction.y,a._input_vector.x,a._input_vector.y,a.talking])
    frame+=1
    if frame==120:
        var f=File.new()
        assert(f.open(output,File.WRITE)==OK)
        f.store_string(JSON.print({"schema":1,"godot":Engine.get_version_info(),"cases":cases,"rows":rows},"  "))
        f.close()
        quit()
'''

def validate_review(review,pin):
    require(isinstance(review,dict)and set(review)==set('schema commit whole_file_approved symbols sources function_sha256 scope semantics'.split()),'NPC sprite review fields')
    require(type(review['schema'])is int and review['schema']==1 and review['commit']==pin and review['whole_file_approved']is False and review['symbols']==SOURCES,'NPC sprite reference review domain')
    require(set(review['sources'])==set(SOURCES)and set(review['function_sha256'])==set(SOURCES),'NPC sprite source identities')
    for path,symbols in SOURCES.items():require(set(review['function_sha256'][path])==set(symbols),'NPC sprite method identities')


def validate_result(result):
    require(isinstance(result,dict)and set(result)=={'schema','godot','cases','rows'}and type(result['schema'])is int and result['schema']==1,'NPC sprite result schema')
    require(isinstance(result['godot'],dict)and result['godot'].get('string')=='3.6.2-stable (official)'and result['cases']==CASES and len(result['rows'])==120*len(CASES),'NPC sprite native result domain')
    for index,row in enumerate(result['rows']):
        require(isinstance(row,list)and len(row)==9 and row[:2]==[index//len(CASES),index%len(CASES)]and all(type(v)is int for v in row[:3])and 0<=row[2]<200 and row[3]in ('Idle','Walk','Talk'),'NPC sprite native row')
        require(all(type(v)in (float,int)and math.isfinite(v)and -1<=v<=1 for v in row[4:8])and type(row[8])is bool,'NPC sprite native values')
    return result


def main():
    p=argparse.ArgumentParser();p.add_argument('--godot',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args();out=a.out.resolve()
    require(out.is_relative_to(ROOT/'build')and out!=ROOT/'build'and not out.exists(),'Fresh owned NPC sprite reference directory required')
    review=json.loads((ROOT/'compatibility/reviews/npc-sprite-v0410.json').read_bytes());ex=Extractor(ROOT)
    validate_review(review,ex.lock['commit'])
    functions={};hashes={}
    for path,names in SOURCES.items():
        functions[path],hashes[path]=extract(ex.data(path),review['sources'][path],names)
    require(hashes==review['function_sha256'],'NPC sprite reference method review changed')
    out.mkdir(parents=True);project=out/'temporary';project.mkdir();(project/'.owned-npc-reference').write_text('native-sprite-v1')
    try:
        files={'npc.gd':NPC+functions['Scripts/Main/npc.gd'],'sprite.gd':SPRITE+functions['Scripts/Main/character_sprite.gd'],
               'probe.gd':PROBE,'ui.gd':'extends Node\nvar empty=false\nfunc is_stack_empty():return empty\nfunc is_in_battle():return false\n',
               'global.gd':'extends Node\nvar talker\nvar player=Node2D.new()\nfunc _ready():add_child(player)\nfunc get_player():return player\n',
               'project.godot':'config_version=4\n[application]\nconfig/name="NPC native sprite oracle"\n[logging]\nfile_logging/enable_logging=false\n[physics]\ncommon/physics_fps=60\n',
               'cases.json':json.dumps(CASES)}
        for name in ('4dir','PartyMember'):files[name+'.json']=json.dumps(ex.yaml('Data/Animations/'+name+'.yaml'))
        for name,value in files.items():(project/name).write_text(value,encoding='utf8')
        generated={name:hashlib.sha256((project/name).read_bytes()).hexdigest()for name in files}
        invoke(a.godot.resolve(),project,['--fixed-fps','60','--script',str(project/'probe.gd'),'--encore-out='+str(out/'reference.json')],out/'godot.log')
        result=validate_result(json.loads((out/'reference.json').read_bytes()))
        receipt=dict(source_commit=ex.lock['commit'],engine_sha256=hashlib.sha256(a.godot.read_bytes()).hexdigest(),source_sha256=ex.sources,methods_sha256=hashes,tool_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),generated_sha256=generated,reference_sha256=hashlib.sha256((out/'reference.json').read_bytes()).hexdigest(),adapters=['Player position/UI stack/talker/talking/mute are injected service inputs','Stationary source NPC physics unchanged; native AnimationTree and source CharacterSprite construction/travel/blend unchanged','Generated placeholder texture supplies only sheet dimensions; no image claim','Readiness wires source data directly, not original NPC scene lifecycle'],gameplay=False,hardware=False)
        (out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf8');print('Native NPC sprite rows:',len(result['rows']),flush=True)
    finally:
        if project.is_relative_to(out)and(project/'.owned-npc-reference').is_file():shutil.rmtree(project)

if __name__=='__main__':main()
