from pathlib import Path
import hashlib,json,subprocess,os
r=Path('/mnt/d/GameDev/3DS/mother-encore-3ds');cache=r/'build/podunk-routes/host';state=json.loads((cache/'source.json').read_text())['source_sha256'];sources=sorted((r/'runtime').glob('*.cpp'));headers=sorted((r/'include/encore').glob('*.hpp'))
for p in sources+headers:
 if state.get(str(p.relative_to(r)))!=hashlib.sha256(p.read_bytes()).hexdigest():raise RuntimeError('Stale actual core: '+str(p))
objects=[cache/(p.stem+'.o')for p in sources];out=r/'build/full-port/player-action-world-regression-tests';flags=['g++','-std=c++17','-Wall','-Wextra','-Werror','-O0','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie','-fno-exceptions','-fno-rtti','-Iinclude','-DENCORE_TEST_ROOM_PATH="romfs/world/house/opening.encroom"']
env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
with(r/'build/full-port/player-action-world-regressions.log').open('wb')as log:
 for name,args in [('world',[]),('persistent_player',[]),('scene_player',['romfs/world/house/opening.encroom','tests/fixtures/mayor-initial-story.encroom','tests/fixtures/scene-player.tsv','romfs/world/podunk/podunk.encroom'])]:
  log.write(('Target '+name+'\n').encode());log.flush()
  subprocess.run(flags+['tests/'+name+'_tests.cpp',*[str(p)for p in objects],'-o',str(out)],cwd=r,stdout=log,stderr=subprocess.STDOUT,check=True)
  subprocess.run([str(out),*args],cwd=r,stdout=log,stderr=subprocess.STDOUT,env=env,check=True)
out.resolve().relative_to((r/'build/full-port').resolve());out.unlink();print('Actual GCC9 ASan/UBSan World, persistent owner and scene-transfer regressions passed; temporary ELF retired')
