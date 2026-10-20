from pathlib import Path
import hashlib,json,subprocess,os
r=Path('/mnt/d/GameDev/3DS/mother-encore-3ds');cache=r/'build/podunk-routes/host';state=json.loads((cache/'source.json').read_text())['source_sha256'];sources=sorted((r/'runtime').glob('*.cpp'));headers=sorted((r/'include/encore').glob('*.hpp'))
for p in sources+headers:
 if state.get(str(p.relative_to(r)))!=hashlib.sha256(p.read_bytes()).hexdigest():raise RuntimeError('Stale actual core '+str(p))
objects=[str(cache/(p.stem+'.o'))for p in sources];flags=['g++','-std=c++17','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-O0','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie','-fno-exceptions','-fno-rtti','-Iinclude'];env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
cases=[('tutorial','tests/graveyard_tutorial_tests.cpp',['romfs/world/podunk/podunk.encroom','romfs/world/podunk/story.encstory','romfs/world/podunk/podunk.encmap','romfs/ui/opening.encitems','romfs']),('story','tests/field_story_tests.cpp',['romfs/world/podunk/story.encstory','romfs/world/podunk/podunk.encroom','romfs/world/podunk/podunk.encmap','romfs/world/world.enclinks','romfs/ui/opening.encitems','romfs/story/opening.encintro','romfs/battle/straydog/bridge.encround','romfs/battle/straydog/bridge.encbattle']),('session','tests/native_session_tests.cpp',['romfs']),('restore','tests/session_restore_tests.cpp',['romfs'])]
for name,test,args in cases:
 out=r/('build/full-port/iggy-release-'+name+'-tests')
 with(r/('build/full-port/iggy-release-'+name+'-compile.log')).open('wb')as log:subprocess.run(flags+[test,*objects,'-o',str(out)],cwd=r,stdout=log,stderr=subprocess.STDOUT,check=True)
 with(r/('build/full-port/iggy-release-'+name+'-result.log')).open('wb')as log:result=subprocess.run([str(out),*args],cwd=r,stdout=log,stderr=subprocess.STDOUT,env=env)
 print(name,result.returncode,flush=True)
 if result.returncode:raise SystemExit(result.returncode)
