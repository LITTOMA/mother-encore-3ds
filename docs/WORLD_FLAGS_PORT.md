# Pinned world flags and fresh-opening enablement

Source baseline: Act 2 v0.4.1.0, commit `7d9246600fffe518408f5830d4848635019005a3`.
This narrow shared-core module does not approve any whole script or implement a
playable event/door/cutscene system. Source fingerprints and reviewed functions
are in `compatibility/reviews/world-flags-v0410.json`.

## Implemented

- All 178 story flag names and their initial false values; object flags start empty
- Missing dictionary reads return false. Missing story writes are no-ops without a signal; object writes insert. Known writes request `flags_updated` even for unchanged values when emission is enabled
- Appear/disappear predicate, and `FlaggableObject` story/object selection and fallback identity (`scene name/leaf node name`, not a full node path)
- `FlagLandmark` visibility versus deferred permanent deletion. `queue_free()` does not set visibility false in its branch and cannot be cancelled by restoring the flag
- Outputs of `npc.update_visibility_changed`: effective tree visibility gates body collision, interaction and physics processing; no-dialog also disables interaction
- Outputs of `Openable Door._update_door_state`, `lock`, `unlock`: collider writes are deferred; a flag override changes `_unlocked` and prompt only, not the pending collider write
- A finite 21-body allowlist for freshly loaded opening-house shape enablement, after ready/deferred door writes. It rejects unknown body paths, any object flag entries and any true story flag except `visited_podunk`. This is deliberately not a post-event or loaded-save evaluator

`FlagWriteResult.flags_updated` is an emission request; the caller owns dispatch.
The module does not emulate Godot signal ordering, editor behavior or node trees.
The caller must flush landmark deletion at the appropriate scene lifecycle boundary.
NPC visibility input must account for ancestors and actual notifier events; the
module does not equate offscreen to hidden. Initial house NPCs have no appearance
flags, all body shapes enabled, and no initial movement flags set.

## Fresh opening facts

Initialize `WorldFlags`, then set `visited_podunk=true` for
`AreaRoom._ready -> _update_visit_flags`. The house region is Podunk.
`Mt Itoi Landscape` has a door target `(520,404)`, but `Door.change_scene` subtracts
7 from target Y, so the actual actor spawn is **(520,397)**, facing down after the
transition. `DebugStartPos` `(456,409)` applies only to directly running the room
in a debug build; it is not the released new-game path.

Enabled body shapes are `Collisions`, six NPC bodies, the phone static body,
four present static bodies, `DoorBlock/Entrance`, all four non-player door
bodies, and player door bodies 2/3. Player door bodies 1/4 are disabled.
Visibility alone does not disable ordinary static bodies. In particular, the
invisible present static bodies and `DoorBlock/Entrance` remain active.
The non-player door bodies have layer 1596 and mask 0; initial player mask 4353
has no overlapping bits, so those four shapes are filtered separately by physics.

The bedroom camera area center `(504,376)` and half-size `(208,104)` imply
left/top/right/bottom limits `(296,272,712,480)` for the original 320x180 viewport.
`camarea.gd` grows each dimension to at least the viewport size and applies the
limits after an idle frame. No camera implementation is included here, and
texture boundaries are not physical world or camera boundaries.

The initial lamp trigger is `Poltergeist/Cutscene Area`, dialogue
`Podunk/cutscenes/lamp_attack`; it is eligible while both `doll_melody` and
`poltergeist` are false. Its rectangle is `(409,369)-(425,417)` after transforms.
Eligibility is separate from overlap monitoring. `CutsceneArea` enables processing
on player entry, waits out cutscene/battle/pause-menu state, then checks flags,
closes the command menu, pauses the player and opens dialogue. Walking through
this region without the event is not equivalent opening gameplay.

## Evidence and reproduction

`reports/m4-world-flags-reference-final/` contains a clean run of isolated,
unchanged reviewed function bodies in official Godot 3.6.2, source and engine
fingerprints, JSON results, and the generated C++ fixture. The harness substitutes
only the signal service, boolean fields and required native node tree; it does
not run the full game. Coverage: registry 178, appearance 64, writes 24, door
flag updates 96, NPC visibility 8, landmark lifecycle 2, object identity 12.

```
python3 tools/reference_world_flags.py --godot /path/to/Godot_v3.6.2-stable_linux_headless.64 --work build/new-world-flags --reports reports/new-world-flags
```

The command requires fresh output paths. The complete source hashes and native
registry must match. It never changes upstream or regenerates accepted fixture
expectations in place. The first incomplete registry extraction missed a line
with a trailing comment (`wally_fought`); the native registry comparison caught
it, and the parser and C++ registry were corrected to 178. Earlier diagnostics
remain separate from the reviewed evidence.

The generated fixture is consumed by `tests/tests_world_flags.cpp`. Standalone
GCC and ASan/UBSan runs passed 2208 checks (these historical checks did not include LeakSanitizer). These checks are not full-game, emulator or
hardware verification. Build-system integration and aggregate testing belong to
the enclosing world-runtime change.

An independent native check covered the exact trigger rectangle
and player polygon. On initial entry at player Y397, X432 is outside and
X431.99 overlaps. On retreat after entry, native overlap persists at X432 and
X432.01 and ends by X433. This hysteresis and callback timing are not implemented
by the flags module; the probe is not a full movement or event execution test.
