from pathlib import Path
import hashlib,json,subprocess,sys,time
r=Path(__file__).resolve().parents[2];sys.path.insert(0,str(r));from tools import native_content as nc,native_session,native_restore
before=json.loads((r/'build/full-port/object-activation-before.json').read_bytes());results=[]
def step(name,args):
 start=time.time()
 with(r/('build/full-port/object-release-'+name+'.log')).open('wb')as log:p=subprocess.run([sys.executable,*args],cwd=r,stdout=log,stderr=subprocess.STDOUT)
 results.append(dict(step=name,returncode=p.returncode,seconds=time.time()-start));(r/'build/full-port/object-release-generation.json').write_text(json.dumps(results,indent=2)+'\n',encoding='utf8');print(name,p.returncode,flush=True)
 if p.returncode:raise SystemExit(p.returncode)
tool=['--tex3ds','build/tooling/pinned-tex3ds/tex3ds','--tex3ds-command','py -3.11 build/tooling/run_tex3ds_wsl.py']
step('map',['tools/podunk_field.py','compile',*tool]);step('props',['tools/field_props.py','assets',*tool]);step('player',['tools/field_player_assets.py','assets','--from-atlases','build/iggy-tutorial/swing-graphics-candidate-2'])
for name,old in before['rooms'].items():
 new=nc.parse_pack((r/name).read_bytes());new.pop('build_input_sha256');assert new==old,('Room semantics changed during drawing-only activation',name)
changes={}
for name,module,path in [('session',native_session,'content/native-session.json'),('restore',native_restore,'content/native-restore.json')]:
 old=json.loads((r/path).read_bytes());new=module.extract();changes[name]={k:dict(before=old.get(k),after=new.get(k))for k in old.keys()|new.keys()if old.get(k)!=new.get(k)};print(name,'changed keys',list(changes[name]),flush=True)
(r/'build/full-port/object-release-dependent-candidates.json').write_text(json.dumps(changes,indent=2)+'\n',encoding='utf8');print('Actual map/props/player generation done; dependent candidates retained for semantic review before extraction.',flush=True)
