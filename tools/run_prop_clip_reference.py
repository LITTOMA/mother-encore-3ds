"""Source prop animations in official Godot; manual AnimationPlayer clock.

ButtonPrompt/autoload services are stubs. Its visual material is explicitly
quarantined from a numeric scene copy; source animations, shapes and script
remain exact. The original scene is retained alongside that copy.
This is not the original map, an audible test, or 3DS verification.
"""
import argparse,hashlib,json,re,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.prop_clips import read,source
from tools.run_scene_reference import invoke

PROBE='''extends SceneTree
var target
var player
var rows=[]
var completed=[]
func _init():
    call_deferred("run")
func finished(name):
    completed.append(name)
func capture(action):
    rows.append({"index":rows.size(),"action":action,"position":"%.17f" % player.current_animation_position,"frame":target.get_node("Sprite").frame,"visible":target.get_node("Sprite").visible,"body":not target.get_node("StaticBody2D/CollisionShape2D").disabled,"hitbox":not target.get_node("Hitbox/CollisionShape2D").disabled,"interaction":not target.get_node("interact/CollisionShape2D").disabled,"playing":player.is_playing(),"current":player.current_animation if player.is_playing() else "","completed":completed.duplicate()})
    completed.clear()
func run():
    yield(self,"idle_frame")
    var file=File.new()
    assert(file.open("res://input.json",File.READ)==OK)
    var config=JSON.parse(file.get_as_text()).result
    file.close()
    target=load("res://"+config.scene).instance()
    target.New_parent=NodePath("..")
    player=target.get_node("AnimationPlayer")
    player.playback_process_mode=AnimationPlayer.ANIMATION_PROCESS_MANUAL
    get_root().add_child(target)
    player.connect("animation_finished",self,"finished")
    for action in config.actions:
        if action.op=="play":player.play(action.name)
        elif action.op=="advance":player.advance(action.delta)
        else:assert(false)
        capture(action)
    var output=File.new()
    assert(output.open("res://../native.json",File.WRITE)==OK)
    output.store_string(JSON.print({"engine":Engine.get_version_info(),"rows":rows},"  "))
    output.close()
    target.free()
    yield(self,"idle_frame")
    yield(self,"idle_frame")
    quit()
'''

def numeric_scene(raw,path):
 from tools.godot_text import Document,SubRef
 doc=Document(path,raw);prompts=[n for n in doc.nodes if n.attrs.get('name')=='ButtonPrompt'and n.attrs.get('parent')=='interact']
 if len(prompts)!=1 or not isinstance(prompts[0].props.get('material'),SubRef):raise ValueError('Unreviewed prompt visual boundary')
 material=prompts[0].props['material'].id;kind,props=doc.sub[material]
 if kind!='ShaderMaterial'or not isinstance(props.get('shader'),SubRef):raise ValueError('Unreviewed prompt material class')
 shader=props['shader'].id
 if doc.sub[shader][0]!='Shader':raise ValueError('Unreviewed prompt shader class')
 adapted=raw;removed=[]
 for rid,kind in ((shader,'Shader'),(material,'ShaderMaterial')):
  pattern=r'^\[sub_resource type="'+kind+r'" id='+str(rid)+r'\]\n.*?(?=^\[|\Z)';adapted,count=re.subn(pattern,'',adapted,flags=re.M|re.S)
  if count!=1:raise ValueError('Ambiguous prompt render resource')
  removed.append({'id':rid,'type':kind})
 adapted,count=re.subn(r'^material = SubResource\( '+str(material)+r' \)\n','',adapted,flags=re.M)
 if count!=1:raise ValueError('Prompt material is shared outside numeric boundary')
 steps=doc.header.attrs['load_steps'];adapted,count=re.subn(r'load_steps='+str(steps)+r'\b','load_steps='+str(steps-2),adapted,count=1)
 if count!=1:raise ValueError('Missing scene resource count')
 result=Document(path,adapted)
 for rid,(kind,properties)in doc.sub.items():
  if rid not in(shader,material)and result.sub.get(rid)!=(kind,properties):raise ValueError('Numeric scene changed source resource')
 return adapted,{'property':'interact/ButtonPrompt:material','resources':removed,'scope':'Prompt rendering only; native GLES2 shader parameter lookup failed in isolated service stub. No graphics/shader equivalence claim.'}

