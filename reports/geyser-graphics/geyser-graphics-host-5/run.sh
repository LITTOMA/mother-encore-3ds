#!/bin/bash
set -eu
cd /mnt/d/GameDev/3DS/mother-encore-3ds
export ASAN_OPTIONS=detect_leaks=1
export UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1
g++ -std=c++17 -O1 -g -Wall -Wextra -Wpedantic -Wno-misleading-indentation -fno-rtti -fno-exceptions -fno-pie -no-pie -fno-omit-frame-pointer -fsanitize=address,undefined -Itests/dungeon_key_renderer_stubs -Iinclude -Iplatform/ctr tests/geyser_graphics_tests.cpp runtime/geyser_timeline.cpp runtime/geyser_graphics.cpp build/full-port/shadow-core-host-2/libencore-shadow.a -o build/full-port/geyser-graphics-host-5/check
build/full-port/geyser-graphics-host-5/check build/full-port/geyser-graphics-fixture-1/data/world/catacombs/geyser.encgeysergfx build/full-port/geyser-graphics-fixture-1 build/full-port/geyser-graphics-fixture-1/native.tsv build/full-port/geyser-graphics-host-5/scratch
