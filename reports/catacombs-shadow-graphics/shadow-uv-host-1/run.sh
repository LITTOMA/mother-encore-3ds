#!/bin/bash
set -eu
cd /mnt/d/GameDev/3DS/mother-encore-3ds
export ASAN_OPTIONS=detect_leaks=1
export UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1
g++ -std=c++17 -O1 -g -Wall -Wextra -Wpedantic -Wno-misleading-indentation -fno-rtti -fno-exceptions -fno-pie -no-pie -fno-omit-frame-pointer -fsanitize=address,undefined -Iinclude -Itests/dungeon_key_renderer_stubs -Iplatform/ctr tests/shadow_graphics_tests.cpp build/full-port/shadow-core-host-2/libencore-shadow.a -o build/full-port/shadow-uv-host-1/graphics
build/full-port/shadow-uv-host-1/graphics build/full-port/shadow-fixture-1 build/full-port/shadow-uv-host-1/scratch-fake
build/full-port/shadow-uv-host-1/graphics build/full-port/shadow-fixture-1 build/full-port/shadow-uv-host-1/scratch-real build/full-port/shadow-graphics-candidate-2
