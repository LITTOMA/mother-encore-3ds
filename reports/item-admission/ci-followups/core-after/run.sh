#!/bin/bash
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
