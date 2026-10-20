from pathlib import Path
import subprocess,json,os
r=Path('/mnt/d/GameDev/3DS/mother-encore-3ds');cache=r/'build/podunk-routes/host';objects=sorted(cache.glob('*.o'))
with(r/'build/full-port/iggy-walk-planner-3-compile.log').open('wb')as f:subprocess.run(['g++','-std=c++17','-O0','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie','-fno-exceptions','-fno-rtti','-Iinclude','build/full-port/iggy-walk-planner-3.cpp',*[str(p)for p in objects],'-o','build/full-port/iggy-walk-planner-3'],cwd=r,stdout=f,stderr=subprocess.STDOUT,check=True)
destination=json.loads((r/'build/full-port/iggy-walk-destination.json').read_text())
with(r/'build/full-port/iggy-walk-plan-3.json').open('wb')as f,(r/'build/full-port/iggy-walk-planner-3-result.log').open('wb')as log:
 subprocess.run(['build/full-port/iggy-walk-planner-3','romfs/world/podunk/podunk.encroom','romfs/world/podunk/podunk.encmap','build/full-port/iggy-walk-flags.txt','8','-39','1300','4713','4',destination['boundary']],cwd=r,stdout=f,stderr=log,check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'))
p=json.loads((r/'build/full-port/iggy-walk-plan-3.json').read_text());print('Route',len(p['waypoints']),'expanded',p['expanded'],'last',p['waypoints'][-14:],flush=True)
