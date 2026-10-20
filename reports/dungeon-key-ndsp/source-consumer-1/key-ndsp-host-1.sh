#!/bin/bash
set -eu
cd /mnt/d/GameDev/3DS/mother-encore-3ds
mkdir -p build/full-port/key-ndsp-host-1
g++ -std=c++17 -O1 -g -Wpedantic -Wall -Wextra -Wno-misleading-indentation -ffunction-sections -fdata-sections -Wl,--gc-sections -fno-rtti -fno-exceptions -fno-pie -no-pie -fno-omit-frame-pointer -fsanitize=address,undefined -Itests/fixtures/audio_ndsp -Iplatform/ctr -Iinclude tests/dungeon_key_ndsp_tests.cpp platform/ctr/audio_player.cpp runtime/dungeon_key_media.cpp runtime/dungeon_key_clocks.cpp runtime/dungeon_key_state.cpp runtime/dungeon_keys.cpp runtime/audio_data.cpp runtime/content.cpp runtime/file_io.cpp -o build/full-port/key-ndsp-host-1/check
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 build/full-port/key-ndsp-host-1/check romfs/sound/banks/opening.encaudio romfs/ build/full-port/key-media-fixture-1/source.enckmp build/full-port/key-media-fixture-1 build/full-port/key-ndsp-test-pcm-1
