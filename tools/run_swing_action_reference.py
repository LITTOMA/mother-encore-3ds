"""Run original logical input methods and the Bat/Idle AnimationTree branch.

Fixture singletons supply one Ninten skill and a run audio player. This is not
the full Player physics loop, hardware input, rendering or Area2D dispatch.
Audio playback flags are asynchronous observations, not the stable projection.
"""
import argparse,hashlib,json,math,re,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import field_swing
from tools.godot_text import Document,SubRef
from tools.run_scene_reference import invoke
METHODS=('_input','_do_attack','_get_skills_button_actions','_attack_unleash','_attack_animation_finished','_set_running','get_current_skill_action')

PROBE='''extends SceneTree
var rows=[]
var target
var tree
var case_id=0
var state_singleton
var audio_singleton
func _init():
    call_deferred("run")
func capture(op):
    var special=target.get_node("SpecialAnimations")
    var bat=special.get_node("NintenBat")
    bat._process(0)
    var playback=tree.get("parameters/playback")
    var audio=target.get_node("AudioStreamPlayer")
    rows.append({"case":case_id,"op":op,"frame":special.frame,"main_frame":target.get_node("Position/main").frame,"main":target.get_node("Position/main").visible,"special":special.visible,"hitbox":not target.get_node("HitboxPivot/BatHitbox/CollisionShape2D").disabled,"behind":bat.show_behind_parent,"bat_visible":bat.visible,"bat_frame":bat.frame,"pivot":"%.17f" % target.get_node("HitboxPivot").rotation_degrees,"attack":target._state==target.ATTACK,"state":target._state,"crouch":target._crouch,"idle":target._idle,"tap_run":target._tap_run,"running":target._running,"walk":target._walk,"skill":target._current_skill_action,"node":playback.get_current_node(),"time":"%.17f" % playback.get_current_play_position(),"run_playing":audio_singleton.get_sfx("run").playing,"sound":audio.stream.resource_path if audio.stream else "","rumble":state_singleton.rumble.size()})
func run():
    yield(self,"idle_frame")
    state_singleton=get_root().get_node("global")
    audio_singleton=get_root().get_node("audioManager")
    var f=File.new()
    assert(f.open("res://input.json",File.READ)==OK)
    var config=JSON.parse(f.get_as_text()).result
    f.close()
    var modes=[]
    for definition in config.cases:
        state_singleton.rumble.clear()
        state_singleton.entering_door=definition.door
        target=load("res://probe.tscn").instance()
        get_root().add_child(target)
        target._party_member.available=definition.available
        target._party_member.incapacitated=definition.incapacitated
        target._paused=definition.paused
        target._climbing=definition.climbing
        target._spinning=definition.spinning
        target._state=definition.state
        target._current_skill_action=definition.skill
        tree=target.get_node("AnimationTree")
        modes.append(tree.process_mode)
        tree.process_mode=AnimationTree.ANIMATION_PROCESS_MANUAL
        tree.active=true
        tree.set("parameters/Bat/Bat/blend_position",Vector2(definition.direction[0],definition.direction[1]))
        tree.set("parameters/Idle/blend_position",Vector2(definition.direction[0],definition.direction[1]))
        tree.advance(0)
        audio_singleton.start_run()
        # Let the fixture's run player actually enter playback before the
        # original stop-if-playing method is tested. No attack time advances.
        yield(self,"idle_frame")
        yield(self,"idle_frame")
        for action in definition.actions:
            if action.op=="input":
                var event=InputEventJoypadButton.new()
                event.button_index=action.button
                event.pressed=action.pressed
                target._input(event)
            elif action.op=="advance":tree.advance(action.delta)
            elif action.op=="flush":yield(self,"idle_frame")
            else:assert(false)
            capture(action)
        assert(state_singleton.rumble.size()<=2)
        target.free()
        yield(self,"idle_frame")
        case_id+=1
    assert(f.open("res://native.json",File.WRITE)==OK)
    f.store_string(JSON.print({"engine":Engine.get_version_info(),"original_process_modes":modes,"rows":rows},"  "))
    f.close()
    quit()
'''

