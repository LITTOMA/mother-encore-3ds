from pathlib import Path
import hashlib,json,subprocess,os
r=Path('/mnt/d/GameDev/3DS/mother-encore-3ds');cache=r/'build/podunk-routes/host';state=json.loads((cache/'source.json').read_text())['source_sha256'];sources=sorted((r/'runtime').glob('*.cpp'));headers=sorted((r/'include/encore').glob('*.hpp'))
for p in sources+headers:
 if state.get(str(p.relative_to(r)))!=hashlib.sha256(p.read_bytes()).hexdigest():raise RuntimeError('Stale actual core: '+str(p))
objects=[cache/(p.stem+'.o')for p in sources];flags=['g++','-std=c++17','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-O0','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie','-fno-exceptions','-fno-rtti','-Itests/fixtures/audio_ndsp','-Iplatform/ctr','-Iinclude'];env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
for name,test,extra,args in [('data','tests/player_audio_data_tests.cpp',[],['build/full-port/player-action-audio-fixture/world/fixture/FieldSwingAction.encaction','tests/fixtures/field-swing-v0410.encswing','tests/fixtures/field-swing-action-v0410.encaction','romfs/sound/banks/opening.encaudio']),('ndsp','tests/player_action_audio_tests.cpp',['platform/ctr/audio_player.cpp'],['build/full-port/player-action-audio-fixture','romfs/sound/banks/opening.encaudio','romfs/'])]:
 out=r/('build/full-port/player-action-audio-'+name+'-tests')
 with(r/('build/full-port/player-action-audio-'+name+'-compile.log')).open('wb')as log:subprocess.run(flags+[test,*extra,*[str(p)for p in objects],'-o',str(out)],cwd=r,stdout=log,stderr=subprocess.STDOUT,check=True)
 with(r/('build/full-port/player-action-audio-'+name+'-result.log')).open('wb')as log:code=subprocess.run([str(out),*args],cwd=r,stdout=log,stderr=subprocess.STDOUT,env=env).returncode
 print(name,'exit',code,flush=True)
 if code:raise SystemExit(code)
