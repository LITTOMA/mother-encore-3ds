import concurrent.futures,json,os,subprocess,time
from pathlib import Path
root=Path('/mnt/d/GameDev/3DS/mother-encore-3ds');os.chdir(root);out=root/'build/full-port/terrain-core-host-1';sources=json.loads((out/'sources.json').read_text());flags=['-std=c++17','-O1','-g','-Wall','-Wextra','-Wpedantic','-Wno-misleading-indentation','-fno-rtti','-fno-exceptions','-fno-pie','-fno-omit-frame-pointer','-fsanitize=address,undefined','-Iinclude'];started=time.time()
def compile(source):
 target=out/(source.replace('/','_')+'.o')
 with(out/(source.replace('/','_')+'.log')).open('wb')as log:p=subprocess.run(['g++',*flags,'-c',source,'-o',str(target)],stdout=log,stderr=subprocess.STDOUT)
 if p.returncode:raise RuntimeError('Shared-core compile failed:'+source)
 print('Compiled '+source,flush=True);return str(target)
with concurrent.futures.ThreadPoolExecutor(max_workers=4)as pool:objects=list(pool.map(compile,sources))
subprocess.run(['ar','rcs',str(out/'libencore-terrain.a'),*objects],check=True)
cmd=['g++',*flags,'-no-pie','tests/terrain_admission_tests.cpp',str(out/'libencore-terrain.a'),'-o',str(out/'admission-check')];subprocess.run(cmd,check=True)
subprocess.run([str(out/'admission-check'),'build/full-port/terrain-fixture-1/source.encterr'],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1',UBSAN_OPTIONS='print_stacktrace=1:halt_on_error=1'))
(out/'completion.json').write_text(json.dumps(dict(compiled_sources=len(sources),elapsed_seconds=time.time()-started,admission_exit_code=0,scope='Fresh current GCC ASan+UBSan library and actual FieldScene admission regression;not full tests or Clang/cross/hardware.'))+'\n');print('CURRENT_SHARED_CORE_AND_ADMISSION_COMPLETE',flush=True)
