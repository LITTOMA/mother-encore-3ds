"""Original motion-to-Bat entry paths and frames, with explicit initial states.

The complete original state-machine topology is also queried with inert child
animations to check path closure; source motion clips run in a separate eight-
state subset. Neither fixture executes movement physics or the full Player.
"""
import argparse,hashlib,json,re,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import field_swing,run_swing_action_reference as action
from tools.godot_text import Document,SubRef
from tools.run_scene_reference import invoke

PROBE='''extends SceneTree
var rows=[]
func _init():call_deferred("run")
func capture(target,tree,id,phase,delta):
    var playback=tree.get("parameters/playback")
    var path=[]
    for name in playback.get_travel_path():path.append(name)
    rows.append({"case":id,"phase":phase,"delta":delta,"node":playback.get_current_node(),"path":path,"time":"%.17f" % playback.get_current_play_position(),"main_frame":target.get_node("Position/main").frame,"main":target.get_node("Position/main").visible,"special":target.get_node("SpecialAnimations").visible,"frame":target.get_node("SpecialAnimations").frame,"hit":not target.get_node("HitboxPivot/BatHitbox/CollisionShape2D").disabled,"pivot":"%.17f" % target.get_node("HitboxPivot").rotation_degrees,"attack":target._state==target.ATTACK})
func run():
    yield(self,"idle_frame")
    var f=File.new()
    assert(f.open("res://input.json",File.READ)==OK)
    var config=JSON.parse(f.get_as_text()).result
    f.close()
    var paths={}
    for state in config.states:
        if state==config.attack:continue
        print("Topology path: ",state)
        var target=load("res://topology.tscn").instance()
        get_root().add_child(target)
        var tree=target.get_node("AnimationTree")
        tree.process_mode=AnimationTree.ANIMATION_PROCESS_MANUAL
        tree.active=true
        tree.advance(0)
        var playback=tree.get("parameters/playback")
        playback.start(state)
        tree.advance(0)
        assert(playback.get_current_node()==state)
        playback.travel(config.attack)
        tree.advance(0)
        var path=[]
        for name in playback.get_travel_path():path.append(name)
        if playback.get_current_node()!=state:path.push_front(playback.get_current_node())
        paths[state]=path
        tree.active=false
        target.free()
    var id=config.case_offset
    for definition in config.cases:
        print("Entry case: ",id," ",definition.state)
        var target=load("res://probe.tscn").instance()
        get_root().add_child(target)
        target._party_member.incapacitated=definition.incapacitated
        var tree=target.get_node("AnimationTree")
        tree.process_mode=AnimationTree.ANIMATION_PROCESS_MANUAL
        tree.active=true
        for param in config.parameters:tree.set(param,Vector2(definition.direction[0],definition.direction[1]))
        tree.advance(0)
        var playback=tree.get("parameters/playback")
        playback.start(definition.state)
        tree.advance(0)
        tree.advance(definition.warmup)
        assert(playback.get_current_node()==definition.state)
        capture(target,tree,id,0,0)
        var event=InputEventJoypadButton.new()
        event.button_index=1
        event.pressed=true
        target._input(event)
        capture(target,tree,id,1,0)
        for i in range(40):
            tree.advance(1.0/60.0)
            capture(target,tree,id,2,1.0/60.0)
            if i==0:
                var path=[]
                for name in playback.get_travel_path():path.append(name)
                if playback.get_current_node()!=definition.state:path.push_front(playback.get_current_node())
                assert(path==paths[definition.state])
            yield(self,"idle_frame")
            capture(target,tree,id,3,0)
        target.free()
        yield(self,"idle_frame")
        id+=1
    assert(f.open("res://native.json",File.WRITE)==OK)
    f.store_string(JSON.print({"engine":Engine.get_version_info(),"paths":paths,"rows":rows},"  "))
    f.close()
    quit()
'''

