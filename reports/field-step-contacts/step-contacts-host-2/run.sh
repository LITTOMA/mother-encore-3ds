#!/bin/bash
set -eu
cd /mnt/d/GameDev/3DS/mother-encore-3ds
export ASAN_OPTIONS=detect_leaks=1
export UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1
g++ -std=c++17 -O1 -g -Wall -Wextra -Wpedantic -Wno-misleading-indentation -fno-rtti -fno-exceptions -fno-pie -no-pie -fno-omit-frame-pointer -fsanitize=address,undefined -Iinclude tests/field_step_contacts_tests.cpp runtime/field_step_contacts.cpp build/full-port/shadow-core-host-2/libencore-shadow.a -o build/full-port/step-contacts-host-2/contacts
build/full-port/step-contacts-host-2/contacts build/full-port/catacombs-steps-fixture-1
