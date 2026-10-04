# Reviewed phone integration inputs

This directory contains generated staging content for the pinned original
Carol → Phone → first Dad sequence. It does not itself alter the shipping Room,
House or Audio packs. `dad_normal` is retained as a checked source graph for
later Record/menu integration; this frontend does not execute that graph.

## Regeneration and verification

```sh
python tools/phone_dialogue.py record --godot /path/to/official/godot3.6.2
python tools/phone_assets.py compile --tex3ds /path/to/official/tex3ds
python tools/phone_assets.py record-native --godot /path/to/official/godot3.6.2
python tools/native_phone.py compile
python tools/phone_dialogue.py verify
python tools/phone_assets.py verify
python tools/native_phone.py verify
python -m unittest discover -s tests -p 'test_phone_*.py' -v
```

`record` calls the original YAML parser offline. Exact phrase shapes and full
source fingerprints reject unreviewed changes. The original phone atlas is
compiled unchanged to RGBA8 `romfs/graphics/ui/phone/phone.t3x`.

## Source frontend

`dialogue.json` contains six linear programs: direct Carol call, area Carol
call, entrance reminder, Carol's literal reminder, first Dad call, and phone
no-answer. Commands have dense `phrase` indices and original `source_label`.
`ShowDialogue.dialogue_key` refers to `texts[].identity`. Text carries original
translation keys, segments, speaker, voice, dynamic-name tokens and hint spans.
The source hint color is external `text_contracts.hint_color_hex`.

Command extensions use the agreed checked Room schema:

- `HideDialogue.flags=1`: source box-close sound
- `ShowDialogue.flags=1`: preserve inherited talker, including None; voice is independently specified by the text segment
- `AwaitDialogue.flags=2`: advance at completed source text, without a fake A
- `TurnActor.flags=3`: queued target-actor direction captured at dispatch
- `TurnActor.flags=0`: direct direction vector (axis override bits require a target actor)
- `MoveCamera.flags=1`: replace absolute world X, retaining current camera Y

`presentation.json` contains the original InteractDialog object, precise source
shape/scale plus transformed geometry, ring keys and audio events, ordered
phone/Carol dispatch and the two inherited entrance-area shapes. Original
AnimationPlayer track order is frame track 0, audio track 1. The timestamps
within those tracks remain distinct.

## Independent pack and runtime

`romfs/data/opening.encphone` uses independently versioned `ENCPHN01`, schema 1,
capabilities 1, rules 1. The loader validates CRC, spans, UTF-8/string starts,
references, finite geometry and transforms, atlas bounds, track ownership,
ordered keys and recognized phone policies. Failed loads retain previous data.

`PhoneData` owns bytes. `PhoneView` is read-only and must not outlive its owner or
a successful replacement load. Resolve its external flag/program identities
against Room before enabling interaction. `PhoneFlagQuery` receives a pack flag
reference index, and reports failure for an unresolved binding.

`PhoneRuntime` starts Idle and never infers ringing from a flag. `ring(index)` is
idempotent while Ring is current. `interact(index, query, result)` selects the
last matching ordered flag row, sets Idle, and returns a program string offset,
phone-location string offset and typed hangup request. Open the selected program
before emitting that returned request. It does not turn the phone, create an
actor, set a talker, mark NPC dialogue seen, charge cash or consume a card.

`advance(delta, sink)` is the source idle-animation phase, including while the
dialogue is active. Ring/Idle selection is immediate; animation keys apply on
the next advance. Native AnimationPlayer playback position rounds to float32
after each addition; discrete keys use a half-open `[previous,current)` interval.
`pose(index)` provides the texture-resource index, frame, center and source
Y-sort origin for a renderer.

Sound requests retain source bus and position. Their object index identifies the
same source audio player: Ring and Hangup replace/restart its stream instead of
layering voices. A disabled platform audio backend should accept a resolved
request while separately reporting unavailability; rejecting the request is a
runtime error. These records do not prove positional audio parity or audibility.

## Evidence

`native-animation.json` and `.bin` record 360 ticks of the original Idle/Ring
Animation resources in official Godot 3.6.2. Recording setters stand in for
sprite/audio devices. The C++ test compares every tick, including initial play,
idempotent re-ring, repeated loops and transition to/from Idle. This is an
AnimationPlayer oracle, not execution of the whole original phone script/game.

Bounded verification logs are under `build/phone-stage/`. At initial delivery:
26 Python tests pass; C++ pack checks and all 360 native-animation observations
pass under ASan/UBSan with leak detection disabled; both new runtime source
files compile with official devkitARM. Full build/link, integrated gameplay,
GUI, audio audibility and hardware verification belong to the parent milestone.
