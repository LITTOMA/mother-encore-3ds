# Catacombs switch graph and original-method reference

This candidate describes the five switches and nine controlled objects in the
fixed original Catacombs. It does not open a playable scene. PR71 depends on
PR69; production maps, catalogs, audio, session admission and saved bytes remain
unchanged. The C++ loader admits metadata only, not GDScript execution.

The source compiler follows effective instances and script inheritance. It
checks stable IDs, explicit or leaf-name flags, flag storage and reset policy,
one-way controls, initial object state, signed update order, positions/camera
offsets, pause/focus policy and all nine control edges. Ready/camera/wall timing,
music fade and denial sound/channel are source-derived resource values.

| Control | Source flag | Targets | Policy |
|---|---|---|---|
| Bottom2/switch | global hidden_entrance_opened | Wall Door | One-way, pause and focus |
| Bottom2/switch1 | object Catacombs/switch1 | Gate3, Gate4, Gate5, Gate8 | Resets on area leave |
| Bottom2/switch2 | object Catacombs/switch2 | Gate9, Gate10 | Pause and focus |
| Bottom2/Plate1 | object Catacombs/Plate1 | Gate6 | One-way player contact |
| Bottom2/Plate4 | object Catacombs/Plate4 | Gate11 | One-way player contact |

Gate3 and Gate8 start on. Controlled state is the initial bit XOR registered
switch flags. Registration happens during enter-tree, before controlled Ready.
Leaving the area resets switch1's flag without updating its current animation;
re-entry uses StayOn/StayOff from the saved flags. This source distinction must
survive scene teardown and future save integration.

ENCDSW01 format1/capability1 has five bounded sections: canonical ASCII strings,
one48-byte policy,64-byte switch rows,48-byte controlled rows and8-byte edges.
The loader rejects unknown versions/capabilities/kinds, invalid finite values,
duplicate identities, missing/wrong-kind/unowned edges, unsafe/unused strings,
padding, checksum and source mismatches. Rejected loads retain the old owner.
The CLI and fixture generator write only under build; they cannot stage RomFS.
The earlier1360-byte development prototype lacked denial sound/channel spans;
the completed1412-byte candidate includes them in its48-byte policy. Neither
prototype nor candidate has been admitted by a production scene or save.

Official Godot3.6.2 runs byte-identical Flag/Abstract/List/TwoStates switch and
ControlledObject/ControlledTwoStatesObject/Gate/Wall scripts. An additional
uninstantiated original synced-switch script resolves the source type
signature. Original global flag initialization/setters and the two Catacombs
callbacks are extracted unchanged. Player, UI, camera, audio occupancy and named
AnimationPlayer request clips are explicit services. No source animation
timeline, physical contact, audible output or whole-map equivalence is claimed.

Two runs produce identical28 observations covering all five switches, early
and paused input, source can-switch denial, both camera workflows, one-way
repeats, area reset and saved-state Ready. The hidden switch emits two pause/UI
requests: the scene callback precedes its control action. The wall callback can
finish its post-animation delay and restore player/music while the explicitly
held camera request remains pending; the later camera return completes the
second unpause. This is preserved evidence, not a reason to combine the two
source continuations or invent a common completion time.

Targeted checks:14 Python source/codec methods and209 GCC ASan/UBSan loader
checks with leak checking, compiling all native inputs from source. CMake
collection has278 tests and1282 Python methods (916 aggregate,366 dedicated).
After importing PR69's phone schedule fix, collection has279 tests and1283
Python methods (900 aggregate,383 dedicated): the original17 phone methods
moved to an exclusive dedicated test and one partition regression was added.
Windows native executables were not built. Full current CI and console builds
are checked separately; no emulator dungeon, hardware or complete-game pass.
Inspection and the existing source-list test found the three new dungeon
components absent from the console source list. The list is corrected; three
checks pass after the retained failure. This is source registration validation,
not a successful ARM compile; the next genuine console job must verify it.
Evidence is in reports/catacombs-switches/metadata-oracle-1.
