#!/bin/bash
set -eu
cd /mnt/d/GameDev/3DS/mother-encore-3ds
export ASAN_OPTIONS=detect_leaks=1
export UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1
FLAGS="-std=c++17 -O1 -g -Wall -Wextra -Wpedantic -Wno-misleading-indentation -fno-rtti -fno-exceptions -fno-pie -no-pie -fno-omit-frame-pointer -fsanitize=address,undefined -Iinclude"
g++ $FLAGS tests/catacombs_steps_tests.cpp build/full-port/catacombs-steps-core-host-1/libencore-steps.a -o build/full-port/catacombs-steps-host-1/check
build/full-port/catacombs-steps-host-1/check build/full-port/catacombs-steps-fixture-1
