from pathlib import Path
import hashlib,json,subprocess,sys,time
r=Path(__file__).resolve().parents[2];sys.path.insert(0,str(r))
from tools import native_content as nc,native_restore,native_session
baseline=json.loads((r/'build/full-port/story-object-generation-before.json').read_bytes());steps=[]
def step(label,args):
 begin=time.time();log=r/('build/full-port/story-object-rebind-'+label+'.log')
 with log.open('wb')as f:p=subprocess.run([sys.executable,*args],cwd=r,stdout=f,stderr=subprocess.STDOUT)
 steps.append(dict(step=label,returncode=p.returncode,seconds=time.time()-begin,log=str(log.relative_to(r))));(r/'build/full-port/story-object-rebind-results.json').write_text(json.dumps(steps,indent=2)+'\n',encoding='utf8');print(label,p.returncode,flush=True)
 if p.returncode:raise SystemExit(p.returncode)
step('field-room',['tools/podunk_field.py','compile'])
for name,old in baseline['rooms'].items():
 fresh=nc.parse_pack((r/name).read_bytes());fresh.pop('build_input_sha256');assert fresh==old,('Room gameplay changed',name)
print('ALL3 Room decoded game tables/strings exact; only build input identity may change',flush=True)
comparisons={}
for label,module,path in [('session',native_session,'content/native-session.json'),('restore',native_restore,'content/native-restore.json')]:
 old=json.loads((r/path).read_bytes());new=module.extract();changed={k:dict(before=old.get(k),after=new.get(k))for k in old.keys()|new.keys()if old.get(k)!=new.get(k)}
 comparisons[label]=changed;(r/'build/full-port/story-object-rebind-comparison.json').write_text(json.dumps(comparisons,indent=2)+'\n',encoding='utf8')
 print(label,'changed keys:',list(changed),flush=True);assert set(changed)<=({'dependencies'}if label=='session'else{'dependencies','fingerprints'}),('Unmapped semantic change',label)
 step(label+'-extract',['tools/native_'+label+'.py','extract']);step(label+'-compile',['tools/native_'+label+'.py','compile'])
step('new-game',['tools/new_game_assets.py','reuse-session','--base',baseline['head']])
print('Original source extractors rebound only validated dependency identities; defaults/game data unchanged',flush=True)
