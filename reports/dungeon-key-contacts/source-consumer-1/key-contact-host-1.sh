#!/bin/bash
set -eu
cd /mnt/d/GameDev/3DS/mother-encore-3ds
mkdir -p build/full-port/key-contact-host-1
g++ -std=c++17 -O1 -g -Wpedantic -Wall -Wextra -Wno-misleading-indentation -fno-rtti -fno-exceptions -fno-pie -no-pie -fno-omit-frame-pointer -fsanitize=address,undefined -Iinclude tests/dungeon_key_contacts_tests.cpp runtime/dungeon_key_contacts.cpp runtime/field_area_contacts.cpp runtime/dungeon_key_media.cpp runtime/dungeon_key_clocks.cpp runtime/dungeon_key_state.cpp runtime/dungeon_keys.cpp runtime/audio_data.cpp runtime/content.cpp runtime/file_io.cpp -o build/full-port/key-contact-host-1/check
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 build/full-port/key-contact-host-1/check build/full-port/key-media-fixture-2/source.enckmp build/full-port/key-contact-fixture-1/reference.tsv
