from pathlib import Path
import hashlib,json,os,struct,subprocess
r=Path(__file__).resolve().parents[2];source=r/'build/full-port/ready-rng-helper-1';out=r/'build/full-port/ready-rng-reference-1';out.mkdir(exist_ok=False);exe=r/'build/godot-reference/Godot_v3.6.2-stable_win64.exe';sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();assert sha(exe)=='e748acc92ee5205dd3cc1754d4422870f6a68ae6e00a16ff88bd3438c957adfd'
input_data=json.loads((source/'input.json').read_bytes());ops=input_data['ops'];prediction=json.loads((source/'prediction.json').read_bytes());(out/'input.json').write_bytes((source/'input.json').read_bytes());(out/'project.godot').write_text('config_version=4\n[logging]\nfile_logging/enable_logging=false\n',encoding='utf8')
(out/'probe.gd').write_text('''extends SceneTree
func _init():
    var input=File.new()
    if input.open("res://input.json",File.READ)!=OK:
        quit(2)
        return
    var parsed=JSON.parse(input.get_as_text())
    input.close()
    if parsed.error!=OK:
        quit(3)
        return
    var ops=parsed.result.ops
    var output=File.new()
    if output.open("res://reference.bin",File.WRITE)!=OK:
        quit(4)
        return
    output.store_buffer("ERDY362\\n".to_ascii())
    output.store_32(ops.size())
    var mirror=RandomNumberGenerator.new()
    seed(0)
    mirror.seed=0
    for index in range(ops.size()):
        var op=ops[index]
        var value=0.0
        var kind=0
        if op.kind=="Seed":
            seed(int(op.seed))
            mirror.seed=int(op.seed)
        elif op.kind=="Randi":
            kind=1
            value=randi()
            if value!=mirror.randi():
                quit(5)
                return
        elif op.kind=="Randf":
            kind=2
            value=randf()
            mirror.randi()
        elif op.kind=="RandRange":
            kind=3
            value=rand_range(op.low,op.high)
            var exponent=mirror.randi()
            if exponent!=0:
                mirror.randi()
                mirror.randi()
        else:
            quit(6)
            return
        output.store_32(index)
        output.store_32(kind)
        output.store_double(value)
        output.store_64(mirror.state)
    output.store_64(mirror.state)
    var actual_next=randi()
    var mirror_next=mirror.randi()
    output.store_32(actual_next)
    output.store_32(mirror_next)
    output.close()
    var meta=File.new()
    meta.open("res://engine.json",File.WRITE)
    meta.store_string(JSON.print(Engine.get_version_info()))
    meta.close()
    if actual_next!=mirror_next:
        quit(7)
    else:
        quit()
''',encoding='utf8')
startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
with(out/'godot.log').open('wb')as f:p=subprocess.run([str(exe),'--no-window','--path',str(out),'-s','probe.gd','--audio-driver','Dummy'],cwd=out,stdout=f,stderr=subprocess.STDOUT,env=dict(os.environ,APPDATA=str(out/'user-roaming')),startupinfo=startup,timeout=60)
assert p.returncode==0,p.returncode;assert not any(x in(out/'godot.log').read_text(errors='replace')for x in ['SCRIPT ERROR','ERROR:'])
engine=json.loads((out/'engine.json').read_bytes());assert engine['hash']=='3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8';raw=(out/'reference.bin').read_bytes();magic,count=struct.unpack_from('<8sI',raw);assert magic==b'ERDY362\n'and count==len(ops)and len(raw)==12+count*24+16
for i,row in enumerate(prediction['rows']):
 index,kind,value,state=struct.unpack_from('<IIdQ',raw,12+i*24);assert index==i and kind==['Seed','Randi','Randf','RandRange'].index(row['kind'])and struct.pack('<d',value)==struct.pack('<d',row['value'])and state==row['state'],(i,ops[i],value,row['value'],state,row['state'])
state,next_actual,next_mirror=struct.unpack_from('<QII',raw,12+count*24);assert state==prediction['state']and next_actual==next_mirror
proof=dict(scope='ActualGodot3.6.2 global built-ins for source-derived children-first scene-ready operation sequence; validates numeric samples and every intermediate PCG state. Does not instantiate full source scene or prove unrelated unported scripts.',engine=engine,engine_sha256=sha(exe),operations=count,sparkle_samples=len(prediction['samples']),post_ready_state=state,next_randi=next_actual,sources=input_data['sources'],files_sha256={p.name:sha(p)for p in out.iterdir()if p.is_file()})
(out/'proof.json').write_text(json.dumps(proof,indent=2)+'\n',encoding='utf8');print('Actual engine globalRNG replay:',count,'operations, every numeric value/PCG state exact;',proof['sparkle_samples'],'sparkle samples; finalstate',state)
