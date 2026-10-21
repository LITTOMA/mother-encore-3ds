#!/bin/bash
set -eu
cd /mnt/d/GameDev/3DS/mother-encore-3ds
export ASAN_OPTIONS=detect_leaks=1
export UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1
g++ -std=c++17 -O1 -g -Wall -Wextra -Wpedantic -Wno-misleading-indentation -fno-rtti -fno-exceptions -fno-pie -no-pie -fno-omit-frame-pointer -fsanitize=address,undefined -Iinclude tests/dungeon_terrain_tests.cpp build/full-port/terrain-core-host-1/libencore-terrain.a -o build/full-port/terrain-host-3/check
build/full-port/terrain-host-3/check build/full-port/terrain-fixture-2/source.encterr build/full-port/terrain-fixture-2/cells.tsv build/full-port/terrain-fixture-2/synthetic-no-navigation.encterr
build/full-port/terrain-host-2/field-area-check tests/fixtures romfs
build/full-port/terrain-core-host-1/admission-check build/full-port/terrain-fixture-2/source.encterr
