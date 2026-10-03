# Godot 3.6.2 global RNG and first Lamp round audit

This is a mechanism-level audit against official Godot
`3.6.2.stable.official.3cd3caab6`, with game source pinned to
`7d9246600fffe518408f5830d4848635019005a3`. It is not evidence of an
unmodified full-game run or of RNG continuity from the overworld.

## Verified mechanism

`include/encore/source_random.hpp` and `runtime/source_random.cpp` implement the
global GDScript calls, not a replacement for every Godot random API:

* `seed(uint64_t)` uses PCG32 XSH-RR state64/output32. The multiplier is
  6364136223846793005. Godot's sequence input is 1442695040888963407;
  PCG seeding transforms it to the stream increment 2885390081777926815.
  Starting with zero state: advance, add the supplied seed modulo 2^64,
  advance. A negative GDScript integer seed converts to uint64 modulo 2^64.
* `randi()` emits the permutation of the **old** state and advances once.
  GDScript receives a nonnegative integer, including outputs above INT32_MAX.
  Game `%` selections intentionally preserve the source's modulo bias.
* Global `randf()` is `Math::randf()`: convert the single uint32 output to
  binary32, divide by `(float)UINT32_MAX` (which rounds to 2^32), then promote
  that binary32 result to the binary64 GDScript Variant. Both 0 and 1 are
  possible. Dividing in double would produce different damage thresholds.
* Global `rand_range(from,to)` binds to the **double** overload of
  `Math::random`, using `RandomPCG::randd()`. Its first draw controls the
  exponent. A zero first draw returns fraction zero with no further draws.
  Otherwise two further draws, high word before low word, supply a 64-bit
  significand with its top and bottom bits set. The result is
  `ldexp(double(significand), -64-clz(first_draw))` followed by double multiply
  and add. Equal bounds still consume RNG; reversed bounds are not reordered.
  The implementation prevents FMA contraction of the multiply/add.
* `state()/set_state()` expose the internal state, for controlled continuation.
  **A saved state is not a seed.** `seed` and `set_state` reset the diagnostic
  raw-draw count; two seeding warmup steps are excluded from that count.
* The core never reads a clock and never reseeds itself. The caller injects the
  boundary seed/state. Godot `randomize()` instead reseeds with
  `(OS.unix_seconds + OS.ticks_usec) * current_pcg_state + 1442695040888963407`,
  modulo 2^64. Reproducing that requires the actual clock inputs and prior
  state. This module deliberately does not pretend a seed chosen on another
  machine reproduces them.

The official source excerpts are retained under
`../godot-rng/engine-3.6.2/`; their hashes and source URL are in `reference.json`.
Relevant boundaries are `gdscript_functions.cpp:428–446`,
`math_funcs.h:296–305`, `math_funcs.cpp:35–55,180–185`,
`random_pcg.h:65–124`, `random_pcg.cpp:38–52`, and `pcg.cpp:6–24`.
The PCG implementation is credited there to M. E. O'Neill under Apache-2.0;
the Godot wrappers carry their original MIT notices. The native implementation
contains algorithm constants only, with no Lamp/party/skill tuning.

## Global calls, RNG objects, and arithmetic are distinct

Godot's `RandomNumberGenerator` object uses its own PCG state.
Its `randf()` uses `RandomPCG::randf()`, normally **two** raw draws and binary32
arithmetic, unlike the one-draw global `randf()`. Its `randf_range` also follows
the float path. `object-api-difference.json` records actual engine output and
state for both object operations. For seed123:

* Global `randf()`: 0.9415164589881897, one draw
* Object `randf()`: 0.946095883846283, two draws
* Global `rand_range(0,1)`: 0.9460958925790321, three draws

No pinned upstream `.gd` script instantiates `RandomNumberGenerator`; the
global calls discussed here share one stream. The reference harness uses an
independent object solely to observe engine state and raw draws. Global
`randi` sentinels immediately after every recorded float call verify that the
mirror consumed the same stream positions.

GDScript's Variant numeric arithmetic and typed `float` locals remain binary64
even in this engine's ordinary binary32 `real_t` build. The engine probe records
`var typed_float: float = 16777217.0` without rounding to 16777216, and
`0.1+0.2 = 0.30000000000000004`; see `variant-numeric-precision.json`.
Only the global `randf` result already contains the binary32 rounding. Battle
damage arithmetic must not downcast the subsequent variance expression to C++
float. These checks do not claim all engine Vector2/tween arithmetic is double.

