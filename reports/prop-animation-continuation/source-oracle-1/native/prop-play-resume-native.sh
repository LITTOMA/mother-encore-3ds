#!/usr/bin/env bash
set -euo pipefail
mkdir -p build/full-port/prop-play-resume-native
g++ --version > build/full-port/prop-play-resume-native/compiler.txt
g++ -std=c++17 -O1 -g -fno-omit-frame-pointer -fno-pie -no-pie -fsanitize=address,undefined -Iinclude tests/prop_play_resume_tests.cpp runtime/prop_clips.cpp runtime/file_io.cpp -o build/full-port/prop-play-resume-native/check 2> build/full-port/prop-play-resume-native/compile-1.log
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 build/full-port/prop-play-resume-native/check build/full-port/prop-resume-data-1 build/full-port/prop-resume-original-2/states.tsv > build/full-port/prop-play-resume-native/run-1.log 2>&1
cat build/full-port/prop-play-resume-native/run-1.log
