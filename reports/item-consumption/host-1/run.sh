#!/bin/bash
set -eu
cd /mnt/d/GameDev/3DS/mother-encore-3ds
export ASAN_OPTIONS=detect_leaks=1
export UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1
flags=(-std=c++17 -O1 -g -Wall -Wextra -Wpedantic -Wno-misleading-indentation -fno-rtti -fno-exceptions -fno-pie -no-pie -fno-omit-frame-pointer -fsanitize=address,undefined -Iinclude)
out=build/full-port/item-consume-host-1
archive=build/full-port/shadow-core-host-2/libencore-shadow.a
g++ "${flags[@]}" -c runtime/native_session.cpp -o "$out/native_session.o"
g++ "${flags[@]}" -c runtime/field_menus.cpp -o "$out/field_menus.o"
g++ "${flags[@]}" -c runtime/field_menus_data.cpp -o "$out/field_menus_data.o"
g++ "${flags[@]}" -c runtime/field_story.cpp -o "$out/field_story.o"
g++ "${flags[@]}" tests/item_consumption_tests.cpp "$out/native_session.o" "$out/field_menus.o" "$out/field_menus_data.o" "$out/field_story.o" "$archive" -o "$out/item_consumption"
g++ "${flags[@]}" tests/field_menus_tests.cpp "$out/native_session.o" "$out/field_menus.o" "$out/field_menus_data.o" "$out/field_story.o" "$archive" -o "$out/field_menus"
g++ "${flags[@]}" tests/catacombs_item_session_tests.cpp "$out/native_session.o" "$out/field_menus.o" "$out/field_menus_data.o" "$out/field_story.o" "$archive" -o "$out/catacombs_item_session"
g++ "${flags[@]}" tests/native_session_tests.cpp "$out/native_session.o" "$out/field_menus.o" "$out/field_menus_data.o" "$out/field_story.o" "$archive" -o "$out/native_session"
"$out/item_consumption" romfs build/full-port/item-consume-fixture-4
"$out/field_menus" romfs "$out/preferences.encprefs"
"$out/catacombs_item_session" romfs build/full-port/catacombs-catalog-fixture-2
"$out/native_session" romfs
for bad in build/full-port/item-consume-fixture-4/bad-reference-*.bin; do
  name=$(basename "$bad" .bin)
  set +e
  "$out/item_consumption" romfs build/full-port/item-consume-fixture-4 "$bad" >"$out/$name.stdout.log" 2>"$out/$name.stderr.log"
  status=$?
  set -e
  test "$status" -eq 1
  if grep -Eq '(runtime error:|ERROR: AddressSanitizer|ERROR: LeakSanitizer)' "$out/$name.stderr.log"; then exit 1; fi
done