def read_cases(path):
 def unique(pairs):
  out={}
  for k,v in pairs:
   if k in out:raise ValueError('Duplicate swing action fixture field')
   out[k]=v
  return out
 d=json.loads(Path(path).read_text(encoding='utf8'),object_pairs_hook=unique)
 if type(d)is not dict or set(d)!={'schema','scope','cases'}or type(d['schema'])is not int or d['schema']!=1 or type(d['scope'])is not str or not 0<len(d['scope'])<=2048 or type(d['cases'])is not list or not 0<len(d['cases'])<=64:raise ValueError('Swing action case schema rejected')
 for c in d['cases']:
  if type(c)is not dict or set(c)!=set('direction incapacitated available paused door climbing spinning state skill actions'.split())or any(type(c[k])is not bool for k in ('incapacitated','available','paused','door','climbing','spinning'))or type(c['state'])is not int or c['state']not in(0,3)or type(c['skill'])is not str or len(c['skill'])>64 or any(not 32<=ord(x)<127 for x in c['skill']):raise ValueError('Swing action fixture ownership rejected')
  v=c['direction']
  if type(v)is not list or len(v)!=2 or any(type(x)not in(int,float)or not math.isfinite(x)or abs(x)>1 for x in v)or v==[0,0]or type(c['actions'])is not list or not 0<len(c['actions'])<=128:raise ValueError('Swing action fixture direction/actions rejected')
  for a in c['actions']:
   if type(a)is not dict or a.get('op')not in('input','advance','flush'):raise ValueError('Unknown swing action fixture operation')
   if a['op']=='input':
    if set(a)!={'op','button','pressed'}or type(a['button'])is not int or not 0<=a['button']<=31 or type(a['pressed'])is not bool:raise ValueError('Swing action fixture event rejected')
   elif a['op']=='advance':
    if set(a)!={'op','delta'}or type(a['delta'])not in(int,float)or not math.isfinite(a['delta'])or not 0<=a['delta']<=1:raise ValueError('Swing action fixture clock rejected')
   elif set(a)!={'op'}:raise ValueError('Swing action fixture flush rejected')
 return d

