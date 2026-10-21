#!/bin/bash
set -eu
cd /mnt/d/GameDev/3DS/mother-encore-3ds
g++ -std=c++17 -O1 -g -Wall -Wextra -Wpedantic -Wno-misleading-indentation -fno-rtti -fno-exceptions -fno-pie -no-pie -fno-omit-frame-pointer -fsanitize=address,undefined -Iinclude tests/dungeon_terrain_tests.cpp runtime/dungeon_terrain.cpp runtime/field_data.cpp runtime/file_io.cpp -o build/full-port/terrain-host-1/check
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 build/full-port/terrain-host-1/check build/full-port/terrain-fixture-1/source.encterr build/full-port/terrain-fixture-1/cells.tsv
