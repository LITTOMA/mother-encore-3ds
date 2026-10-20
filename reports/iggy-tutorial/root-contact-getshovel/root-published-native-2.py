from pathlib import Path
import hashlib,json,subprocess,os,shutil
r=Path('/mnt/d/GameDev/3DS/mother-encore-3ds');cache=r/'build/podunk-routes/host';state=json.loads((cache/'source.json').read_text())['source_sha256'];sources=sorted((r/'runtime').glob('*.cpp'));headers=sorted((r/'include/encore').glob('*.hpp'))
for p in sources+headers:assert state[str(p.relative_to(r))]==hashlib.sha256(p.read_bytes()).hexdigest(),p
out=r/'build/full-port/root-published-native-2';assert not out.exists();out.mkdir();shutil.copyfile(r/'platform/ctr/field_renderer.hpp',out/'field_renderer.hpp')
flags=['g++','-std=c++17','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-O0','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie','-fno-exceptions','-fno-rtti','-Iinclude'];objects=[str(cache/(p.stem+'.o'))for p in sources];legacy='build/full-port/object-legacy-map-1/legacy.encmap';room='romfs/world/podunk/podunk.encroom';story='romfs/world/podunk/story.encstory';results=[]
cases=[
 ('prop_contact_scene',['romfs']),
 ('prop_contact_data',['build/full-port/prop-contact-data-fixture-1','romfs']),
 ('field_prop_audio',['romfs/sound/banks/opening.encaudio','romfs/','romfs/world/podunk/podunk.encprops','romfs',str(out/'owned-pcm')]),
 ('field_story',[story,room,legacy,'romfs/world/world.enclinks','romfs/ui/opening.encitems','romfs/story/opening.encintro','romfs/battle/straydog/bridge.encround','romfs/battle/straydog/bridge.encbattle']),
 ('cemetery_roots',[legacy,'tests/fixtures/source-field-props-cap2.encprops']),
 ('graveyard_tutorial',[room,story,'romfs/world/podunk/podunk.encmap','romfs/ui/opening.encitems','romfs','formal-source']),
 ('field_prop_renderer',['romfs',legacy,'tests/fixtures/source-field-props-cap2.encprops']),
 ('prop_scene_resources',['romfs',str(out/'owner-fixture')]),
 ('native_session',['romfs']),
 ('field_objects_scene',['romfs','build/full-port/object-story-fixture-1/story.encroom'])]
# The final preexisting objects scene executable takes its dedicated fixture
# protocol; do not substitute unknown arguments here.
cases=cases[4:-1]
for name,args in cases:
 options=[];extra=[]
 if name=='field_prop_audio':options=['-Itests/fixtures/audio_ndsp','-Iplatform/ctr'];extra=['platform/ctr/audio_player.cpp']
 if name=='field_prop_renderer':options=['-I'+str(out),'-Itests/field_prop_renderer_stubs']
 exe=out/name
 with(out/(name+'-compile.log')).open('wb')as log:subprocess.run(flags+options+['tests/'+name+'_tests.cpp',*extra,*objects,'-o',str(exe)],cwd=r,stdout=log,stderr=subprocess.STDOUT,check=True)
 with(out/(name+'-result.log')).open('wb')as log:p=subprocess.run([str(exe),*args],cwd=r,stdout=log,stderr=subprocess.STDOUT,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'))
 results.append(dict(name=name,returncode=p.returncode,args=args,exe_bytes=exe.stat().st_size,exe_sha256=hashlib.sha256(exe.read_bytes()).hexdigest()));(out/'proof.json').write_text(json.dumps(dict(results=results,source_sha256=state),indent=2)+'\n');print(name,p.returncode,flush=True)
 if p.returncode:raise SystemExit(p.returncode)
