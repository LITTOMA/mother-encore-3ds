#!/bin/bash
set -eu
cd /mnt/d/GameDev/3DS/mother-encore-3ds
export ASAN_OPTIONS=detect_leaks=1
export UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1
out=build/full-port/item-admission-items-after-1
g++ -std=c++17 -O1 -g -fno-rtti -fno-exceptions -fno-pie -no-pie -fno-omit-frame-pointer -fsanitize=address,undefined -Iinclude "$out/items_data_tests.cpp" build/full-port/shadow-core-host-2/libencore-shadow.a -Wl,-Map="$out/items.map" -o "$out/items" >"$out/compile.stdout.log" 2>"$out/compile.stderr.log"
python3 build/full-port/check-storage-linked-inputs.py "$out"
"$out/items" romfs/ui/opening.encitems
