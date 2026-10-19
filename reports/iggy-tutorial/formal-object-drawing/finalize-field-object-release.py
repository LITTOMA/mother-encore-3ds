from pathlib import Path
import hashlib,json,subprocess,sys,time
r=Path(__file__).resolve().parents[2];sys.path.insert(0,str(r));from tools import native_session,native_restore
changes=json.loads((r/'build/full-port/object-release-dependent-candidates.json').read_bytes());assert set(changes['session'])=={'dependencies'}and not changes['restore'];dep=changes['session']['dependencies'];assert{k for k in dep['before'].keys()|dep['after'].keys()if dep['before'].get(k)!=dep['after'].get(k)}=={'romfs/world/podunk/podunk.encroom','content/podunk-field.json'}
def step(name,args):
 with(r/('build/full-port/object-final-'+name+'.log')).open('wb')as log:p=subprocess.run([sys.executable,*args],cwd=r,stdout=log,stderr=subprocess.STDOUT)
 print(name,p.returncode,flush=True)
 if p.returncode:raise SystemExit(p.returncode)
step('session-extract',['tools/native_session.py','extract']);step('session-compile',['tools/native_session.py','compile'])
old=json.loads((r/'content/native-restore.json').read_bytes());new=native_restore.extract();diff={k for k in old.keys()|new.keys()if old.get(k)!=new.get(k)};assert diff<={'dependencies','fingerprints'},diff
(r/'build/full-port/object-final-restore-comparison.json').write_text(json.dumps(dict(changed=sorted(diff)),indent=2)+'\n',encoding='utf8')
if diff:step('restore-extract',['tools/native_restore.py','extract'])
step('restore-compile',['tools/native_restore.py','compile']);step('new-game',['tools/new_game_assets.py','reuse-session','--base','3b32208148cdceedfbf2980179b1ce68596264c3']);step('catalog',['tools/resource_catalog.py','compile']);step('object-verify',['tools/field_objects_assets.py','verify']);step('stage',['tools/stage_native_romfs.py','--verify-only'])
print('Object drawing resource generation and required typed source admission passed; no root callback/gameplay completion claim.',flush=True)