def prepare(work,recipe):
 proof=action.prepare(work);p,ex=field_swing.read();raw=ex.text(p['scene']);d=Document(p['scene'],raw);nodes={n.attrs['name']:n for n in d.nodes};ap=nodes['AnimationPlayer'];root_id=nodes['AnimationTree'].props['tree_root'].id;machine=d.sub[root_id][1];names=[v['name']for v in recipe['states']];ids=set();animations={}
 def refs(v):
  if isinstance(v,SubRef):yield v.id
  elif isinstance(v,list):
   for x in v:yield from refs(x)
  elif isinstance(v,dict):
   for x in v.values():yield from refs(x)
 def add(i):
  if i in ids:return
  ids.add(i);kind,props=d.sub[i]
  for child in refs(props):add(child)
  if kind=='AnimationNodeAnimation':
   name=props['animation'];reference=ap.props['anims/'+name];animations[name]=reference.id;add(reference.id)
 for name in names:add(machine['states/'+name+'/node'].id)
 t=machine['transitions'];edges=[(a,b,c.id)for a,b,c in zip(t[::3],t[1::3],t[2::3])if a in names and b in names]
 for _,_,i in edges:add(i)
 shape=next(n for n in d.nodes if n.attrs['name']=='CollisionShape2D'and n.attrs.get('parent')=='HitboxPivot/BatHitbox');add(shape.props['shape'].id)
 pattern=r'^\[sub_resource type="[^"]+" id=(\d+)\]\n.*?(?=^\[|\Z)';blocks={int(m[1]):m[0]for m in re.finditer(pattern,raw,re.M|re.S)}
 base=(work/'probe.tscn').read_text(encoding='utf8');header=base[:base.index('[sub_resource')];nodepart=base[base.index('[node name="Player"'):];nodepart=nodepart[:nodepart.index('[node name="AnimationPlayer"')]
 reduced='[sub_resource type="AnimationNodeStateMachine" id='+str(root_id)+']\n'
 for name in names:
  pos=machine['states/'+name+'/position'];reduced+='states/'+name+'/node = SubResource( '+str(machine['states/'+name+'/node'].id)+' )\nstates/'+name+'/position = Vector2( '+str(pos.x)+', '+str(pos.y)+' )\n'
 reduced+='transitions = [ '+', '.join('"'+a+'", "'+b+'", SubResource( '+str(i)+' )'for a,b,i in edges)+' ]\nstart_node = "'+machine['start_node']+'"\n'
 scene=header+''.join(blocks[i]for i in blocks if i in ids)+reduced+nodepart+'[node name="AnimationPlayer" type="AnimationPlayer" parent="."]\n'+''.join('"anims/'+name+'" = SubResource( '+str(i)+' )\n'for name,i in animations.items())+'[node name="AnimationTree" type="AnimationTree" parent="."]\ntree_root = SubResource( '+str(root_id)+' )\nanim_player = NodePath("../AnimationPlayer")\n'
 check=Document('probe.tscn',scene)
 if any(check.sub[i]!=d.sub[i]for i in ids):raise ValueError('Motion entry fixture altered source clips/nodes/edges')
 # New motion states are admitted only when their animated targets exist in
 # this typed fixture. Unknown targets never become missing-node warnings.
 for name,clip_id in animations.items():
  clip=d.sub[clip_id][1]
  for key,value in clip.items():
   if key.endswith('/path')and value.path not in('Position/main:frame','Position/main:visible','SpecialAnimations:visible','SpecialAnimations:frame','HitboxPivot:rotation_degrees','HitboxPivot/BatHitbox/CollisionShape2D:disabled','SpecialAnimations:texture','SpecialAnimations/NintenBat:show_behind_parent','.'):
    raise ValueError('Unreviewed entry property: '+name+' '+value.path)
 (work/'probe.tscn').write_text(scene,encoding='utf8')
 # The entire root's topology, positions and transitions stay byte-identical.
 # Only inert child animation nodes are substituted for this path-only query.
 state_ids=[v.id for k,v in machine.items()if k.endswith('/node')];transition_ids=[v.id for v in t[2::3]]
 topology='[gd_scene format=2]\n[sub_resource type="Animation" id=9999]\nlength = 1.0\n'
 topology+=''.join('[sub_resource type="AnimationNodeAnimation" id='+str(i)+']\nanimation = "Fixture"\n'for i in state_ids)
 topology+=''.join(blocks[i]for i in blocks if i in transition_ids)+blocks[root_id]+'[node name="Topology" type="Node"]\n[node name="AnimationPlayer" type="AnimationPlayer" parent="."]\nanims/Fixture = SubResource( 9999 )\n[node name="AnimationTree" type="AnimationTree" parent="."]\ntree_root = SubResource( '+str(root_id)+' )\nanim_player = NodePath("../AnimationPlayer")\n'
 if Document('topology.tscn',topology).sub[root_id]!=d.sub[root_id]:raise ValueError('Full source topology changed')
 (work/'topology.tscn').write_text(topology,encoding='utf8');proof.update(sources={**proof['sources'],**ex.sources},scope=__doc__,original_root_sha256=hashlib.sha256(blocks[root_id].encode()).hexdigest(),selected_states=names,selected_resources=sorted(ids),full_topology_states=len(state_ids),full_topology_edges=len(transition_ids))
 params=[k for k in nodes['AnimationTree'].props if k.endswith('/blend_position')and k.split('/')[1]in names]
 return proof,params

