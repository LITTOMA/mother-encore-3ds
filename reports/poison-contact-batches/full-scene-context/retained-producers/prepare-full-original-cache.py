from pathlib import Path,PurePosixPath
import argparse,hashlib,json,os,re,subprocess,time
r=Path.cwd();a=argparse.ArgumentParser();a.add_argument('--bootstrap',action='store_true');bootstrap=a.parse_args().bootstrap;out=r/'build/full-port/full-original-oracle-1';project=out/'project';source=r/'upstream/MOTHER-Encore';engine=r/'build/godot-reference/Godot_v3.6.2-stable_win64.exe';assert(out/'.owned-original-full-oracle').read_bytes()==b'original-full-oracle-v1';inputs=json.loads((out/'source-inputs.json').read_bytes());assert hashlib.sha256(engine.read_bytes()).hexdigest()=='e748acc92ee5205dd3cc1754d4422870f6a68ae6e00a16ff88bd3438c957adfd'
if(project/'project.godot').read_bytes()!=(source/'project.godot').read_bytes():(out/'editor1-project.godot').write_bytes((project/'project.godot').read_bytes());(project/'project.godot').write_bytes((source/'project.godot').read_bytes())
assert all(hashlib.sha256((project/row['path']).read_bytes()).hexdigest()==row['sha256']for row in inputs['manifest']);descriptors=[row['path']for row in inputs['manifest']if row['path'].endswith('.import')]
if bootstrap:
 original=(source/'project.godot').read_text('utf8')
 def section(name):
  match=re.search(r'^\['+re.escape(name)+r'\]\s*\n(.*?)(?=^\[|\Z)',original,re.M|re.S);return '['+name+']\n'+match[1]if match else ''
 text='config_version=4\n[application]\nconfig/name="Original asset import bootstrap only"\n'+section('debug')+section('rendering');(out/'asset-bootstrap-project.godot').write_text(text,encoding='utf8');(project/'project.godot').write_text(text,encoding='utf8')
def resolved(value):
 path=PurePosixPath(value[len('res://'):]);assert value.startswith('res://')and not path.is_absolute()and all(v not in['.','..']for v in path.parts);p=(project/str(path)).resolve();assert p.is_relative_to(project.resolve());return p
def outputs():
 expected=set();orphan=[];pending=[]
 for name in descriptors:
  # An import descriptor belongs to its adjacent source file. Its generated
  # deps.source_file/remap may be stale after an upstream move. Let the real
  # editor regenerate it;never manufacture a cache under the stale name.
  actual='res://'+name[:-7]
  if not resolved(actual).is_file():orphan.append(dict(path=name,reason='Adjacent tracked source absent;not materializable. Required scene dependencies remain subject to clean boot rejection.'));continue
  text=(project/name).read_text('utf8');src=re.search(r'^source_file="(res://[^"\n]+)"',text,re.M);block=re.search(r'^dest_files=\s*\[(.*?)\]',text,re.M|re.S)
  if not src or not block or src[1].casefold()!=actual.casefold()or re.search(r'^valid=false$',text,re.M):pending.append(name);continue
  values=re.findall(r'"(res://[^"\n]+)"',block[1])
  if not values:pending.append(name)
  for value in values:expected.add(resolved(value))
 return expected,orphan,pending
expected,orphan,pending=outputs();assert expected;plan=dict(initial_expected_files=len(expected),orphan_import_descriptors=orphan,stale_or_pending_descriptors=pending,scope='Only cache preparation,not passed gameplay. Native editor regenerates stale path metadata;no needed source dependencies are waived.');(out/'cache-plan.json').write_text(json.dumps(plan,indent=2)+'\n');env=dict(os.environ)
for key,child in [('APPDATA','user-roaming'),('LOCALAPPDATA','user-local'),('XDG_DATA_HOME','user-data'),('XDG_CONFIG_HOME','user-config')]:env[key]=str(out/child)
cmd=[str(engine),'--path',str(project),'--no-window','--audio-driver','Dummy','--video-driver','GLES2','--debug','--editor'];start=time.monotonic();complete_at=None;previous=-1;suffix='bootstrap'if bootstrap else'full'
with(out/f'cache-preparation-{suffix}.log').open('wb')as log:
 process=subprocess.Popen(cmd,env=env,stdout=log,stderr=subprocess.STDOUT,creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0));(out/f'cache-process-{suffix}.json').write_text(json.dumps(dict(pid=process.pid,command=cmd,asset_bootstrap_only=bootstrap),indent=2)+'\n')
 try:
  while process.poll()is None:
   expected,orphan,pending=outputs();count=sum(p.is_file()and p.stat().st_size>0 for p in expected)
   progress=(count,len(expected),len(pending))
   if progress!=previous:print('Materialized original caches',count,'/',len(expected),'pending descriptors',len(pending),flush=True);previous=progress
   if not pending and count==len(expected):
    if complete_at is None:complete_at=time.monotonic()
    if time.monotonic()-complete_at>=5:break
   else:complete_at=None
   if time.monotonic()-start>600:raise TimeoutError('Full original cache preparation timed out;not an accepted boot')
   time.sleep(3)
 finally:
  if process.poll()is None:process.terminate();process.wait(timeout=30)
missing=[str(p.relative_to(project))for p in expected if not p.is_file()or not p.stat().st_size];result=dict(return_code=process.returncode,elapsed_seconds=time.monotonic()-start,expected=len(expected),missing=missing,pending=pending,orphan=orphan,asset_bootstrap_only=bootstrap,scope='Owned editor deliberately stopped after cache materialization;NOT a passed editor/gameplay run. Subsequent clean original boot required;any required orphan dependency still rejects.');(out/f'cache-completion-{suffix}.json').write_text(json.dumps(result,indent=2)+'\n');print('Cache preparation done;clean boot remains required',len(missing),'missing');assert not missing and not pending
