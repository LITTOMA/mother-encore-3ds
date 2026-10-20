from pathlib import Path
import hashlib,json,subprocess,os,shutil
r=Path('/mnt/d/GameDev/3DS/mother-encore-3ds');cache=r/'build/podunk-routes/host'
state=json.loads((cache/'source.json').read_text())['source_sha256'];sources=sorted((r/'runtime').glob('*.cpp'));headers=sorted((r/'include/encore').glob('*.hpp'))
for p in sources+headers:assert state[str(p.relative_to(r))]==hashlib.sha256(p.read_bytes()).hexdigest(),p
out=r/'build/full-port/root-fixture-compat-native-1';assert not out.exists();out.mkdir()
flags=['g++','-std=c++17','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-O0','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie','-fno-exceptions','-fno-rtti','-Iinclude'];objects=[str(cache/(p.stem+'.o'))for p in sources]
legacy='build/full-port/object-legacy-map-1/legacy.encmap';story='tests/fixtures/pre-prop-dialogue-story.encstory';fixture='build/full-port/root-step-fixture-compat-1';results=[];compiled=set()
cases=[('field_interaction','field_interaction',['romfs',legacy,story]),('field_interaction_policy','field_interaction',['romfs',legacy,story,'tests/fixtures/podunk-mom-interaction-policy.encmick']),('step_scene_resources','step_scene_resources',[fixture,'romfs']),('world_run_audio','world_run_audio',[fixture,'tests/fixtures/mayor.encsteps','tests/fixtures/pause-audio.tsv','tests/fixtures/step-contacts.tsv'])]
for name,source,args in cases:
 exe=out/source
 if source not in compiled:
  shutil.copyfile(r/('tests/'+source+'_tests.cpp'),out/(source+'_tests.cpp'))
  with(out/(source+'-compile.log')).open('wb')as log:subprocess.run(flags+['tests/'+source+'_tests.cpp',*objects,'-o',str(exe)],cwd=r,stdout=log,stderr=subprocess.STDOUT,check=True)
  compiled.add(source)
 with(out/(name+'-result.log')).open('wb')as log:p=subprocess.run([str(exe),*args],cwd=r,stdout=log,stderr=subprocess.STDOUT,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'))
 results.append(dict(name=name,source=source,returncode=p.returncode,args=args,exe_bytes=exe.stat().st_size,exe_sha256=hashlib.sha256(exe.read_bytes()).hexdigest()))
 (out/'proof.json').write_text(json.dumps(dict(results=results,source_sha256=state),indent=2)+'\n');print(name,p.returncode,flush=True)
 if p.returncode:raise SystemExit(p.returncode)
