#!/bin/bash
set -eu
cd /mnt/d/GameDev/3DS/mother-encore-3ds
export ASAN_OPTIONS=detect_leaks=1
export UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1
flags=(-std=c++17 -O1 -g -Wall -Wextra -Wpedantic -Wno-misleading-indentation -fno-rtti -fno-exceptions -fno-pie -no-pie -fno-omit-frame-pointer -fsanitize=address,undefined -Iinclude)
out=build/full-port/item-admission-host-1
archive=build/full-port/shadow-core-host-2/libencore-shadow.a
compiler_pids=()
g++ "${flags[@]}" -c runtime/native_session.cpp -o "$out/native_session.o" >"$out/compile-core-native_session.stdout.log" 2>"$out/compile-core-native_session.stderr.log" &
compiler_pids+=($!)
g++ "${flags[@]}" -c runtime/field_menus.cpp -o "$out/field_menus.o" >"$out/compile-core-field_menus.stdout.log" 2>"$out/compile-core-field_menus.stderr.log" &
compiler_pids+=($!)
compile_status=0
for compiler_pid in "${compiler_pids[@]}"; do wait "$compiler_pid" || compile_status=1; done
test "$compile_status" -eq 0
compiler_pids=()
g++ "${flags[@]}" -c runtime/field_menus_data.cpp -o "$out/field_menus_data.o" >"$out/compile-core-field_menus_data.stdout.log" 2>"$out/compile-core-field_menus_data.stderr.log" &
compiler_pids+=($!)
g++ "${flags[@]}" -c runtime/field_story.cpp -o "$out/field_story.o" >"$out/compile-core-field_story.stdout.log" 2>"$out/compile-core-field_story.stderr.log" &
compiler_pids+=($!)
compile_status=0
for compiler_pid in "${compiler_pids[@]}"; do wait "$compiler_pid" || compile_status=1; done
test "$compile_status" -eq 0
compiler_pids=()
g++ "${flags[@]}" -c runtime/item_details.cpp -o "$out/item_details.o" >"$out/compile-core-item_details.stdout.log" 2>"$out/compile-core-item_details.stderr.log" &
compiler_pids+=($!)
g++ "${flags[@]}" -c runtime/storage_data.cpp -o "$out/storage_data.o" >"$out/compile-core-storage_data.stdout.log" 2>"$out/compile-core-storage_data.stderr.log" &
compiler_pids+=($!)
compile_status=0
for compiler_pid in "${compiler_pids[@]}"; do wait "$compiler_pid" || compile_status=1; done
test "$compile_status" -eq 0
compiler_pids=()
g++ "${flags[@]}" tests/item_consumption_tests.cpp "$out/native_session.o" "$out/field_menus.o" "$out/field_menus_data.o" "$out/field_story.o" "$out/item_details.o" "$out/storage_data.o" "$archive" -Wl,-Map="$out/item_consumption.map" -o "$out/item_consumption" >"$out/compile-test-item_consumption.stdout.log" 2>"$out/compile-test-item_consumption.stderr.log" &
compiler_pids+=($!)
g++ "${flags[@]}" tests/field_menus_tests.cpp "$out/native_session.o" "$out/field_menus.o" "$out/field_menus_data.o" "$out/field_story.o" "$out/item_details.o" "$out/storage_data.o" "$archive" -Wl,-Map="$out/field_menus.map" -o "$out/field_menus" >"$out/compile-test-field_menus.stdout.log" 2>"$out/compile-test-field_menus.stderr.log" &
compiler_pids+=($!)
compile_status=0
for compiler_pid in "${compiler_pids[@]}"; do wait "$compiler_pid" || compile_status=1; done
test "$compile_status" -eq 0
compiler_pids=()
g++ "${flags[@]}" tests/catacombs_item_session_tests.cpp "$out/native_session.o" "$out/field_menus.o" "$out/field_menus_data.o" "$out/field_story.o" "$out/item_details.o" "$out/storage_data.o" "$archive" -Wl,-Map="$out/catacombs_item_session.map" -o "$out/catacombs_item_session" >"$out/compile-test-catacombs_item_session.stdout.log" 2>"$out/compile-test-catacombs_item_session.stderr.log" &
compiler_pids+=($!)
g++ "${flags[@]}" tests/native_session_tests.cpp "$out/native_session.o" "$out/field_menus.o" "$out/field_menus_data.o" "$out/field_story.o" "$out/item_details.o" "$out/storage_data.o" "$archive" -Wl,-Map="$out/native_session.map" -o "$out/native_session" >"$out/compile-test-native_session.stdout.log" 2>"$out/compile-test-native_session.stderr.log" &
compiler_pids+=($!)
compile_status=0
for compiler_pid in "${compiler_pids[@]}"; do wait "$compiler_pid" || compile_status=1; done
test "$compile_status" -eq 0
compiler_pids=()
g++ "${flags[@]}" tests/item_details_tests.cpp "$out/native_session.o" "$out/field_menus.o" "$out/field_menus_data.o" "$out/field_story.o" "$out/item_details.o" "$out/storage_data.o" "$archive" -Wl,-Map="$out/item_details.map" -o "$out/item_details" >"$out/compile-test-item_details.stdout.log" 2>"$out/compile-test-item_details.stderr.log" &
compiler_pids+=($!)
g++ "${flags[@]}" tests/catacombs_item_details_tests.cpp "$out/native_session.o" "$out/field_menus.o" "$out/field_menus_data.o" "$out/field_story.o" "$out/item_details.o" "$out/storage_data.o" "$archive" -Wl,-Map="$out/catacombs_item_details.map" -o "$out/catacombs_item_details" >"$out/compile-test-catacombs_item_details.stdout.log" 2>"$out/compile-test-catacombs_item_details.stderr.log" &
compiler_pids+=($!)
compile_status=0
for compiler_pid in "${compiler_pids[@]}"; do wait "$compiler_pid" || compile_status=1; done
test "$compile_status" -eq 0
compiler_pids=()
g++ "${flags[@]}" tests/catacombs_storage_tests.cpp "$out/native_session.o" "$out/field_menus.o" "$out/field_menus_data.o" "$out/field_story.o" "$out/item_details.o" "$out/storage_data.o" "$archive" -Wl,-Map="$out/catacombs_storage.map" -o "$out/catacombs_storage" >"$out/compile-test-catacombs_storage.stdout.log" 2>"$out/compile-test-catacombs_storage.stderr.log" &
compiler_pids+=($!)
g++ "${flags[@]}" tests/storage_runtime_tests.cpp "$out/native_session.o" "$out/field_menus.o" "$out/field_menus_data.o" "$out/field_story.o" "$out/item_details.o" "$out/storage_data.o" "$archive" -Wl,-Map="$out/storage_runtime.map" -o "$out/storage_runtime" >"$out/compile-test-storage_runtime.stdout.log" 2>"$out/compile-test-storage_runtime.stderr.log" &
compiler_pids+=($!)
compile_status=0
for compiler_pid in "${compiler_pids[@]}"; do wait "$compiler_pid" || compile_status=1; done
test "$compile_status" -eq 0
python3 build/full-port/check-storage-linked-inputs.py "$out"
"$out/item_consumption" romfs build/full-port/item-consume-fixture-5 >"$out/item_consumption.stdout.log" 2>"$out/item_consumption.stderr.log"
"$out/field_menus" romfs "$out/preferences.encprefs" >"$out/field_menus.stdout.log" 2>"$out/field_menus.stderr.log"
"$out/catacombs_item_session" romfs build/full-port/catacombs-catalog-fixture-3 >"$out/catacombs_item_session.stdout.log" 2>"$out/catacombs_item_session.stderr.log"
"$out/native_session" romfs >"$out/native_session.stdout.log" 2>"$out/native_session.stderr.log"
"$out/item_details" romfs >"$out/item_details.stdout.log" 2>"$out/item_details.stderr.log"
"$out/catacombs_item_details" build/full-port/catacombs-details-fixture-2 >"$out/catacombs_item_details.stdout.log" 2>"$out/catacombs_item_details.stderr.log"
"$out/catacombs_storage" build/full-port/catacombs-storage-fixture-2 >"$out/catacombs_storage.stdout.log" 2>"$out/catacombs_storage.stderr.log"
"$out/storage_runtime" romfs >"$out/storage_runtime.stdout.log" 2>"$out/storage_runtime.stderr.log"
python3 tests/check_storage_sort_failures.py "$out/catacombs_storage" build/full-port/catacombs-storage-fixture-2 "$out/sort-failures"
for bad in build/full-port/item-consume-fixture-5/bad-reference-*.bin; do
 name=$(basename "$bad" .bin)
 set +e
 "$out/item_consumption" romfs build/full-port/item-consume-fixture-5 "$bad" >"$out/$name.stdout.log" 2>"$out/$name.stderr.log"
 status=$?
 set -e
 test "$status" -eq 1
 if grep -Eq '(runtime error:|ERROR: AddressSanitizer|ERROR: LeakSanitizer)' "$out/$name.stderr.log"; then exit 1; fi
done
