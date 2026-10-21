"""Bounded original run/step callbacks and actual AnimatedSprite Shadow scene.

Selected unchanged source methods run against explicit player identity and
named-SFX state services. Manual callbacks do not claim movement or contacts.
"""
import argparse,hashlib,json,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import catacombs_steps as steps,run_footstep_reference as original
from tools.extract_battle_entry import Extractor,require
from tools.reference_movement import extract
from tools.run_scene_reference import invoke

# A distinct input protocol; historical cases/expected observations stay intact.
CASES=original.CASES+[
 [0,0,0,1,0],[5,1,'shadow',0],[1,0,1,1],[0,1,0,1,0],
 [5,1,'ripple',1],[1,0,1,1],[2,0,0],[1,0,0,1],
 [5,0,'magic',1],[1,0,1,0],[1,0,0,0],[1,0,1,2],
 [2,0,1],[1,0,1,1],[1,0,0,1],[1,1,1,1],
 [5,1,'magic',1],[1,1,0,1],[1,2,1,1],
 [2,1,0],[1,1,1,1],[1,1,0,1],[2,1,1],
 [0,1,0,1,0],[1,1,1,1],[0,1,0,1,0],[0,0,0,1,0],
 [1,1,0,1],[0,1,0,1,0],[3,0,0,0,0],[0,0,0,1,0]]

PROBE=r'''extends SceneTree
func _init():call_deferred("run")
func shadow(body):
    var s=body.get_node("Shadow")
    return {"mode":body.get_shadow(),"frame":s.frame,"behind":s.show_behind_parent,"playing":s.playing,"start":s.start_anim}
func run():
    var file=File.new()
    assert(file.open("res://input.json",File.READ)==OK)
    var config=JSON.parse(file.get_as_text()).result
    file.close()
    var actor=load("res://actor.gd").new()
    var other=load("res://party.gd").new()
    var outsider=Node2D.new()
    for body in [actor,other]:
        body.add_child(load("res://"+config.shadow_scene).instance())
        get_root().add_child(body)
        body.set_shadow(config.shadow)
    var world=load("res://world.gd").new()
    world.player=actor
    actor.global=world
    var manager=load("res://manager.gd").new()
    actor.audioManager=manager
    actor.run_sound=config.default
    manager.names=config.names
    var sounds=[]
    for path in config.sounds:
        var stream=load(path)
        assert(stream is AudioStreamMP3 and stream.loop and stream.loop_offset==0)
        sounds.append(stream)
    manager.current.stream=sounds[config.names.find(config.default)]
    var areas=[]
    for row in config.areas:
        var area=load("res://step.gd").new()
        area.global=world
        area.entering_sound=row.enter
        area.exiting_sound=row.exit
        area.enter_shadow_effect=row.enter_shadow
        area.exit_shadow_effect=row.exit_shadow
        area.enabled=row.enabled
        areas.append(area)
    var animations=[]
    var sprite=actor.get_node("Shadow")
    for name in sprite.frames.get_animation_names():
        var frames=[]
        for i in sprite.frames.get_frame_count(name):
            var t=sprite.frames.get_frame(name,i)
            assert(t is AtlasTexture)
            assert(not(t.atlas.flags&Texture.FLAG_FILTER) and not(t.atlas.flags&Texture.FLAG_MIPMAPS) and not(t.atlas.flags&Texture.FLAG_REPEAT) and not(t.atlas.flags&Texture.FLAG_ANISOTROPIC_FILTER))
            frames.append({"atlas":t.atlas.resource_path,"region":[t.region.position.x,t.region.position.y,t.region.size.x,t.region.size.y]})
        animations.append({"name":name,"speed":sprite.frames.get_animation_speed(name),"loop":sprite.frames.get_animation_loop(name),"frames":frames})
    var rows=[]
    for action in config.cases:
        manager.events=[]
        match int(action[0]):
            0:
                actor._paused=bool(action[2])
                actor._state=actor.MOVE if bool(action[3]) else 1
                actor._climbing=bool(action[4])
                actor._set_running(bool(action[1]))
            1:
                var body=outsider if int(action[3])==2 else(actor if int(action[3])==1 else other)
                if bool(action[2]):areas[int(action[1])]._on_Stepping_Sounds_body_entered(body)
                else:areas[int(action[1])]._on_Stepping_Sounds_body_exited(body)
            2:
                if bool(action[2]):areas[int(action[1])].enable()
                else:areas[int(action[1])].disable()
            3,4:
                actor._paused=int(action[0])==3
                world.entering_door=int(action[0])==4
                actor._move_state(1.0/60)
                world.entering_door=false
            5:
                var seeded_body=actor if int(action[1])==1 else other
                seeded_body.set_shadow(action[2])
                seeded_body.get_node("Shadow").frame=int(action[3])
            _:assert(false)
        rows.append({"action":action,"wanted":config.names.find(actor.run_sound),"registered":manager.names.find(manager.current.stream.resource_path.get_file().get_basename()),"playing":manager.current.playing,"running":actor._running,"events":manager.events.duplicate(true),"player":shadow(actor),"party":shadow(other)})
    assert(file.open("res://../reference.json",File.WRITE)==OK)
    file.store_string(JSON.print({"schema":1,"godot":Engine.get_version_info(),"animations":animations,"rows":rows},"  "))
    file.close()
    for area in areas:area.free()
    actor.free()
    other.free()
    outsider.free()
    world.free()
    manager.free()
    quit()
'''

