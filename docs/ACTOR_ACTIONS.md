# `lamp_attack` actor actions: bounded source adapter

Pinned source: `7d9246600fffe518408f5830d4848635019005a3` (Act 2 v0.4.1.0).
This implements only the actions exercised by the opening lamp sequence. It is
not an arbitrary GDScript interpreter, the complete NPC controller, or a battle.

## Actual instances and presentation

`Maps/podunk/Nintens House.tscn/Objects/lamp` instances the generic
`Nodes/Reusables/npc.tscn`, at **(496,390)**, with `Data/Animations/Lamp.yaml`,
empty transition overrides and no shadow. The standalone
`Nodes/Overworld/Actors/Lamp.tscn` is not the source for this sequence.

The dialogue creates replacement `Actor` nodes. `actor.tscn` sets both collision
layer and mask to zero. Free movement is therefore original behavior, not a
workaround for the room's collision solver. Hiding the replaced generic NPC
invokes `npc.update_visibility_changed`, disabling its collision and physics.
The latter lifecycle is source-audited; the isolated action reference does not
instantiate the complete room or NPC lifecycle.

| Property | Ninten replacement | Lamp replacement |
|---|---|---|
| Sprite frame dimensions | 31×29 | 17×22 |
| Sprite local position | (0,9) | (0,9) |
| Sprite drawing offset | (0,-13) | (0,-11) |
| Emote local position, relative to CharacterSprite | (0,-37) | (0,-30) |
| Initial shadow | visible | hidden |

Ninten's replacement uses `PartyMember.yaml`, not Player.tscn's animation
tracks. Its right-facing Idle frame is 21 (zero-based). The source actor collapses
a replacement's initial diagonal to its dominant cardinal axis, retaining Y on
a tie. Later explicit turns do not perform that collapse.

The sprite's world drawing center is root position + sprite local position +
sprite drawing offset. An emote inherits sprite local position but **not** the
sprite's drawing offset. Consequently shake does not shake the emote, whereas a
jump carries the emote. The surprise sheet has 12×12 cells, 31×32 pixels each.

## Actions and scheduling

- Position movement uses normalized direction × speed, with native float vector
  arithmetic. If both remaining axis distances are within
  `max(ceil(abs(speed*delta)),1)`, it snaps, emits `finished_movement`, completes
  waiting movement coroutines and emits `finished_action`. The stored velocity
  is retained when snapping/idle. Idle root positions are rounded on the next
  physics callback, not during the movement's final callback
- Starting another position command during motion waits on `finished_action`.
  The source shares this signal across movements, jumps and shakes. A jump can
  therefore release a queued movement before the old movement ends; the narrow
  adapter preserves this observed behavior rather than serializing all actions
- Dialogue `actorsturn` calls `turn_to`, not direct `set_direction`. The runtime
  `actor_turn_to` normalizes and rounds current/target directions, applies the
  first rotation immediately, then waits a new 0.08-second SceneTreeTimer after
  every intermediate rotation **including the final target direction**. The
  source literal rotation is 45 **radians**, followed by rounding; it must not be
  replaced with a degree conversion. Left→right is down-left, down, down-right,
  right; four timer waits complete on the twentieth 60 Hz idle tick. Animation
  frames update in the following animation phase when a timer changes direction.
  During ROTATING the root retains fractional coordinates; IDLE rounds them on
  its next physics callback. Movements started while turning wait for completion.
  Overlapping turns and turns during motion/jump/shake fail closed. Direct
  `actor_turn` remains only the source `set_direction` operation
- Jump changes CharacterSprite local position. Upward motion is quartic ease-out
  for 60% of the duration; downward motion quadratic ease-in for 40%. This YAML
  leaves `crouch=false` and `shadow=true`, so it does not switch to Jump/Crouch
  animations or force the lamp's hidden shadow visible. Overlapping jumps keep
  running in creation order, with the newer property writer applied last
- Shake changes drawing offset, immediately to `old + magnitude`, then alternates
  via new 0.05-second SceneTreeTimers. The number of complete oscillations is
  `int(length*10)`; each timer discards overshoot. Finite half/full-second shakes
  are supported. Looping or overlapping shakes fail closed
- Lamp Open keeps frame 0 until 1.5833 seconds, then advances to frames 1, 2, 3 at
  1.5833, 1.6663, 1.7493. Its non-looping duration is 1.8323 seconds. Idle emits
  `finished_action` immediately; Open does not emit it on animation completion
