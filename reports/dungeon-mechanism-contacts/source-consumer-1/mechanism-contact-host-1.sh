set -eu
cd /mnt/d/GameDev/3DS/mother-encore-3ds
g++ --version | head -1
g++ -std=c++17 -O1 -g -Wall -Wextra -Wpedantic -fno-exceptions -fno-rtti -fno-omit-frame-pointer -fno-pie -no-pie -fsanitize=address,undefined -Iinclude -Itests tests/dungeon_mechanism_contacts_tests.cpp runtime/dungeon_mechanism_bindings.cpp runtime/dungeon_mechanism_state.cpp runtime/dungeon_mechanism_presentation.cpp runtime/dungeon_mechanism_audio.cpp runtime/dungeon_mechanism_contacts.cpp build/full-port/wall-host-current-1/libencore-current.a -o build/full-port/mechanism-media-candidate-2/contact-check > build/full-port/mechanism-contact-compile-1.log 2>&1
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 build/full-port/mechanism-media-candidate-2/contact-check build/full-port/mechanism-media-candidate-2/source.encdmp build/full-port/mechanism-contact-fixture/contacts.tsv
