"""Unchanged original KeyNumber script and real native SceneTreeTween steps.

The HBox/Label use native Godot nodes and source origin. Region/counter storage
is an explicit service; font/icon pixels and minimum-size layout are excluded.
No pause flag or wall-clock wait changes tween.is_running in this probe.
"""
import argparse,hashlib,json,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import dungeon_key_indicator as d
from tools.run_battle_victory_reference import function
from tools.run_dungeon_keys_reference import GLOBAL
from tools.run_scene_reference import invoke
PROBE='''extends Node
var config
var hud
var rows=[]
var stepped={}
var append_attempts=[]
func capture(ci,pi):
    var panel=hud.get_node("HBoxContainer")
    rows.append({"case":ci,"phase":pi,"x":panel.rect_position.x,"y":panel.rect_position.y,"visible":panel.visible,"open":hud._is_open,"text":panel.get_node("Money").text,"running":hud._tween.is_running() if hud._tween else false})
func _ready():call_deferred("run")
func run():
    var f=File.new()
    if f.open("res://input.json",File.READ)!=OK:
        get_tree().quit(2)
        return
    config=JSON.parse(f.get_as_text()).result
    f.close()
    for ci in config.cases.size():
        hud=load("res://source.gd").new()
        var panel=HBoxContainer.new()
        panel.name="HBoxContainer"
        panel.rect_position=Vector2(config.parameters[0],config.parameters[1])
        var label=Label.new()
        label.name="Money"
        label.text=config.initial_text
        panel.add_child(label)
        hud.add_child(panel)
        get_tree().root.add_child(hud)
        uiManager._key=hud
        globaldata.keys={}
        stepped={}
        capture(ci,-1)
        for pi in config.cases[ci].size():
            var operation=config.cases[ci][pi]
            print("KEY_HUD_OPERATION_BEGIN ",ci," ",pi)
            if operation.op=="update":
                if operation.count>0 and hud.get_node("HBoxContainer").rect_position.x==config.parameters[2] and hud._tween and hud._tween.is_running() and stepped.has(hud._tween.get_instance_id()):
                    append_attempts.append({"case":ci,"phase":pi})
                globaldata.keys[config.region]=operation.count
                uiManager.update_key_indicator()
            elif operation.op=="close":uiManager.close_key_indicator()
            elif operation.op=="step":
                if hud._tween:
                    stepped[hud._tween.get_instance_id()]=true
                    hud._tween.custom_step(operation.delta)
            else:
                push_error("Unknown key HUD reference action")
                get_tree().quit(3)
                return
            capture(ci,pi)
            print("KEY_HUD_OPERATION_END ",ci," ",pi)
        if hud._tween:hud._tween.kill()
        hud.free()
    f.open("res://../indicator.json",File.WRITE)
    f.store_string(JSON.print({"engine":Engine.get_version_info(),"rows":rows,"append_attempts":append_attempts},"  "))
    f.close()
    print("KEY_INDICATOR_NATIVE_COMPLETE rows=",rows.size())
    get_tree().quit(17)
'''

def cases():
 def u(n):return dict(op='update',count=n)
 def t(n):return dict(op='step',delta=n)
 def c():return dict(op='close')
 return[
  [u(0),u(1),t(0),t(.1),u(2),t(.1),t(0),u(3),t(0),t(.05),t(.05),t(.2),u(0),t(.1),u(2),t(.2),c(),t(.2),t(.01)],
  [u(1),t(.2),u(2),t(.05),u(3),t(.05),t(.1),t(.1),t(.3),u(0),c(),t(.2),u(0)],
  [u(1),t(.2),u(2),t(.1),u(0),t(.2),u(0),u(1),t(.2),t(0)],
  [c(),t(1),u(0),t(.2)],
  [u(1),t(.1),c(),t(.05),u(2),t(.05),c(),t(.1),u(3),t(.2),t(0),u(1),u(2),u(3),t(.9),t(0)],
  [u(1),t(.2),u(2),t(0),u(3),t(.3)],
  [u(1),t(.2),c(),t(0),u(2),t(.2)],
  [u(1),t(.2),c(),u(2),u(3),t(.2),t(.05),t(.05),t(.2)]
 ]

