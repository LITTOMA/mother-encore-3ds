#!/bin/bash
set -eu
cd /mnt/d/GameDev/3DS/mother-encore-3ds
export ASAN_OPTIONS=detect_leaks=1
export UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1
flags=(-std=c++17 -O1 -g -Wall -Wextra -Wpedantic -Wno-misleading-indentation -fno-rtti -fno-exceptions -fno-pie -no-pie -fno-omit-frame-pointer -fsanitize=address,undefined -Iinclude)
out=build/full-port/catalog-session-host-1
archive=build/full-port/shadow-core-host-2/libencore-shadow.a
g++ "${flags[@]}" -c runtime/native_session.cpp -o "$out/native_session.o"
g++ "${flags[@]}" tests/catacombs_item_session_tests.cpp "$out/native_session.o" "$archive" -o "$out/catalog-session"
g++ "${flags[@]}" tests/native_session_tests.cpp "$out/native_session.o" "$archive" -o "$out/native-session"
"$out/catalog-session" romfs build/full-port/catacombs-catalog-fixture-2
"$out/native-session" romfs
