from pathlib import Path
import argparse,hashlib,json,os,subprocess,time
rargs=argparse.ArgumentParser();rargs.add_argument('--name',default='clean-original-boot-1');rargs.add_argument('--no-debug',action='store_true');args=rargs.parse_args()
r=Path.cwd();out=r/'build/full-port/full-original-oracle-1';p=out/'project';src=r/'upstream/MOTHER-Encore';engine=r/'build/godot-reference/Godot_v3.6.2-stable_win64.exe'
assert (out/'.owned-original-full-oracle').read_bytes()==b'original-full-oracle-v1'
assert hashlib.sha256(engine.read_bytes()).hexdigest()=='e748acc92ee5205dd3cc1754d4422870f6a68ae6e00a16ff88bd3438c957adfd'
manifest=json.loads((out/'source-inputs.json').read_bytes())['manifest'];changed=[]
for row in manifest:
 name=row['path'];actual=hashlib.sha256((p/name).read_bytes()).hexdigest()
 if actual!=row['sha256']:
  assert name=='project.godot' or name.endswith('.import'),name
  changed.append(dict(path=name,before=row['sha256'],after=actual))
(out/'before-clean-boot-differences.json').write_text(json.dumps(changed,indent=2)+'\n')
(p/'project.godot').write_bytes((src/'project.godot').read_bytes())
env=dict(os.environ)
for key,child in [('APPDATA','user-roaming'),('LOCALAPPDATA','user-local'),('XDG_DATA_HOME','user-data'),('XDG_CONFIG_HOME','user-config')]:env[key]=str(out/child)
cmd=[str(engine),'--path',str(p),'--no-window','--audio-driver','Dummy','--video-driver','GLES2']+([]if args.no_debug else['--debug'])+['--quit'];start=time.monotonic();assert not(out/(args.name+'.log')).exists();timed_out=False;code=None
with (out/(args.name+'.log')).open('wb') as log:
 try:result=subprocess.run(cmd,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=90,creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0));code=result.returncode
 except subprocess.TimeoutExpired:timed_out=True
text=(out/(args.name+'.log')).read_text('utf8',errors='replace');errors=[line for line in text.splitlines() if 'ERROR:' in line or 'SCRIPT ERROR' in line or 'WARNING:' in line]
proof=dict(command=cmd,return_code=code,timed_out=timed_out,elapsed_seconds=time.monotonic()-start,error_count=len(errors),diagnostics=errors,scope='Clean original main-scene first-iteration boot only,not gameplay or emulator/hardware validation.',original_configuration_sha256=hashlib.sha256((src/'project.godot').read_bytes()).hexdigest(),changed_native_import_metadata=changed)
(out/(args.name+'.json')).write_text(json.dumps(proof,indent=2)+'\n');print(json.dumps({k:v for k,v in proof.items()if k not in ['diagnostics','changed_native_import_metadata']}),flush=True)
assert code==0 and not errors and not timed_out
