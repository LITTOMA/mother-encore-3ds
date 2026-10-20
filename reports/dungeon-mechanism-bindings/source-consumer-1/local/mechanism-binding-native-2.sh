#!/usr/bin/env bash
set -euo pipefail
mkdir -p build/full-port/mechanism-binding-native-1
g++ -std=c++17 -O1 -g -Wall -Wextra -Wpedantic -fno-exceptions -fno-rtti -fno-omit-frame-pointer -fno-pie -no-pie -fsanitize=address,undefined -Iinclude tests/dungeon_mechanism_bindings_tests.cpp runtime/dungeon_mechanism_bindings.cpp build/full-port/wall-host-current-1/libencore-current.a -o build/full-port/mechanism-binding-native-2/check > build/full-port/mechanism-binding-native-2/compile.log 2>&1
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 build/full-port/mechanism-binding-native-2/check build/full-port/mechanism-bindings-candidate-2/source.encdma > build/full-port/mechanism-binding-native-2/run.log 2>&1
cat build/full-port/mechanism-binding-native-2/run.log
