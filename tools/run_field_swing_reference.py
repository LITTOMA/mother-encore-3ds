"""Run unchanged source attack animations and finish method in real Godot.

An isolated typed numeric scene; no claim of the full Player/AnimationTree,
input, rendering, overlap dispatch, audio or rumble implementation.
"""
import argparse,hashlib,json,re,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.field_swing import read,source
from tools.godot_text import Document
from tools.run_scene_reference import invoke

PROBE='''extends SceneTree
var target
var anim
var rows=[]
func _init():
    call_deferred("run")
func capture(action):
    var special=target.get_node("SpecialAnimations")
    var bat=special.get_node("NintenBat")
    bat._process(0)
    rows.append({"index":rows.size(),"action":action,"position":"%.17f" % anim.current_animation_position,"frame":special.frame,"main":target.get_node("Position/main").visible,"special":special.visible,"hitbox":not target.get_node("HitboxPivot/BatHitbox/CollisionShape2D").disabled,"behind":bat.show_behind_parent,"bat_frame":bat.frame,"pivot":"%.17f" % target.get_node("HitboxPivot").rotation_degrees,"playing":anim.is_playing(),"attack":target._state==1,"tap_run":target._tap_run})
func run():
    yield(self,"idle_frame")
    var file=File.new()
    assert(file.open("res://input.json",File.READ)==OK)
    var config=JSON.parse(file.get_as_text()).result
    file.close()
    target=load("res://probe.tscn").instance()
    anim=target.get_node("AnimationPlayer")
    anim.playback_process_mode=AnimationPlayer.ANIMATION_PROCESS_MANUAL
    get_root().add_child(target)
    for action in config.actions:
        if action.op=="play":
            target._state=1
            target._tap_run=false
            anim.play(action.name)
        elif action.op=="advance":anim.advance(action.delta)
        elif action.op=="flush":yield(self,"idle_frame")
        else:assert(false)
        capture(action)
    var output=File.new()
    assert(output.open("res://../native.json",File.WRITE)==OK)
    output.store_string(JSON.print({"engine":Engine.get_version_info(),"rows":rows},"  "))
    output.close()
    target.free()
    yield(self,"idle_frame")
    quit()
'''

