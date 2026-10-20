from pathlib import Path
import hashlib,json,subprocess,sys,time
r=Path(__file__).resolve().parents[2];sys.path.insert(0,str(r))
from tools import native_session,native_restore
out=r/'build/full-port/root-dependent-generation-1';assert not out.exists();out.mkdir()
old=json.loads((r/'content/native-session.json').read_bytes());candidate=native_session.extract()
changes={k for k in old.keys()|candidate.keys()if old.get(k)!=candidate.get(k)};assert changes=={'dependencies','mutable_flags'},changes
assert old['key_acquisitions']==candidate['key_acquisitions'] and old['defaults']==candidate['defaults'] and old['schema']==candidate['schema']==11
assert set(candidate['mutable_flags'])-set(old['mutable_flags'])=={'got_shovel','returned_shovel'}and set(old['mutable_flags'])<=set(candidate['mutable_flags'])
before_defaults=hashlib.sha256(native_session.encode_snapshot(old['defaults'],old['compatibility'])).hexdigest();assert before_defaults=='a9cfbb498a20cb2bbb4a0a56dc5a87f9f13e37bac7ebbef14c875b67c93e7e9f'
steps=[]
def run(name,args):
 start=time.monotonic()
 with(out/(name+'.log')).open('wb')as log:p=subprocess.run([sys.executable,*args],cwd=r,stdout=log,stderr=subprocess.STDOUT)
 steps.append(dict(step=name,args=args,returncode=p.returncode,seconds=time.monotonic()-start));(out/'steps.json').write_text(json.dumps(steps,indent=2)+'\n');print(name,p.returncode,flush=True)
 if p.returncode:raise SystemExit(p.returncode)
run('audio',['tools/append_audio_assets.py','--scope','prop-nodes','--stage','--out','build/full-port/prop-audio-append-publication-1','--ffmpeg','C:/Users/litto/AppData/Local/Microsoft/WinGet/Links/ffmpeg.exe','--ffprobe','C:/Users/litto/AppData/Local/Microsoft/WinGet/Links/ffprobe.exe'])
run('session',['tools/native_session.py','extract']);assert json.loads((r/'content/native-session.json').read_bytes())==candidate
assert json.loads((r/'content/native-restore.json').read_bytes())==native_restore.extract();run('restore',['tools/native_restore.py','compile'])
run('naming',['tools/new_game_assets.py','reuse-session','--base','2fb5484928f655375e62864a62c159ee8343fcf2'])
run('catalog',['tools/resource_catalog.py','compile'])
run('stage',['tools/stage_native_romfs.py','--verify-only'])
(out/'proof.json').write_text(json.dumps(dict(source='Original generators only',default_save_sha256=before_defaults,session_resource_version=11,unchanged_key_policies=old['key_acquisitions'],new_mutable_flags=['got_shovel','returned_shovel'],restore_recipe_exact=True,naming_game_ui_binary_exact=True),indent=2)+'\n',encoding='utf8');print('Dependent source resources generated; save schema/defaults and old key policies preserved',flush=True)
