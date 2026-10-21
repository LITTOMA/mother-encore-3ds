"""Observe original poison callbacks driving real Character/damage/Flash methods.
Manual body delivery and shader/UI command holders;not full physics or scene.
"""
import argparse,hashlib,json,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import world_damage as damage,catacombs_poison as poison,catacombs_steps as steps
from tools.run_scene_reference import invoke
PROBE=r'''func run():
    global.currentScene=self
    var file=File.new()
    assert(file.open("res://input.json",File.READ)==OK)
    var config=JSON.parse(file.get_as_text()).result
    file.close()
    var actor=create_actor(int(config.hp),int(config.maxhp))
    actor.run_sound=config.initial_run
    actor.shadow_mode=config.initial_shadow
    var outsider=Node2D.new()
    add_child(outsider)
    var areas=[]
    for binding in config.areas:
        var area=load("res://Nodes/Reusables/Overlap/Poison.tscn").instance()
        area.stepping_sound=NodePath("Step")
        var step=load("res://step.gd").new()
        step.name="Step"
        step.entering_sound=config.step.enter
        step.exiting_sound=config.step.exit
        step.enter_shadow_effect=config.step.enter_shadow
        step.exit_shadow_effect=config.step.exit_shadow
        step.enabled=config.step.enabled
        area.add_child(step)
        for role in 2:
            var shape=config.shapes[int(binding.first)+role]
            var polygon=area.get_node("CollisionPolygon2D" if role==0 else "StaticBody2D/CollisionPolygon2D")
            polygon.position=Vector2(shape.position[0],shape.position[1])
            var points=PoolVector2Array()
            for index in range(int(shape.first),int(shape.first+shape.count)):points.append(Vector2(config.points[index][0],config.points[index][1]))
            polygon.polygon=points
        add_child(area)
        areas.append(area)
    var geyser=load("res://geyser.gd").new()
    add_child(geyser)
    seed(int(config.seed))
    var rows=[]
    for index in config.actions.size():
        var action=config.actions[index]
        match action[0]:
            "area":
                var body=actor if action[3]=="player" else outsider
                var area=areas[int(action[1])]
                if action[2]:area._on_Area2D_body_entered(body)
                else:area._on_Poison_body_exited(body)
            "geyser":
                var body=actor if action[2]=="player" else outsider
                if action[1]:geyser._on_Poison_Geyser_body_entered(body)
                else:geyser._on_Poison_Geyser_body_exited(body)
            "pause":
                actor._paused=bool(action[1])
                if action[1]:actor.pause_flash_anim()
                else:actor.resume_flash_anim()
            "tick":
                actor.physics_tail()
                if actor._flash_anim.is_playing():actor._flash_anim.advance(float(action[1]))
            _:assert(false)
        var row=capture(actor,str(index))
        row.action=action.duplicate(true)
        if action[0]=="tick":row.action[1]=bits(action[1])
        row.continuous=actor._is_continuous_damage
        row.damage=actor._attack_damage
        row.variance=actor._damage_variance
        row.paused=actor._paused
        row.run_sound=actor.run_sound
        row.shadow=actor.shadow_mode
        rows.append(row)
    rows[-1].rng_next=randi()
    assert(file.open("res://../reference.json",File.WRITE)==OK)
    file.store_string(JSON.print({"schema":1,"godot":Engine.get_version_info(),"rows":rows},"  "))
    file.close()
    retire(actor)
    get_tree().quit()
'''

def source():
 _,d,dp=damage.source();p,m,pp=poison.source();_,step,sp=steps.source();require=damage.require
 frozen=ROOT/'reports/world-damage/native1';review=json.loads((ROOT/'compatibility/reviews/world-damage-v0410.json').read_bytes());receipt=json.loads((frozen/'receipt.json').read_bytes());require(review['producer_sha256']==hashlib.sha256(Path(damage.__file__).read_bytes()).hexdigest()and review['native_producer_sha256']==hashlib.sha256((ROOT/'tools/run_world_damage_reference.py').read_bytes()).hexdigest()and receipt['source']['checked_source']==dp,'Poison damage checked Character dependency changed');files={}
 for name,h in receipt['source']['generated_sha256'].items():b=(frozen/'observer'/name).read_bytes();require(hashlib.sha256(b).hexdigest()==h,'Poison damage frozen actor changed');files[name]=b.decode()
 files.pop('cases.json');prefix=files['probe.gd'].split('func run():')[0];require(prefix!=files['probe.gd'],'Poison damage observer prefix missing');files['probe.gd']=prefix+PROBE
 files['actor.tscn']=files['actor.tscn'].replace('[node name="Actor" type="Node2D"]','[node name="Actor" type="KinematicBody2D"]',1)
 files['base.gd']=files['base.gd'].replace('extends Node2D\n','extends KinematicBody2D\nclass_name PartyObject\n',1)
 files['player.gd']+='\nvar run_sound=""\nvar shadow_mode=""\nfunc is_paused():return _paused\nfunc set_shadow(value):shadow_mode=value\n'
 config=files['project.godot'];line=next(v for v in config.splitlines()if v.startswith('_global_script_classes='));classes=json.loads(line.split('=',1)[1]);classes.append(dict(base='KinematicBody2D',**{'class':'PartyObject'},language='GDScript',path='res://base.gd'));config=config.replace(line,'_global_script_classes='+json.dumps(classes));config=config.replace('"Item":""}', '"Item":"","PartyObject":""}');files['project.godot']=config
 s=poison.f.Sources();n=p['paths'];files[n['area']]=s.text(n['area']);files[n['area_script']]=s.text(n['area_script']);files['step.gd']=s.text(n['step_script']);files['geyser.gd']='extends Area2D\n'+''.join(damage.method(s.text(n['geyser_script']),name)for name in p['symbols']['geyser_script'])
 target={v['step']for v in m['areas']};require(len(target)==1,'Poison damage source step coverage');selected=next(v for v in step['areas']if v['id']in target);actions=[]
 for i in range(len(m['areas'])):actions +=[['area',i,True,'player']]+[['tick',1/60]for _ in range(5)]+[['area',i,False,'player']]
 actions +=[['area',0,True,'player'],['area',1,True,'player'],['area',0,False,'player']]+[['tick',1/60]for _ in range(55)]
 actions +=[['geyser',True,'player']]+[['tick',1/60]for _ in range(65)]
 actions +=[['pause',True],['area',0,True,'player'],['area',0,True,'other'],['geyser',False,'player']]+[['tick',1/60]for _ in range(10)]
 actions +=[['pause',False]]+[['tick',1/60]for _ in range(10)]+[['geyser',True,'player']]+[['tick',1/60]for _ in range(55)]+[['geyser',False,'player'],['geyser',True,'other']]
 inputs=dict(hp=25,maxhp=30,seed=1,initial_run='wood',initial_shadow=step['shadows']['default'],areas=m['areas'],shapes=m['shapes'],points=m['points'],step=selected,actions=actions)
 files['input.json']=json.dumps(inputs);proof=dict(damage=dp,poison=pp,steps=sp,generated_sha256={n:hashlib.sha256(v.encode()).hexdigest()for n,v in files.items()},copied={},scope='Complete original Character/Status and selected original Player damage/physics/Flash plus complete Poison.gd/prefab and Stepping Sounds.gd;manual body callbacks.Explicit run-sound/Shadow state,shader/UI/audio/camera/vibration/gameover command holders;no automatic contact/elapsedShadow/GPU/factory/formal scene admission.')
 # Hurt is independently checked by the existing world-damage source inventory.
 _,ex=damage.read();hurt=d['strings'][1];data=ex.data(hurt);proof['copied']={hurt:hashlib.sha256(data).hexdigest()};return files,{hurt:data},proof

