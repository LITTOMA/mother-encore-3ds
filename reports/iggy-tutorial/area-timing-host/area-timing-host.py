from pathlib import Path
import subprocess,json,hashlib,os
r=Path('/mnt/d/GameDev/3DS/mother-encore-3ds');cache=r/'build/podunk-routes/host';state=json.loads((cache/'source.json').read_text())['source_sha256'];sources=sorted((r/'runtime').glob('*.cpp'));headers=sorted((r/'include/encore').glob('*.hpp'))
added={'runtime/field_area_contacts.cpp','include/encore/field_area_contacts.hpp'}
for p in sources+headers:
 name=str(p.relative_to(r))
 if name not in added:
  assert state.get(name)==hashlib.sha256(p.read_bytes()).hexdigest(),name
  assert 'field_area_contacts.hpp'not in p.read_text(),name
flags=['g++','-std=c++17','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-O0','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie','-fno-exceptions','-fno-rtti','-Iinclude']
with(r/'build/full-port/area-timing-core-compile.log').open('wb')as f:subprocess.run(flags+['-c','runtime/field_area_contacts.cpp','-o',str(cache/'field_area_contacts.o')],cwd=r,stdout=f,stderr=subprocess.STDOUT,check=True)
with(r/'build/full-port/area-timing-test-compile.log').open('wb')as f:subprocess.run(flags+['tests/field_area_contacts_tests.cpp',*[str(cache/(p.stem+'.o'))for p in sources],'-o','build/full-port/area-timing-tests'],cwd=r,stdout=f,stderr=subprocess.STDOUT,check=True)
with(r/'build/full-port/area-timing-result.log').open('wb')as f:p=subprocess.run(['build/full-port/area-timing-tests','romfs/world/player/swing.encswing','reports/iggy-tutorial/area-timing-reference-3/reference.tsv'],cwd=r,stdout=f,stderr=subprocess.STDOUT,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'))
print('Targeted actual GCC9 ASan/UBSan',p.returncode,flush=True)
if p.returncode:raise SystemExit(p.returncode)
updated=json.loads((cache/'source.json').read_text());updated['source_sha256']={str(p.relative_to(r)):hashlib.sha256(p.read_bytes()).hexdigest()for p in sources+headers};(cache/'source.json').write_text(json.dumps(updated,indent=2)+'\n')
