import hashlib,json,subprocess,re,time
from pathlib import Path
root=Path.cwd();base=root/'build/full-port';script=base/'key-clocks-host-4.sh';inputs=[Path(p)for p in ['runtime/dungeon_key_clocks.cpp','runtime/dungeon_key_state.cpp','runtime/dungeon_keys.cpp','runtime/content.cpp','runtime/file_io.cpp','tests/dungeon_key_clocks_tests.cpp','build/full-port/key-clocks-host-4.sh']]+list(Path('include/encore').glob('*.hpp'))
sha=lambda p:hashlib.sha256((root/p).read_bytes()).hexdigest()
source={p.as_posix():sha(p)for p in inputs};start=time.time()
with (base/'key-clocks-host-5.stdout.log').open('wb')as out,(base/'key-clocks-host-5.stderr.raw.log').open('wb')as err:
 result=subprocess.run(['wsl','-d','Ubuntu-20.04','--','bash','--noprofile','--norc','/mnt/d/GameDev/3DS/mother-encore-3ds/build/full-port/key-clocks-host-4.sh'],stdout=out,stderr=err)
text=(base/'key-clocks-host-5.stdout.log').read_text('utf8');unchanged=source=={p.as_posix():sha(p)for p in inputs}
proof={'status':'passed'if result.returncode==0 and unchanged and 'Dungeon key clocks:'in text else'failed','process_exit':result.returncode,'seconds':time.time()-start,'inputs_unchanged':unchanged,'source_sha256':source,'stdout_sha256':hashlib.sha256((base/'key-clocks-host-5.stdout.log').read_bytes()).hexdigest(),'raw_stderr_sha256':hashlib.sha256((base/'key-clocks-host-5.stderr.raw.log').read_bytes()).hexdigest(),'scope':'Direct actual GCC9.4 C++ source compile with ASan/UBSan and normal leak checking. Not make test, Clang, cross-build, emulator or hardware. Raw WSL UTF16 launcher diagnostic is retained separately.','sanitizers':'address,undefined; detect_leaks=1; UBSAN halt_on_error=1; no PIE; no exceptions/RTTI'}
(base/'key-clocks-host-5-proof.json').write_text(json.dumps(proof,indent=2)+'\n',encoding='utf8')
print(text.strip());print(json.dumps({k:proof[k]for k in ['status','process_exit','seconds','inputs_unchanged']}));raise SystemExit(0 if proof['status']=='passed'else1)
