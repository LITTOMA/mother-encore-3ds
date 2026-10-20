from pathlib import Path
import hashlib,json,subprocess,os
r=Path('/mnt/d/GameDev/3DS/mother-encore-3ds');cache=r/'build/podunk-routes/host';state=json.loads((cache/'source.json').read_text())['source_sha256'];sources=sorted((r/'runtime').glob('*.cpp'));headers=sorted((r/'include/encore').glob('*.hpp'))
for p in sources+headers:
 if state.get(str(p.relative_to(r)))!=hashlib.sha256(p.read_bytes()).hexdigest():raise RuntimeError('Stale core '+str(p))
objects=[str(cache/(p.stem+'.o'))for p in sources];out=r/'build/full-port/catalog12-owner-tests';flags=['g++','-std=c++17','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-O0','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie','-fno-exceptions','-fno-rtti','-Iinclude'];env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
with(r/'build/full-port/catalog12-owner-compile.log').open('wb')as log:subprocess.run(flags+['tests/prop_scene_resources_tests.cpp',*objects,'-o',str(out)],cwd=r,stdout=log,stderr=subprocess.STDOUT,check=True)
with(r/'build/full-port/catalog12-owner-result.log').open('wb')as log:subprocess.run([str(out),'build/full-port/prop-catalog12-7d-source','build/full-port/build/prop-catalog12-case'],cwd=r,stdout=log,stderr=subprocess.STDOUT,env=env,check=True)
print('Exact7d catalog12 ownership regression passed with GCC9 ASan/UBSan and leak checking')