def run(out,engine):
 out=Path(out).resolve();engine=Path(engine).resolve()
 if out.exists()or not out.is_relative_to(ROOT/'build')or not engine.is_file():raise ValueError('Fresh owned swing reference directory and actual Godot required')
 p,ex=read();r=source();
 if 'gdscript/warnings/unused_argument=false'not in ex.text('project.godot'):raise ValueError('Source warning profile differs')
 raw=ex.text(p['scene']);doc=Document(p['scene'],raw);ap=next(n for n in doc.nodes if n.attrs['name']=='AnimationPlayer');project=out/'project';project.mkdir(parents=True)
 copied={}
 for path in (p['texture'],p['bat_texture'],p['bat_script']):
  target=project/path;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(ex.data(path));copied[path]=hashlib.sha256(target.read_bytes()).hexdigest()
 def method(path,name):
  match=re.search(r'^func '+re.escape(name)+r'\(.*?(?=^func |\Z)',ex.text(path),re.M|re.S)
  if not match:raise ValueError('Native source method missing: '+name)
  return match[0]
 finish=method(p['script'],'_attack_animation_finished');fetch=method(p['fetcher'],'get_frames')
 (project/'player.gd').write_text('extends Node2D\nconst MOVE=0\nvar _state=1\nvar _tap_run=false\nfunc get_current_skill_action():\n    return "swing"\n'+finish,encoding='utf8')
 (project/'fetch.gd').write_text('class_name SpriteDataFetcher\nextends Node\nonready var _sprite_node=get_node("../SpecialAnimations")\n'+fetch,encoding='utf8')
 (project/'global.gd').write_text('extends Node\nfunc get_player():\n    return get_tree().get_root().get_node("Player")\n',encoding='utf8')
 original=[]
 for clip in p['clips']:
  rid=ap.props['anims/'+clip['name']].id;match=re.search(r'^\[sub_resource type="Animation" id='+str(rid)+r'\]\n.*?(?=^\[|\Z)',raw,re.M|re.S)
  if not match:raise ValueError('Source animation block missing')
  original.append(match[0])
 geom=next(n for n in doc.nodes if n.attrs['name']=='CollisionShape2D'and n.attrs.get('parent')=='HitboxPivot/BatHitbox')
 rid=geom.props['shape'].id;shape=re.search(r'^\[sub_resource type="CapsuleShape2D" id='+str(rid)+r'\]\n.*?(?=^\[|\Z)',raw,re.M|re.S)
 if not shape:raise ValueError('Source capsule block missing')
 original.append(shape[0]);real=r['real']
 scene='[gd_scene load_steps=15 format=2]\n'+''.join('[ext_resource path="res://'+path+'" type="'+kind+'" id='+str(i)+']\n'for i,path,kind in [(4,p['texture'],'Texture'),(18,p['bat_texture'],'Texture'),(19,p['bat_script'],'Script'),(2,'player.gd','Script'),(17,'fetch.gd','Script')])+''.join(original)
 def vec(a,b):return 'Vector2( '+str(a)+', '+str(b)+' )'
 scene+='[node name="Player" type="Node2D"]\nscript = ExtResource( 2 )\n[node name="Position" type="Node2D" parent="."]\n[node name="main" type="Sprite" parent="Position"]\n[node name="SpecialAnimations" type="Sprite" parent="."]\nvisible = false\nposition = '+vec(*real[2:4])+'\ntexture = ExtResource( 4 )\nhframes = 10\nvframes = 10\nframe = '+str(r['policy'][1])+'\n[node name="NintenBat" type="Sprite" parent="SpecialAnimations"]\ntexture = ExtResource( 18 )\nhframes = 10\nvframes = 10\nscript = ExtResource( 19 )\n[node name="HitboxPivot" type="Position2D" parent="."]\nposition = '+vec(*real[4:6])+'\nrotation = '+str(real[6])+'\n[node name="BatHitbox" type="Area2D" parent="HitboxPivot"]\ncollision_layer = 2\ncollision_mask = 2\nposition = '+vec(*real[7:9])+'\nrotation = '+str(real[9])+'\n[node name="CollisionShape2D" type="CollisionShape2D" parent="HitboxPivot/BatHitbox"]\nposition = '+vec(*real[10:12])+'\nrotation = '+str(real[12])+'\nshape = SubResource( '+str(rid)+' )\ndisabled = true\n[node name="SpriteDataFetcher2" type="Node" parent="."]\nscript = ExtResource( 17 )\n[node name="AnimationPlayer" type="AnimationPlayer" parent="."]\n'+''.join('"anims/'+c['name']+'" = SubResource( '+str(ap.props['anims/'+c['name']].id)+' )\n'for c in p['clips'])
 (project/'probe.tscn').write_bytes(scene.encode('utf8'))
 check=Document('probe.tscn',scene)
 for c in p['clips']:
  rid=ap.props['anims/'+c['name']].id
  if check.sub[rid]!=doc.sub[rid]:raise ValueError('Native swing clip changed')
 actions=[]
 for c in r['clips']:
  actions.extend([{'op':'play','name':c['name']},{'op':'advance','delta':0}]);last=0
  for now in sorted({k['time']for k in c['keys']}):
   if now>last:actions.append({'op':'advance','delta':now-last});last=now
   actions.append({'op':'advance','delta':0.00000001});last+=0.00000001
  actions.extend([{'op':'advance','delta':1},{'op':'flush'}])
  actions.extend([{'op':'play','name':c['name']}]+[{'op':'advance','delta':1/60}for _ in range(22)]+[{'op':'flush'}])
 config={'actions':actions};(project/'input.json').write_bytes((json.dumps(config,indent=2)+'\n').encode('utf8'));(project/'probe.gd').write_text(PROBE,encoding='utf8')
 (project/'project.godot').write_text('config_version=4\n[application]\nconfig/name="Swing source numeric probe"\n[autoload]\nglobal="*res://global.gd"\n[rendering]\nquality/driver/driver_name="GLES2"\n[debug]\ngdscript/warnings/unused_argument=false\n',encoding='utf8')
 invoke(engine,project,['--debug','--editor','--quit'],out/'import.log');invoke(engine,project,['--debug','--script',str(project/'probe.gd')],out/'godot.log')
 actual=json.loads((out/'native.json').read_bytes());require_count=len(actions)
 version=actual['engine']
 if(version['major'],version['minor'],version['patch'],version['hash'])!=(3,6,2,'3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8'):raise ValueError('Unreviewed source engine')
 if len(actual['rows'])!=require_count:raise ValueError('Incomplete native swing observations')
 # JSON.print rounds action numbers: preserve original clocks in transport.
 for index,(row,action)in enumerate(zip(actual['rows'],actions)):
  if row['index']!=index or row['action']['op']!=action['op']or(action['op']=='play'and row['action']['name']!=action['name']):raise ValueError('Native swing input sequence differs')
  row['action']=action
 ids={c['name']:c['id']for c in r['clips']};lines=['op value frame main special hitbox behind bat_frame pivot playing attack position']
 for row in actual['rows']:
  a=row['action'];value=ids[a['name']]if a['op']=='play'else repr(a['delta'])if a['op']=='advance'else 0
  lines.append(' '.join(map(str,[a['op'],value,row['frame'],*[int(row[k])for k in ('main','special','hitbox','behind')],row['bat_frame'],row['pivot'],int(row['playing']),int(row['attack']),row['position']])))
 (out/'states.tsv').write_text('\n'.join(lines)+'\n',encoding='utf8')
 (out/'reference.json').write_text(json.dumps(actual,indent=2)+'\n',encoding='utf8')
 (out/'receipt.json').write_text(json.dumps(dict(scope=__doc__,input_sha256=hashlib.sha256((project/'input.json').read_bytes()).hexdigest(),probe_sha256=hashlib.sha256((project/'probe.gd').read_bytes()).hexdigest(),source_project_sha256=hashlib.sha256(ex.data('project.godot')).hexdigest(),engine_sha256=hashlib.sha256(engine.read_bytes()).hexdigest(),copied=copied,sources=ex.sources,scene_sha256=hashlib.sha256((project/'probe.tscn').read_bytes()).hexdigest(),rows=len(actions)),indent=2)+'\n',encoding='utf8')
 print('Native source swing:',len(actions),'observations; isolated property/callback scope')
 return out
if __name__=='__main__':
 a=argparse.ArgumentParser();a.add_argument('--out',type=Path,required=True);a.add_argument('--engine',type=Path,required=True);o=a.parse_args();run(o.out,o.engine)
