"""Original Geyser automatic body contacts, selected real Character and clocks.

Manual animation advance and teleport services; no complete room or physics-frame
equivalence. In particular, observe deletion exits instead of inventing them.
"""
import argparse,hashlib,json,math,struct,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import run_poison_contact_reference as contact,run_geyser_reference as geyser
from tools.run_scene_reference import invoke
from tools.extract_battle_entry import Extractor,require
PHASES=['initial','armed','enter','stay','hit','leave','reenter','before_retire','retired']
EXTRA=set('case phase position layer mask enabled paused continuous damage variance run_sound shadow contacts geyser'.split())
PROBE=r'''var config
var rows=[]
var contacts=[]
var selected
func notice(body,entered):contacts.append([entered,body==selected])
func settle():
    for _i in 4:yield(get_tree(),"physics_frame")
    yield(get_tree(),"idle_frame")
func shape(body):
    var collider=CollisionPolygon2D.new()
    collider.name="CollisionShape2D"
    collider.position=Vector2(config.player.position[0],config.player.position[1])
    var points=PoolVector2Array()
    for point in config.player.points:points.append(Vector2(point[0],point[1]))
    collider.polygon=points
    body.add_child(collider)
    body.collision_layer=config.player.layer
    body.collision_mask=config.player.mask
    body.position=Vector2(20000,20000)
func describe(actor,body,vent,ci,phase):
    var row=capture(actor,str(ci)+":"+phase)
    row.case=ci
    row.phase=phase
    row.position=[bits(body.position.x),bits(body.position.y)]
    row.layer=body.collision_layer
    row.mask=body.collision_mask
    row.enabled=!body.get_node("CollisionShape2D").disabled
    row.paused=actor._paused
    row.continuous=actor._is_continuous_damage
    row.damage=actor._attack_damage
    row.variance=actor._damage_variance
    row.run_sound=actor.run_sound
    row.shadow=actor.shadow_mode
    row.contacts=contacts.duplicate(true)
    row.geyser={"alive":is_instance_valid(vent)}
    if is_instance_valid(vent):
        var animation=vent.get_node("AnimationPlayer")
        row.geyser={"alive":true,"queued":vent.is_queued_for_deletion(),"monitoring":vent.monitoring,"disabled":vent.get_node("CollisionShape2D").disabled,"overlap":vent.overlaps_body(body),"time":bits(animation.current_animation_position),"frame":vent.get_node("Sprite").frame}
    contacts.clear()
    rows.append(row)
func run():
    global.currentScene=self
    var file=File.new()
    assert(file.open("res://input.json",File.READ)==OK)
    config=JSON.parse(file.get_as_text()).result
    file.close()
    var geometry=[]
    for ci in config.cases.size():
        var test=config.cases[ci]
        var actor=create_actor(int(config.hp),int(config.maxhp))
        actor.run_sound=config.initial_run
        actor.shadow_mode=config.initial_shadow
        shape(actor)
        var body=actor
        if test.other:
            body=KinematicBody2D.new()
            add_child(body)
            shape(body)
        selected=actor
        var vent=load("res://geyser.tscn").instance()
        vent.position=Vector2(config.spawn[0],config.spawn[1])
        add_child(vent)
        var animation=vent.get_node("AnimationPlayer")
        animation.playback_process_mode=AnimationPlayer.ANIMATION_PROCESS_MANUAL
        vent.connect("body_entered",self,"notice",[true])
        vent.connect("body_exited",self,"notice",[false])
        if geometry.empty():
            var collider=vent.get_node("CollisionShape2D")
            var extents=collider.shape.extents
            for point in [Vector2(-extents.x,-extents.y),Vector2(extents.x,-extents.y),Vector2(extents.x,extents.y),Vector2(-extents.x,extents.y)]:
                var world=collider.global_transform.xform(point)
                geometry.append([bits(world.x),bits(world.y)])
        contacts.clear()
        seed(int(config.seed))
        yield(settle(),"completed")
        describe(actor,body,vent,ci,"initial")
        animation.advance(config.arm)
        yield(settle(),"completed")
        describe(actor,body,vent,ci,"armed")
        actor._paused=test.paused_enter
        body.collision_layer=int(test.layer)
        body.collision_mask=int(test.mask)
        body.get_node("CollisionShape2D").disabled=!test.enabled
        body.position=Vector2(config.center[0]-config.player.center[0],config.center[1]-config.player.center[1])
        yield(settle(),"completed")
        describe(actor,body,vent,ci,"enter")
        actor._paused=false
        body.get_node("CollisionShape2D").disabled=false
        if test.monitoring_stop:animation.advance(config.disarm-config.arm)
        yield(settle(),"completed")
        describe(actor,body,vent,ci,"stay")
        actor.physics_tail()
        if actor._flash_anim.is_playing():actor._flash_anim.advance(1.0/60.0)
        describe(actor,body,vent,ci,"hit")
        actor._paused=test.paused_exit
        body.position=Vector2(20000,20000)
        yield(settle(),"completed")
        describe(actor,body,vent,ci,"leave")
        actor._paused=false
        body.collision_layer=config.player.layer
        body.collision_mask=config.player.mask
        body.position=Vector2(config.center[0]-config.player.center[0],config.center[1]-config.player.center[1])
        yield(settle(),"completed")
        describe(actor,body,vent,ci,"reenter")
        if test.animation_finish:animation.advance(config.finish)
        else:vent.queue_free()
        describe(actor,body,vent,ci,"before_retire")
        yield(settle(),"completed")
        describe(actor,body,vent,ci,"retired")
        rows[-1].rng_next=randi()
        if body!=actor:body.free()
        retire(actor)
        yield(settle(),"completed")
    assert(file.open("res://../reference.json",File.WRITE)==OK)
    file.store_string(JSON.print({"schema":1,"godot":Engine.get_version_info(),"geometry":geometry,"rows":rows},"  "))
    file.close()
    get_tree().quit()
'''

