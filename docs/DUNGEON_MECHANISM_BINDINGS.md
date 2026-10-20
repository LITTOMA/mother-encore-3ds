# Source graph and animation consumer

`DungeonMechanismState` connects the checked switch graph to its actual property
and wall timelines. Animation completion now drives the graph's waiting source
methods. The previous short request-clip reference remains useful for isolated
methods; the combined 232-observation reference tests the joined execution.

## Independent content

`content/catacombs-mechanism-bindings.json` reviews fourteen original owners,
four animation profiles and role mappings. It pins the existing graph, property
clip and wall timeline reviews. The compiler rechecks their source behavior and
the actual effective scene child order. No owner ID, profile ID, clip ID or map
path is a game-content table in C++.

ENCDMA01 format 1/capability 1 is a 4,060-byte candidate, independent of Room,
rules and game saves. Its four sections hold:

| Section | Checked contents |
|---|---|
| Binding rows, 40 bytes | Owner ID/kind, profile ID and seven execution-role slots in source child order |
| Profile rows, 16 bytes | Profile ID, typed family and exact owned payload span |
| Profile payload | Three unchanged ENCPCL01 resources and one ENCDWT01 wall timeline |
| Graph | Unchanged ENCDSW01 source graph |

Loading checks all nested versions/capabilities, scene/owner/path identity,
profile families, role coverage, duplicates, bounds, checksums and source pin.
The absent wall Off animation stays unsupported. Unused profiles and unowned
bytes reject the candidate. The immutable shared data owner keeps borrowed
graph views alive; loading an already initialized owner is rejected explicitly.

## Execution ownership

Each source owner has its own property player or wall state. A group advance
processes active players in source order. On completion it invokes the graph
continuation and applies resulting animation requests before advancing the next
owner. Stopped players are not resampled by automatic processing. Wall audio
requests are immediate; wall camera calls stay in the separate deferred queue.

Idle boundaries, timers and camera completion are explicit. The wall's own
delay can restore control/music before the independent return-camera request
completes. A scene epoch and unique per-request callback tickets distinguish
those asynchronous requests. Reusing the lower graph operation token alone
would let a repeated old focus callback complete the later return; the joined
owner rejects that stale ticket even within the same scene.

Frames, input operations and callback dispatch commit atomically. Overflow or
invalid input preserves all owners' poses, flags, pending callbacks and queued
events. The bounded public queue carries every supported graph request to its
consumer; unknown request kinds are errors. This component does not invoke an
external media sink during a fallible state transition.

## Actual verification and limits

Direct GCC 9.4 ASan/UBSan and normal leak checking pass 8,331 loader checks and
37,358 joined execution checks. The latter replay all 232 original observations
twice, including fourteen poses at every observation, service order, completion
clocks, one-way/reset behavior and independent wall/camera continuations.
Negative cases cover stale epochs, wrong callback kinds, repeated focus tickets,
bounded waiter/event queues and preservation of all fourteen owners on failure.
The loader checks every truncation of the 4,060-byte resource, nested schema and
semantic mutations with valid checksums, fresh-owner rejection and shared lifetime.

Both new units compile from current source against the exact earlier 104-unit
archive. All 224 earlier source/header fingerprints still match; the stale
98-unit historical cache is not used. Eighteen focused Python/source-list tests
pass, including frozen-reference fingerprints, producer/engine identity and
unknown-action rejection. Adversarial test copies update only their private
checksums; published original observations and expectations are unchanged.

Desktop and real 3DS source lists include both units. Full checks and cross-build
of this new consumer await its published commit. The earlier 0f203b2 has actual
Clang ordinary/sanitizer results of 276/276 each and a genuine 3DSX/CIA pass;
those do not verify these subsequently added classes.

The new binary remains under `build/`. Live scene admission, actual textures,
body shapes/contact, sound preparation, camera execution and save integration
remain to be connected before Catacombs or Pippi rescue can be played. No new
emulator or hardware pass is implied by the offline joined consumer tests.
