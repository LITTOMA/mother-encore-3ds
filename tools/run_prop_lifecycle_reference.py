"""Original breakable-prop callbacks with controlled visibility/service inputs.

Reuses the checked numeric prop scene, unchanged script/animations/shapes and
explicit prompt-material quarantine. Visibility signals, UI and rumble are
service boundaries; deferred calls and queue_free use native SceneTree. No map,
rendering, audible, emulator or hardware equivalence is claimed.
"""
import argparse,hashlib,json,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.run_prop_clip_reference import run as prepare
from tools.run_scene_reference import invoke

PROBE='''extends SceneTree
class ViewSignals:
    extends Node2D
    signal screen_entered
    signal screen_exited
    func enter():emit_signal("screen_entered")
    func exit():emit_signal("screen_exited")
class Callback:
    extends Node
    var runner
    func check_start():
        runner.calls.append({"alive":is_instance_valid(runner.target),"queued":runner.target.is_queued_for_deletion() if is_instance_valid(runner.target) else false})
var target
var parent
var rows=[]
var calls=[]
var current_case=""
var services
var flag_store
var dialogue_service
func _init():
    call_deferred("run")
func capture(action):
    var row={"case":current_case,"action":action,"alive":is_instance_valid(target),"calls":calls.duplicate(true),"rumble":services.events.duplicate(true),"dialogue":dialogue_service.events.duplicate(),"clones":[]}
    if is_instance_valid(target):
        var a=target.get_node("AnimationPlayer")
        row.props={"frame":target.get_node("Sprite").frame,"visible":target.get_node("Sprite").visible,"body":not target.get_node("StaticBody2D/CollisionShape2D").disabled,"hitbox":not target.get_node("Hitbox/CollisionShape2D").disabled,"interaction":not target.get_node("interact/CollisionShape2D").disabled,"node_visible":target.visible,"prompt":target.get_node("interact/ButtonPrompt").enabled,"queued":target.is_queued_for_deletion(),"playing":a.is_playing(),"current":a.current_animation if a.is_playing() else ""}
    for child in parent.get_children():
        if child is Sprite:
            row.clones.append({"frame":child.frame,"visible":child.visible,"x":child.position.x,"y":child.position.y})
    rows.append(row)
    calls.clear()
    services.events.clear()
    dialogue_service.events.clear()
func run():
    yield(self,"idle_frame")
    services=get_root().get_node("global")
    flag_store=get_root().get_node("globaldata")
    dialogue_service=get_root().get_node("uiManager")
    var input=File.new()
    assert(input.open("res://lifecycle-input.json",File.READ)==OK)
    var config=JSON.parse(input.get_as_text()).result
    input.close()
    for test in config.cases:
        current_case=test.name
        flag_store.flags=test.flags.duplicate(true)
        parent=Node2D.new()
        get_root().add_child(parent)
        var callback=Callback.new()
        callback.name="Callback"
        callback.runner=self
        parent.add_child(callback)
        target=load("res://prop-probe.tscn").instance()
        target.position=Vector2(32,64)
        target.New_parent=NodePath("..")
        target.called_object=NodePath("../Callback" if test.callback else "../Missing")
        target.call_object_function="check_start" if test.function else ""
        target.object_function_disappear_flag=test.callback_flag
        target.flag=test.flag
        var original_view=target.get_node("VisibilityNotifier2D")
        target.remove_child(original_view)
        original_view.free()
        var view=ViewSignals.new()
        view.name="VisibilityNotifier2D"
        target.add_child(view)
        target.get_node("AnimationPlayer").playback_process_mode=AnimationPlayer.ANIMATION_PROCESS_MANUAL
        parent.add_child(target)
        capture({"op":"ready"})
        for action in test.actions:
            if action.op=="advance":target.get_node("AnimationPlayer").advance(action.delta)
            elif action.op=="grow":target.grow()
            elif action.op=="hit":target._on_Hitbox_area_entered(null)
            elif action.op=="enter":view.enter()
            elif action.op=="exit":view.exit()
            elif action.op=="interact":target.interact()
            elif action.op=="flag":flag_store.flags[action.id]=action.value
            elif action.op=="flush":
                yield(self,"idle_frame")
                yield(self,"idle_frame")
            else:assert(false)
            capture(action)
        parent.free()
    var output=File.new()
    assert(output.open("res://../lifecycle-native.json",File.WRITE)==OK)
    output.store_string(JSON.print({"engine":Engine.get_version_info(),"rows":rows},"  "))
    output.close()
    yield(self,"idle_frame")
    quit()
'''

