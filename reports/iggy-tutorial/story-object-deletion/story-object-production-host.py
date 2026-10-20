from pathlib import Path
import hashlib,json,subprocess,os
r=Path('/mnt/d/GameDev/3DS/mother-encore-3ds');cache=r/'build/podunk-routes/host';state=json.loads((cache/'source.json').read_text())['source_sha256'];sources=sorted((r/'runtime').glob('*.cpp'));headers=sorted((r/'include/encore').glob('*.hpp'))
for p in sources+headers:assert state[str(p.relative_to(r))]==hashlib.sha256(p.read_bytes()).hexdigest(),p
flags=['g++','-std=c++17','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-O0','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie','-fno-exceptions','-fno-rtti','-Iinclude']
cases=[('production-tutorial','tests/graveyard_tutorial_tests.cpp',['romfs/world/podunk/podunk.encroom','romfs/world/podunk/story.encstory','romfs/world/podunk/podunk.encmap','romfs/ui/opening.encitems','romfs']),('production-session','tests/native_session_tests.cpp',['romfs']),('production-restore','tests/session_restore_tests.cpp',['romfs'])]
for name,test,args in cases:
 target='build/full-port/story-object-'+name+'-tests'
 with(r/('build/full-port/story-object-'+name+'-compile.log')).open('wb')as f:subprocess.run(flags+[test,*[str(cache/(p.stem+'.o'))for p in sources],'-o',target],cwd=r,stdout=f,stderr=subprocess.STDOUT,check=True)
 with(r/('build/full-port/story-object-'+name+'-result.log')).open('wb')as f:p=subprocess.run([target,*args],cwd=r,stdout=f,stderr=subprocess.STDOUT,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'))
 print('Current production data with typed object core:',name,p.returncode,flush=True)
 if p.returncode:raise SystemExit(p.returncode)