def source():
 p,m,proof=steps.source();_,actor,area,base=original.source()
 require(base==json.loads((ROOT/'compatibility/reviews/run-loop-v0410.json').read_bytes()),'Existing run-loop semantic review changed')
 ex=Extractor(ROOT);raw=ex.data(p['party_script']);methods,hashes=extract(raw,hashlib.sha256(raw).hexdigest(),['set_shadow','get_shadow'])
 files={'actor.gd':actor,'step.gd':area,'party.gd':'extends KinematicBody2D\n'+methods,'manager.gd':original.MANAGER,'world.gd':'extends Node\nvar player\nvar entering_door=false\nfunc get_player():return player\n','probe.gd':PROBE,'input.json':json.dumps(dict(default=m['default'],shadow=m['shadow'],shadow_scene=p['shadow_scene'],names=[s['name']for s in m['sounds']],sounds=[s['source']for s in m['sounds']],areas=m['areas'],cases=CASES))}
 copied=[p['shadow_scene'],p['shadow_script']]+[name for name in p['sources']if name.startswith('Graphics/Character Sprites/')or name.startswith('Audio/Sound effects/Footsteps/')]
 generated={name:hashlib.sha256(value.encode()).hexdigest()for name,value in files.items()}
 receipt=dict(source=proof,party_methods=hashes,base_review_sha256=hashlib.sha256((ROOT/'compatibility/reviews/run-loop-v0410.json').read_bytes()).hexdigest(),cases=CASES,generated_sha256=generated,copied_source={name:hashlib.sha256(ex.data(name)).hexdigest()for name in copied},adapters=['Manual source callback calls, no area physics','Selected original PartyObject set/get methods on KinematicBody2D; actual full Shadow scene/script/textures','Player identity, named-SFX state/events, pause/door state are explicit injected services','Ordinary movement is assert-false; only paused/door MOVE branches execute','Repeated-animation observation sets frame explicitly; no elapsed animation timing or pixel/audio comparison'],scope='Source stepping, run-loop and actual Shadow animation resource/mode state only; no complete player/scene/renderer/NDSP/hardware pass')
 return files,copied,receipt

def validate_result(doc):
 require(type(doc)is dict and set(doc)=={'schema','godot','animations','rows'}and type(doc['schema'])is int and doc['schema']==1 and type(doc['godot'])is dict and doc['godot'].get('string')=='3.6.2-stable (official)','Catacombs native stepping result schema/engine')
 _,m,_=steps.source();names=[s['name']for s in m['sounds']]
 require(type(doc['animations'])is list and all(type(a)is dict and set(a)=={'name','speed','loop','frames'}and type(a['name'])is str for a in doc['animations'])and sorted(doc['animations'],key=lambda a:a['name'])==sorted(m['shadows']['animations'],key=lambda a:a['name']),'Native Shadow frame order/atlas/speed/loop differs from original')
 require(type(doc['rows'])is list and len(doc['rows'])==len(CASES),'Catacombs native stepping observation count')
 for row,action in zip(doc['rows'],CASES):
  require(type(row)is dict and set(row)=={'action','wanted','registered','playing','running','events','player','party'}and row['action']==action,'Catacombs stepping action/order')
  require(all(type(row[k])is int and 0<=row[k]<len(names)for k in['wanted','registered'])and all(type(row[k])is bool for k in['playing','running']),'Catacombs stepping state domain')
  require(type(row['events'])is list and len(row['events'])<=1 and all(type(v)is list and len(v)==2 and type(v[0])is int and 1<=v[0]<=3 and type(v[1])is int and 0<=v[1]<len(names)for v in row['events']),'Catacombs stepping audio event domain')
  for k in['player','party']:
   s=row[k];require(type(s)is dict and set(s)=={'mode','frame','behind','playing','start'}and s['mode']in['shadow','ripple','magic']and s['start']==s['mode']and type(s['frame'])is int and 0<=s['frame']<(1 if s['mode']=='shadow'else 3)and type(s['behind'])is bool and type(s['playing'])is bool,'Catacombs original Shadow observation domain')
 return doc

