#!/usr/bin/env bash
set -euo pipefail
mkdir -p build/full-port/camera-random-shake-native
g++ --version > build/full-port/camera-random-shake-native/compiler.txt
g++ -std=c++17 -O1 -g -fno-omit-frame-pointer -fno-pie -no-pie -fsanitize=address,undefined -Iinclude -Itests '-DENCORE_TEST_ROOM_PATH="romfs/world/house/opening.encroom"' tests/camera_random_shake_tests.cpp runtime/cutscene_camera.cpp runtime/source_random.cpp runtime/room_data.cpp runtime/file_io.cpp -o build/full-port/camera-random-shake-native/check 2> build/full-port/camera-random-shake-native/compile.log
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 build/full-port/camera-random-shake-native/check build/full-port/camera-random-shaker-original-3/states.tsv > build/full-port/camera-random-shake-native/run-1.log 2>&1
cat build/full-port/camera-random-shake-native/run-1.log
