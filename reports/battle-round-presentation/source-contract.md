# Original Lamp round presentation contract

Pinned upstream: `7d9246600fffe518408f5830d4848635019005a3`.
This review implements a bounded action presentation host, not a general Godot interpreter.

## Scheduling and concurrency

- BattleSystem._start_action first yields its 0.05 SceneTreeTimer and the appended dialogue. Nonempty skill dialogue must finish before the skill animation starts. Text printing is AbstractDialogueBox._physics_process, independent from idle AnimationPlayer/Tween/Timer updates. Printing uses strictly greater than the character quantum, increments visible characters one beyond the phrase length before finishing, then starts the scene's 1.25-second Timer. A increases the print multiplier; B increases it further; either advances an already-finished phrase. These values are source data in binary64 RoundRule records.
- Party basic attacks wait for BattleSpriteParty.apply_damage. They do not wait for animation_finished. Ninten's source bash method track applies damage at 0.0714286 seconds; the entire source animation lasts 0.642857 seconds. The later _try_pause key has no effect when no battle pause was requested.
- EnemySprite.attack uses two sequential quartic tweens: offset Y rises to -10 over 0.1 seconds, then returns over 0.1 seconds. It emits apply_damage after the tween finishes.
- A hit starts FlyingNumber.run, the HitEffect animation, rolling HP, and the target's enemy-hit or party-response animation concurrently. The ordinary _apply_damage path does not wait for any of these visual effects or HP. _do_attack_damage waits one idle frame afterward. Each target then has its 0.08 timer. A nonempty action dialogue adds the source 0.4 timer at the end.
- Party hide_away internally waits for a nonloop current animation to finish before its 0.12 tween. BattleSystem does not wait for hide_away before emitting action.done. The next enemy action and next menu may overlap HP/effect/hiding activity. The next round has its separate source 0.4 timer; it must not be delayed until HP finishes.
- PartyInfoPlate HP uses 8 source atlas transition frames per integer and a 1/30-second base quantum. A decrement first changes the integer and wraps the subframe to 7; get_current_hp returns the ceiling while subframes remain. Reaching the target at subframe zero emits completion on the next quantum. The original hundreds/tens propagation condition is retained literally.
- A SMASH can kill this Lamp on round one. Its original .7-second enemy defeat clip plays and hides afterward. The main scheduler owns the explicit VictoryPending boundary; this helper never grants victory rewards, returns to the overworld, or changes a win flag.

## Shared random stream

- FlyingNumber.run consumes source rand_range(32,64) and then randi for the horizontal sign, synchronously in the Hit callback. These are the original GLOBAL Godot RNG calls.
- The file HitEffect.gd defines random flips and offsets, but Battle.tscn neither references that script nor connects animation_started to it. This scene therefore consumes zero random draws for HitEffect. The asset test checks this exact absence.
- A Miss uses RisingNumber (no RNG), then one randi for dodge sign. A sufficiently strong undefended party hit consumes the original rand_range(1,_hits) after FlyingNumber draws to select the hit pose. The scoped source has three external hit-animation bindings.

## Data and rendering

- All frames, paths, dimensions, colors, motion keys, labels and gameplay tuning come from opening.encround and source-pinned textures. The helper's C++ contains schemas, interpolation, state machines and formulas only. Ninten's 640x1152 atlas is repacked losslessly to 960x768 (15x12), preserving every linear frame index; there is no crop or scaling.
- RoundRenderer loads checked texture dimensions separately and leaves the established steady background renderer unchanged. It adds source sprite rotation and additive glow with the existing Citro2D blend mechanism, then restores normal alpha blending. The Lamp alpha domain is proven to be exactly {0,255}, which permits this bounded additive pass. General partially transparent shader parity is not claimed.
- The source target pointer, finished-dialogue cursor, source action frames, hit atlas, original bitmap damage numbers, SMASH sprite/background, and enemy hit/defeat properties are retained. Source plate quake and party bounce/shake remain data-driven.
- Expanded display anchors are explicit adaptation data: party/plate effects follow the bottom-centered party, enemy effects follow the centered Lamp, dialogue follows the top center, and the target name stays at the top right. There is no nonuniform scaling.
- Source EnemySprite defeat includes an audio track at time zero. The extractor verifies its exact stream/time/offset declaration, but this visual resource does not add it to the separately owned audio bank. Attack/hit/defeat audible verification remains outside this helper's claims. No GPU screenshot, emulator, hardware, or audio playback verification was performed by this worker.

## Native probe and checks

`run_native_probe.py` copies the original PartyInfoPlate HP methods and AbstractDialogueBox printing methods unchanged into isolated wrappers. It executes official Godot 3.6.2 and records 100 rows in native-hp-text.json. Menu refresh, unused character state and text sound are explicit inert boundaries. Native Timer auto-advance is not simulated by this probe; strict Timer completion remains separately mechanism-tested. The TSV file is a direct integer projection of those JSON rows and C++ checks every HP integer, digit atlas frame, scrolling flag, printed count and phrase-finished flag.

Initial wrapper failure (untyped adapter base_scroll_speed) is retained in native-hp-text-failed.log; no upstream file was changed. The corrected wrapper runs successfully. Focused C++ tests also cover exact source attack keys, method-event gating, native enemy quartic offsets, concurrent number/effect lifetime, shared RNG identity/order, source SMASH time scaling and defeat hiding. Python negative cases reject source changes, unknown recipe fields, invalid source IDs, path traversal and invalid atlas geometry.

## First emulator integration corrections

Parent evidence `reports/battle-round-visual/initial-menu.jpg` showed Ninten almost entirely behind the plate. The cause was a continuation handoff: BattleEntry enters Commands at the start of its source 0.12-second show tween, while main immediately snapshots its current position. Treating this intermediate position as the permanent shown base froze it near Y113.22 instead of the source Y96 endpoint. The action host now reads the existing external PartyShow track, stores its actual endpoint as the permanent base, and carries the observed interpolation progress forward. No coordinates were added to C++ and no packet changed. The focused tests now pass 470 checks including this regression.

Parent evidence `reports/battle-round-visual/target.jpg` showed broken Lamp glyphs. Its source label is 114 pixels wide, while the validated glyph advances total 23, yielding a half-pixel centered origin. The established entry renderer snapped text origins, but the new renderer did not. Upstream project.godot enables GPU pixel snap. New overlay text now rounds X and Y after alignment using the established floor(origin+0.5) convention. The actual ARM header compilation with warnings as errors succeeds. Corrected emulator appearance remains the parent's verification step; these observations do not assert a completed GPU comparison.