EXTRA={'action','continuous','damage','variance','paused','run_sound','shadow'}
def validate_result(doc,proof):
 from tests.prepare_world_damage import append_observation
 files,_,current=source();require=damage.require
 for k in current:require(proof.get(k)==current[k],'Poison damage source/observer changed: '+k)
 config=json.loads(files['input.json']);_,model,_=damage.source();_,sm,_=steps.source();require(type(doc)is dict and set(doc)=={'schema','godot','rows'}and type(doc['schema'])is int and doc['schema']==1 and doc['godot'].get('string')=='3.6.2-stable (official)'and type(doc['rows'])is list and len(doc['rows'])==len(config['actions']),'Poison damage native schema/coverage');seen=set()
 for index,(row,action)in enumerate(zip(doc['rows'],config['actions'])):
  if action[0]=='tick':
   import struct
   action=['tick',struct.unpack('<I',struct.pack('<f',action[1]))[0]]
  require(type(row)is dict and EXTRA<=set(row)and row['action']==action and row['label']==str(index)and all(type(row[k])is bool for k in['continuous','paused'])and all(type(row[k])is int and 0<=row[k]<=1000000 for k in['damage','variance'])and row['run_sound']in{s['name']for s in sm['sounds']}and row['shadow']in{s['name']for s in sm['shadows']['animations']},'Poison damage typed callback/Run/Shadow observation')
  base={k:v for k,v in row.items()if k not in EXTRA};append_observation([],base,model,seen)
 require(type(doc['rows'][-1].get('rng_next'))is int and 0<=doc['rows'][-1]['rng_next']<=0xffffffff,'Poison damage final shared RNG observation');return doc

def main():
 a=argparse.ArgumentParser(description=__doc__);a.add_argument('--out',type=Path,required=True);out=a.parse_args().out.resolve();require=damage.require;require(out.is_relative_to(ROOT/'build')and out!=ROOT/'build'and not out.exists(),'Fresh poison damage output under build required');engine=ROOT/'build/godot-reference/Godot_v3.6.2-stable_win64.exe';require(hashlib.sha256(engine.read_bytes()).hexdigest()=='e748acc92ee5205dd3cc1754d4422870f6a68ae6e00a16ff88bd3438c957adfd','Original engine changed');files,copied,proof=source();out.mkdir();project=out/'temporary';project.mkdir();(project/'.owned-poison-damage').write_bytes(b'poison-damage-reference-v1')
 for name,b in copied.items():p=project/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(b)
 def write():
  for name,v in files.items():p=project/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(v.encode())
 write();(out/'inputs.json').write_text(json.dumps(proof,indent=2)+'\n');invoke(engine,project,['--video-driver','GLES2','--debug','--editor','--quit'],out/'editor.log');(out/'editor-project.godot').write_bytes((project/'project.godot').read_bytes());write();invoke(engine,project,['--video-driver','GLES2','--debug','--fixed-fps','60'],out/'godot.log');require(all(hashlib.sha256((project/n).read_bytes()).hexdigest()==h for n,h in proof['generated_sha256'].items())and all((project/n).read_bytes()==b for n,b in copied.items()),'Poison damage executed inputs changed');raw=(out/'reference.json').read_bytes();doc=validate_result(json.loads(raw),proof);proof.update(engine_sha256=hashlib.sha256(engine.read_bytes()).hexdigest(),producer_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),result_sha256=hashlib.sha256(raw).hexdigest());(out/'receipt.json').write_text(json.dumps(proof,indent=2)+'\n');print('Actual original poison-to-Character:',len(doc['rows']),'observations;not full physics/factory/scene')
if __name__=='__main__':main()
