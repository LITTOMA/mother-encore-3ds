# Original Podunk OpenableDoor

This slice covers all ten original `Scripts/Main/Openable Door.gd` Sprite
instances. They are separate from the fourteen Area2D `Door.gd` scene warps.
The full official Godot 3.6.2 export and inherited instance overrides produce
the authoring IR, seven exact source PNG/atlas resources, two actual animation
profiles and `romfs/data/podunk.encopenable`. The yellow door's overrides are
preserved; the other nine use the common profile. No JSON is read at runtime.

The isolated export strips scripts, so eight child Sprite textures are null
there. The original serialized `sprite` property invokes `_set_texture` before
Ready. The compiler follows the original property/resource identity rather than
using those quarantined null values. `create` must run while the source scene is
instanced, before its postorder Ready roster; it sets the source child texture
and registers the source Area connections. Ready applies the original positions,
lock state, flags signal connection and queued Normal animation.

All source content, shape/node references, sounds, dialogue translations,
animation tracks, timers and tuning are binary fields. The reader checks version,
family/capability, scene identity/pin/SHA, CRC, reserved fields, count limits,
UTF-8, source proofs, unique child identities and every texture/audio/dialogue
reference. Failed loading preserves the previous valid data. Atlas receipts
bind the authoring IR, producer, genuine tex3ds executable and generated output.

The runtime retains actual lock/unlock deferred collision assignments, and
changes geometry only when the shared SceneTree message queue commits them.
Animation tracks apply from the native first process interval. Timer expiration
uses the original strict `time_left < 0` boundary, so exactly zero does not emit
timeout. Source overlapping bodies are PartyObject identities from the real
physics owner; they are not proximity guesses. Hidden doors still follow their
actual child process/pause policies.

OpenableDoor assigns `ButtonPrompt.enabled` directly. That does not call
`set_enabled` or refresh visibility. Opening first calls Canvas hide and then
assigns enabled=false without rewriting the prompt's hidden/press/animation
state. The corresponding host callbacks use the separately audited plain
assignment and Canvas hide APIs.

Interaction searches actual party inventories in source order, then key items.
It uses the real Item instance, original owner lookup and drop operation, writes
the source flag without a synthetic signal, sets `global.item` and invokes the
checked source dialogue. Activating/deactivating flags retain synchronous signal
dispatch before the remaining interaction branches. Missing keys select the
original locked dialogue; unknown inventory/item/geometry/audio/dialogue backends
reject. `_use_key` can insert the source empty flag key; it is not silently
sanitized. Source one-way timer logic may close twice, which remains intact.

The GPU primitive renders the checked nearest-filtered source sprite in the
common live Canvas/YSort queue. It accepts the actual parent transform,
visibility, tint and camera; it never drives timing or gameplay. Unsupported
sheared/rotated transforms reject rather than drawing an invented placement.
Current source transforms are axis aligned. Whole-scene activation and actual
Host wiring remain the scene owner's work; this resource never admits Podunk
as a whole.

`tools/field_openable_door.py compile` checks pinned source inputs and existing
tex3ds receipts. `--tex3ds` regenerates the real seven images with a four-worker
process pool. `stage_files` admits the binary and texture files; `audio_bindings`
provides the three source effect/PCM conversions for the common sound pipeline.
Lifecycle role 16 is reserved for integration after Door13/Prompt14/DeadBush15.
No private native export or report is needed by a clean clone to compile.

The exact Timer and AnimationPlayer policies were reviewed against the official
[Godot 3.6.2 Timer](https://github.com/godotengine/godot/blob/3.6.2-stable/scene/main/timer.cpp)
and [AnimationPlayer](https://github.com/godotengine/godot/blob/3.6.2-stable/scene/animation/animation_player.cpp)
source. Manual parser cases are source-only and are not automatic tests.
Compilation/resource admission is separate from behavioral, emulator and
hardware verification, which remain unverified for this slice.
