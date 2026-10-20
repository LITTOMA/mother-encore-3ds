from pathlib import Path
import hashlib,json,subprocess,os,shutil
r=Path('/mnt/d/GameDev/3DS/mother-encore-3ds');cache=r/'build/podunk-routes/host';state=json.loads((cache/'source.json').read_text())['source_sha256'];sources=sorted((r/'runtime').glob('*.cpp'));headers=sorted((r/'include/encore').glob('*.hpp'))
for p in sources+headers:
 if state.get(str(p.relative_to(r)))!=hashlib.sha256(p.read_bytes()).hexdigest():raise RuntimeError('Stale core '+str(p))
objects=[str(cache/(p.stem+'.o'))for p in sources];prepared=r/'build/full-port/player-presentation-headers';prepared.mkdir(exist_ok=True)
for name in ['opening_renderer.hpp','field_player_renderer.hpp','field_renderer.hpp']:shutil.copyfile(r/'platform/ctr'/name,prepared/name)
flags=['g++','-std=c++17','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-O0','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie','-fno-exceptions','-fno-rtti','-I'+str(prepared),'-Itests/field_prop_renderer_stubs','-Iinclude'];env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
for name,test,args in [('fresh','tests/fresh_house_restore_tests.cpp',['romfs']),('renderer','tests/player_renderer_tests.cpp',['romfs']),('props','tests/field_prop_renderer_tests.cpp',['romfs']),('catalog','tests/resource_catalog_tests.cpp',['romfs/system/native.encresources','romfs','build/full-port/player-catalog-fixture']),('legacy-catalog','tests/swing_catalog_tests.cpp',['tests/fixtures/catalog-cap12-swing.encresources','tests/fixtures/catalog-cap11.encresources']),('resources','tests/field_swing_resources_tests.cpp',['build/full-port/player-swing-resources-fixture','build/full-port/player-swing-resources-fixture/system/legacy.encresources'])]:
 out=r/('build/full-port/player-presentation-'+name)
 with(r/('build/full-port/player-presentation-'+name+'-compile.log')).open('wb')as log:subprocess.run(flags+[test,*objects,'-o',str(out)],cwd=r,stdout=log,stderr=subprocess.STDOUT,check=True)
 with(r/('build/full-port/player-presentation-'+name+'-result.log')).open('wb')as log:result=subprocess.run([str(out),*args],cwd=r,stdout=log,stderr=subprocess.STDOUT,env=env)
 print(name,result.returncode,flush=True)
 if result.returncode:raise SystemExit(result.returncode)
