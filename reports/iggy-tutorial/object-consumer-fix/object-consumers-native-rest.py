from pathlib import Path
import hashlib,json,subprocess,os,shutil
r=Path('/mnt/d/GameDev/3DS/mother-encore-3ds');cache=r/'build/podunk-routes/host';state=json.loads((cache/'source.json').read_text())['source_sha256'];sources=sorted((r/'runtime').glob('*.cpp'));headers=sorted((r/'include/encore').glob('*.hpp'))
for p in sources+headers:assert state[str(p.relative_to(r))]==hashlib.sha256(p.read_bytes()).hexdigest(),p
out=r/'build/full-port/object-consumers-host-2';assert not out.exists();out.mkdir();shutil.copyfile(r/'platform/ctr/field_renderer.hpp',out/'field_renderer.hpp')
flags=['g++','-std=c++17','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-O0','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie','-fno-exceptions','-fno-rtti','-Iinclude','-DENCORE_TEST_ROOM_PATH="romfs/world/house/opening.encroom"'];core=[str(cache/(p.stem+'.o'))for p in sources]
legacy='build/full-port/object-legacy-map-1/legacy.encmap';room='romfs/world/podunk/podunk.encroom';links='romfs/world/world.enclinks';swing='build/full-port/object-consumer-swing-1';step='build/full-port/object-consumer-step-1';cases=[
('field_static_actor',['tests/fixtures/mayor-static-actor.encmap',legacy]),
('field_route_warp',[room,legacy,links]),
('resource_catalog',['romfs/system/native.encresources','romfs',str(out/'catalog-fixture')]),
('field_interaction',['romfs',legacy]),
('field_interaction',['romfs',legacy,'tests/fixtures/podunk-mom-interaction-policy.encmick']),
('field_swing_resources',[swing,swing+'/system/legacy.encresources']),
('cemetery_roots',[legacy]),
('field_prop_renderer',['romfs',legacy]),
('prop_scene_resources',['romfs',str(out/'prop-fixture')]),
('step_scene_resources',[step,'romfs']),
('world_run_audio',[step,'tests/fixtures/mayor.encsteps','tests/fixtures/pause-audio.tsv','tests/fixtures/step-contacts.tsv']),
('field_story',['romfs/world/podunk/story.encstory',room,legacy,links,'romfs/ui/opening.encitems','romfs/story/opening.encintro','romfs/battle/straydog/bridge.encround','romfs/battle/straydog/bridge.encbattle']),
('field_npc_motion',['romfs/world/podunk/npcs/lindgren.encmick','romfs/world/podunk/npcs/bridge-dog.encmick','romfs/world/podunk/podunk.encmick',legacy]),
('mayor_scene_resources',['romfs'])]
results=[];built=set()
for i,(name,args)in enumerate(cases[10:],10):
 exe=out/(name+'-regression');options=[]
 if name=='field_prop_renderer':options=['-I'+str(out),'-Itests/field_prop_renderer_stubs']
 if name not in built:
  with(out/(name+'-compile.log')).open('wb')as log:subprocess.run(flags+options+['tests/'+name+'_tests.cpp',*core,'-o',str(exe)],cwd=r,stdout=log,stderr=subprocess.STDOUT,check=True)
  built.add(name)
 label='%02d-%s'%(i,name)
 with(out/(label+'-result.log')).open('wb')as log:p=subprocess.run([str(exe),*args],cwd=r,stdout=log,stderr=subprocess.STDOUT,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'))
 results.append(dict(label=label,returncode=p.returncode,args=args,exe_sha256=hashlib.sha256(exe.read_bytes()).hexdigest()));(out/'results.json').write_text(json.dumps(results,indent=2)+'\n');print(label,p.returncode,flush=True)
 if p.returncode:raise SystemExit(p.returncode)
print('Remaining4 affected actual native cases passed; GCC9.4 ASan/UBSan with leak checking; no full-CI/GPU/hardware claim.',flush=True)
