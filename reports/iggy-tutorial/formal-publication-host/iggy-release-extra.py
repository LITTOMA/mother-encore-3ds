from pathlib import Path
import subprocess,json,hashlib,os
r=Path('/mnt/d/GameDev/3DS/mother-encore-3ds');cache=r/'build/podunk-routes/host'
state=json.loads((cache/'source.json').read_text())['source_sha256'];sources=sorted((r/'runtime').glob('*.cpp'))
for p in sources+sorted((r/'include/encore').glob('*.hpp')):
 if state.get(str(p.relative_to(r)))!=hashlib.sha256(p.read_bytes()).hexdigest():raise RuntimeError('Stale actual core '+str(p))
env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
flags=['g++','-std=c++17','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-O0','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie','-fno-exceptions','-fno-rtti','-Iinclude']
with(r/'build/full-port/iggy-release-cemetery-compile.log').open('wb')as f:
 subprocess.run(flags+['tests/cemetery_roots_tests.cpp',*[str(cache/(p.stem+'.o'))for p in sources],'-o','build/full-port/iggy-release-cemetery-tests'],cwd=r,stdout=f,stderr=subprocess.STDOUT,check=True)
for name,args in [('cemetery',['build/full-port/iggy-release-cemetery-tests']),('candidate',['build/full-port/iggy-release-tutorial-tests','build/full-port/iggy-release-candidate/podunk.encroom','build/full-port/iggy-release-candidate/story.encstory','romfs/world/podunk/podunk.encmap','romfs/ui/opening.encitems','romfs'])]:
 with(r/('build/full-port/iggy-release-'+name+'-result.log')).open('wb')as f:
  p=subprocess.run(args,cwd=r,stdout=f,stderr=subprocess.STDOUT,env=env)
 print(name,p.returncode,flush=True)
 if p.returncode:raise SystemExit(p.returncode)