## Controlled first-round boundary

Inject the seed/state after the first command menu has settled and immediately
before `_end_player_action_choices` caches the enemy action. The selected party
attack already targets the actual Lamp. Use the same object for subsequent
battle logic and presentation; **do not reseed per action or per round**.

Original `BattleSystem._init` calls `randomize` before party construction.
Before the menu, `_physics_process` consumes two global ranges per shaking
enemy sprite each eligible physics frame (`_shake_time > SHAKE_FREQ`, then one
subtraction, not a catch-up loop). `_add_players_and_npc_transitions` can consume a global
sign draw for centered party objects. `BattleItemPool.roll_item` can draw and
construct items (whose UID code even calls global `randomize`), but Lamp's
empty drop pool exits without a draw. Other world/UI code also uses or reseeds
the global generator. Therefore a fixed menu seed is a reproducible local
comparison, not proof of identical launch-to-battle random continuation. The
unported entry draws cannot be silently discarded while claiming that proof.

## Exact scoped call order

Fresh Ninten's menu label “Bash” maps through `PartyMember.get_basic_skill` and
`globaldata.SKILL_ATTACK` to **attack.yaml**, not bash.yaml. The party's selected
target is maintained; that does not draw merely because there is one enemy.
The source scene, scripts, and source-derived content fingerprints are in
`source-audit.json`.

1. After player choice, `_cache_enemy_and_npc_actions` calls
   `_choose_random_action`. Lamp's original ordered skills are tackle then
   float with weights2 then1; a global `rand_range(0,3)` selects the first
   cumulative weight for which `i <= cumulative`. This consumes normally3 raw
   draws, even if the selected action will later be cancelled by defeat.
2. `_do_actions` sorts by priority then speed. There is **no random speed tie
   breaker**. Fresh Ninten speed6 precedes Lamp speed5. Guard priority3 precedes
   either ordinary action.
3. `_start_action` retargets if required, then obtains action dialogue. Ordinary
   attack/tackle/float dialogue is a string, so `SkillAction.get_dialog` does
   not make the array-choice `randi` call.
4. `_do_skill` first checks general fail, then gathers per-target miss decisions
   **before** attack-animation waiting. `_chance_roll` returns without a draw
   when chance<=0 or multiplier<=0. Fresh attack/tackle/float/guard have
   fail0/miss0 and no ailments, so all these checks consume0. There is no
   additional independent speed/dodge probability.
5. A successful damaging action enters `_do_attack_damage`: eligible SMASH
   chance first, then `_calculate_damage`. Ninten's attack has crit5 and guts8;
   the source minimum makes its chance5%, and `_chance_roll` consumes
   `randi()%100+1`, succeeding on `roll <= effective_percentage`. Lamp guts0
   with absent crit contributes no SMASH draw. A 100% chance, unlike 0%, would
   still draw.
6. Normal damage draws global `randf()` exactly once at the final variance
   expression, **even when variance is zero**. Defense, critical multipliers,
   affinities and defending occur in source order before this final floor.
   The distinct fixed/percentage/reach-full-percent branches would draw once
   in their branch and once again at the common tail; those are outside this
   first attack/tackle slice and must not be generalized to one draw.
7. `_create_rising_num(..., flying_num=true)` immediately runs
   `FlyingNumber.run`: `rand_range(32,64)` then `randi()%2`, normally4 raw draws.
   This is presentation and occurs before the next combat action. It also runs
   for displayed zero damage. SMASH creation and the bound hit-effect animation
   add no random draws in this scene.
8. If Lamp survived and selected tackle, its previously empty ENEMY targets
   cause `_retargeting` to call `randi()%opposite_side.size()` **even for the sole
   target Ninten**. This consumes1. Then miss0/SMASH0 skip, variance consumes1,
   and the flying damage number consumes4. Float targets SELF and does none of
   these draws. A defeated/incapacitated Lamp exits `_start_action` before
   retargeting; do not consume a cancelled enemy target draw.
9. Fresh participants have no status/passive-healing RNG at round completion.