def run(work,reports,engine,recipe_path):
 from tools.field_swing_entry import read
 recipe=read(recipe_path);work=work.resolve();reports=reports.resolve();engine=engine.resolve()
 if work.exists()or reports.exists()or not work.is_relative_to(ROOT/'build')or not reports.is_relative_to(ROOT/'reports'):raise ValueError('Fresh owned swing entry reference directories required')
 work.mkdir(parents=True);reports.mkdir(parents=True);proof,params=prepare(work,recipe);ids={v['id']:v['name']for v in recipe['states']};cases=[]
 for state in recipe['states']:
  if state['id']==recipe['attack']:continue
  for direction in ([0,1],[0,-1],[1,0],[-1,0],[1,1],[-1,1],[1,-1],[-1,-1]):
   warmups=[.04]if state['name']!='Crouch'else[0,.04,.08,.2]
   for warmup in warmups:cases.append(dict(state=state['name'],id=state['id'],direction=direction,warmup=warmup,incapacitated=state['name'].startswith('Fainted')))
 config=dict(states=list(ids.values()),attack=ids[recipe['attack']],parameters=params,cases=cases,case_offset=0);(work/'input-all.json').write_text(json.dumps(config),encoding='utf8');(work/'probe.gd').write_text(PROBE,encoding='utf8')
 invoke(engine,work,['--editor','--quit'],reports/'import.log');actual=None
 # Bound each real engine process, retaining the complete case set. A large
 # idle-frame sequence must not exceed the reference runner's 55-second limit.
 for start in range(0,len(cases),24):
  batch=dict(config,cases=cases[start:start+24],case_offset=start);raw=json.dumps(batch);(work/'input.json').write_text(raw,encoding='utf8');(reports/f'input-{start}.json').write_text(raw,encoding='utf8')
  invoke(engine,work,['--fixed-fps','60','--script',str(work/'probe.gd')],reports/f'godot-{start}.log');part=json.loads((work/'native.json').read_bytes());require_rows=len(batch['cases'])*82
  if len(part['rows'])!=require_rows:raise ValueError('Incomplete source entry batch')
  if actual is None:actual=part
  else:
   if actual['engine']!=part['engine']or actual['paths']!=part['paths']:raise ValueError('Source entry engine/path changed between batches')
   actual['rows'].extend(part['rows'])
 if actual['engine']['string']!='3.6.2-stable (official)'or actual['paths']!=recipe['paths']or len(actual['rows'])!=len(cases)*82:raise ValueError('Source entry engine/path/coverage differs')
 for i,c in enumerate(cases):
  rows=actual['rows'][i*82:(i+1)*82]
  if any(v['case']!=i for v in rows)or rows[0]['phase']!=0 or rows[0]['node']!=c['state']or rows[0]['attack']or rows[1]['phase']!=1 or not rows[1]['attack']or rows[-1]['attack']:raise ValueError('Incomplete source entry state/control sequence')
 sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();by_name={v:k for k,v in ids.items()};lines=[]
 for row in actual['rows']:
  c=cases[row['case']];initial=actual['rows'][row['case']*82];lines.append(' '.join(map(str,[row['case'],c['id'],*c['direction'],int(c['incapacitated']),initial['time'],row['phase'],0 if row['phase']!=2 else repr(1/60),by_name[row['node']],row['time'],row['main_frame'],row['frame'],int(row['main']),int(row['special']),int(row['hit']),row['pivot'],int(row['attack'])])))
 (reports/'reference.tsv').write_text('\n'.join(lines)+'\n',encoding='utf8');(reports/'reference.json').write_text(json.dumps(actual,separators=(',',':'))+'\n',encoding='utf8');(reports/'cases.json').write_text(json.dumps(cases,indent=2)+'\n',encoding='utf8');proof.update(engine=actual['engine'],engine_sha256=sha(engine),paths=actual['paths'],cases=len(cases),observations=len(actual['rows']),recipe_sha256=sha(recipe_path),input_sha256=sha(work/'input-all.json'),probe_sha256=sha(work/'probe.gd'),tools={Path(__file__).relative_to(ROOT).as_posix():sha(Path(__file__)),'tools/run_swing_action_reference.py':sha(ROOT/'tools/run_swing_action_reference.py')},outputs={p.name:sha(p)for p in reports.iterdir()if p.is_file()});(reports/'source.json').write_text(json.dumps(proof,indent=2)+'\n',encoding='utf8');print('Original motion entry:',len(cases),'cases;',len(actual['rows']),'observations; complete source topology path closure')
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--work',type=Path,required=True);p.add_argument('--reports',type=Path,required=True);p.add_argument('--engine',type=Path,required=True);p.add_argument('--recipe',type=Path,default=ROOT/'content/field-swing-entry.json');o=p.parse_args();run(o.work,o.reports,o.engine,o.recipe)
