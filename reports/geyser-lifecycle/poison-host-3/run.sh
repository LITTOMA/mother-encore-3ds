#!/bin/bash
set -eu
cd /mnt/d/GameDev/3DS/mother-encore-3ds
export ASAN_OPTIONS=detect_leaks=1
export UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1
g++ -std=c++17 -O1 -g -Wall -Wextra -Wpedantic -Wno-misleading-indentation -fno-rtti -fno-exceptions -fno-pie -no-pie -fno-omit-frame-pointer -fsanitize=address,undefined -Iinclude tests/poison_hazards_tests.cpp runtime/poison_hazards.cpp build/full-port/shadow-core-host-2/libencore-shadow.a -o build/full-port/poison-host-3/check
build/full-port/poison-host-3/check build/full-port/poison-fixture-1/catacombs.encpoison build/full-port/catacombs-steps-fixture-1/catacombs.encsteps build/full-port/poison-fixture-1/clocks.tsv build/full-port/poison-fixture-1/contacts.tsv