def prepare(work):
 p,ex=field_swing.read();raw=ex.text(p['scene']);doc=Document(p['scene'],raw);nodes={}
 for n in doc.nodes:
  parent=n.attrs.get('parent');nodes['.'if parent is None else n.attrs['name']if parent=='.'else parent+'/'+n.attrs['name']]=n
 ap=nodes['AnimationPlayer'];state=nodes['AnimationTree'];machine=doc.sub[state.props['tree_root'].id]
 if machine[0]!='AnimationNodeStateMachine'or machine[1].get('start_node')!='Idle':raise ValueError('Unreviewed swing graph initial state')
 # Keep exactly the two source states and their direct source transitions.
 selected={name:machine[1]['states/'+name+'/node'].id for name in ('Idle','Bat')};transitions=machine[1]['transitions'];edges=[]
 for a,b,ref in zip(transitions[::3],transitions[1::3],transitions[2::3]):
  if a in selected and b in selected:edges.append((a,b,ref.id))
 if[(a,b)for a,b,_ in edges]!=[('Idle','Bat'),('Bat','Idle')]:raise ValueError('Unreviewed swing branch transitions')
 if doc.sub[edges[0][2]]!=('AnimationNodeStateMachineTransition',{})or doc.sub[edges[1][2]]!=('AnimationNodeStateMachineTransition',{'switch_mode':2,'auto_advance':True}):raise ValueError('Unreviewed swing transition policy')
 resources=set();animations={}
 def refs(v):
  if isinstance(v,SubRef):yield v.id
  elif isinstance(v,list):
   for x in v:yield from refs(x)
  elif isinstance(v,dict):
   for x in v.values():yield from refs(x)
 def add(i):
  if i in resources:return
  resources.add(i);kind,props=doc.sub[i]
  for child in refs(props):add(child)
  if kind=='AnimationNodeAnimation':
   name=props['animation'];reference=ap.props['anims/'+name];animations[name]=reference.id;add(reference.id)
 for i in [*selected.values(),*[i for _,_,i in edges],nodes['HitboxPivot/BatHitbox/CollisionShape2D'].props['shape'].id]:add(i)
 blocks=[]
 for match in re.finditer(r'^\[sub_resource type="[^"]+" id=(\d+)\]\n.*?(?=^\[|\Z)',raw,re.M|re.S):
  if int(match[1])in resources:blocks.append(match[0])
 # Source nodes retain all relevant properties. Only material/render owners,
 # the unrelated state branches and the full Player script are out of scope.
 ext={};node_blocks=[]
 for path in ('.','Position','Position/main','SpecialAnimations','SpecialAnimations/NintenBat','HitboxPivot','HitboxPivot/BatHitbox','HitboxPivot/BatHitbox/CollisionShape2D','SpriteDataFetcher2'):
  n=nodes[path];name=n.attrs['name'];parent=n.attrs.get('parent');kind=n.attrs.get('type','Area2D'if path=='HitboxPivot/BatHitbox'else'CollisionShape2D');text='[node name="'+name+'" type="'+kind+'"'+(' parent="'+parent+'"'if parent is not None else'')+']\n'
  for key,value in n.props.items():
   if key in ('material','use_parent_material','__meta__'):continue
   if key=='script'and path in('.','SpriteDataFetcher2')or path=='SpriteDataFetcher2'and key=='_sprite_path':continue
   # Preserve the raw property spelling and collect referenced ext resources.
   block=re.search(r'^\[node name="'+re.escape(name)+r'"[^\n]*'+('parent="'+re.escape(parent)+'"'if parent else'')+r'[^\n]*\]\n(.*?)(?=^\[|\Z)',raw,re.M|re.S)
   if block is None:raise ValueError('Missing source node block: '+path)
   line=re.search(r'^'+re.escape(key)+r' = (.*)$',block[1],re.M)
   if line is None:raise ValueError('Multiline node property outside swing fixture: '+key)
   text+=key+' = '+line[1]+'\n'
   for rid in re.findall(r'ExtResource\( (\d+) \)',line[1]):ext[int(rid)]=doc.ext[int(rid)]
  if path=='.':text+='script = ExtResource( 9001 )\n'
  if path=='SpriteDataFetcher2':text+='script = ExtResource( 9002 )\n'
  if path=='HitboxPivot/BatHitbox':text+='collision_layer = 2\ncollision_mask = 2\n'
  node_blocks.append(text)
 # Animation blocks can refer to textures independently of the selected nodes.
 for text in blocks:
  for rid in re.findall(r'ExtResource\( (\d+) \)',text):ext[int(rid)]=doc.ext[int(rid)]
 header='[gd_scene load_steps='+str(len(resources)+len(ext)+4)+' format=2]\n'
 for rid,(path,kind) in sorted(ext.items()):
  target=work/path;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(ex.data(path));header+='[ext_resource path="res://'+path+'" type="'+kind+'" id='+str(rid)+']\n'
 header+='[ext_resource path="res://player.gd" type="Script" id=9001]\n[ext_resource path="res://fetch.gd" type="Script" id=9002]\n'
 machine_id=state.props['tree_root'].id
 reduced='[sub_resource type="AnimationNodeStateMachine" id='+str(machine_id)+']\n'+''.join('states/'+name+'/node = SubResource( '+str(rid)+' )\n'for name,rid in selected.items())+'transitions = [ '+', '.join('"'+a+'", "'+b+'", SubResource( '+str(i)+' )'for a,b,i in edges)+' ]\nstart_node = "Idle"\n'
 scene=header+''.join(blocks)+reduced+''.join(node_blocks)+'[node name="AudioStreamPlayer" type="AudioStreamPlayer" parent="."]\n[node name="AnimationPlayer" type="AnimationPlayer" parent="."]\n'+''.join('"anims/'+name+'" = SubResource( '+str(i)+' )\n'for name,i in animations.items())+'[node name="AnimationTree" type="AnimationTree" parent="."]\ntree_root = SubResource( '+str(machine_id)+' )\nanim_player = NodePath("../AnimationPlayer")\n'
 check=Document('probe.tscn',scene)
 if any(check.sub[i]!=doc.sub[i]for i in resources):raise ValueError('Swing fixture altered an original resource')
 (work/'probe.tscn').write_text(scene,encoding='utf8')
 code=ex.text(p['script']);symbols={}
 def method(code,name):
  match=re.search(r'^func '+re.escape(name)+r'\(.*?(?=^func |^static func |\Z)',code,re.M|re.S)
  if not match:raise ValueError('Missing original swing method: '+name)
  symbols[name]=hashlib.sha256(match[0].encode()).hexdigest();return match[0]
 wrapper='''extends Node2D
enum {MOVE,ATTACK_PREP,ATTACK,CAMERA,JUMPING,TELEPORTING,SOOT,BOUNCING,LANDING}
class PartyFixture:
    var available=true
    var incapacitated=false
    func has_field_skill(_id):return available
    func is_incapacitated():return incapacitated
var _party_member=PartyFixture.new()
var _state=MOVE
var _current_skill_action=""
var _paused=false
var _climbing=false
var _spinning=false
var _is_continuous_damage=false
var _crouch=true
var _idle=true
var _tap_run=true
var _running=true
var _walk=true
var run_sound="wood"
var camera
var _direction=Vector2(0,1)
onready var _anim_tree=$AnimationTree
onready var _anim_state=_anim_tree.get("parameters/playback")
func blend_animation(_param,_direction):assert(false)
'''
 (work/'player.gd').write_text(wrapper+''.join(method(code,name)for name in METHODS),encoding='utf8')
 fetch=ex.text(p['fetcher']);(work/'fetch.gd').write_text('class_name SpriteDataFetcher\nextends Node\nonready var _sprite_node=get_node("../SpecialAnimations")\n'+method(fetch,'get_frames'),encoding='utf8')
 for path in (p['sound'],'Audio/Sound effects/Footsteps/wood.mp3'):
  target=work/path;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(ex.data(path))
 (work/'global.gd').write_text('extends Node\nvar entering_door=false\nvar can_pause=false\nvar rumble=[]\nfunc get_player():return get_tree().get_root().get_node("Player")\nfunc start_joy_vibration(a,b,c,d):rumble.append([a,b,c,d])\n',encoding='utf8')
 (work/'globaldata.gd').write_text('extends Node\nfunc get_all_field_skills():return ["swing"]\nfunc get_field_skill(_id):return {"is_skill_button":true}\n',encoding='utf8')
 (work/'audio.gd').write_text('extends Node\nvar player=AudioStreamPlayer.new()\nfunc _ready():\n    add_child(player)\n    player.stream=load("res://Audio/Sound effects/Footsteps/wood.mp3")\nfunc get_sfx(_id):return player\nfunc play_sfx(_stream,_id):assert(false)\nfunc start_run():player.play()\n',encoding='utf8')
 inputs=re.search(r'^\[input\]\n.*?(?=^\[|\Z)',ex.text('project.godot'),re.M|re.S)
 if not inputs:raise ValueError('Source input map missing')
 (work/'project.godot').write_text('config_version=4\n[application]\nconfig/name="Original swing action branch"\n[autoload]\nglobal="*res://global.gd"\nglobaldata="*res://globaldata.gd"\naudioManager="*res://audio.gd"\n[rendering]\nquality/driver/driver_name="GLES2"\n[debug]\ngdscript/warnings/unused_argument=false\n'+inputs[0],encoding='utf8')
 return dict(sources=ex.sources,symbols=symbols,resources=sorted(resources),edges=edges,scope=__doc__)

