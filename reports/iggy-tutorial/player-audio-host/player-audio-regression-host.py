from pathlib import Path
import hashlib,json,subprocess,os
r=Path('/mnt/d/GameDev/3DS/mother-encore-3ds');cache=r/'build/podunk-routes/host';state=json.loads((cache/'source.json').read_text())['source_sha256'];sources=sorted((r/'runtime').glob('*.cpp'));headers=sorted((r/'include/encore').glob('*.hpp'))
for p in sources+headers:
 if state.get(str(p.relative_to(r)))!=hashlib.sha256(p.read_bytes()).hexdigest():raise RuntimeError('Stale core '+str(p))
objects=[str(cache/(p.stem+'.o'))for p in sources];flags=['g++','-std=c++17','-O0','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie','-fno-exceptions','-fno-rtti','-Itests/fixtures/audio_ndsp','-Iplatform/ctr','-Iinclude'];env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
base=['romfs/sound/banks/opening.encaudio','romfs/','romfs/world/house/opening.encroom']
for name,source,extra,args in [('legacy','tests/audio_player_regression.cpp',['platform/ctr/audio_player.cpp'],base),('ndsp','tests/audio_player_tests.cpp',['platform/ctr/audio_player.cpp'],base),('run','tests/run_audio_player_tests.cpp',['platform/ctr/audio_player.cpp','platform/ctr/music_region_player.cpp'],['romfs/sound/banks/opening.encaudio','romfs/','romfs/sound/banks/podunk.encmusic','romfs/sound/banks/podunk.encaudio','romfs/','tests/fixtures/mayor.encsteps']),('world','tests/player_action_world_tests.cpp',[],['build/full-port/player-action-audio-fixture'])]:
 out=r/('build/full-port/player-audio-regression-'+name)
 with(r/('build/full-port/player-audio-regression-'+name+'-compile.log')).open('wb')as log:subprocess.run(flags+[source,*extra,*objects,'-o',str(out)],cwd=r,stdout=log,stderr=subprocess.STDOUT,check=True)
 with(r/('build/full-port/player-audio-regression-'+name+'-result.log')).open('wb')as log:result=subprocess.run([str(out),*args],cwd=r,stdout=log,stderr=subprocess.STDOUT,env=env)
 print(name,result.returncode,flush=True)
 if result.returncode:raise SystemExit(result.returncode)
