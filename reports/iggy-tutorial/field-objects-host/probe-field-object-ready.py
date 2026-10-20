from pathlib import Path
import hashlib,json,os,subprocess
r=Path(__file__).resolve().parents[2];up=r/'upstream/MOTHER-Encore';out=r/'build/full-port/field-objects-reference-1';out.mkdir(exist_ok=False)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
exe=r/'build/godot-reference/Godot_v3.6.2-stable_win64.exe';assert sha(exe)=='e748acc92ee5205dd3cc1754d4422870f6a68ae6e00a16ff88bd3438c957adfd'
source=up/'Scripts/Main/DroppedItem.gd';script=source.read_text();ready=script[script.index('func _ready():'):script.index('func _set_item(')]
sparkles=up/'Scripts/misc/sparkles.gd';(out/'sparkles.gd').write_bytes(sparkles.read_bytes())
life='''extends Sprite
var ready_flag=false
func _get_flag_status():
    return ready_flag
func _update_sprite():
    pass
func _on_player_paused():
    pass
func _on_player_unpaused():
    pass
'''+ready
(out/'life.gd').write_text(life,encoding='utf8')
(out/'global.gd').write_text('''extends Node
signal paused
signal unpaused
func get_player():
    return self
''',encoding='utf8')
(out/'project.godot').write_text('''config_version=4
[application]
config/name="Source-owned object ready and AnimatedSprite probe"
[autoload]
global="*res://global.gd"
[logging]
file_logging/enable_logging=false
[physics]
common/physics_fps=60
''',encoding='utf8')
(out/'probe.gd').write_text('''extends SceneTree
var rows=[]
var live
func make_item(value):
    var parent=load("res://life.gd").new()
    parent.ready_flag=value
    var effect=AnimatedSprite.new()
    effect.name="Sparkles"
    var frames=SpriteFrames.new()
    for i in range(40):
        frames.add_frame("default",ImageTexture.new())
    frames.set_animation_speed("default",12.0)
    frames.set_animation_loop("default",true)
    effect.frames=frames
    effect.frame=22
    effect.playing=true
    effect.set_script(load("res://sparkles.gd"))
    parent.add_child(effect)
    get_root().add_child(parent)
    return parent
func snapshot(phase,tick):
    if is_instance_valid(live):
        rows.append({"phase":phase,"tick":tick,"alive":true,"visible":live.visible,"queued":live.is_queued_for_deletion(),"frame":live.get_node("Sparkles").frame})
    else:
        rows.append({"phase":phase,"tick":tick,"alive":false,"visible":false,"queued":true,"frame":-1})
func _init():
    call_deferred("run")
func run():
    for s in range(32):
        for flag in [false,true]:
            seed(s)
            var obj=make_item(flag)
            rows.append({"phase":"ready","seed":s,"flag":flag,"alive":true,"visible":obj.visible,"queued":obj.is_queued_for_deletion(),"frame":obj.get_node("Sparkles").frame,"next_randi":randi()})
            obj.free()
    seed(9)
    live=make_item(false)
    snapshot("initial",0)
    for tick in range(1,431):
        yield(self,"idle_frame")
        if tick==3:
            live.ready_flag=true
        snapshot("clock",tick)
    live.queue_free()
    snapshot("queue",430)
    yield(self,"idle_frame")
    snapshot("deleted",431)
    seed(9)
    live=make_item(true)
    snapshot("suppressed-ready",0)
    yield(self,"idle_frame")
    snapshot("suppressed-deleted",1)
    var f=File.new()
    if f.open("res://result.json",File.WRITE)!=OK:
        quit(2)
        return
    f.store_string(JSON.print({"engine":Engine.get_version_info(),"rows":rows},"  "))
    f.close()
    quit()
''',encoding='utf8')
startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
with(out/'godot.log').open('wb')as f:
 p=subprocess.run([str(exe),'--no-window','--path',str(out),'-s','probe.gd','--fixed-fps','60','--audio-driver','Dummy'],cwd=out,stdout=f,stderr=subprocess.STDOUT,startupinfo=startup,env=dict(os.environ,APPDATA=str(out/'user-roaming')),timeout=60)
assert p.returncode==0,p.returncode
assert not any(x in(out/'godot.log').read_text(errors='replace')for x in ['SCRIPT ERROR','ERROR:']), (out/'godot.log').read_text(errors='replace')
data=json.loads((out/'result.json').read_bytes());assert data['engine']['hash']=='3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8'
with(out/'reference.tsv').open('w',encoding='utf8',newline='\n')as f:
 f.write('phase\ttick_or_seed\tflag\talive\tvisible\tqueued\tframe\tnext_randi\n')
 for row in data['rows']:f.write('\t'.join(str(v)for v in [row['phase'],row.get('seed',row.get('tick')),int(row.get('flag',False)),int(row['alive']),int(row['visible']),int(row['queued']),row['frame'],row.get('next_randi',0)])+'\n')
proof=dict(engine_sha256=sha(exe),engine=data['engine'],scope='Original sparkles.gd unchanged; original DroppedItem._ready verbatim with isolated Flag/Player/sprite assignment stubs. Actual engine ready ordering, clamp, hidden clock, idle show and deferred deletion only; not full scene, collection, texture assignment or gameplay.',sources={str(p.relative_to(up)):sha(p)for p in[source,sparkles]},ready_function_sha256=hashlib.sha256(ready.encode()).hexdigest(),files_sha256={p.name:sha(p)for p in out.iterdir()if p.is_file()})
(out/'proof.json').write_text(json.dumps(proof,indent=2)+'\n');print('Actual original engine object ready/clock observations:',len(data['rows']),flush=True)