def source():
 files,copied,cp=contact.source();gf,media,gp=geyser.source();_,gm,_=geyser.g.source();_,pm,_=contact.poison.source();prefix=files['probe.gd'].split('var config')[0];require(prefix!=files['probe.gd'],'Geyser contact actor prefix missing');files['probe.gd']=prefix+PROBE
 script=geyser.g.source()[0]['paths']['script'];files[script]=gf[script];files['geyser.tscn']=gf['geyser.tscn'].replace('res://audio.gd','res://geyser_audio.gd');files['geyser_audio.gd']=gf['audio.gd']
 manager=gf['manager.gd'];needle='\tcommands.append([stream.resource_path,node_name])';require(manager.count(needle)==1,'Geyser source named voice observation changed');files['manager.gd']=manager.replace(needle,needle+'\n\tif node_name=="damage":global.events.append(["hurt",stream.resource_path,node_name])');files['base.gd']=files['base.gd'].replace('class_name PartyObject\n','class_name PartyObject\nsignal paused\nsignal unpaused\n',1);files['ui.gd']+='\nfunc is_in_cutscene():return false\nfunc is_game_over():return false\n';files['project.godot']=files['project.godot'].replace('audioManager="*res://audio.gd"','audioManager="*res://manager.gd"');require('[audio]'not in files['project.godot'],'Unexpected existing bus section');files['project.godot']+='\n[audio]\ndefault_bus_layout="res://'+geyser.g.source()[0]['paths']['bus']+'"\n'
 ex=Extractor(ROOT)
 for name in media:copied[name]=ex.data(name)
 prior=json.loads(files['input.json']);spawn=[contact.poison.f.f32(pm['holes'][0]['position'][k]+pm['spawn'][k])for k in [0,1]];center=[contact.poison.f.f32(spawn[k]+gm['shape'][k])for k in [0,1]];cases=[]
 for name in ['ordinary','paused_entry','other_body','disabled_shape','zero_filter','monitoring_stop','paused_exit','animation_retirement']:
  cases.append(dict(name=name,other=name=='other_body',paused_enter=name=='paused_entry',enabled=name!='disabled_shape',layer=0 if name=='zero_filter'else prior['player']['layer'],mask=0 if name=='zero_filter'else prior['player']['mask'],monitoring_stop=name=='monitoring_stop',paused_exit=name=='paused_exit',animation_finish=name=='animation_retirement'))
 spewing=next(v for v in gm['clips']if v['id']==gm['spewing']);switch=[key['time']for key in gm['keys'][spewing['first']:spewing['first']+spewing['count']]if key['kind']==2 and key['value']];stop=[key['time']for key in gm['keys'][spewing['first']:spewing['first']+spewing['count']]if key['kind']==2 and not key['value']and key['time']>0];require(len(switch)==len(stop)==1,'Geyser source monitoring clock selector')
 prior.update(cases=cases,spawn=spawn,center=center,arm=(switch[0]+stop[0])*.5,disarm=(stop[0]+spewing['duration'])*.5,finish=spewing['duration']);files['input.json']=json.dumps(prior)
 proof=dict(character=cp,geyser=gp['source'],generated_sha256={n:hashlib.sha256(v.encode()).hexdigest()for n,v in files.items()},copied={n:hashlib.sha256(v).hexdigest()for n,v in copied.items()},dependencies={n:hashlib.sha256((ROOT/n).read_bytes()).hexdigest()for n in ['tools/run_poison_contact_reference.py','tools/run_geyser_reference.py']},scope='Complete original Geyser script/scene,actual automatic Area2D contact/rectangle/Player footprint,complete Character/Status and selected Player damage/Flash;manual animation clock and teleport,explicit UI/pause/renderer/audio command services. Queue-free/animation retirement are actually observed,not synthesized. No exact physics-frame/factory/complete room/GPU/NDSP/hardware admission.')
 return files,copied,proof

