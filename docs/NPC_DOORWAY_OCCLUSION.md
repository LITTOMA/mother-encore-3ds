# NPC doorway occlusion: source layer restored

## Differential cause

The player and scripted Minnie already share the same sorted world pass and the
same `OpeningActorRenderer::pose` sprite projection. All original 276 `Above`
TileMap cells draw afterward. No separate NPC scissor or depth override exists.
Minnie is correctly masked while crossing those cells, then reappears beyond
their finite lower edge at world y192.

This differs from ordinary player movement. Mom's return door is centered at
(464,188), extents (16,4); Ninten's nine-pixel lower hull triggers the door/fade
before it can walk through the wall at y192. The original `minnie_leave.yaml`
phrase 5 instead sends the scripted actor to (464,200). Source `actor.tscn`
sets both collision layer and mask to zero, so its `move_and_slide` is not
blocked by that wall. The native actor preserves this behavior.

An equal-position alpha/UV comparison using the original textures confirms
both Ninten and Minnie are covered at y176 and both would leak beyond the
mask at y200: 191 and 213 opaque sprite pixels respectively. Thus the user's
correctly occluded player does not establish a different NPC drawing pass.
Evidence is in `reports/npc-doorway-occlusion/equal-position-pixels.json`.

## Actual missing source presentation

`uiManager.gd::open_dialogue_box` opens `Blackbars.tscn` before starting a
dialogue and closes it when its `done` signal fires. That layer-1 CanvasLayer
is above the world but below dialogue UI. It remains open during `showbox:
false` movement phrases. The lower black bar is 28 pixels tall, opening from
y180 to y152 in the original 320x180 viewport. The top bar is 18 pixels tall.
The two-key cubic/eased tracks move in 0.2 seconds on Open and 0.15 on Close;
each non-looping animation has source duration 0.5 seconds. Interrupted toggles
restart their authored clip, and repeated identical toggles do not restart it.

The native renderer previously omitted this source layer. Its restoration
uses the independent checked `opening.encbars` binary, compiled from the pinned
source scene and scripts. The 400x240 adaptation fills the viewport width and
bottom-anchors the original lower bar at y212; source sprite pixels stay 1:1.
At the Mom-room camera offset, this connects to the existing Above mask and
covers the portion of the original exit path behind the cinematic bar.

No original map cell, texture pixel, movement path, actor position, visibility,
camera, collision or script timing is changed. No invented room-mask extension
or early NPC deletion is used. The START fix is preserved independently.

## Verification

- 602 samples from the unchanged source AnimationPlayer in official Godot3.6.2
  match the native animation within 0.000016 pixels.
- Targeted host and ASan/UBSan tests cover initial hidden state, both viewport
  layouts, repeated toggles, interrupted Open/Close, invalid deltas, every
  truncated pack length, and malformed numeric/schema fields.
- ARM, source-path emulator and production START/Exit checks are recorded in
  the checkpoint result when completed. These are not yet hardware evidence.
