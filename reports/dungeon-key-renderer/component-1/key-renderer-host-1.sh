#!/bin/bash
set -eu
cd /mnt/d/GameDev/3DS/mother-encore-3ds
mkdir -p build/full-port/key-renderer-host-1
g++ -std=c++17 -O1 -g -Wall -Wextra -Wpedantic -Wno-misleading-indentation -fno-rtti -fno-exceptions -fno-pie -no-pie -fno-omit-frame-pointer -fsanitize=address,undefined -Itests/dungeon_key_renderer_stubs -Iplatform/ctr -Iinclude tests/dungeon_key_renderer_tests.cpp runtime/dungeon_key_media.cpp runtime/dungeon_key_clocks.cpp runtime/dungeon_key_state.cpp runtime/dungeon_keys.cpp runtime/audio_data.cpp runtime/content.cpp runtime/file_io.cpp -o build/full-port/key-renderer-host-1/check
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 build/full-port/key-renderer-host-1/check build/full-port/key-renderer-fixture-1/source.enckmp build/full-port/key-renderer-fixture-1 build/full-port/key-renderer-fixture-1/scratch build/full-port/key-renderer-fixture-1/original/reference.tsv
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 build/full-port/key-renderer-host-1/check build/full-port/key-media-candidate-1/source.enckmp build/full-port/key-media-candidate-1 build/full-port/key-renderer-genuine-scratch-1 build/full-port/key-media-fixture-2/reference.tsv
