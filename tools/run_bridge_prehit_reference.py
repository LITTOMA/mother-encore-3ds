#!/usr/bin/env python3
"""Execute the original PreHitEffect script and native bite Animation in Godot.

Only a minimal scene is copied into an owned temporary project. This is a
timing/visibility oracle, not a full original battle or pixel equivalence test.
"""
from __future__ import annotations
import argparse,hashlib,json,re,shutil,subprocess,sys,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.extract_battle_entry import Extractor,require

PROBE='''extends SceneTree
var elapsed = 0.0
var events = []
func _mark():
    events.append(elapsed)
func _init():
    call_deferred("_run")
func _run():
    var battle = load("res://effect.tscn").instance()
    get_root().add_child(battle)
    var effect = battle.get_node("PreHitEffect")
    var animation = effect.get_node("AnimationPlayer")
    animation.playback_process_mode = AnimationPlayer.ANIMATION_PROCESS_MANUAL
    effect.connect("apply_damage", self, "_mark")
    effect.play("bite")
    var samples = []
    for tick in range(40):
        elapsed = float(tick + 1) / 60.0
        animation.advance(1.0 / 60.0)
        samples.append({"tick": tick+1, "seconds": elapsed, "position": animation.current_animation_position, "visible": effect.visible, "frame": effect.get_node("Sprite").frame})
        yield(self, "idle_frame")
    var output = File.new()
    if output.open("res://result.json", File.WRITE) != OK:
        quit(2)
        return
    output.store_string(to_json({"godot": Engine.get_version_info(), "events": events, "samples": samples}))
    output.close()
    quit(0)
'''

def main():
    p=argparse.ArgumentParser();p.add_argument('--godot',required=True,type=Path);p.add_argument('--out',required=True,type=Path);a=p.parse_args()
    require(a.godot.is_file(),'Original Godot executable required');out=a.out.resolve();require(out.is_relative_to(ROOT/'build') and not out.exists(),'Fresh bounded build output required')
    ex=Extractor(ROOT);scene=ex.text('Nodes/Ui/Battle/Battle.tscn');script='Scripts/UI/Battle/PreHitEffect.gd';code=ex.text(script)
    animation=re.search(r'^\[sub_resource type="Animation" id=84\]\n(.*?)(?=^\[sub_resource |^\[node |\Z)',scene,re.M|re.S)
    require(animation is not None and 'resource_name = "bite"'in animation[1] and 'anims/bite = SubResource( 84 )'in scene,'Unreviewed bite animation binding')
    source='Graphics/Battle Effects/Bite.png';image=ex.data(source)
    out.mkdir(parents=True);temporary=Path(tempfile.mkdtemp(prefix='owned-bite-',dir=out));(temporary/'.owned-bridge-reference').write_text('bridge-prehit-v1',encoding='utf8')
    try:
        (temporary/'project.godot').write_bytes(b'[application]\nconfig/name="Bridge pre-hit source probe"\n[rendering]\nquality/driver/driver_name="GLES2"\n')
        (temporary/'probe.gd').write_bytes(PROBE.encode());(temporary/'effect.gd').write_bytes(code.encode());(temporary/'Bite.png').write_bytes(image)
        tscn='''[gd_scene load_steps=4 format=2]
[ext_resource path="res://effect.gd" type="Script" id=90]
[ext_resource path="res://Bite.png" type="Texture" id=88]
[sub_resource type="Animation" id=84]
'''+animation[1]+'''
[node name="Battle" type="Node"]
[node name="AudioStreamPlayer" type="AudioStreamPlayer" parent="."]
[node name="PreHitEffect" type="Control" parent="."]
visible = false
script = ExtResource( 90 )
[node name="Sprite" type="Sprite" parent="PreHitEffect"]
texture = ExtResource( 88 )
hframes = 11
[node name="AnimationPlayer" type="AnimationPlayer" parent="PreHitEffect"]
anims/bite = SubResource( 84 )
'''
        (temporary/'effect.tscn').write_bytes(tscn.encode());startup=None
        if sys.platform=='win32':startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
        # Import the original PNG in this isolated project before running it.
        for label,args in [('import',['--editor','--quit']),('run',['--script','res://probe.gd'])]:
            with(out/(label+'.log')).open('wb')as log:
                result=subprocess.run([str(a.godot.resolve()),'--path',str(temporary),*args],stdout=log,stderr=subprocess.STDOUT,startupinfo=startup,timeout=40)
            require(result.returncode==0,'Godot '+label+' failed; see retained actual log')
        result=json.loads((temporary/'result.json').read_text(encoding='utf8'))
        # Preserve actual native observations even when a later assertion fails.
        (out/'result.json').write_bytes((json.dumps(result,indent=2)+'\n').encode())
        require(result['godot']['major']==3 and result['godot']['minor']==6 and result['godot']['patch']==2,'Wrong reference engine')
        require(len(result['events'])==1 and abs(result['events'][0]-.25)<=1/60+.00001,'Unexpected original bite damage signal')
        require(any(s['visible']for s in result['samples']) and not result['samples'][-1]['visible'],'Original bite visibility lifetime')
        result.update(scope='Original extracted bite Animation and unchanged PreHitEffect.gd, manually advanced by native Godot at 60 Hz; minimal scene, not full battle',sources=ex.sources,probe_sha256=hashlib.sha256(PROBE.encode()).hexdigest(),godot_sha256=hashlib.sha256(a.godot.read_bytes()).hexdigest())
        (out/'result.json').write_bytes((json.dumps(result,indent=2)+'\n').encode());print('Native original bite:',result['events'],'damage signal; isolated project removed')
    finally:
        require(temporary.parent==out and (temporary/'.owned-bridge-reference').is_file(),'Temporary source project ownership changed')
        shutil.rmtree(temporary)

if __name__=='__main__':
    try:main()
    except (ValueError,KeyError,OSError,subprocess.SubprocessError)as e:print('BRIDGE REFERENCE ERROR:',e,file=sys.stderr);raise SystemExit(1)
