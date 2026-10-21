#!/bin/bash
set -eu
cd /mnt/d/GameDev/3DS/mother-encore-3ds
export ASAN_OPTIONS=detect_leaks=1
export UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1
g++ -std=c++17 -O1 -g -Wall -Wextra -Wpedantic -Wno-misleading-indentation -fno-rtti -fno-exceptions -fno-pie -no-pie -fno-omit-frame-pointer -fsanitize=address,undefined -Iinclude tests/geyser_instance_presentation_tests.cpp runtime/geyser_timeline.cpp runtime/geyser_media.cpp runtime/geyser_mixer.cpp runtime/geyser_graphics.cpp runtime/poison_hazards.cpp runtime/geyser_lifecycle.cpp runtime/geyser_instance_presentation.cpp build/full-port/shadow-core-host-2/libencore-shadow.a -o build/full-port/geyser-presentation-core-host-4/check
build/full-port/geyser-presentation-core-host-4/check build/full-port/geyser-lifecycle-fixture-2/family.encglf build/full-port/geyser-media-fixture-1/media/data/world/catacombs/geyser.encgeysermedia build/full-port/geyser-media-fixture-1/media build/full-port/geyser-graphics-candidate-1/data/world/catacombs/geyser.encgeysergfx build/full-port/geyser-lifecycle-fixture-2/native.tsv
