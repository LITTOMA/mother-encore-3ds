from pathlib import Path,PurePosixPath
import hashlib,json,os,re,subprocess,time
r=Path.cwd();out=r/'build/full-port/full-original-oracle-1';p=out/'project';src=r/'upstream/MOTHER-Encore';engine=r/'build/godot-reference/Godot_v3.6.2-stable_win64.exe'
assert (out/'.owned-original-full-oracle').read_bytes()==b'original-full-oracle-v1'
assert hashlib.sha256(engine.read_bytes()).hexdigest()=='e748acc92ee5205dd3cc1754d4422870f6a68ae6e00a16ff88bd3438c957adfd'
manifest=json.loads((out/'source-inputs.json').read_bytes())['manifest']
changed=[]
for row in manifest:
 name=row['path'];actual=hashlib.sha256((p/name).read_bytes()).hexdigest()
 if actual!=row['sha256']:
  assert name=='project.godot' or name.endswith('.import'),name
  changed.append(dict(path=name,before=row['sha256'],after=actual))
(out/'before-full-phase2-differences.json').write_text(json.dumps(changed,indent=2)+'\n')
(p/'project.godot').write_bytes((src/'project.godot').read_bytes())
env=dict(os.environ)
for key,child in [('APPDATA','user-roaming'),('LOCALAPPDATA','user-local'),('XDG_DATA_HOME','user-data'),('XDG_CONFIG_HOME','user-config')]:env[key]=str(out/child)
def outputs():
 expected=set();orphans=[];pending=[]
 for row in manifest:
  name=row['path']
  if not name.endswith('.import'):continue
  if not (p/name[:-7]).is_file():orphans.append(name);continue
  text=(p/name).read_text('utf8');source=re.search(r'^source_file="(res://[^"\n]+)"',text,re.M);block=re.search(r'^dest_files=\s*\[(.*?)\]',text,re.M|re.S)
  if not source or not block or source[1].casefold()!=('res://'+name[:-7]).casefold() or re.search(r'^valid=false$',text,re.M):pending.append(name);continue
  values=re.findall(r'"(res://[^"\n]+)"',block[1])
  if not values:pending.append(name)
  for value in values:
   path=PurePosixPath(value[6:]);assert value.startswith('res://') and not path.is_absolute() and '..' not in path.parts
   candidate=(p/str(path)).resolve();assert candidate.is_relative_to(p.resolve());expected.add(candidate)
 return expected,pending,orphans
cmd=[str(engine),'--path',str(p),'--no-window','--audio-driver','Dummy','--video-driver','GLES2','--debug','--editor'];start=time.monotonic();last=None;complete=None;timed_out=False
with (out/'cache-preparation-full-phase2.log').open('wb') as log:
 process=subprocess.Popen(cmd,env=env,stdout=log,stderr=subprocess.STDOUT,creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0));(out/'cache-process-full-phase2.json').write_text(json.dumps(dict(pid=process.pid,command=cmd,full_original_configuration=True),indent=2)+'\n')
 try:
  while process.poll() is None:
   expected,pending,orphans=outputs();missing=[str(v.relative_to(p)) for v in expected if not v.is_file() or not v.stat().st_size];state=(len(expected),len(missing),len(pending))
   if state!=last:print('Original configuration cache progress',state,flush=True);last=state
   if not missing and not pending:
    if complete is None:complete=time.monotonic()
    if time.monotonic()-complete>5:break
   else:complete=None
   if time.monotonic()-start>180:timed_out=True;break
   time.sleep(2)
 finally:
  if process.poll() is None:process.terminate();process.wait(timeout=30)
expected,pending,orphans=outputs();missing=[str(v.relative_to(p)) for v in expected if not v.is_file() or not v.stat().st_size]
result=dict(return_code=process.returncode,elapsed_seconds=time.monotonic()-start,expected=len(expected),missing=missing,pending=pending,orphan_descriptors=orphans,timed_out=timed_out,scope='Original configuration restored for native custom importer. Asset preparation only;owned editor may be deliberately stopped;not a passed game boot.')
(out/'cache-completion-full-phase2.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({k:v for k,v in result.items()if k not in ['missing','pending','orphan_descriptors']}),flush=True)
assert not missing and not pending and not timed_out
