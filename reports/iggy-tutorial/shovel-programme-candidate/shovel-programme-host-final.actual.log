from pathlib import Path
import hashlib,json,subprocess,os
r=Path('/mnt/d/GameDev/3DS/mother-encore-3ds');cache=r/'build/podunk-routes/host';state=json.loads((cache/'source.json').read_text())['source_sha256'];sources=sorted((r/'runtime').glob('*.cpp'));headers=sorted((r/'include/encore').glob('*.hpp'))
for p in sources+headers:assert state[str(p.relative_to(r))]==hashlib.sha256(p.read_bytes()).hexdigest(),p
flags=['g++','-std=c++17','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-O0','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie','-fno-exceptions','-fno-rtti','-Iinclude']
with(r/'build/full-port/shovel-programme-compile-1.log').open('wb')as f:subprocess.run(flags+['tests/shovel_programme_tests.cpp',*[str(cache/(p.stem+'.o'))for p in sources],'-o','build/full-port/shovel-programme-tests'],cwd=r,stdout=f,stderr=subprocess.STDOUT,check=True)
with(r/'build/full-port/shovel-programme-result-1.log').open('wb')as f:p=subprocess.run(['build/full-port/shovel-programme-tests','build/full-port/shovel-programme-fixture-2/source.encroom','build/full-port/field-objects-data-fixture/source.encobjects'],cwd=r,stdout=f,stderr=subprocess.STDOUT,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'))
print('Actual full10label source candidate GCC9 ASan/UBSan:',p.returncode,flush=True)
if p.returncode:raise SystemExit(p.returncode)

