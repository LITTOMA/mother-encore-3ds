from pathlib import Path
import hashlib,json,subprocess,os
r=Path('/mnt/d/GameDev/3DS/mother-encore-3ds');cache=r/'build/podunk-routes/host';state=json.loads((cache/'source.json').read_text())['source_sha256'];sources=sorted((r/'runtime').glob('*.cpp'));headers=sorted((r/'include/encore').glob('*.hpp'));added={'runtime/field_objects.cpp','include/encore/field_objects.hpp'}
for p in sources+headers:
 name=str(p.relative_to(r))
 if name not in added:
  assert state.get(name)==hashlib.sha256(p.read_bytes()).hexdigest(),name
  assert 'encore/field_objects.hpp'not in p.read_text(),name
flags=['g++','-std=c++17','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-O0','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie','-fno-exceptions','-fno-rtti','-Iinclude']
with(r/'build/full-port/field-objects-data-core-compile-1.log').open('wb')as f:subprocess.run(flags+['-c','runtime/field_objects.cpp','-o',str(cache/'field_objects.o')],cwd=r,stdout=f,stderr=subprocess.STDOUT,check=True)
with(r/'build/full-port/field-objects-data-test-compile-1.log').open('wb')as f:subprocess.run(flags+['tests/field_objects_data_tests.cpp',*[str(cache/(p.stem+'.o'))for p in sources],'-o','build/full-port/field-objects-data-tests'],cwd=r,stdout=f,stderr=subprocess.STDOUT,check=True)
with(r/'build/full-port/field-objects-data-result-1.log').open('wb')as f:p=subprocess.run(['build/full-port/field-objects-data-tests','build/full-port/field-objects-data-fixture/source.encobjects','romfs/world/podunk/podunk.encroom'],cwd=r,stdout=f,stderr=subprocess.STDOUT,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'))
print('Actual source metadata GCC9 ASan/UBSan',p.returncode,flush=True)
if p.returncode:raise SystemExit(p.returncode)
updated=json.loads((cache/'source.json').read_text());updated['source_sha256']={str(p.relative_to(r)):hashlib.sha256(p.read_bytes()).hexdigest()for p in sources+headers};(cache/'source.json').write_text(json.dumps(updated,indent=2)+'\n')
