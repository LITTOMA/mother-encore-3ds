import concurrent.futures,json,os,subprocess,time
from pathlib import Path
root=Path('/mnt/d/GameDev/3DS/mother-encore-3ds');os.chdir(root);out=root/'build/full-port/shadow-core-host-2';sources=json.loads((out/'sources.json').read_text());flags=['-std=c++17','-O1','-g','-Wall','-Wextra','-Wpedantic','-Wno-misleading-indentation','-fno-rtti','-fno-exceptions','-fno-pie','-fno-omit-frame-pointer','-fsanitize=address,undefined','-Iinclude'];started=time.time()
def compile(source):
 target=out/(source.replace('/','_')+'.o')
 with(out/(source.replace('/','_')+'.log')).open('wb')as log:p=subprocess.run(['g++',*flags,'-c',source,'-o',str(target)],stdout=log,stderr=subprocess.STDOUT)
 if p.returncode:raise RuntimeError('Shared-core compile failed:'+source)
 print('Compiled '+source,flush=True);return str(target)
with concurrent.futures.ThreadPoolExecutor(max_workers=4)as pool:objects=list(pool.map(compile,sources))
subprocess.run(['ar','rcs',str(out/'libencore-shadow.a'),*objects],check=True)
(out/'completion.json').write_text(json.dumps(dict(compiled_sources=len(sources),elapsed_seconds=time.time()-started,scope='Fresh current 116-source GCC9.4 ASan+UBSan library;test execution follows separately.'))+'\n');print('CURRENT_SHADOW_SHARED_CORE_BUILD_COMPLETE',flush=True)
