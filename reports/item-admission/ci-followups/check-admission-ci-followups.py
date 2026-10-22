from pathlib import Path
import hashlib,json,subprocess,sys,time
root=Path(__file__).resolve().parents[2];out=root/'build/full-port/item-admission-ci-followups-1';out.mkdir(exist_ok=False)
paths=['tests/audio_data_tests.cpp','tests/coin_scene_data_tests.cpp','tests/coin_scene_tests.cpp','tests/coin_scene_source_tests.cpp','tests/test_coin_scene.py','tests/prepare_coin_scene.py','tools/coin_scene.py']
inputs={p:hashlib.sha256((root/p).read_bytes()).hexdigest()for p in paths}
for p in paths:
 dest=out/'source-snapshot'/p;dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes((root/p).read_bytes())
steps=[]
def run(label,command):
 start=time.monotonic()
 with(out/(label+'.stdout.log')).open('wb')as stdout,(out/(label+'.stderr.log')).open('wb')as stderr:r=subprocess.run(command,cwd=root,stdout=stdout,stderr=stderr)
 steps.append(dict(label=label,command=command,returncode=r.returncode,seconds=time.monotonic()-start));(out/'steps.json').write_text(json.dumps(steps,indent=2)+'\n',encoding='utf-8');print(json.dumps(steps[-1]),flush=True);assert r.returncode==0
run('coin-python',[sys.executable,'-X','utf8','-m','unittest','tests.test_coin_scene','-v'])
script='''#!/bin/bash
set -eu
cd /mnt/d/GameDev/3DS/mother-encore-3ds
export ASAN_OPTIONS=detect_leaks=1
export UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1
out=build/full-port/item-admission-ci-followups-1
flags="-std=c++17 -O1 -g -fno-rtti -fno-exceptions -fno-pie -no-pie -fno-omit-frame-pointer -fsanitize=address,undefined -Iinclude"
g++ $flags tests/audio_data_tests.cpp build/full-port/shadow-core-host-2/libencore-shadow.a -Wl,-Map="$out/audio.map" -o "$out/audio" >"$out/audio-compile.stdout.log" 2>"$out/audio-compile.stderr.log"
for test in coin_scene_data coin_scene coin_scene_source; do
 g++ $flags "tests/${test}_tests.cpp" runtime/coin_scene.cpp runtime/coin_scene_data.cpp runtime/coins.cpp runtime/coins_data.cpp runtime/circle_motion.cpp runtime/breakables.cpp runtime/breakables_data.cpp build/full-port/shadow-core-host-2/libencore-shadow.a -Wl,-Map="$out/$test.map" -o "$out/$test" >"$out/$test-compile.stdout.log" 2>"$out/$test-compile.stderr.log"
done
python3 build/full-port/check-storage-linked-inputs.py "$out"
"$out/audio" romfs/sound/banks/opening.encaudio romfs/
fixture=build/full-port/coin-audio-fixture-1
"$out/coin_scene_data" "$fixture/scene.enccf" "$fixture/coins.encoin" "$fixture/breakables.encbrk" "$fixture/partial.enccf"
"$out/coin_scene" "$fixture/scene.enccf" "$fixture/coins.encoin" "$fixture/breakables.encbrk"
"$out/coin_scene_source" "$fixture/scene.enccf" "$fixture/coins.encoin" "$fixture/breakables.encbrk" "$fixture/native.tsv"
'''
(out/'run.sh').write_text(script,encoding='utf-8',newline='\n')
run('gcc-sanitizer',['wsl','-d','Ubuntu-20.04','--','bash','/mnt/d/GameDev/3DS/mother-encore-3ds/build/full-port/item-admission-ci-followups-1/run.sh'])
assert all(hashlib.sha256((root/p).read_bytes()).hexdigest()==h for p,h in inputs.items())
(out/'inputs.json').write_text(json.dumps(inputs,indent=2)+'\n',encoding='utf-8');print('All focused inputs unchanged.')
