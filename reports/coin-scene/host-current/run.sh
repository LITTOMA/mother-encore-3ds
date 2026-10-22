#!/bin/bash
set -eu
cd /mnt/d/GameDev/3DS/mother-encore-3ds
export ASAN_OPTIONS=detect_leaks=1
export UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1
flags=(-std=c++17 -O1 -g -Wall -Wextra -Wpedantic -Wno-misleading-indentation -fno-rtti -fno-exceptions -fno-pie -no-pie -fno-omit-frame-pointer -fsanitize=address,undefined -Iinclude)
out=build/full-port/coin-scene-source-host-2
for module in coin_scene coin_scene_data coins;do
 g++ "${flags[@]}" -c runtime/$module.cpp -o "$out/$module.o"
done
objects=("$out/coin_scene.o" "$out/coin_scene_data.o" "$out/coins.o" "$out/coins_data.o" "$out/circle_motion.o" "$out/breakables.o" "$out/breakables_data.o" build/full-port/shadow-core-host-2/libencore-shadow.a)
for test in coin_scene_source coin_scene coin_scene_data coin_factory;do
 g++ "${flags[@]}" tests/${test}_tests.cpp "${objects[@]}" -o "$out/$test"
done
fixture=build/full-port/coin-scene-fixture-final-1
"$out/coin_scene_source" "$fixture/scene.enccf" "$fixture/coins.encoin" "$fixture/breakables.encbrk" "$fixture/native.tsv"
"$out/coin_scene" "$fixture/scene.enccf" "$fixture/coins.encoin" "$fixture/breakables.encbrk"
"$out/coin_scene_data" "$fixture/scene.enccf" "$fixture/coins.encoin" "$fixture/breakables.encbrk" "$fixture/partial.enccf"
"$out/coin_factory" build/full-port/coin-fixture-final-1/coins.encoin build/full-port/coin-fixture-final-1/factory.tsv
