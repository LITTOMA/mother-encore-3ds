from pathlib import Path
import subprocess,json,hashlib,shutil,os
r=Path('/mnt/d/GameDev/3DS/mother-encore-3ds');out=r/'build/full-port/field-props-release-host';out.mkdir(exist_ok=True);cache=r/'build/podunk-routes/host';manifest=json.loads((cache/'source.json').read_text())['source_sha256']
fresh={'field_scene_resources.cpp','field_scene.cpp','field_props.cpp'}
for p in list((r/'runtime').glob('*.cpp'))+list((r/'include/encore').glob('*.hpp')):
 if p.name not in fresh|{'field_props.hpp'}:assert manifest[str(p.relative_to(r))]==hashlib.sha256(p.read_bytes()).hexdigest(),str(p)
flags=['g++','-std=c++17','-O0','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie','-fno-exceptions','-fno-rtti','-Iinclude']
for name in fresh:
 with(out/(name+'-compile.log')).open('wb')as log:subprocess.run(flags+['-c','runtime/'+name,'-o',str(out/(Path(name).stem+'.o'))],cwd=r,stdout=log,stderr=subprocess.STDOUT,check=True)
objects=[p for p in cache.glob('*.o')if p.stem+'.cpp'not in fresh]+[out/(Path(n).stem+'.o')for n in fresh]
copy=out/'field_renderer.hpp';shutil.copyfile(r/'platform/ctr/field_renderer.hpp',copy);assert copy.read_bytes()==(r/'platform/ctr/field_renderer.hpp').read_bytes()
targets=[('renderer','tests/field_prop_renderer_tests.cpp',['romfs'],['-I'+str(out),'-Itests/field_prop_renderer_stubs']),('owners','tests/prop_scene_resources_tests.cpp',['romfs','build/full-port/field-props-release-host/fixture'],[]),('core','tests/field_props_tests.cpp',['tests/fixtures/source-field-props.encprops','tests/fixtures/prop-lifecycle-native.tsv','romfs'],[])]
for name,source,args,extra in targets:
 with(out/(name+'-compile.log')).open('wb')as log:subprocess.run(flags+extra+[source,*[str(p)for p in objects],'-o',str(out/name)],cwd=r,stdout=log,stderr=subprocess.STDOUT,check=True)
 with(out/(name+'-result.log')).open('wb')as log:subprocess.run([str(out/name),*args],cwd=r,stdout=log,stderr=subprocess.STDOUT,check=True)
 print(name+' passed',flush=True)