def run(out,engine):
 out=Path(out).resolve();engine=Path(engine).resolve()
 if not out.is_relative_to(ROOT/'build')or out.exists()or not engine.is_file():raise ValueError('Fresh owned reference directory and actual Godot required')
 p,ex=read();r=source();project=out/'project';project.mkdir(parents=True)
 # The complete source scene remains byte-identical; only its service
 # dependencies have explicit isolated stubs at the same resource paths.
 doc=ex.text(p['scene'])
 from tools.godot_text import Document
 scene=Document(p['scene'],doc);copied={};stubbed={};packed=[path for path,kind in scene.ext.values()if kind=='PackedScene']
 if len(packed)!=1:raise ValueError('Only the explicit prompt scene may be stubbed')
 for path,kind in scene.ext.values():
  target=project/path;target.parent.mkdir(parents=True,exist_ok=True)
  if kind=='PackedScene':
   stubbed[path]=hashlib.sha256(ex.data(path)).hexdigest()
   target.write_text('[gd_scene load_steps=2 format=2]\n[ext_resource path="res://prompt.gd" type="Script" id=1]\n[node name="ButtonPrompt" type="Node2D"]\nscript = ExtResource( 1 )\n',encoding='utf8')
  else:
   data=ex.data(path);target.write_bytes(data);copied[path]=hashlib.sha256(data).hexdigest()
 target=project/p['scene'];target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(ex.data(p['scene']));copied[p['scene']]=hashlib.sha256(target.read_bytes()).hexdigest()
 adapted,quarantine=numeric_scene(doc,p['scene']);(project/'prop-probe.tscn').write_bytes(adapted.encode('utf8'))
 (project/'prompt.gd').write_text('extends Node2D\nvar enabled=true\nfunc set_enabled(value):\n    enabled=value\n',encoding='utf8')
 (project/'global.gd').write_text('extends Node\nfunc start_joy_vibration(_a,_b,_c,_d):\n    pass\n',encoding='utf8')
 (project/'globaldata.gd').write_text('extends Node\nvar flags={"bat":true,"cem_roots_grown":false,"got_shovel":false}\n',encoding='utf8')
 (project/'ui.gd').write_text('extends Node\nfunc open_dialogue_box(_name):\n    pass\n',encoding='utf8')
 # Match the source setting for its unchanged unused callback argument; do
 # not suppress arbitrary diagnostics or weaken invoke's error checks.
 if 'gdscript/warnings/unused_argument=false'not in ex.text('project.godot'):raise ValueError('Source warning profile changed')
 (project/'project.godot').write_text('config_version=4\n[application]\nconfig/name="Source prop animation numeric probe"\n[autoload]\nglobal="*res://global.gd"\nglobaldata="*res://globaldata.gd"\nuiManager="*res://ui.gd"\n[debug]\ngdscript/warnings/unused_argument=false\n[rendering]\nquality/driver/driver_name="GLES2"\n[logging]\nfile_logging/enable_logging=false\n',encoding='utf8')
 actions=[]
 for c in r['clips']:
  actions.append(dict(op='play',name=c['name'],id=c['id']))
  times={0,c['duration']/2,c['duration'],c['duration']+.001}
  for k in c['keys']:
   if k['time']>0:times.update((max(0,k['time']-.000001),k['time'],k['time']+.000001))
  previous=0
  for time in sorted(times):actions.append(dict(op='advance',delta=time-previous));previous=time
  # Restart leaves properties intact until the first manual processing pass.
  actions.extend([dict(op='play',name=c['name'],id=c['id']),dict(op='advance',delta=0),dict(op='advance',delta=c['duration']+.01)])
  # Retain a normal 60 Hz clock as well as explicit key boundaries. Native
  # AnimationPlayer accumulates float time; Python/double sampling is not its
  # authoritative state at exact interior keys.
  actions.append(dict(op='play',name=c['name'],id=c['id']))
  actions.extend(dict(op='advance',delta=1/60)for _ in range(62))
 # Interruptions do not emit a completion for the replaced animation.
 actions.extend([dict(op='play',name=r['clips'][2]['name'],id=r['clips'][2]['id']),dict(op='advance',delta=.2),dict(op='play',name=r['clips'][3]['name'],id=r['clips'][3]['id']),dict(op='advance',delta=0),dict(op='advance',delta=1)])
 (project/'input.json').write_text(json.dumps({'scene':'prop-probe.tscn','actions':actions},indent=2)+'\n',encoding='utf8');(project/'probe.gd').write_text(PROBE,encoding='utf8')
 invoke(engine,project,['--debug','--editor','--quit'],out/'import.log')
 invoke(engine,project,['--debug','--script',str(project/'probe.gd')],out/'godot.log')
 native=json.loads((out/'native.json').read_text(encoding='utf8'));v=native['engine']
 if(v['major'],v['minor'],v['patch'],v['hash'])!=(3,6,2,'3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8'):raise ValueError('Unreviewed source engine build')
 if len(native['rows'])!=len(actions):raise ValueError('Incomplete native animation observations')
 ids={c['name']:c['id']for c in r['clips']};lines=['op value frame visible body hitbox interaction playing current completed']
 for i,row in enumerate(native['rows']):
  # Godot JSON.print rounds real echoes to six decimal places. Feed the C++
  # oracle the actual input JSON values, never those rounded diagnostic echoes.
  a=actions[i]
  if row['index']!=i or row['action']['op']!=a['op']or(a['op']=='play'and row['action']['name']!=a['name']):raise ValueError('Native input sequence differs')
  lines.append(' '.join(map(str,[a['op'],a['id']if a['op']=='play'else repr(a['delta']),row['frame'],*[int(row[k])for k in ('visible','body','hitbox','interaction','playing')],ids[row['current']]if row['current']else 0,','.join(str(ids[n])for n in row['completed'])or'-'])))
 (out/'states.tsv').write_text('\n'.join(lines)+'\n',encoding='utf8')
 receipt={'schema':1,'sources':copied,'source_project_sha256':hashlib.sha256(ex.data('project.godot')).hexdigest(),'input_sha256':hashlib.sha256((project/'input.json').read_bytes()).hexdigest(),'numeric_input':'TSV reuses exact input JSON reals; native row index/op verified, rounded Godot JSON.print echoes are diagnostics only','stubbed_source_scenes':stubbed,'quarantine':quarantine,'adapted_scene_sha256':hashlib.sha256((project/'prop-probe.tscn').read_bytes()).hexdigest(),'engine_sha256':hashlib.sha256(engine.read_bytes()).hexdigest(),'probe_sha256':hashlib.sha256((project/'probe.gd').read_bytes()).hexdigest(),'rows':len(native['rows']),'scope':__doc__}
 (out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf8');print('Native prop animation:',len(native['rows']),'observations; isolated source scene only')
 return out

if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--out',type=Path,required=True);p.add_argument('--godot',type=Path,required=True);a=p.parse_args();run(a.out,a.godot)