- Surprise is the original eight-key discrete AnimationPlayer track, lasting
  1.3 seconds. Frame 9 remains after completion. The renderer should use the
  actual atlas transparency, not invent a timed visibility cutoff

Required frame order:

1. `actor_physics_step`: actor physics followed by physics SceneTreeTween updates
2. `actor_idle_animations`: CharacterSprite AnimationTree and emote AnimationPlayer
3. DialogueBox WaitTimer callbacks, which may start actions
4. `actor_scene_timers`: shake's SceneTreeTimers

A shake born in a Timer node's timeout receives the current frame's timer delta.
A successor timer created during a SceneTreeTimer callback begins on the next
iteration. `actor_idle_step` is only a convenience when no command dispatch lies
between the two idle phases.

Godot's engine callback delta at 60 Hz is exactly
`0.01666666753590110`, namely `double(1.0f/60.0f)`. This matters: speed 600 yields a
snap threshold of **11**, not 10. Likewise a 0.05-second timer crosses below zero
on the third tick. The native fixture explicitly exercises both boundaries.

## Evidence and reproduction

`compatibility/reviews/actor-actions-v0410.json` binds reviewed source files,
textures and exact action functions including `turn_to`. `tools/reference_actor_actions.py` creates
an isolated project under `build/`, extracts unchanged action functions, copies
unchanged CharacterSprite code and the original YAML parser, and uses the source
Lamp/PartyMember YAML plus the source surprise Animation resource. It never
modifies upstream. Texture rendering and full global services are deliberately
excluded and listed in the review.

```sh
python3 tools/reference_actor_actions.py \
  --godot /path/to/official/godot3.6.2 \
  --work build/actor-actions-reference-new \
  --reports reports/actor-actions-reference-new
```

Use fresh directories. Successful references reject engine warnings/errors and
retain engine logs, JSON, a generated C++ fixture, tool hashes and engine hash.
They require pristine pinned upstream; ordinary C++/Python tests require neither
upstream nor Godot. Reviewed expectations must not be edited to hide mismatches.

Final native evidence: `reports/actor-actions-reference-9/`.
Twenty cases cover free wall crossing, axis snapping, float-ceil boundary,
queued motions, movement/jump overlap, simultaneous jumps, both shake lengths,
Timer-timeout-born shakes and turns, timed turn direction/frame sequences,
movement waiting on turn completion, Lamp frames, Ninten frames and surprise, plus shared
completion-signal wakeup. `tests/actor_actions_tests.cpp` performs 10,196 checks.
Focused ASan+UBSan checks also passed with `ASAN_OPTIONS=detect_leaks=0`;
this is not a leak-check result. Python tests validate reproducibility and reject changed source/function/engine,
unknown commands/properties, missing cases/frames and invalid values.

Earlier probe output remains for diagnosis. Probe 1 exposed an invalid wrapper
that reinitialized CharacterSprite's accumulating tag list; probe 2 missed the
engine's initial idle-before-physics warmup. Neither supplies final expectations.
The final wrapper initializes each real animation profile once and captures only
physics-driven frames after native timers have run. The turn extension also
replaced manual animation advances with native automatic idle processing: manual
advance inside `_process` occurred after Timer internal callbacks and could not
validate the Timer-born turn's first visual frame. Native tree order correctly
preserves the old frame until the next animation phase.

Hardware, emulator, renderer and integrated whole-cutscene verification are
separate from this module's native/headless action tests. Audio and battle
remain explicit external boundaries.


## Doll continuation timer correction

The native Godot3.6.2 SceneTreeTimer probe now establishes float32 storage on every countdown subtraction. A1-second wait expires after61 fixed60Hz ticks (after60 the remainder is approximately+0.0000002794). Timer-node WaitTimer still uses its separately verified double countdown. The actor adapter uses source signal/coroutine ordering for path waits, including an old timer waking while newer motion exists. See reports/doll-actor-reference-final and its retained native timer evidence; the earlier callback-double discussion describes the promoted delta, not SceneTreeTimer storage.

Doll adds STEP/position paths, waits, moonwalk, teleport without cancelling an existing path, directional NPC Walk/Talk and delayed-first-key emotes.13 new source cases pass13,730 checks; the20priorLamp cases retain10,196 checks. Replacement restoration copies position/direction immediately, waits for an actual sprite frame_changed, then the next idle_frame. If the original hidden replacement is already on the requested Idle frame, no signal may occur and the actor stays pending; no artificial timeout is substituted.
