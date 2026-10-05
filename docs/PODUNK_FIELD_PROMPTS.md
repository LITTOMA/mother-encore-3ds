# Podunk ButtonPrompt

`tools/field_prompts.py` imports all 146 actual ButtonPrompt instances from the pinned complete Podunk scene. The independent `ENCFPR01` resource carries source identities, stable parent / node IDs, Ready ordinals, settings, action labels, initial Canvas values, five complete animations and the checked GPU art binding. The game does not parse JSON.

The runtime binds real event-detector, player pause, locale and input signals. It preserves source inverse parent scale, force-show / force-hide precedence, press blocking, discrete visibility / hide methods, easing and the source hidden animation clock. Missing callbacks or unsupported input glyphs reject; Ready keeps `set_process(false)`.

The CTR renderer splits the existing lossless source A + arrow tex3ds atlas into separately animated pieces. HBox layout comes from an official Godot 3.6.2 source-font layout receipt. Label keeps its own material, Arrow inherits the source Flash material; the source glow RGB is zero. GPU draw performs no disk read, bitmap conversion or texture upload. The caller supplies actual parent world transform, ancestor visibility and source pixel-snap policy.

Source extraction, binary compilation and ARM compile-only checks completed. Manual positive / negative cases are in `manualtests/field_prompts_tests.cpp`; they have not been executed. No simulator or hardware visual / timing comparison is claimed. Complete Podunk lifecycle admission remains pending.
