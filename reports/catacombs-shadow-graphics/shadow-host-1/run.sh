#!/bin/bash
set -eu
cd /mnt/d/GameDev/3DS/mother-encore-3ds
export ASAN_OPTIONS=detect_leaks=1
export UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1
FLAGS="-std=c++17 -O1 -g -Wall -Wextra -Wpedantic -Wno-misleading-indentation -fno-rtti -fno-exceptions -fno-pie -no-pie -fno-omit-frame-pointer -fsanitize=address,undefined -Iinclude"
g++ $FLAGS tests/shadow_clock_tests.cpp build/full-port/shadow-core-host-2/libencore-shadow.a -o build/full-port/shadow-host-1/clock
build/full-port/shadow-host-1/clock build/full-port/shadow-fixture-1
g++ $FLAGS -Itests/dungeon_key_renderer_stubs -Iplatform/ctr tests/shadow_graphics_tests.cpp build/full-port/shadow-core-host-2/libencore-shadow.a -o build/full-port/shadow-host-1/graphics
build/full-port/shadow-host-1/graphics build/full-port/shadow-fixture-1 build/full-port/shadow-host-1/scratch-fake
build/full-port/shadow-host-1/graphics build/full-port/shadow-fixture-1 build/full-port/shadow-host-1/scratch-real build/full-port/shadow-graphics-candidate-2
g++ $FLAGS tests/catacombs_steps_tests.cpp build/full-port/shadow-core-host-2/libencore-shadow.a -o build/full-port/shadow-host-1/legacy-polygon
build/full-port/shadow-host-1/legacy-polygon build/full-port/catacombs-steps-fixture-1
g++ $FLAGS tests/field_steps_tests.cpp build/full-port/shadow-core-host-2/libencore-shadow.a -o build/full-port/shadow-host-1/legacy-rectangles
build/full-port/shadow-host-1/legacy-rectangles tests/fixtures romfs/sound/banks/opening.encaudio
