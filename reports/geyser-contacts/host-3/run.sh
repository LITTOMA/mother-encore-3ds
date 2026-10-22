#!/bin/bash
set -eu
cd /mnt/d/GameDev/3DS/mother-encore-3ds
export ASAN_OPTIONS=detect_leaks=1
export UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1
g++ -std=c++17 -O1 -g -Wall -Wextra -Wpedantic -Wno-misleading-indentation -fno-rtti -fno-exceptions -fno-pie -no-pie -fno-omit-frame-pointer -fsanitize=address,undefined -Iinclude tests/geyser_contacts_tests.cpp runtime/geyser_contacts.cpp runtime/geyser_lifecycle.cpp runtime/geyser_timeline.cpp runtime/poison_hazards.cpp runtime/poison_contacts.cpp runtime/poison_damage.cpp runtime/world_damage.cpp build/full-port/shadow-core-host-2/libencore-shadow.a -o build/full-port/geyser-contacts-host-3/check
build/full-port/geyser-contacts-host-3/check build/full-port/geyser-contact-fixture-2/body.encpcn build/full-port/geyser-contact-fixture-2/family.encglf build/full-port/geyser-contact-fixture-2/damage.encwdm build/full-port/geyser-contact-fixture-2/steps.encsteps build/full-port/geyser-contact-fixture-2/native.tsv