def validate_result(doc,proof):
 from tests.prepare_world_damage import append_observation
 files,_,current=source();require(type(proof)is dict and set(proof)in[set(current),set(current)|{'engine_sha256','producer_sha256','result_sha256'}]and all(json.dumps(proof.get(k),sort_keys=True)==json.dumps(v,sort_keys=True)for k,v in current.items()),'Geyser contact executed inputs changed');config=json.loads(files['input.json']);_,dm,_=contact.damage.damage.source();_,sm,_=contact.steps.source()
 require(type(doc)is dict and set(doc)=={'schema','godot','geometry','rows'}and type(doc['schema'])is int and doc['schema']==1 and doc['godot'].get('string')=='3.6.2-stable (official)','Geyser contact engine/schema');require(type(doc['geometry'])is list and len(doc['geometry'])==4 and all(type(p)is list and len(p)==2 and all(type(v)is int and 0<=v<=0xffffffff and math.isfinite(struct.unpack('<f',struct.pack('<I',v))[0])for v in p)for p in doc['geometry']),'Geyser contact rectangle domain');require(type(doc['rows'])is list and len(doc['rows'])==len(config['cases'])*len(PHASES),'Geyser contact exact phase coverage');seen=set()
 for index,row in enumerate(doc['rows']):
  ci=index//len(PHASES);phase=PHASES[index%len(PHASES)];case=config['cases'][ci];require(type(row)is dict and EXTRA<=set(row)and type(row['case'])is int and row['case']==ci and row['phase']==phase and row['label']==str(ci)+':'+phase,'Geyser contact phase identity')
  require(all(type(row[k])is bool for k in ['enabled','paused','continuous'])and all(type(row[k])is int and 0<=row[k]<=0xfffff for k in ['layer','mask','damage','variance'])and row['run_sound']in{v['name']for v in sm['sounds']}and row['shadow']in{v['name']for v in sm['shadows']['animations']}and type(row['position'])is list and len(row['position'])==2 and all(type(v)is int and 0<=v<=0xffffffff and math.isfinite(struct.unpack('<f',struct.pack('<I',v))[0])for v in row['position']),'Geyser contact body state domain')
  require(type(row['contacts'])is list and len(row['contacts'])<=1 and all(type(v)is list and len(v)==2 and all(type(b)is bool for b in v)and v[1]==(not case['other'])for v in row['contacts']),'Geyser contact callback identity/domain')
  g=row['geyser'];require(type(g)is dict and type(g.get('alive'))is bool and set(g)==(set('alive queued monitoring disabled overlap time frame'.split())if g['alive']else{'alive'}),'Geyser contact lifetime fields')
  if g['alive']:require(all(type(g[k])is bool for k in ['queued','monitoring','disabled','overlap'])and type(g['time'])is int and 0<=g['time']<=0x7f7fffff and type(g['frame'])is int and 0<=g['frame']<32,'Geyser contact live pose domain')
  require(g['alive']==(phase!='retired'),'Geyser contact declared retirement phase')
  if phase=='retired':require(type(row.get('rng_next'))is int and 0<=row['rng_next']<=0xffffffff,'Geyser contact final RNG')
  else:require('rng_next'not in row,'Geyser contact unexpected RNG phase')
  append_observation([],{k:v for k,v in row.items()if k not in EXTRA},dm,seen)
 return doc

