#!/bin/bash
set -eu
cd /mnt/d/GameDev/3DS/mother-encore-3ds
export ASAN_OPTIONS=detect_leaks=1
export UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1
FLAGS="-std=c++17 -O1 -g -Wall -Wextra -Wpedantic -Wno-misleading-indentation -fno-rtti -fno-exceptions -fno-pie -no-pie -fno-omit-frame-pointer -fsanitize=address,undefined -Iinclude"
g++ $FLAGS tests/catacombs_music_tests.cpp build/full-port/terrain-core-host-1/libencore-terrain.a -o build/full-port/catacombs-music-host-1/check
build/full-port/catacombs-music-host-1/check build/full-port/catacombs-music-fixture-1
g++ $FLAGS tests/music_scene_graph_tests.cpp build/full-port/terrain-core-host-1/libencore-terrain.a -o build/full-port/catacombs-music-host-1/original-check
build/full-port/catacombs-music-host-1/original-check romfs/sound/banks/podunk.encmusic tests/fixtures/mayor.encmusic tests/fixtures/music-scenes.tsv
