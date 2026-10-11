# Audio and layout verification

Baseline: ed09c5d82e5d83ae51f604307c54706b33355ccb (merged PR #39).

- Resource generation: original extractors followed by make/native-content.mk -j4, successful (see audio-layout-regenerate.log). Required source/format admission is not a test-suite run.
- Windows environment: PYTHONUTF8=1; a local Path.write_text adapter preserves canonical LF for generated JSON. First GBK and CRLF attempts were corrected by rerunning generators. No binary or manifest patched manually.
- Texture receipt migration: strict source projection permits only exact quoted resource path substitutions in item_details.py / field_equipment.py; original IR, texture SHA and size checked before generated producer-record update.
- Audio conversion: installed FFmpeg 7.0 generated all original 27 PCM hashes unchanged; 26 new assets admitted (53 opening + 5 Podunk). Initial FFmpeg 6.0 output for resampled bump.wav was rejected during comparison.
- Host: CMake configure succeeded. MSVC compiled the new shared units, but complete encore_core build failed on the existing POSIX unistd.h dependency in locale_preference.cpp. This is not a passing host build.
- WSL reported CO_E_SERVER_EXEC_FAILURE; no local Linux/3DS build claimed. Cross-build results are recorded separately after Actions.
- Tests/sanitizers: not run (repository manual-only policy); negative coverage added. Emulator/hardware playback remains unverified.

Source-semantic review: the opening extractor changes physical provenance lookup only; dialogue_choice_assets.py changes compiled/staged resource paths only. Original source sections and content strings were compared before accepting the two local-tool SHA updates. House/round/session/restore dependency metadata was regenerated through original extractors; UI recipes retain gameplay fields, guarded by migrate_layout_dependencies.py. Runtime directory projection is included in the room fingerprint. Upstream pin, rule identity and save schema remain unchanged.

First console run: https://github.com/LITTOMA/mother-encore-3ds/actions/runs/38119812103 (c8604d6) failed. ARM compilation rejected passing SceneDoorEvent text (string_view) to a string reference. Staging rejected the title locale recipe dependency on migrated Continue IR. Fixed with an explicit owning string and the same metadata-only dependency migration. The generator also now emits encounter resource paths through Path.as_posix(); Windows-native backslashes would otherwise cause Linux regeneration drift. Full original job log retained locally in build/audio-layout-ci-job.log; the exact failures and local regeneration verification are retained here.