def main():
 a=argparse.ArgumentParser(description=__doc__);a.add_argument('--out',type=Path,required=True);out=a.parse_args().out.resolve();require(out.is_relative_to(ROOT/'build')and out!=ROOT/'build'and not out.exists(),'Fresh Geyser contact output under build required');engine=ROOT/'build/godot-reference/Godot_v3.6.2-stable_win64.exe';require(hashlib.sha256(engine.read_bytes()).hexdigest()=='e748acc92ee5205dd3cc1754d4422870f6a68ae6e00a16ff88bd3438c957adfd','Geyser contact engine changed');files,copied,proof=source();out.mkdir();project=out/'temporary';project.mkdir();(project/'.owned-geyser-contact').write_bytes(b'geyser-contact-reference-v1')
 for name,raw in copied.items():
  if not name.endswith('.import'):target=project/name;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(raw)
 def write():
  for name,text in files.items():target=project/name;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(text.encode())
 write();(out/'inputs.json').write_text(json.dumps(proof,indent=2)+'\n');args=['--video-driver','GLES2','--debug'];invoke(engine,project,args+['--editor','--quit'],out/'fresh-import.log')
 for name,raw in copied.items():
  if name.endswith('.import'):(project/name).write_bytes(raw)
 checks=list((project/'.import').glob('*.md5'));require(len(checks)==5,'Geyser contact fresh media import coverage')
 for path in checks:require(path.resolve().parent==(project/'.import').resolve()and not path.is_symlink(),'Geyser contact checksum escaped');path.unlink()
 invoke(engine,project,args+['--editor','--quit'],out/'source-import.log');(out/'editor-project.godot').write_bytes((project/'project.godot').read_bytes());write();invoke(engine,project,args+['--fixed-fps','60'],out/'godot.log');require(all(hashlib.sha256((project/n).read_bytes()).hexdigest()==h for n,h in proof['generated_sha256'].items())and all((project/n).read_bytes()==b for n,b in copied.items()),'Geyser contact executed inputs changed');raw=(out/'reference.json').read_bytes();doc=validate_result(json.loads(raw),proof);proof.update(engine_sha256=hashlib.sha256(engine.read_bytes()).hexdigest(),producer_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),result_sha256=hashlib.sha256(raw).hexdigest());(out/'receipt.json').write_text(json.dumps(proof,indent=2)+'\n');print('Original automatic Geyser contacts:',len(doc['rows']),'settled states;retirement actually observed;not full room')

if __name__=='__main__':main()
