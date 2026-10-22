#!/bin/bash
set -eu
cd /mnt/d/GameDev/3DS/mother-encore-3ds
export ASAN_OPTIONS=detect_leaks=1
export UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1
flags=(-std=c++17 -O1 -g -Wall -Wextra -Wpedantic -Wno-misleading-indentation -fno-rtti -fno-exceptions -fno-pie -no-pie -fno-omit-frame-pointer -fsanitize=address,undefined -Iinclude)
out=build/full-port/item-admission-host-3
prior=build/full-port/item-admission-host-2
archive=build/full-port/shadow-core-host-2/libencore-shadow.a
compiler_pids=()
g++ "${flags[@]}" tests/field_menus_tests.cpp build/full-port/item-admission-host-1/native_session.o build/full-port/item-admission-host-1/field_menus.o build/full-port/item-admission-host-1/field_menus_data.o build/full-port/item-admission-host-1/field_story.o build/full-port/item-admission-host-1/item_details.o build/full-port/item-admission-host-1/storage_data.o build/full-port/item-admission-host-2/session_restore.o "$archive" -Wl,-Map="$out/field_menus.map" -o "$out/field_menus" >"$out/compile-test-field_menus.stdout.log" 2>"$out/compile-test-field_menus.stderr.log" &
compiler_pids+=($!)
g++ "${flags[@]}" tests/native_session_tests.cpp build/full-port/item-admission-host-1/native_session.o build/full-port/item-admission-host-1/field_menus.o build/full-port/item-admission-host-1/field_menus_data.o build/full-port/item-admission-host-1/field_story.o build/full-port/item-admission-host-1/item_details.o build/full-port/item-admission-host-1/storage_data.o build/full-port/item-admission-host-2/session_restore.o "$archive" -Wl,-Map="$out/native_session.map" -o "$out/native_session" >"$out/compile-test-native_session.stdout.log" 2>"$out/compile-test-native_session.stderr.log" &
compiler_pids+=($!)
compile_status=0
for compiler_pid in "${compiler_pids[@]}"; do wait "$compiler_pid" || compile_status=1; done
test "$compile_status" -eq 0
python3 build/full-port/check-storage-linked-inputs.py "$out"
test_status=0
if ! "$prior/item_consumption" romfs build/full-port/item-consume-fixture-5 >"$out/item_consumption.stdout.log" 2>"$out/item_consumption.stderr.log"; then test_status=1; fi
if ! "$out/field_menus" romfs "$out/preferences.encprefs" >"$out/field_menus.stdout.log" 2>"$out/field_menus.stderr.log"; then test_status=1; fi
if ! "$prior/catacombs_item_session" romfs build/full-port/catacombs-catalog-fixture-3 >"$out/catacombs_item_session.stdout.log" 2>"$out/catacombs_item_session.stderr.log"; then test_status=1; fi
if ! "$out/native_session" romfs build/full-port/catacombs-catalog-fixture-3/legacy11.encsession >"$out/native_session.stdout.log" 2>"$out/native_session.stderr.log"; then test_status=1; fi
if ! "$prior/item_details" romfs >"$out/item_details.stdout.log" 2>"$out/item_details.stderr.log"; then test_status=1; fi
if ! "$prior/catacombs_item_details" build/full-port/catacombs-details-fixture-2 >"$out/catacombs_item_details.stdout.log" 2>"$out/catacombs_item_details.stderr.log"; then test_status=1; fi
if ! "$prior/catacombs_storage" build/full-port/catacombs-storage-fixture-2 >"$out/catacombs_storage.stdout.log" 2>"$out/catacombs_storage.stderr.log"; then test_status=1; fi
if ! "$prior/storage_runtime" romfs >"$out/storage_runtime.stdout.log" 2>"$out/storage_runtime.stderr.log"; then test_status=1; fi
if ! python3 tests/check_storage_sort_failures.py "$prior/catacombs_storage" build/full-port/catacombs-storage-fixture-2 "$out/sort-failures"; then test_status=1; fi
for bad in build/full-port/item-consume-fixture-5/bad-reference-*.bin; do
 name=$(basename "$bad" .bin)
 set +e
 "$prior/item_consumption" romfs build/full-port/item-consume-fixture-5 "$bad" >"$out/$name.stdout.log" 2>"$out/$name.stderr.log"
 status=$?
 set -e
 test "$status" -eq 1 || test_status=1
 if grep -Eq '(runtime error:|ERROR: AddressSanitizer|ERROR: LeakSanitizer)' "$out/$name.stderr.log"; then test_status=1; fi
done
exit "$test_status"
