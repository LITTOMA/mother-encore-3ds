#!/bin/bash
set -eu
cd /mnt/d/GameDev/3DS/mother-encore-3ds
export ASAN_OPTIONS=detect_leaks=1
export UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1
g++ -std=c++17 -O1 -g -Wall -Wextra -Wpedantic -Wno-misleading-indentation -fno-rtti -fno-exceptions -fno-pie -no-pie -fno-omit-frame-pointer -fsanitize=address,undefined -Iinclude tests/poison_contacts_tests.cpp runtime/poison_contacts.cpp runtime/poison_damage.cpp runtime/poison_hazards.cpp runtime/world_damage.cpp build/full-port/shadow-core-host-2/libencore-shadow.a -o build/full-port/poison-contacts-host-1/check
build/full-port/poison-contacts-host-1/check build/full-port/poison-contact-fixture-1/contacts.encpcn build/full-port/poison-contact-fixture-1/damage.encwdm build/full-port/poison-contact-fixture-1/steps.encsteps build/full-port/poison-contact-fixture-1/native.tsv
