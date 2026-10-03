# Original Lamp victory and world return

This bounded stage continues the actual opening encounter after its shared battle core reaches `VictoryPending`. It does not synthesize a win or bypass enemy HP. The original pinned `BattleSystem.gd`, `BattleParticipant.gd`, `BattleDialogueBox.tscn`, `AbstractDialogueBox.gd`, `uiManager.gd`, camera script and source animation tracks drive the implementation.

## Execution and mutable state

`BattleOutcome` owns the party's committed HP/PP, experience, level, bank balance, cash and earned-cash counter. Baselines, rewards, policies, progression references, dialogue and media are loaded from the checked external `opening.encround` package. The fresh Lamp contract has no item drops or level-up: Ninten starts at EXP0/level1, receives EXP3, and remains below the EXP9 threshold. Five currency units go to the bank and earned-cash counter; wallet cash stays zero. A level-up or item-bearing contract is rejected rather than silently skipped.

The source sequence disables dialogue auto-advance, waits for an unfinished prior dialogue, commits current rolling HP according to the original raw-digit rule, runs the original victory animation and unconditional three-second banner gate, then opens the original localized EXP message. Accept or cancel can accelerate printing and acknowledge it once it is finished. Waiting here does not award EXP, set the story flag, or deposit currency.

Acknowledgement executes EXP, the encounter win flag, bank/earned-cash credit and the earned-cash notification flag in original order. `battle_to_ov` begins the original return transition immediately; movement stays paused until its animation finishes. The source Lamp has overworld-battle-music enabled, so the normal victory-music branch does not run for this encounter.

Enemy defeat separately deletes its associated original world actor and removes its externally bound collision body. This occurs before rewards. The geometry audit confirms that the two reward flags do not modify another blocking body in this room; the surviving solver geometry is retained. The original delayed Room Shaker timer keeps advancing during battle but suppresses camera/audio/random requests there. After return it resumes source shakes and random interval updates on the shared RNG stream, rather than hitting the previous deferred placeholder. Its sound remains a typed request; this stage does not claim its PCM bank mapping or audibility.

The earlier fresh-world body-activation validator remains strict and is not repurposed as a general saved-world evaluator.

## Return presentation and world bridge

The original transition's top/bottom curtains, party plate, player jump, hide-background/enemy callbacks and final turn use external animation records. Source 320×180 coordinates remain the reference; the default 400×240 viewport expands the canvas at 1:1 pixel scale. The jump bridges the current UI plate to the existing player's world position, rather than reinitializing the room.

The player camera's `battle_to_ov` handler replaces the paused return tween with the source `return_offset` tween. The later `Camera2D.reset()` is intentionally a no-op: the original loop assigns local iteration variables rather than camera fields. Original position, direction, flags and actor deletion survive the transition. Re-entering the completed trigger cannot restart the encounter.

## Evidence and boundaries

Independent Godot 3.6.2 extracted-function checks covered cases with delayed acknowledgement, a previous unfinished dialogue, mid-digit HP and cancel acknowledgement, plus source geometry/camera audits. The scoped oracle proves reward order and values, native transition callback timing and unchanged RNG. It does not claim to reproduce the full original scene tree, framebuffer or audio mixer.

`battle_outcome` is the shared runtime integration test through real room trigger, real combat, delayed EXP acknowledgement, rewards, return, subsequent movement through the former enemy body and no retrigger. Presentation and binary validation have their own source-based tests. Final console verification is recorded separately with the executable hash.

Still outside this milestone: other encounters, item rewards, level-ups, defeat/revive, source save integration, and action sound mapping. The cloud emulator lacks a usable NDSP/DSP setup, so audibility is not verified. The new state is session state, not a claim that original save files are supported.
