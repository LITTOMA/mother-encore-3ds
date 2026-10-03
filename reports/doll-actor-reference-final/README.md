# Doll Actor action extension evidence

Pinned upstream commit: `7d9246600fffe518408f5830d4848635019005a3`.
The upstream checkout remains read-only. Official Godot 3.6.2 runs isolated,
unchanged Actor action functions and the original CharacterSprite/YAML parser.
This is a bounded source adapter, not a general GDScript interpreter.

## Implemented domain

- `actor_move_path(actor, path_index)` reads checked external Room3 movement path
  records. The lower-level copied-path API is also used by native reference tests
- Position and STEP vectors, sequential waits, optional source Walk animation,
  moonwalk, independent teleport, NPC Talk/Idle, Doll Float, and exclamation
- Four concurrent path coroutines, each with at most eight vector/wait entries.
  The actual authored Doll paths contain at most four entries
- Step targets are relative to the position when each entry starts. STEP uses
  the source angle-to-point then `-cos/-sin`, preserving float trig arithmetic
- Each vector emits `finished_movement`; the whole path emits one
  `finished_action` after its final wait. Shared signal callbacks resume in
  registration order. A jump or shake completion can release a pending path
- Wait sets IDLE and clears talking. A second move/turn can begin during a wait;
  the old path can subsequently set IDLE while that new operation is in progress
- Teleport preserves the current movement target and all existing coroutines
- Moonwalk changes visual blend direction, while stored direction follows
  motion. Finishing the queue flips stored direction and clears moonwalk
- Nonparty actors ignore rounded diagonal blends, as source Actor.blend_position
  explicitly requires. Mimmie consequently needs only four directional clips
- Source Actor Walk uses PartyMember tracks, independent of ordinary Player
  tracks. Directional animation bindings and all frames/times come from Room3
- NPC Talk->Idle waits for the end transition. Delayed first animation/emote
  keys preserve the existing frame; no invented frame-zero key is inserted

## Native timing correction

SceneTreeTimer's countdown is rounded to float32 after each subtraction. Earlier
Actor documentation described double retention; the one-second Doll waits expose
the difference. `timer.json` records exact native decimal time_left values:

- After the first tick: 0.98333334922790530
- After sixty ticks: +0.00000027939677240
- After sixty-one ticks: -0.01666638813912870; timeout emits

Both the old Lamp tests and the new Doll tests pass with this corrected timer
arithmetic. A timer created by a SceneTreeTimer callback waits until the next
iteration. Existing timers from physics or a Timer-node callback process in the
current iteration. Timer creation order is shared by waits, turns, and shakes.

## Verification

Thirteen official-engine cases cover authored Doll/Mimmie/Ninten actions and
boundary interactions: overlapping old wait/new movement, wait/turn overlap,
zero waits, teleport during motion, diagonal STEP arithmetic, moonwalk,
Walk/Talk/Float frames, and exclamation timing.

- `host-tests.txt`: 13,730 checks, zero failures
- `asan-ubsan.txt`: same checks under ASan/UBSan with `detect_leaks=0`
- `lamp-regression.txt`: unchanged 20-case Lamp oracle, 10,196 checks, zero failures
- `python-tests.txt`: 23 provenance/domain/regeneration tests passed
- `receipt.json`: actual engine, native result, fixture, and tool hashes
- `validation-receipt.json`: runtime/tests/pack hashes at focused verification

No emulator, cross-build, whole-game, or hardware verification is claimed by
this module. LeakSanitizer was disabled due to the established environment
limitation; this is not a leak-check result.

Reproduction uses fresh build/report directories:

```sh
python tools/reference_doll_actor_actions.py \
  --godot /path/to/official/godot3.6.2 \
  --work build/doll-actor-reference-new \
  --reports reports/doll-actor-reference-new
```

The earlier `doll-actor-reference-1` used the wrong emote subresource (dot instead
of exclamation) and is retained only as diagnostic history. Reference 2 corrected
that selector. Final has the same action fixture as reference 2 and additionally
records the independent exact SceneTreeTimer countdown probe.

## Separate original cleanup oracle

`../doll-cleanup-reference-1/` runs unchanged Actor.update_npcs with the original
CharacterSprite4dir animation tree and minimal global/UI services. A hidden NPC
with a different initial Idle frame receives its pose immediately, changes frame
in the next native animation phase, then becomes visible and frees its proxy on
the following idle_frame. A replacement already showing the same Idle Up frame
never emits frame_changed during the ten-frame observation and leaves the
cleanup coroutine pending. The implementation must wait for the source signal,
not manufacture a timeout. This evidence does not itself implement world cleanup.
