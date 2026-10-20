set -eu
cd /mnt/d/GameDev/3DS/mother-encore-3ds
g++ --version | head -1
g++ -std=c++17 -O1 -g -Wall -Wextra -Wpedantic -fno-exceptions -fno-rtti -fno-omit-frame-pointer -fno-pie -no-pie -fsanitize=address,undefined -Iinclude -Itests tests/dungeon_rooms_tests.cpp runtime/dungeon_rooms.cpp -o build/full-port/dungeon-rooms-candidate-1/check build/full-port/wall-host-current-1/libencore-current.a > build/full-port/dungeon-rooms-compile-1.log 2>&1
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 build/full-port/dungeon-rooms-candidate-1/check build/full-port/dungeon-rooms-fixture-1/source.encdrooms build/full-port/dungeon-rooms-fixture-1/reference.tsv build/full-port/dungeon-rooms-fixture-1/geometry.tsv
