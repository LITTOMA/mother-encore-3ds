#!/usr/bin/env bash
# Focused lane checks only. Full build/package gates belong to integration.
set -euo pipefail
cd "$(dirname "$0")/.."
PROJECT="${ENCORE_SOURCE_PROJECT:-$PWD}"
mkdir -p build reports
COMMON=(-std=c++17 -Wall -Wextra -Werror -O2 -Iinclude -I"$PROJECT/include")
g++ "${COMMON[@]}" runtime/music_regions.cpp "$PROJECT/runtime/audio_data.cpp" tests/music_regions_tests.cpp -o build/music_regions_tests >reports/controller-build.log 2>&1
build/music_regions_tests romfs/sound/banks/podunk.encmusic romfs/sound/banks/podunk.encaudio >reports/controller-tests.log
g++ "${COMMON[@]}" "-I$PROJECT/tests/fixtures/audio_ndsp" -Iplatform/ctr runtime/music_regions.cpp "$PROJECT/runtime/audio_data.cpp" platform/ctr/music_region_player.cpp tests/music_region_player_tests.cpp -o build/music_region_player_tests >reports/adapter-build.log 2>&1
build/music_region_player_tests romfs/sound/banks/podunk.encmusic romfs/sound/banks/podunk.encaudio "$PWD/romfs/" >reports/adapter-tests.log
g++ "${COMMON[@]}" runtime/music_regions.cpp "$PROJECT/runtime/audio_data.cpp" tests/music_regions_reference.cpp -o build/music_regions_reference >reports/reference-probe-build.log 2>&1
build/music_regions_reference romfs/sound/banks/podunk.encmusic reports/source-reference/native.json
python3 tools/compare_music_reference.py >reports/source-comparison.log
