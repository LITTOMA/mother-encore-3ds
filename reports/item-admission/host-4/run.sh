#!/bin/bash
set -eu
cd /mnt/d/GameDev/3DS/mother-encore-3ds
export ASAN_OPTIONS=detect_leaks=1
export UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1
flags=(-std=c++17 -O1 -g -Wall -Wextra -Wpedantic -Wno-misleading-indentation -fno-rtti -fno-exceptions -fno-pie -no-pie -fno-omit-frame-pointer -fsanitize=address,undefined -Iinclude)
out=build/full-port/item-admission-host-4
g++ "${flags[@]}" tests/native_session_tests.cpp build/full-port/item-admission-host-1/native_session.o build/full-port/item-admission-host-1/field_menus.o build/full-port/item-admission-host-1/field_menus_data.o build/full-port/item-admission-host-1/field_story.o build/full-port/item-admission-host-1/item_details.o build/full-port/item-admission-host-1/storage_data.o build/full-port/item-admission-host-2/session_restore.o build/full-port/shadow-core-host-2/libencore-shadow.a -Wl,-Map="$out/native_session.map" -o "$out/native_session" >"$out/compile.stdout.log" 2>"$out/compile.stderr.log"
python3 build/full-port/check-storage-linked-inputs.py "$out"
"$out/native_session" romfs build/full-port/catacombs-catalog-fixture-3/legacy11.encsession
