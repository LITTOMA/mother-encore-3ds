# Bounded Lamp victory presentation

Source: Mother: Encore Act2v0.4.1.0, commit `7d9246600fffe518408f5830d4848635019005a3`. This report covers the shared presentation and original return visuals; reward/world ownership remains in the outcome scheduler and world bridge.

The existing media sampler now plays Ninten's original victory animation (frames 81–95 with source offsets), the looping `YouWin` image, the original `transitionOut` curtains/info-plate tracks, and the original world-sprite jump. Authored frames, geometry, times, resource references and callback keys are in the generated presentation IR and compiled `opening.encround`, never C++ game-content tables. Assets are source-pinned lossless atlases; `world_party` uses the original 31×29 Ninten frames, `victory` the original 93×8 banner frames.

`BattleDialogueBox.play_win` uses its independent 3-second, float32 strict-negative timer. Completing the 1.6-second actor animation or 0.6-second looping banner does not release this gate. Outcome text is printed with `auto_advance=false`; an A or B edge while printing changes speed and a later A or B edge acknowledges the completed text. The actual source `_input` maps both buttons to `btn_next`; a direct `_action_press(false, true)` call is not a complete B input event. The source timer already running when `set_auto_advance(false)` is called is preserved.

Return poses use the same shared sampler and expose typed callbacks without mutating world actors/camera. The original single-party callback frames at 60 Hz are turn/hide 6, hide background 45, hide enemies 48, jump 99, land 134, rotate back 144 and animation finish 162, relative to return start. The AnimationPlayer consumes its creation-frame delta, accumulates float32 time, and uses half-open key intervals. Newly created jump tweens consume their creation pass. The native jump callback at playback 1.666665792 starts from plate global (128,174.777527), center x160.5. Native source first-jump samples are included in the host assertions.

The existing shared discrete-track sampler now preserves the previous value on an exact key boundary. Native `YouWin` time 0.10000000149011610 still shows frame1; the following frame shows frame2. Continuous interpolation is unchanged.

`begin_party_return` receives the raw current viewport transform of the world actor and display expansion. It applies the external source (0,-4) offset exactly once, preserves integer-sized assets, and returns an absolute viewport pose. Source curtain and sprite draw slots are reused so black curtains precede the returning sprite and party info plate. Default 400×240 expands the view; 320×180 remains the reference mode. No background rasterizer or 60-FPS steady-menu path was changed.

## Validation

- `host-tests.log`: 670 shared presentation checks passed, including the prior 470 checks and original 100-frame HP/text reference, manual acknowledgment, HP raw-digit stop, source gates, callback ordering, native jump samples and expanded viewport pose
- `sanitizer-tests.log`: the same 670 checks passed under ASan/UBSan with `ASAN_OPTIONS=detect_leaks=0`; this does not verify leaks
- `python-tests.log`: 8 asset/export tests passed, including atlas preservation, source fingerprints, exact victory media/event extraction and an unknown-track rejection
- Real devkitARM syntax checking passed for the integrated `platform/ctr/main.cpp` and renderer headers; the parent owns final linked cross-build and emulator evidence
- Precise source timing/pose reference: `../battle-victory-reference/reviewed/reference.json`

No emulator or hardware actions were performed by this work item. No audible result, GPU framebuffer equality, full-game compatibility, additional encounters, items or level-up presentation is claimed. Audio mapping remains under the existing explicit audio limitations.
