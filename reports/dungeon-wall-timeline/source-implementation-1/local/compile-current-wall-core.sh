#!/usr/bin/env bash
set -euo pipefail
task_source="$1"
if [[ ! "$task_source" =~ ^runtime/[a-z_]+\.cpp$ ]]; then exit 2; fi
task_stem="${task_source##*/}"
task_stem="${task_stem%.cpp}"
g++ -std=c++17 -O1 -g -Wall -Wextra -Wpedantic -fno-exceptions -fno-rtti -fno-omit-frame-pointer -fno-pie -fsanitize=address,undefined -Iinclude '-DENCORE_TEST_ROOM_PATH="romfs/world/house/opening.encroom"' -c "$task_source" -o "build/full-port/wall-host-current-1/obj/$task_stem.o" > "build/full-port/wall-host-current-1/compile-$task_stem.log" 2>&1
