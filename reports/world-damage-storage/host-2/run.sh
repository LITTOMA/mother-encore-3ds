#!/bin/bash
set -eu
cd /mnt/d/GameDev/3DS/mother-encore-3ds
export ASAN_OPTIONS=detect_leaks=1
export UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1
g++ -std=c++17 -O1 -g -Wall -Wextra -Wpedantic -Wno-misleading-indentation -fno-rtti -fno-exceptions -fno-pie -no-pie -fno-omit-frame-pointer -fsanitize=address,undefined -Ibuild/worktrees/world-damage-final/include build/worktrees/world-damage-final/tests/world_damage_tests.cpp build/worktrees/world-damage-final/runtime/world_damage.cpp build/full-port/shadow-core-host-2/libencore-shadow.a -o build/full-port/world-damage-storage-host-2/check
build/full-port/world-damage-storage-host-2/check build/full-port/world-damage-fixture-2/world-damage.encwdm build/full-port/world-damage-fixture-2/native.tsv
