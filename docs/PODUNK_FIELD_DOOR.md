# Podunk Door lifecycle and scene transition

The complete 14 original `Door.gd` instances are compiled from the official
Godot 3.6.2 full-scene export and inherited SceneState overrides into
`romfs/data/podunk.encdoor`. Authoring JSON and private exports are never read by
the game. Binary identity, sections, CRC, source proofs, Ready order, child
identities, geometry, audio and every destination SHA are checked on loading.

`FieldDoorRuntime::ready` resolves the real source Position2D and AudioStreamPlayer
and connects the actual body-entered signal. A body event pauses the real player,
waits for an actual idle frame, then begins the source coroutine. Fade transitions
resume from actual `fade_in_done` and `fade_out_mostly_done` signals. The original
cutscene branch keeps `global.entering_door` true; flag assignment does not emit
`flags_updated`. Multiple body callbacks waiting on the same idle frame retain
their original order and observe the authoritative entering flag on resumption.

A cross-scene transition first requires a source-bound destination candidate.
Preparing that candidate may parse/check resources, but must not execute Ready,
advance RNG, detach nodes or change the current scene. Unsupported destinations
reject before pausing the player or changing audiovisual state. This is an
admission guard; it does not claim any destination has become playable.

After fade-in, persistence is appended, the deferred transition is scheduled,
then on-screen enemies are cleared. The consumer drives each original operation
in order: detach player, disable collisions, detach persistent nodes, instance
destination, old AreaRoom leave signal, free old scene, assign new scene, optional
parameters, add root with postorder Ready, add player, create followers, party
position, reparent persistent nodes, set SceneTree current and update key prompt.
It then waits for actual tree-changed and idle-frame events, enables collisions
and emits scene-changed. The backend must implement each typed step against real
scene objects and reject pending mechanisms. Empty parameters are the complete
observed scope of these 14 Doors; nonempty parameter authoring is rejected.

After a warp, the source camera/visibility/direction changes precede an idle
frame. Real breadcrumb/follower updates precede fade-out. At the mostly-done
signal, end audio, conditional unpause, persistent removal, deferred Door free,
optional respawn snapshot, entering reset and done signal retain source order.
Respawn updates the original in-memory fields; it does not write a save file.

The unused `_special_guest` method has no callers in the pinned upstream. Its
AoOni dynamic consumer is explicitly unavailable if requested. Native Fade and
all destination scene consumers remain required backend work. This pack never
admits the whole Podunk scene or silently substitutes the opening House.

## SceneHost integration

Initialize each borrowed typed runtime with its actual signal/node host, then
populate `FieldSceneConsumers` with the checked data/runtime pairs, including
`door_data`/`door` and `prompt_data`/`prompt`. Configure a fresh `FieldSceneHost`
with the shared authoritative flag/AreaRoom signal bus. Call `ready_next` once
per original postorder entry, or `ready_to_boundary` until its first pending
entry. Only completed typed Ready operations create `script_admission` receipts
for geometry. Partial callback failure poisons startup; retrying must not consume
RNG or duplicate source effects. Missing typed consumers preserve the cursor.

Door is lifecycle role 13; ButtonPrompt is role 14. The next unimplemented source
entry is `Objects/Doors/Openable Door`, ordinal 3696. It needs its own Sprite,
flag bus, deferred collision, Prompt, animation, timer, PartyObject overlap,
inventory/key and dialogue consumer. A `scene_ready` cursor is not permission to
ignore any other unknown method or full-scene capability.

Manual parser cases are source-only in `manualtests/field_door_tests.cpp`. They
are not invoked automatically. Compilation and source/format admission are
reported separately from behavior, emulator or hardware verification.
