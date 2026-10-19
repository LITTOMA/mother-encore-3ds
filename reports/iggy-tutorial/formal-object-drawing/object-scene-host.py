from pathlib import Path
import hashlib,json,subprocess,os
r=Path('/mnt/d/GameDev/3DS/mother-encore-3ds');cache=r/'build/podunk-routes/host';state=json.loads((cache/'source.json').read_text())['source_sha256'];sources=sorted((r/'runtime').glob('*.cpp'));headers=sorted((r/'include/encore').glob('*.hpp'))
for p in sources+headers:assert state[str(p.relative_to(r))]==hashlib.sha256(p.read_bytes()).hexdigest(),p
flags=['g++','-std=c++17','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-O0','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie','-fno-exceptions','-fno-rtti','-Iinclude'];core=[str(cache/(p.stem+'.o'))for p in sources];old='build/full-port/field-objects-data-fixture/source.encobjects';room='romfs/world/podunk/podunk.encroom';bound='build/full-port/bound-objects-2/'
cases=[('field_objects_scene',[bound+'source.encobjects',bound+'source.encmap','build/full-port/story-object-fixture-1/source.encroom','romfs/world/podunk/podunk.encmick']),('field_objects_bound',[bound+'source.encobjects',bound+'source.encmap',room,old]),('field_objects_data',[old,room]),('field_objects',[old,room,'reports/iggy-tutorial/field-objects-reference-1/reference.tsv']),('story_object_delete',['build/full-port/story-object-fixture-1/source.encroom',old,room]),('shovel_programme',['build/full-port/shovel-programme-fixture-2/source.encroom',old]),('field_data',['romfs/world/podunk/podunk.encmap','romfs/world/world.enclinks',room,'romfs/story/opening.encintro']),('field_area',['tests/fixtures','romfs']),('field_props',['tests/fixtures/source-field-props.encprops','tests/fixtures/prop-lifecycle-native.tsv','romfs'])]
for name,args in cases:
 exe='build/full-port/scene-regression-'+name
 with(r/('build/full-port/scene-compile-'+name+'.log')).open('wb')as f:subprocess.run(flags+['tests/'+name+'_tests.cpp',*core,'-o',exe],cwd=r,stdout=f,stderr=subprocess.STDOUT,check=True)
 with(r/('build/full-port/scene-result-'+name+'.log')).open('wb')as f:p=subprocess.run([exe,*args],cwd=r,stdout=f,stderr=subprocess.STDOUT,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'))
 print(name,p.returncode,flush=True)
 if p.returncode:raise SystemExit(p.returncode)
for name in ['field_objects','field_scene']:subprocess.run(flags+['-fsyntax-only','runtime/'+name+'.cpp'],cwd=r,check=True)
