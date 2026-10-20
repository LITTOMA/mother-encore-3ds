# Catacombs switch property clips

Three candidate profiles describe Lever, Plate and Gate:11 source animations,
948 bytes total, using the existing ENCPCL01 format1/capability1. Stable clip
IDs express the switch method owner's animation roles; Reset remains a separate
source clip. Nothing is staged in RomFS and no dungeon/texture binding is added.
Wall-door methods/audio and key/lock presentation are outside this subset.

Only Sprite frames and body-disabled properties are admitted. The compiler
checks exact source/texture/script identities, complete animation order, frame
grid, KinematicBody ownership, typed values, finite ordered times and source
track modes. Independent property tracks commute when sorted for the existing
format. A continuous single constant at time zero is exact under the closed
owner; multi-key curves, transforms, audio/method tracks and unknown properties
reject. Initial visibility/body state and clip changes preserve untouched fields.

Official Godot3.6.2 evaluates the exact source animation/shape subresources and
initial node properties in numeric copies. Script attachments and signal
dispatch are explicitly quarantined; original scenes remain byte-identical
outside the engine project. Source textures and unused audio resources are
copied only to the private reference project, with source fingerprints. This
is property evaluation, not original gameplay, physical contact or audible output.

Two runs produce identical809 observations, including exact key boundaries,
restarts, completion, property carry-over and60Hz steps. The shared core replays
all observations twice. This exposed a pre-existing zero-delta error: the old
player left a future single-key collision track untouched. Godot's effective
zero delta uses value interpolation; a one-key track returns that key before
its timestamp. Multiple non-looping keys before their first timestamp return
no value. The core now follows that behavior without consuming future interval
events. Normal nonzero updates retain the original half-open interval/end rule.

The review uses the exact engine commit3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8:
[AnimationPlayer zero-delta branch](https://github.com/godotengine/godot/blob/3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8/scene/animation/animation_player.cpp#L457)
and [one-key interpolation](https://github.com/godotengine/godot/blob/3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8/scene/resources/animation.cpp#L1604).
No original expectation, game resource, format/capability or save was changed
to hide the mismatch. Ordinary animation playback must not introduce an extra
zero-delta seek that the source did not perform.

Checks:11 Python source methods;4873 GCC ASan/UBSan checks with leak checking,
replaying809 observations twice; unchanged Root fixtures pass2632 checks/81
observations and3959 checks/414 observations. Actual Windows CMake collection
has284 tests and1294 Python methods (911 aggregate,383 dedicated), metadata
only. Full current host/Clang/console, emulator and hardware verification remain
separate. There is no playable Catacombs or full-presentation claim.

Evidence: reports/catacombs-switch-clips/reference-oracle-1. The retained
native1 failure identifies row512, Gate On, zero advance, body mismatch. The
native2 print still called the old shared player unchanged; native3 corrects
that diagnostic text. Both later semantic runs pass against unchanged source
expectations. Prerequisite fbbc25e console evidence and exact17-target/4753626B
owned temporary-file retirement proof are retained alongside this work.