def run(out,engine):
 out=Path(out).resolve();engine=Path(engine).resolve();d.require(out.is_relative_to(ROOT/'build')and not out.exists()and engine.is_file(),'Fresh original key HUD reference required')
 review,source=d.read();d.require(hashlib.sha256(engine.read_bytes()).hexdigest()==review['engine']['native_sha256'],'Key HUD native engine identity differs');model,proof=d.source_model();policy=d.keys.parse(model['keys']);config={k:v for k,v in model.items()if k!='keys'};config.update(region=policy['region'],cases=cases())
 project=out/'project';project.mkdir(parents=True);(project/'.owned-key-indicator-reference').write_text('source-key-indicator-reference-v1',encoding='ascii')
 raw=source.data(review['paths']['script']);(project/'source.gd').write_bytes(raw);(project/'probe.gd').write_text(PROBE,encoding='utf8');(project/'input.json').write_text(json.dumps(config,indent=2)+'\n',encoding='utf8')
 (project/'global.gd').write_text(GLOBAL,encoding='utf8');(project/'data.gd').write_text('extends Node\nvar keys={}\n',encoding='utf8')
 key_review,_=d.keys.read();text=source.text(key_review['paths']['ui_script']);methods='\n'.join(function(text,n)for n in['get_key_count','update_key_indicator','close_key_indicator']);(project/'ui.gd').write_text('extends Node\nvar _key\n'+methods,encoding='utf8')
 (project/'probe.tscn').write_text('[gd_scene load_steps=2 format=2]\n[ext_resource path="res://probe.gd" type="Script" id=1]\n[node name="Probe" type="Node"]\nscript = ExtResource( 1 )\n',encoding='utf8')
 (project/'project.godot').write_text('config_version=4\n[application]\nconfig/name="Original key indicator state"\nrun/main_scene="res://probe.tscn"\n[autoload]\nglobal="*res://global.gd"\nglobaldata="*res://data.gd"\nuiManager="*res://ui.gd"\n',encoding='utf8')
 invoke(engine,project,['--debug','--editor','--quit'],out/'import.log');invoke(engine,project,[],out/'engine.log',expected_error="Can't append to a SceneTreeTween that has started")
 log=(out/'engine.log').read_text('utf8');engine_errors=[line for line in log.splitlines()if line.startswith('ERROR:')];script_errors=[line for line in log.splitlines()if line.startswith('SCRIPT ERROR:')]
 d.require(engine_errors==["ERROR: Can't append to a SceneTreeTween that has started. Use stop() first."]*3 and script_errors==["SCRIPT ERROR: Attempt to call function 'from' in base 'null instance' on a null instance."]*3 and'WARNING:'not in log,'Key HUD unexpected original diagnostic')
 observed=json.loads((out/'indicator.json').read_text('utf8'));d.require(observed['engine']['string']=='3.6.2-stable (official)'and len(observed['rows'])==sum(len(c)+1 for c in config['cases'])and observed['append_attempts']==[dict(case=1,phase=4),dict(case=5,phase=4),dict(case=6,phase=4)],'Key indicator original coverage/engine/diagnostic trigger')
 d.require((project/'source.gd').read_bytes()==raw,'Key HUD original script changed');(out/'input.json').write_bytes((project/'input.json').read_bytes());proof.update(engine_sha256=hashlib.sha256(engine.read_bytes()).hexdigest(),producer_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),indicator_sha256=hashlib.sha256((out/'indicator.json').read_bytes()).hexdigest(),input_sha256=hashlib.sha256((out/'input.json').read_bytes()).hexdigest(),cases=len(config['cases']),rows=len(observed['rows']),adapters={p.name:hashlib.sha256(p.read_bytes()).hexdigest()for p in project.glob('*.gd')})
 proof.update(expected_negative_exit=17,expected_original_diagnostics=engine_errors+script_errors)
 (out/'receipt.json').write_text(json.dumps(proof,indent=2)+'\n',encoding='utf8');print('Original key HUD:',proof['cases'],'cases;',proof['rows'],'native observations;three individually mapped source append failures and paired script errors;state/tweens only,no fonts/icon/rendering claim')
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',type=Path,required=True);p.add_argument('--godot',type=Path,required=True);a=p.parse_args();run(a.out,a.godot)
