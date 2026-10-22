#!/bin/bash
set -eu
cd /mnt/d/GameDev/3DS/mother-encore-3ds
export ASAN_OPTIONS=detect_leaks=1
export UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1
flags=(-std=c++17 -O1 -g -Wall -Wextra -Wpedantic -Wno-misleading-indentation -fno-rtti -fno-exceptions -fno-pie -no-pie -fno-omit-frame-pointer -fsanitize=address,undefined -Iinclude)
out=build/full-port/dropped-domain-host-11
scene=build/full-port/coin-scene-fixture-final-1
old=build/full-port/dropped-reentry-host-10
g++ -I"$old/source-snapshot/include" "${flags[@]}" -c "$old/source-snapshot/runtime/item_holder_grant.cpp" -o "$out/before-grant.o"
g++ -I"$old/source-snapshot/include" "${flags[@]}" -c "$old/source-snapshot/runtime/dropped_items.cpp" -o "$out/before-actor.o"
g++ -I"$old/source-snapshot/include" "${flags[@]}" tests/item_holder_grant_tests.cpp "$out/before-grant.o" "$old/dropped_items_data.o" "$old/breakables_data.o" build/full-port/shadow-core-host-2/libencore-shadow.a -o "$out/grant-before"
g++ -I"$old/source-snapshot/include" "${flags[@]}" tests/dropped_item_lifecycle_tests.cpp "$out/before-actor.o" "$out/before-grant.o" "$old/dropped_items_data.o" "$old/breakables_data.o" build/full-port/shadow-core-host-2/libencore-shadow.a -o "$out/actor-before"
for kind in grant actor; do
 set +e
 if test "$kind" = grant; then
  "$out/grant-before" "$out/standalone/source.encdrp" "$out/standalone/key-control.encdrp" >"$out/$kind-before.stdout.log" 2>"$out/$kind-before.stderr.log"
 else
  "$out/actor-before" "$out/standalone/source.encdrp" >"$out/$kind-before.stdout.log" 2>"$out/$kind-before.stderr.log"
 fi
 status=$?
 set -e
 test "$status" -ne 0
 grep -Eq '(runtime error:|ERROR: AddressSanitizer)' "$out/$kind-before.stderr.log"