def run(work,reports,engine,cases):
 work=work.resolve();reports=reports.resolve();engine=engine.resolve()
 if work.exists()or reports.exists()or not work.is_relative_to(ROOT/'build')or not reports.is_relative_to(ROOT/'reports'):raise ValueError('Fresh owned swing action reference directories required')
 definitions=read_cases(cases)
 work.mkdir(parents=True);reports.mkdir(parents=True);proof=prepare(work);(work/'input.json').write_text(json.dumps(definitions),encoding='utf8');(work/'probe.gd').write_text(PROBE,encoding='utf8')
 invoke(engine,work,['--editor','--quit'],reports/'import.log');invoke(engine,work,['--fixed-fps','60','--script',str(work/'probe.gd')],reports/'godot.log')
 actual=json.loads((work/'native.json').read_bytes());count=sum(len(c['actions'])for c in definitions['cases'])
 if actual['engine']['string']!='3.6.2-stable (official)'or len(actual['rows'])!=count or actual['original_process_modes']!=[1]*len(definitions['cases']):raise ValueError('Swing action native coverage/process mode differs')
 expected=[(i,a)for i,c in enumerate(definitions['cases'])for a in c['actions']]
 for row,(i,a)in zip(actual['rows'],expected):
  if row['case']!=i or row['op']['op']!=a['op']:raise ValueError('Swing action native input ownership differs')
  row['op']=a
 lines=[]
 for row,(i,a)in zip(actual['rows'],expected):
  c=definitions['cases'][i];operation={'input':1,'advance':2,'flush':3}[a['op']];value=a.get('button',a.get('delta',0));flags=sum(int(row[k])<<bit for bit,k in enumerate(('crouch','idle','tap_run','running','walk')));selected=0 if row['skill']==''else 1 if row['skill']=='swing'else 2
  # Transport original clocks rather than rounded JSON numbers. Physics and
  # audio playback clocks are intentionally outside this stable projection.
  lines.append(' '.join(map(str,[i,*c['direction'],int(c['incapacitated']),int(c['available']),int(c['paused']),int(c['door']),int(c['climbing']),int(c['spinning']),int(c['state']==0),0 if not c['skill']else 1 if c['skill']=='swing'else 2,operation,value,int(a.get('pressed',False)),row['frame'],row['main_frame'],*[int(row[k])for k in ('main','special','hitbox','behind','bat_visible')],row['bat_frame'],row['pivot'],int(row['attack']),row['rumble'],int(row['node']=='Bat'),selected,flags,row['time'],int(bool(row['sound']))])))
 (reports/'reference.tsv').write_text('\n'.join(lines)+'\n',encoding='utf8')
 sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
 (reports/'reference.json').write_text(json.dumps(actual,indent=2)+'\n',encoding='utf8');proof.update(engine=actual['engine'],engine_sha256=sha(engine),case_sha256=sha(cases),observations=count,tools={Path(__file__).relative_to(ROOT).as_posix():sha(Path(__file__))},fixture_sources={p.relative_to(work).as_posix():sha(p)for p in work.iterdir()if p.suffix in('.gd','.tscn','.json','.godot')},outputs={p.name:sha(p)for p in reports.iterdir()if p.is_file()});(reports/'source.json').write_text(json.dumps(proof,indent=2)+'\n',encoding='utf8');print('Original swing action:',count,'logical input/AnimationTree observations; isolated source branch')
if __name__=='__main__':
 a=argparse.ArgumentParser();a.add_argument('--work',type=Path,required=True);a.add_argument('--reports',type=Path,required=True);a.add_argument('--engine',type=Path,required=True);a.add_argument('--cases',type=Path,required=True);o=a.parse_args();run(o.work,o.reports,o.engine,o.cases)
