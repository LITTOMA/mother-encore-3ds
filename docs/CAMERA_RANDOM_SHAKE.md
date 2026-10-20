# Source camera Shaker slice

The Catacombs wall's `_shake(duration)` requests magnitude5, random direction,
interval0.02, weight1 and no diminution. Its On animation calls it with0.5 and
1.0seconds. The previous native camera subset accepted a zero vector but never
drew a random direction. Formal House/Podunk RoomShaker commands use `(1,0)`;
the separate house-door ram policy uses source direction `(2,0)`. Mayor has no
shake command. The initial random-camera component bound incorrectly rejected
that existing door policy; the [wall timeline follow-up](DUNGEON_WALL_TIMELINE.md)
documents the failed full checks and source-based correction. RoomShaker's
admitted axis and all formal resource bytes remain unchanged.

`CutsceneCamera` now accepts explicit interval, weight and diminution parameters.
The explicit random physics overload uses the caller's shared `SourceRandom`.
There is no private seed or copied gameplay policy. Each stochastic step draws
the global GDScript `rand_range` twice, rounds the coordinates and retries while
the direction equals the previous one. A pair normally consumes six raw draws;
the global floating API's rare zero-exponent case consumes fewer. The final two
steps still draw even when they restore the original property. Only one shake
step executes per physics callback, including a large delta. Diminution runs
every frame, including frames with no interval crossing.

The source camera samples its child's offset one physics pass later. Concurrent
shakers retain their original property values and update in source insertion
order. Noncurrent cameras keep advancing. A missing RNG owner fails before
updating any camera, including the legacy target-taking overload; the legacy
shake API now rejects random requests. Invalid explicit parameters also fail
before admission. Stochastic processing commits camera and random state together;
ordinary axis-only movement does not copy the camera or consume RNG. Limits on
shaker count, duration, magnitude, direction range, step count and retry attempts
are execution bounds; unknown values reject rather than invent behavior.

The pinned official3.6.2 Shaker is copied unchanged except for adding a runner
reference and forwarding its two exact random call sites to the GLOBAL builtin.
A separate raw PCG mirror records consumption and verifies three subsequent
`randi` values. It never substitutes the RNG object's floating API. The manual
Node2D property adapter explicitly rounds input delta through binary32 as a
native engine physics callback does. Two runs match all845 observations in10
cases, including45 frames with multiple direction attempts. The first draft
passed binary64 manual deltas; that transport was corrected before freezing
or running the native comparison. Initial evidence is retained separately.

The current source math helper delegates to C `round`/`roundf`, consistent with
the native rounding operation. The exact primary source was read from
[Godot's pinned math implementation](https://github.com/godotengine/godot/blob/3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8/core/math/math_funcs.h#L263).
The file is retained only under build for review, not copied into a release.

GCC9.4 ASan/UBSan with leak checking passes10590 checks, replaying both complete
reference trajectories and subsequent RNG state. The unchanged Lamp and
Doll/Mimmie frozen camera samples pass7246 checks with current camera source.
Five Python checks cover source/adapter identity, repeated outputs, frozen
projection, random-call consumption and unsafe/nonfresh output rejection.
Actual Windows Debug CTest metadata contains285 tests; Python discovery has
1299 methods=916 aggregate+383 dedicated with exact identities. No Windows
native executable was built. Initial metadata attempts used a test-disabled
cache, then omitted the required Visual Studio configuration, and then exposed
an overbroad temporary collection wrapper; those logs remain. Only the final
helper supplies `-C Debug` to the actual CTest query, forwarding all other source
reads unchanged. The production scheduler and checks were not weakened.

This is a Shaker property/RNG slice. The probe does not run the full Camera2D
script, its recovery tween, idle completion signal, live animation method
dispatch, viewport, contact, sound or a playable Catacombs scene. Those owners
still need integration before admitting the map. No resource format, catalog,
save schema, formal content or hardware verification is changed or claimed.
Evidence is under reports/camera-random-shake/source-oracle-1.