done
g++ "${flags[@]}" -c runtime/dropped_items_data.cpp -o "$out/dropped_items_data.o"
g++ "${flags[@]}" -c runtime/item_holder_grant.cpp -o "$out/item_holder_grant.o"
g++ "${flags[@]}" -c runtime/dropped_items.cpp -o "$out/dropped_items.o"
g++ "${flags[@]}" -c runtime/breakables_data.cpp -o "$out/breakables_data.o"
g++ "${flags[@]}" -c runtime/breakables.cpp -o "$out/breakables.o"
g++ "${flags[@]}" -c runtime/coin_scene_data.cpp -o "$out/coin_scene_data.o"
g++ "${flags[@]}" -c runtime/coin_scene.cpp -o "$out/coin_scene.o"
g++ "${flags[@]}" -c runtime/coins_data.cpp -o "$out/coins_data.o"
g++ "${flags[@]}" -c runtime/coins.cpp -o "$out/coins.o"
g++ "${flags[@]}" -c runtime/circle_motion.cpp -o "$out/circle_motion.o"
g++ "${flags[@]}" tests/dropped_items_data_tests.cpp "$out/dropped_items_data.o" "$out/item_holder_grant.o" "$out/dropped_items.o" "$out/breakables_data.o" "$out/breakables.o" "$out/coin_scene_data.o" "$out/coin_scene.o" "$out/coins_data.o" "$out/coins.o" "$out/circle_motion.o" build/full-port/shadow-core-host-2/libencore-shadow.a -o "$out/dropped_items_data"
g++ "${flags[@]}" tests/item_holder_grant_tests.cpp "$out/dropped_items_data.o" "$out/item_holder_grant.o" "$out/dropped_items.o" "$out/breakables_data.o" "$out/breakables.o" "$out/coin_scene_data.o" "$out/coin_scene.o" "$out/coins_data.o" "$out/coins.o" "$out/circle_motion.o" build/full-port/shadow-core-host-2/libencore-shadow.a -o "$out/item_holder_grant"
g++ "${flags[@]}" tests/dropped_item_actor_tests.cpp "$out/dropped_items_data.o" "$out/item_holder_grant.o" "$out/dropped_items.o" "$out/breakables_data.o" "$out/breakables.o" "$out/coin_scene_data.o" "$out/coin_scene.o" "$out/coins_data.o" "$out/coins.o" "$out/circle_motion.o" build/full-port/shadow-core-host-2/libencore-shadow.a -o "$out/dropped_item_actor"
g++ "${flags[@]}" tests/dropped_item_lifecycle_tests.cpp "$out/dropped_items_data.o" "$out/item_holder_grant.o" "$out/dropped_items.o" "$out/breakables_data.o" "$out/breakables.o" "$out/coin_scene_data.o" "$out/coin_scene.o" "$out/coins_data.o" "$out/coins.o" "$out/circle_motion.o" build/full-port/shadow-core-host-2/libencore-shadow.a -o "$out/dropped_item_lifecycle"
g++ "${flags[@]}" tests/dropped_item_source_tests.cpp "$out/dropped_items_data.o" "$out/item_holder_grant.o" "$out/dropped_items.o" "$out/breakables_data.o" "$out/breakables.o" "$out/coin_scene_data.o" "$out/coin_scene.o" "$out/coins_data.o" "$out/coins.o" "$out/circle_motion.o" build/full-port/shadow-core-host-2/libencore-shadow.a -o "$out/dropped_item_source"
g++ "${flags[@]}" tests/dropped_breakable_source_tests.cpp "$out/dropped_items_data.o" "$out/item_holder_grant.o" "$out/dropped_items.o" "$out/breakables_data.o" "$out/breakables.o" "$out/coin_scene_data.o" "$out/coin_scene.o" "$out/coins_data.o" "$out/coins.o" "$out/circle_motion.o" build/full-port/shadow-core-host-2/libencore-shadow.a -o "$out/dropped_breakable_source"
g++ "${flags[@]}" tests/coin_scene_tests.cpp "$out/dropped_items_data.o" "$out/item_holder_grant.o" "$out/dropped_items.o" "$out/breakables_data.o" "$out/breakables.o" "$out/coin_scene_data.o" "$out/coin_scene.o" "$out/coins_data.o" "$out/coins.o" "$out/circle_motion.o" build/full-port/shadow-core-host-2/libencore-shadow.a -o "$out/coin_scene"
g++ "${flags[@]}" tests/coin_scene_source_tests.cpp "$out/dropped_items_data.o" "$out/item_holder_grant.o" "$out/dropped_items.o" "$out/breakables_data.o" "$out/breakables.o" "$out/coin_scene_data.o" "$out/coin_scene.o" "$out/coins_data.o" "$out/coins.o" "$out/circle_motion.o" build/full-port/shadow-core-host-2/libencore-shadow.a -o "$out/coin_scene_source"
"$out/dropped_breakable_source" "$out/composed"
for index in 0 1 2 3 4 5 6; do
 set +e
 "$out/dropped_breakable_source" "$out/negative-$index" >"$out/negative-$index.stdout.log" 2>"$out/negative-$index.stderr.log"
 status=$?
 set -e
 test "$status" -eq 1
 grep -q '^Dropped source failure ' "$out/negative-$index.stderr.log"
 if grep -Eq '(AddressSanitizer|runtime error:)' "$out/negative-$index.stderr.log"; then exit 1; fi
done
"$out/dropped_item_source" "$out/standalone"
"$out/dropped_item_lifecycle" "$out/standalone/source.encdrp"
"$out/dropped_item_actor" "$out/standalone/source.encdrp"
"$out/item_holder_grant" "$out/standalone/source.encdrp" "$out/standalone/key-control.encdrp"
"$out/dropped_items_data" "$out/standalone/source.encdrp" "$out/standalone/partial.encdrp" "$out/standalone/breakables.encbrk"
"$out/coin_scene" "$scene/scene.enccf" "$scene/coins.encoin" "$scene/breakables.encbrk"
"$out/coin_scene_source" "$scene/scene.enccf" "$scene/coins.encoin" "$scene/breakables.encbrk" "$scene/native.tsv"
