#!/usr/bin/env bash
set -euo pipefail
mkdir -p build/full-port/wall-host-current-1/obj
g++ --version > build/full-port/wall-host-current-1/compiler.txt
xargs -P4 -n1 bash build/full-port/compile-current-wall-core.sh < build/full-port/wall-host-current-1/core-sources.txt
ar rcs build/full-port/wall-host-current-1/libencore-current.a build/full-port/wall-host-current-1/obj/*.o
g++ -std=c++17 -O1 -g -fno-exceptions -fno-rtti -fno-omit-frame-pointer -fno-pie -no-pie -fsanitize=address,undefined -Iinclude -Itests '-DENCORE_TEST_ROOM_PATH="romfs/world/house/opening.encroom"' -DENCORE_HOUSE_ROUTE_TESTS=1 tests/battle_outcome_tests.cpp build/full-port/wall-host-current-1/libencore-current.a -o build/full-port/wall-host-current-1/house-runtime > build/full-port/wall-host-current-1/house-link.log 2>&1
g++ -std=c++17 -O1 -g -fno-exceptions -fno-rtti -fno-omit-frame-pointer -fno-pie -no-pie -fsanitize=address,undefined -Iinclude -Itests '-DENCORE_TEST_ROOM_PATH="romfs/world/house/opening.encroom"' tests/playable_opening_tests.cpp build/full-port/wall-host-current-1/libencore-current.a -o build/full-port/wall-host-current-1/playable-opening > build/full-port/wall-host-current-1/opening-link.log 2>&1
g++ -std=c++17 -O1 -g -fno-exceptions -fno-rtti -fno-omit-frame-pointer -fno-pie -no-pie -fsanitize=address,undefined -Iinclude -Itests '-DENCORE_TEST_ROOM_PATH="romfs/world/house/opening.encroom"' tests/pillow_sequence_tests.cpp build/full-port/wall-host-current-1/libencore-current.a -o build/full-port/wall-host-current-1/pillow-sequence > build/full-port/wall-host-current-1/pillow-link.log 2>&1
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 build/full-port/wall-host-current-1/house-runtime romfs/battle/lamp/opening.encround romfs/battle/lamp/opening.encbattle romfs/world/house/opening.enchouse > build/full-port/wall-host-current-1/house-run.log 2>&1
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 build/full-port/wall-host-current-1/playable-opening romfs > build/full-port/wall-host-current-1/opening-run.log 2>&1
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 build/full-port/wall-host-current-1/pillow-sequence romfs > build/full-port/wall-host-current-1/pillow-run.log 2>&1
cat build/full-port/wall-host-current-1/house-run.log build/full-port/wall-host-current-1/opening-run.log build/full-port/wall-host-current-1/pillow-run.log
