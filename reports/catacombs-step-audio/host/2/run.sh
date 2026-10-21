#!/bin/bash
set -eu
cd /mnt/d/GameDev/3DS/mother-encore-3ds
export ASAN_OPTIONS=detect_leaks=1
export UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1
g++ -std=c++17 -O1 -g -Wall -Wextra -Wpedantic -Wno-misleading-indentation -fno-rtti -fno-exceptions -fno-pie -no-pie -fno-omit-frame-pointer -fsanitize=address,undefined -Iinclude -Iplatform/ctr -Itests/fixtures/audio_ndsp tests/field_run_bank_audio_tests.cpp platform/ctr/audio_player.cpp runtime/field_step_contacts.cpp build/full-port/shadow-core-host-2/libencore-shadow.a -o build/full-port/step-bank-host-2/run-bank
build/full-port/step-bank-host-2/run-bank romfs/sound/banks/opening.encaudio romfs build/full-port/catacombs-steps-fixture-1/catacombs.encsteps build/full-port/catacombs-step-audio-candidate-1/sound/banks/catacombs-steps.encaudio build/full-port/catacombs-step-audio-candidate-1 build/full-port/step-bank-host-2/scratch build/full-port/step-bank-host-2/native.tsv build/full-port/catacombs-steps-fixture-1/body.tsv
