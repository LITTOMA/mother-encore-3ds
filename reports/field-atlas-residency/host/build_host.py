from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
import subprocess,json
root=Path('/mnt/d/GameDev/3DS/mother-encore-3ds');out=root/'build/field-atlas/host';out.mkdir(parents=True,exist_ok=True)
flags=['g++','-std=c++17','-O0','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-fno-exceptions','-fno-rtti','-Iinclude']
def compile(path):
    obj=out/(path.stem+'.o')
    with(out/(path.stem+'.log')).open('wb')as log:p=subprocess.run(flags+['-c',str(path),'-o',str(obj)],cwd=root,stdout=log,stderr=subprocess.STDOUT)
    if p.returncode:raise RuntimeError('Compile failed: '+str(path))
    return str(obj)
with ThreadPoolExecutor(max_workers=4)as pool:objects=list(pool.map(compile,sorted((root/'runtime').glob('*.cpp'))))
for name in ('house_map_present','encounter_residency'):
    with(out/(name+'-link.log')).open('wb')as log:p=subprocess.run(flags+['-no-pie','tests/'+name+'_tests.cpp',*objects,'-o',str(out/name)],cwd=root,stdout=log,stderr=subprocess.STDOUT)
    if p.returncode:raise RuntimeError('Link failed: '+name)
    args=[str(out/name),'romfs']
    if name=='house_map_present':args.append(str(out/'checked-map-and-exit.encsave'))
    with(out/(name+'-test.log')).open('wb')as log:subprocess.run(args,cwd=root,stdout=log,stderr=subprocess.STDOUT,check=True)
with(out/'loading-texture-build.log').open('wb')as log:subprocess.run(flags+['-Wall','-Wextra','-Werror','-Itests/loading_texture_stubs','-I.','-no-pie','tests/loading_texture_tests.cpp','-o',str(out/'loading_texture')],cwd=root,stdout=log,stderr=subprocess.STDOUT,check=True)
with(out/'loading-texture-test.log').open('wb')as log:subprocess.run([str(out/'loading_texture'),str(out)],cwd=root,stdout=log,stderr=subprocess.STDOUT,check=True)
(out/'source.json').write_text(json.dumps(dict(source_commit=subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip(),level='GCC host ASan/UBSan; not GPU, emulator or hardware validation',flags=flags,objects=len(objects)),indent=2))
print('GCC ASan/UBSan: actual ownership loader, encounter residency and map/fixture checks passed',flush=True)
