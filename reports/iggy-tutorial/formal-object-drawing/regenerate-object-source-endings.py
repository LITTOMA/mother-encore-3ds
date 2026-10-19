from pathlib import Path
import json, subprocess, sys, time
r=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(r))
from tools import native_content as nc, native_session, native_restore
baseline=json.loads((r/'build/full-port/object-activation-before.json').read_bytes())
steps=[]
def run(name,args):
    start=time.time()
    with (r/f'build/full-port/object-lf-{name}.log').open('wb') as log:
        p=subprocess.run([sys.executable,*args],cwd=r,stdout=log,stderr=subprocess.STDOUT)
    steps.append(dict(name=name,returncode=p.returncode,seconds=time.time()-start))
    (r/'build/full-port/object-lf-generation.json').write_text(json.dumps(steps,indent=2)+'\n',encoding='utf8')
    print(name,p.returncode,flush=True)
    if p.returncode: raise SystemExit(p.returncode)
tool=['--tex3ds','build/tooling/pinned-tex3ds/tex3ds','--tex3ds-command','py -3.11 build/tooling/run_tex3ds_wsl.py']
run('map',['tools/podunk_field.py','compile',*tool])
run('props',['tools/field_props.py','assets',*tool])
run('player',['tools/field_player_assets.py','assets','--from-atlases','build/iggy-tutorial/swing-graphics-candidate-2'])
for name,old in baseline['rooms'].items():
    new=nc.parse_pack((r/name).read_bytes());new.pop('build_input_sha256')
    assert new==old,('Room semantic change',name)
old=json.loads((r/'content/native-session.json').read_bytes());new=native_session.extract()
diff={k for k in old.keys()|new.keys() if old.get(k)!=new.get(k)}
assert diff=={'dependencies'},diff
dep=old['dependencies'];newdep=new['dependencies']
assert {k for k in dep.keys()|newdep.keys() if dep.get(k)!=newdep.get(k)}=={'content/podunk-field.json','romfs/world/podunk/podunk.encroom'}
run('session-extract',['tools/native_session.py','extract'])
run('session-compile',['tools/native_session.py','compile'])
assert json.loads((r/'content/native-restore.json').read_bytes())==native_restore.extract()
run('restore-compile',['tools/native_restore.py','compile'])
run('new-game',['tools/new_game_assets.py','reuse-session','--base','3b32208148cdceedfbf2980179b1ce68596264c3'])
run('catalog',['tools/resource_catalog.py','compile'])
run('objects-verify',['tools/field_objects_assets.py','verify'])
run('stage',['tools/stage_native_romfs.py','--verify-only'])
print('Original generators completed; all Room content unchanged.',flush=True)
