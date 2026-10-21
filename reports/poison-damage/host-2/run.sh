#!/bin/bash
set -eu
cd /mnt/d/GameDev/3DS/mother-encore-3ds
export ASAN_OPTIONS=detect_leaks=1
export UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1
g++ -std=c++17 -O1 -g -Wall -Wextra -Wpedantic -Wno-misleading-indentation -fno-rtti -fno-exceptions -fno-pie -no-pie -fno-omit-frame-pointer -fsanitize=address,undefined -Iinclude tests/poison_damage_tests.cpp runtime/world_damage.cpp runtime/poison_hazards.cpp runtime/poison_damage.cpp build/full-port/shadow-core-host-2/libencore-shadow.a -o build/full-port/poison-damage-host-2/check
build/full-port/poison-damage-host-2/check build/full-port/poison-damage-fixture-1/damage.encwdm build/full-port/poison-damage-fixture-1/poison.encpoison build/full-port/poison-damage-fixture-1/steps.encsteps build/full-port/poison-damage-fixture-1/native.tsv
