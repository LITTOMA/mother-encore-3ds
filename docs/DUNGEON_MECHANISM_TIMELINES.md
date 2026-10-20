# Combined source mechanism callbacks

The earlier switch reference used short named request animations to isolate
source script methods. The wall reference separately exercised real native
animation tracks. The new reference combines the original graph scripts with
the original Lever, Plate, Gate and Wall Door scenes and their native timelines.
It is an integration reference for the next scene consumer, not a playable
Catacombs implementation.

`tools/run_dungeon_mechanism_timeline_reference.py` copies the source scripts
without edits, applies the already reviewed effective instance properties and
source child order, and uses native Godot AnimationPlayers in manual mode. Its
camera, player, UI and music services remain explicit request/completion
adapters. The only wall scene changes are the same two logged audio-property
selectors used by the independent wall reference; each logged write forwards
to the native AudioStreamPlayer. Private texture and sound copies come from
the fixed upstream and are not published in the evidence or release.

The scene initializes original flags before entering the graph. Ready timers
run normally. During the manual animation batch, Engine.time_scale is zero so
native scene timers cannot introduce wall-clock variability; manual advance
uses 1/60 and flushes the native deferred message queue. These flushes do not
represent separate real idle frames. The actual idle-frame continuation and
wall delay are then released through normal processing. This distinction is
part of the trace contract, not an automatic frame-timing equivalence claim.

Two final runs, 232 observations each, match byte for byte. The observations
include all fourteen mechanism poses, assigned clips, positions, state and
ordered service requests. They establish:

- Main-switch control starts the wall's On animation while the camera focus
  request is still awaiting completion. Wall time must continue during focus.
- Audio is requested once on its source key, and the two deferred shakes retain
  their original durations and camera parameters.
- The completed wall's own delay restores player/UI control and music while
  the independent return-camera request is still pending. Completing that
  return invokes its separate unpause continuation.
- A one-way main switch ignores repetition. Leaving the area resets the other
  lever's flag while preserving its current animation and pose.

The first construction attempt used the Document API incorrectly and stopped
before engine execution. The second omitted original flag initialization, so
the source setter rejected the unknown flag and the wall attempted its absent
Off animation. Both failures are retained. A subsequent provenance check also
detected the editor converting generated adapter indentation; the producer now
restores the authored adapters after import and checks them again after runtime.
All four completed traces before/after that restoration are byte-identical.
No source script or existing expected observation was changed to mask a mismatch.

Six source/provenance/negative checks pass. Raw logs, source hashes, adapters,
observations and cleanup receipts are under
`reports/dungeon-mechanism-timelines/source-integration-1`. No native composite
consumer, physical contact, actual camera motion, audible output, full map,
save integration, emulator route or hardware pass is claimed by this reference.
The next consumer must obtain profile/role bindings and source ordering from
checked independent resources and drive these callbacks from real animation
completion rather than simulated acknowledgments.
