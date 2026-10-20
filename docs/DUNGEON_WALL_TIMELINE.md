# Catacombs wall timeline candidate

The fixed source Wall Door has four animations: On, RESET, StayOff and StayOn.
On changes the sprite frame and collision shape, assigns the sound player's
`playing` property and defers two `_shake(duration)` calls. The existing property
format deliberately rejects audio and method tracks. A separate typed resource
now represents these operations without broadening that format or interpreting
GDScript.

`content/catacombs-wall-timeline.json` records the reviewed source, graph owner,
animation roles, source permission and the newly assigned port audio identity.
The source itself has no numeric audio ID. `tools/dungeon_wall_timeline.py`
checks the fixed scripts, scene tracks, sound import policy and graph dependency
before producing the 948-byte candidate under `build/`. It does not publish
media, change the resource catalog, or alter game saves.

## Independent binary contract

ENCDWT01 has format 1 and capability 1, independent of Room, rules and saves.
The checked container includes the upstream pin, CRC and five owned sections:

| Section | Contents |
|---|---|
| Strings | Exact owner path and audio source URI, without aliases or unowned bytes |
| Policy, 128 bytes | Scene-owned object ID, audio identity/hash, four animation roles, gain/loop/bus/pitch, camera parameters |
| Properties | Unchanged ENCPCL01, restricted here to frame and body-disabled tracks |
| Cue clips | Four clip IDs and contiguous cue ranges |
| Typed cues | Binary32 key time, audio-playing or camera-duration kind, binary64 value |

Cue order preserves source track order. It is not a global chronological sort.
Unknown fields, tracks, methods, versions, capabilities, unsafe paths and
unowned data reject the candidate. C++ loading is atomic. Matching a graph and
audio bank checks identity and policy; it does not prepare sound output.

## Execution and source comparison

`DungeonWallState` composes the shared property player and exports commands with
the resource owner, assigned clip and caller-provided instance generation.
Audio requests occur immediately, including repeated writes of the same value.
Animation completion follows those requests. Camera calls enter a separate
deferred queue; changing animation or playing RESET does not cancel calls that
were already queued. Queue limits are checked before changing state.

Manual advance and automatic processing remain distinct. Native Godot manual
`advance(0)` samples the single future audio key and can reassign audio after
completion. Automatic processing stops when the animation stops. Continuing the
same partly played animation preserves its position, as fixed by the preceding
shared-player change. Six source cases, 309 observations each, are frozen from
two original-engine runs in the preceding continuation report. The new state
replays both projections exactly, including deferred ordering.

The positive audio lookup test uses explicitly synthetic metadata with no PCM
file. The real current root audio bank rejects the missing wall sound identity.
This test does not establish prepared or audible original sound.

## Opening regression found by full checks

The random-camera addition incorrectly limited direction components to one.
That rejected the existing source `Openable Door.gd` call
`shake_camera(1, 0.2, Vector2(2,0))`, also present in all four checked house-door
records. Full checks on 578e456 and c17c7f7 consequently failed playable_opening,
house_runtime and pillow_sequence. Original data and expected traces were correct.

The executor now permits finite nonunit components within the existing geometry
bound of one million, without normalization. The artificial rejection test for
`(2,0)` becomes an actual out-of-range rejection. Existing 845 random-camera
golden observations are unchanged. Two new original-engine runs give the same
180 observations: the real door call and two explicitly declared numeric Shaker
fixtures. No random calls occur for these fixed directions.

## Actual verification and remaining integration

Evidence: `reports/dungeon-wall-timeline/source-implementation-1`.

- GCC 9.4, ASan/UBSan and leak checking: 2,010 metadata checks and 2,284 state
  checks; 550 nonunit-direction, 10,590 random-camera and 7,246 legacy camera
  checks pass against a freshly compiled archive of all 104 current core units.
- The same current archive passes the three previously failing opening tests:
  20,906 house-route, 54,851 playable-opening and 27,914 Pillow checks.
- Nineteen focused Python/source-list checks pass. Actual Windows Debug CMake
  metadata lists 290 tests and 1,321 Python methods, partitioned exactly into
  938 aggregate and 383 dedicated methods. This is metadata verification,
  not a Windows native test run.
- The first metadata test exposed a null file-path string construction under
  UBSan; an explicit rejection now precedes the file helper. The original
  failure and subsequent passing logs are retained. Local WSL has no CMake;
  its failed configure is retained and the actual direct build is identified.

Both new core units are registered for desktop and real 3DS builds. Full checks
and cross-builds for this new slice remain pending until its published commit.
The wall still needs live audio/camera consumers, scene collision and controlled
switch ownership, textures, map admission and save integration. Catacombs and
Pippi rescue are not yet playable; emulator and hardware acceptance of this
slice remain unverified.