def cases():
 def a(op,**values):return dict(op=op,**values)
 common=dict(flags={'bat':False,'cem_roots_grown':False,'got_shovel':False},flag='',callback_flag='',callback=True,function=True)
 rows=[]
 def add(name,actions,**fields):rows.append(dict(common,**fields,name=name,actions=actions))
 add('ready_idle_and_interact',[a('advance',delta=0),a('interact'),a('enter'),a('interact'),a('flag',id='bat',value=True),a('interact'),a('exit'),a('interact')])
 add('ready_hidden_grow',[a('advance',delta=0),a('enter'),a('interact'),a('grow'),a('advance',delta=0),a('advance',delta=.5),a('advance',delta=.5),a('advance',delta=0)],flag='cem_roots_grown')
 add('ready_flag_true',[a('advance',delta=0)],flag='cem_roots_grown',flags={'bat':False,'cem_roots_grown':True,'got_shovel':False})
 add('break_undefined_finish_flag',[a('advance',delta=0),a('enter'),a('hit'),a('advance',delta=0),a('advance',delta=.75),a('flush')])
 add('hidden_hit_preserves_clone_visibility',[a('advance',delta=0),a('hit'),a('advance',delta=1),a('flush')],flag='cem_roots_grown')
 add('double_hit_before_processing',[a('advance',delta=0),a('hit'),a('hit'),a('advance',delta=0),a('advance',delta=1),a('flush')])
 add('grow_interrupted_by_break',[a('advance',delta=0),a('grow'),a('advance',delta=.2),a('hit'),a('advance',delta=1),a('advance',delta=0),a('flush')])
 add('two_grow_waiters',[a('advance',delta=0),a('grow'),a('grow'),a('advance',delta=1),a('advance',delta=0)])
 for name,extra in [('deferred_then_delete',{}),('defined_true_still_delete',{'flags':{'bat':False,'cem_roots_grown':False,'got_shovel':True}}),('missing_target_still_delete',{'callback':False}),('empty_function_still_delete',{'function':False})]:
  add(name,[a('advance',delta=0),a('hit'),a('advance',delta=.75),a('flush')],callback_flag='got_shovel',**extra)
 add('flag_checked_at_finish',[a('advance',delta=0),a('hit'),a('flag',id='got_shovel',value=True),a('advance',delta=1),a('flush')],callback_flag='got_shovel')
 add('grow_waiter_before_deferred_delete',[a('advance',delta=0),a('grow'),a('hit'),a('advance',delta=1),a('advance',delta=0),a('flush')],callback_flag='got_shovel')
 return rows

def run(out,engine):
 out=prepare(out,engine);project=out/'project'
 for name in ('native.json','receipt.json','states.tsv'):(out/name).rename(out/('clip-'+name))
 (project/'global.gd').write_bytes(b'extends Node\nvar events=[]\nfunc start_joy_vibration(a,b,c,d):\n    events.append([a,b,c,d])\n')
 (project/'ui.gd').write_bytes(b'extends Node\nvar events=[]\nfunc open_dialogue_box(name):\n    events.append(name)\n')
 (project/'lifecycle-probe.gd').write_bytes(PROBE.encode())
 config={'cases':cases()};(project/'lifecycle-input.json').write_bytes((json.dumps(config,indent=2)+'\n').encode())
 invoke(Path(engine).resolve(),project,['--debug','--script',str(project/'lifecycle-probe.gd')],out/'lifecycle-godot.log')
 r=json.loads((out/'lifecycle-native.json').read_text(encoding='utf8'));expected=sum(1+len(c['actions'])for c in config['cases'])
 if len(r['rows'])!=expected:raise ValueError('Incomplete native lifecycle observations')
 write_tsv(config,r,out/'lifecycle-states.tsv')
 receipt={'schema':1,'cases':len(config['cases']),'rows':len(r['rows']),'input_sha256':hashlib.sha256((project/'lifecycle-input.json').read_bytes()).hexdigest(),'probe_sha256':hashlib.sha256((project/'lifecycle-probe.gd').read_bytes()).hexdigest(),'script_sha256':hashlib.sha256((project/'Scripts/Main/Dead Bush.gd').read_bytes()).hexdigest(),'clip_reference':json.loads((out/'clip-receipt.json').read_text()),'scope':__doc__}
 (out/'lifecycle-receipt.json').write_bytes((json.dumps(receipt,indent=2)+'\n').encode());print('Native prop lifecycle:',receipt['cases'],'cases,',expected,'observations; controlled source callbacks only')

def write_tsv(config,native,path):
 from tools.prop_clips import source
 ids={c['name']:c['id']for c in source()['clips']};rows=iter(native['rows']);lines=['source-controlled-lifecycle-v1']
 flag_names=['bat','cem_roots_grown','got_shovel']
 for case in config['cases']:
  lines.append('start '+case['name']+' '+' '.join(str(int(v))for v in [bool(case['flag']),bool(case['callback_flag']),case['callback']and case['function'],*[case['flags'][n]for n in flag_names]]))
  for action in [dict(op='ready'),*case['actions']]:
   row=next(rows)
   if row['case']!=case['name']or row['action']['op']!=action['op']:raise ValueError('Native lifecycle input order differs')
   props=row.get('props',{});clones=row['clones'];dialogue=row['dialogue'];rumble=row['rumble'];calls=row['calls']
   if len(dialogue)>1 or any(d not in ('Reusable/hardtobreak','Reusable/easytobreak')for d in dialogue)or any(not c['alive']or not c['queued']for c in calls):raise ValueError('Unreviewed lifecycle observation')
   if any(c['frame']!=1 or(c['x'],c['y'])!=(32,48)for c in clones)or any(r!=[0,.5,.7,.8]for r in rumble):raise ValueError('Source lifecycle geometry/rumble differs')
   a=flag_names.index(action['id'])if action['op']=='flag'else action.get('delta',0);b=int(action.get('value',False))
   fields=[int(row['alive']),*[int(props.get(k,0))for k in ('frame','visible','body','hitbox','interaction','node_visible','prompt','queued','playing')],ids.get(props.get('current'),0),len(clones),sum(int(c['visible'])<<i for i,c in enumerate(clones)),len(rumble),0 if not dialogue else 1 if dialogue[0].endswith('/hardtobreak')else 2,len(calls)]
   lines.append('step '+action['op']+' '+format(a,'.17g')+' '+str(b)+' '+' '.join(map(str,fields)))
 try:next(rows);raise ValueError('Unowned lifecycle observations')
 except StopIteration:pass
 Path(path).write_bytes(('\n'.join(lines)+'\n').encode())

if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--out',type=Path,required=True);p.add_argument('--godot',type=Path,required=True);a=p.parse_args();run(a.out,a.godot)
