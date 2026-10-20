from pathlib import Path
import subprocess,json,hashlib,os
r=Path('/mnt/d/GameDev/3DS/mother-encore-3ds');cache=r/'build/podunk-routes/host';state=json.loads((cache/'source.json').read_text())['source_sha256'];sources=sorted((r/'runtime').glob('*.cpp'));headers=sorted((r/'include/encore').glob('*.hpp'))
for p in sources+headers:assert state[str(p.relative_to(r))]==hashlib.sha256(p.read_bytes()).hexdigest(),p
flags=['g++','-std=c++17','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-O0','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie','-fno-exceptions','-fno-rtti','-Iinclude']
cases=[('story-object-delete','tests/story_object_delete_tests.cpp',['build/full-port/story-object-fixture-1/source.encroom','build/full-port/field-objects-data-fixture/source.encobjects','romfs/world/podunk/podunk.encroom']),('field-objects-data','tests/field_objects_data_tests.cpp',['build/full-port/field-objects-data-fixture/source.encobjects','romfs/world/podunk/podunk.encroom']),('field-objects-lifecycle','tests/field_objects_tests.cpp',['build/full-port/field-objects-data-fixture/source.encobjects','romfs/world/podunk/podunk.encroom','reports/iggy-tutorial/field-objects-reference-1/reference.tsv']),('story-key-grant','tests/story_key_grant_tests.cpp',['build/full-port/story-key-fixture-regression/source.encroom','tests/fixtures/pre-key-grant-room.encroom'])]
for name,source,args in cases:
 target='build/full-port/'+name+'-tests'
 with(r/('build/full-port/'+name+'-compile-current.log')).open('wb')as f:subprocess.run(flags+[source,*[str(cache/(p.stem+'.o'))for p in sources],'-o',target],cwd=r,stdout=f,stderr=subprocess.STDOUT,check=True)
 with(r/('build/full-port/'+name+'-result-current.log')).open('wb')as f:p=subprocess.run([target,*args],cwd=r,stdout=f,stderr=subprocess.STDOUT,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'))
 print('Current object codec/world GCC9 ASan/UBSan:',name,p.returncode,flush=True)
 if p.returncode:raise SystemExit(p.returncode)