Source-derived total raw counts: attack+tackle15, attack+float9, lethal attack9,
guard+tackle9, guard+float3. These totals assume every range's exponent draw is
nonzero. The precise rule is per-call consumption, not blindly skipping a
fixed count. Source-method engine fixtures, including actual FlyingNumber.run,
are recorded at `../battle-round-reference/final/reference.json`. The companion
`first-round-call-order.json` extracts its attack-round draw events, preserves
their provenance, and normalizes signed Godot state strings to uint64. That
fixture verifies the attack+tackle, attack+float, and lethal SMASH branches;
the guard counts above are source audit conclusions, not additional native
guard-round execution claims.

Actual extracted-method reference summaries:

| Menu seed | Result | Ninten HP | Lamp HP | Raw draws | Next randi sentinel |
|---|---|---:|---:|---:|---:|
|0|attack16, tackle1|61|14|15|78826807|
|1|attack17, tackle1|61|13|15|1007932533|
|123|attack17, float|62|13|9|451443022|
|2|attack15, float|62|15|9|77322774|
|59|SMASH attack67, Lamp cancelled|62|0|9|1179546988|

## Presentation audit, including calls that must not be added

* A missed attack creates ordinary `RisingNumber`, which does not draw, then
  `EnemySprite.dodge` / `BattleSpriteParty.dodge` consumes1 direction draw.
  This is conditional on a miss; source initial miss0 never reaches it.
* `BattleSpriteParty.bounce_up_hit` draws `rand_range(1,_hits)` when `_hits>0`.
  Ninten's scene has `_hits=3`. It runs only when not defending and damage is
  greater than maxHP/16. Fresh tackle against equipped Ninten does not reach
  that threshold; ordinary party shake and PartyInfoPlate.quake are deterministic.
* **HitEffect.gd is unbound in Battle.tscn.** Its2 flip draws and2 offset ranges
  are tempting static-search matches but are not executable here. The Control
  has no script and the scene has no `animation_started` connection; BattleSystem
  does not install one dynamically. Consuming those8 draws would be wrong.
* `AbstractDialogueBox` and `SpeechBubble` can consume one range per voiced
  printing step. `BattleDialogueBox._handle_phrase` explicitly clears its
  AudioStreamPlayer stream when the phrase lacks `sound`. Normal formatted
  first-round text has no sound field, so it contributes0. Spoken/scripted
  dialogue and its printing cadence require a separate future audit.
* EnemySprite's generic `Shaker` uses repeated random directions; the Lamp
  normal hit/nonboss defeat animations do not call it. The source boss defeat
  method track does. SMASH's sprite animation itself has no RNG call.
* Party/NPC scripts, confusion, other target modes, item drops, stat/status
  actions, player idle blinking and unrelated world activity remain outside
  this isolated round boundary. A new reachable source path must be audited
  before assuming its RNG contribution is zero.

## Reproduce and verification limits

Run from the project root:

```
python3 tools/run_battle_random_reference.py --godot /path/to/Godot_v3.6.2-stable_linux_headless.64
c++ -std=c++17 -O2 -Iinclude runtime/source_random.cpp tests/source_random_tests.cpp -o build/battle-round-random/source_random_tests
build/battle-round-random/source_random_tests reports/battle-round-random/reference.bin
```

The generator rejects Godot4 and nonmatching engine versions. `reference.bin`
preserves binary64 results and uint64 state; `reference.json` is decoded from
that binary, not Godot's precision-shortening JSON formatter. Every float call
is followed by a real-engine global/object sentinel check. Eight seeds cover
ordinary values, signed/unsigned boundaries, a seed chosen to produce PCG
state0, and a seed chosen to emit UINT32_MAX. Equal/reversed ranges and mixed
API calls are included. Forty cases contain1,888 recorded calls.

`host-tests.log` and `sanitizer-tests.log` record32,253 checks against those
unchanged engine bytes. ASan/UBSan ran with `ASAN_OPTIONS=detect_leaks=0` under
the existing ptrace limitation; this is not a leak-check pass.
`arm-compile.log` records compilation of this mechanism with the configured
devkitARM toolchain; that is not execution on ARM, a complete3DS build, or
hardware validation. Parent integration owns the combined build/test results.
`verification.json` retains compiler commands, versions, and source/object
hashes. `wrong-engine-rejection.log` preserves the negative Godot4 check.
`PCG-NOTICE.txt` and `PCG-APACHE-2.0.txt` are the PCG attribution and license for
inclusion with centralized release notices.
