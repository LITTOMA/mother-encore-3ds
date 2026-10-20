# Formal object ready/drawing resource evidence

Baseline: 3b32208148cdceedfbf2980179b1ce68596264c3, fixed upstream 7d9246600fffe518408f5830d4848635019005a3. Scope and compatibility are described in `docs/FIELD_OBJECT_DRAWING.md`. This evidence is not an emulator, GPU, hardware or complete-game acceptance.

`source-proof.json` records exact changed source hashes, genuine texture identity, all four legacy/formal Room decoded-content comparisons and the 12 changed / 444 unchanged preexisting tracked resource files. Only the Podunk Room build identity changes; its decoded contents remain exact. Production metadata/texture are 672/4117 bytes. Existing program, rules, Session binary and save format are unchanged.

Native checks used actual WSL Ubuntu GCC9.4, ASan/UBSan and leak checking. Drivers check every core source/header against the retained cache fingerprint before linking. The 12 final `scene-result-*` logs cover scene41, art53, typed links60, bound70, metadata187, lifecycle3022, delete95, complete candidate programme6784, legacy map3107, Area663, Props6787 and resource owner38. `object-field-data-failure-1.log` and `object-field-props-failure-1.log` preserve actual failures; corrected diagnostics and the missing-file rejection are separate. Early/final drivers are retained together, so an early driver is not itself a claim that a later version passed.

`object-integrated-python-1.log` covers 39 relevant source/codec checks; layout8 and dependency/source lists7 are separate. Source producer9 is included in that integrated39. `object-methods-final.json` records 1233 collected methods (868 aggregate,365 dedicated). Windows CMake configuration is not a Windows native build.

`object-native-content-1.log` is a real GNU make invocation of the complete production resource DAG: 47 starts,47 successful completions, catalog/encounters last. `object-lf-generation.json` and its per-step logs retain the final source-recipe line-ending preservation and original generator runs. `object-lf-stage.log` confirms376 native source files. The initial unclassified-extension failure is preserved separately. Generated resources/receipts were not edited by hand.

`local-tex3ds-transport*.py` are the before/after ignored local invocation adapters, not release executables or changes to the production generator. The exact Linux tool writes to a fresh owned temporary directory and its successful bytes are copied to the requested repository output. The old output was uid0/mode644 under WSL uid1000. The first map conversion returned1 with “Failed to open output file”; its raw stderr file was accidentally overwritten by a successful retry, so that original raw stderr is unavailable. `object-release-generation-driver-1.log` retains the failed step returncode. Do not interpret later successful map logs as that failure's raw output.

`owned-elf-retirement.json` preserves hashes, sizes and corresponding result logs before deleting twelve owned `scene-regression-*` ELF files, freeing630115848 bytes. Core cache, converters, the accepted f029 game and recorded save remain.

The `older-ci` directory contains terminal raw logs for different, earlier source revisions:

| Source | Run/job | Actual result |
|---|---|---|
| aafff212c26cda42f32acebe871989e8b6fb7e7c |39324374256 /118306290025 |GCC ordinary253/255; aggregate Python and field_equipment_source timeouts; no GCC sanitizer pass |
| a5b6600da1a8fa21a325a60d8f2553f57c350df8 |39328653406 /118320909209 |GCC ordinary254/255; Windows-separator source path failed on Linux; no GCC sanitizer pass |
|3b32208148cdceedfbf2980179b1ce68596264c3 |39332072532 /118332458907 |Clang242 ordinary +242 sanitizer passed |
|3b32208148cdceedfbf2980179b1ce68596264c3 |39332072532 /118332458724 |Genuine 3DSX/CIA and packaged resource checks passed |

These older runs do not cover this new drawing slice. New-slice cross-build and emulator verification remain pending at this commit. GCC for3b was still running at the last recorded snapshot; do not infer its final result from Clang.