def write_files(project,files):
 for name,value in files.items():
  target=project/name;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(value.encode())

def main():
 a=argparse.ArgumentParser(description=__doc__);a.add_argument('--out',type=Path,required=True);a.add_argument('--godot',type=Path,default=ROOT/'build/godot-reference/Godot_v3.6.2-stable_win64.exe');o=a.parse_args();out=o.out.resolve()
 require(out.is_relative_to(ROOT/'build')and out!=ROOT/'build'and not out.exists(),'Fresh owned stepping reference under build required');engine=o.godot.resolve();require(hashlib.sha256(engine.read_bytes()).hexdigest()=='e748acc92ee5205dd3cc1754d4422870f6a68ae6e00a16ff88bd3438c957adfd','Original engine SHA changed')
 files,copied,receipt=source();out.mkdir(parents=True);project=out/'temporary';project.mkdir();(project/'.owned-catacombs-steps-reference').write_bytes(b'catacombs-steps-reference-v1');ex=Extractor(ROOT)
 config='config_version=4\n[application]\nconfig/name="Original Catacombs stepping callbacks"\n[logging]\nfile_logging/enable_logging=false\n[rendering]\nquality/driver/driver_name="GLES2"\n'
 # An original .import remap points at a cache not supplied in the source repo.
 # Build fresh caches first, then import with verbatim original settings. Neither
 # editor stage suppresses diagnostics or copies an environment import cache.
 for name in copied:
  if name.endswith(('.png','.mp3')):
   target=project/name;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(ex.data(name))
 write_files(project,{'project.godot':config});invoke(engine,project,['--video-driver','GLES2','--debug','--editor','--quit'],out/'fresh-import.log')
 for name in copied:
  target=project/name;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(ex.data(name))
 # Godot3.6's _test_for_reimport compares source/destination MD5, not modified
 # parameter text. Remove only this fresh owned project's eleven checksum
 # records; keep imported files available while the editor reloads settings.
 expected={Path(name).name+'-'+hashlib.md5(('res://'+name).encode()).hexdigest()+'.md5'for name in copied if name.endswith(('.png','.mp3'))}
 checksums=list((project/'.import').glob('*.md5'));require({p.name for p in checksums}==expected and not(project/'.import').is_symlink()and(project/'.owned-catacombs-steps-reference').read_bytes()==b'catacombs-steps-reference-v1','Unexpected fresh import checksum cache')
 receipt['fresh_cache_checksums_retired']={str(p.relative_to(project)):hashlib.sha256(p.read_bytes()).hexdigest()for p in checksums}
 for path in checksums:
  require(path.resolve().parent==(project/'.import').resolve()and not path.is_symlink(),'Fresh import checksum path escaped');path.unlink()
 invoke(engine,project,['--video-driver','GLES2','--debug','--editor','--quit'],out/'source-settings-import.log');write_files(project,files)
 invoke(engine,project,['--video-driver','GLES2','--debug','--script',str(project/'probe.gd')],out/'godot.log');result=validate_result(json.loads((out/'reference.json').read_bytes()))
 receipt.update(producer_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),engine_sha256=hashlib.sha256(engine.read_bytes()).hexdigest(),result_sha256=hashlib.sha256((out/'reference.json').read_bytes()).hexdigest())
 (out/'receipt.json').write_bytes((json.dumps(receipt,indent=2)+'\n').encode());print('Original Catacombs stepping:',len(result['rows']),'manual source callback/run/Shadow observations;no native contacts or scene pass')
if __name__=='__main__':main()
