#!/usr/bin/env bash
# Focused preparation + affected service + existing audio trace; no full suite.
set -euo pipefail
cd "$(dirname "$0")/.."
BASE="${ENCORE_SOURCE_PROJECT:-$PWD}"
FROZEN="${ENCORE_MUSIC_REGION_RESOURCES:-$PWD}"
COMMON=(-std=c++17 -O2 -Wall -Wextra -Werror -Iinclude -I"$BASE/include" -Iplatform/ctr -I"$BASE/tests/fixtures/audio_ndsp")
CORE=("$BASE/runtime/audio_data.cpp" "$BASE/runtime/room_data.cpp" "$BASE/runtime/content.cpp" "$BASE/runtime/file_io.cpp")
NEW=(runtime/music_regions.cpp platform/ctr/audio_player.cpp platform/ctr/music_region_player.cpp platform/ctr/music_region_service.cpp)
ARGS=("$BASE/romfs/data/opening.encaudio" "$BASE/romfs/" "$FROZEN/romfs/data/podunk.encmusic" "$FROZEN/romfs/data/podunk.encaudio" "$FROZEN/romfs/")
mkdir -p build reports
g++ "${COMMON[@]}" "${CORE[@]}" "${NEW[@]}" tests/music_region_prepare_tests.cpp -Wl,--wrap=fopen,--wrap=fread,--wrap=fclose,--wrap=ferror -o build/music_region_prepare_tests >reports/prepare-build.log 2>&1
build/music_region_prepare_tests "${ARGS[@]}" >reports/prepare-tests.log 2>&1
g++ "${COMMON[@]}" "${CORE[@]}" "${NEW[@]}" tests/music_region_service_tests.cpp -Wl,--wrap=fopen -o build/music_region_service_tests >reports/service-build.log 2>&1
build/music_region_service_tests "${ARGS[@]}" >reports/service-tests.log 2>&1
g++ "${COMMON[@]}" "${CORE[@]}" platform/ctr/audio_player.cpp tests/audio_player_regression.cpp -o build/audio_regression_hooked >reports/audio-regression-hooked-build.log 2>&1
build/audio_regression_hooked "${ARGS[@]:0:2}" "$BASE/romfs/data/opening.encroom" >reports/audio-regression-hooked.log
cmp "${ENCORE_AUDIO_REGRESSION_REFERENCE:-reports/startup-music-gpu-checkpoint/inputs/podunk-music-service/audio-regression-original.log}" reports/audio-regression-hooked.log
