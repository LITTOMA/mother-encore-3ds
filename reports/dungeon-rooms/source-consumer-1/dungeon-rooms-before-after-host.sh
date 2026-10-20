set -eu
cd /mnt/d/GameDev/3DS/mother-encore-3ds
g++ -std=c++17 -O1 -g -Wall -Wextra -Wpedantic -fno-exceptions -fno-rtti -fno-omit-frame-pointer -fno-pie -no-pie -fsanitize=address,undefined -Iinclude -Itests tests/field_camera_projection_tests.cpp runtime/dungeon_rooms.cpp build/full-port/wall-host-current-1/libencore-current.a -o build/full-port/dungeon-rooms-candidate-1/old-field-projection-check > build/full-port/dungeon-rooms-old-projection-compile.log 2>&1
set +e
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 build/full-port/dungeon-rooms-candidate-1/old-field-projection-check tests/fixtures build/full-port/dungeon-rooms-fixture-1/source.encdrooms build/full-port/dungeon-rooms-fixture-1/reference.tsv > build/full-port/dungeon-rooms-old-projection.log 2>&1
result=$?
set -e
test "$result" -eq 1
cat build/full-port/dungeon-rooms-old-projection.log
g++ -std=c++17 -O1 -g -Wall -Wextra -Wpedantic -fno-exceptions -fno-rtti -fno-omit-frame-pointer -fno-pie -no-pie -fsanitize=address,undefined -Iinclude -Itests tests/field_camera_projection_tests.cpp runtime/dungeon_rooms.cpp build/full-port/dungeon-rooms-candidate-1/field-scene.o build/full-port/wall-host-current-1/libencore-current.a -o build/full-port/dungeon-rooms-candidate-1/field-projection-check > build/full-port/dungeon-rooms-projection-compile-2.log 2>&1
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 build/full-port/dungeon-rooms-candidate-1/field-projection-check tests/fixtures build/full-port/dungeon-rooms-fixture-1/source.encdrooms build/full-port/dungeon-rooms-fixture-1/reference.tsv
