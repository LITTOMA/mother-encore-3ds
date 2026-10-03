# 2026-10-03 scoped update

The original dialogue-owned Blackbars layer is now restored from a checked independent resource. See NPC_DOORWAY_OCCLUSION.md and its current checkpoint; the older boundaries below are historical.

# Native opening-room presentation

Current renderer uses the original source textures and shared-core actor/camera state on the 400×240 upper screen. Debug text remains on the lower screen.

## Layering fix

The prior checkpoint drew only the room background, Ninten and lamp. That omitted the original Objects TileMap overlays and made the player incorrectly draw over furniture.
`tools/house_layers.py` now compiles the actual208×192 Tileset texture and14 reviewed source regions. The external binary Overlay section retains both texture placement and independent Godot Y-sort origin.
The renderer sorts these tiles together with Ninten/lamp by source rootY, rather than using texture top edge or drawing all objects either above or below every actor.

Reported objects: chair sortY384, bed432, TV464. Texture placements are respectively(496,368),(400,400),(496,432). Sorting depth must remain independent from these texture-top positions.
Source: pinned Maps/podunk/Nintens House.tscn Objects.cell_y_sort and Godot3.6.2 TileMap quadrant ordering. No hand-painted masks were added.

## Sprites and motion

- Ninten atlas310×580,10×20 frames, centered31×29 at rootY-4 during ordinary walking
- Lamp atlas68×22,4 frames of17×22; initial centered rootY-2
- Cutscene actor center is root + sprite_position + sprite_offset; jump and shake remain separate properties
- Emote atlas372×384,12×12 frames of31×32; emote center uses root + sprite_position + emote_position, without Sprite.offset
- Shadow uses original15×5 texture at root+(0,7), only when source actor state says visible
- Scene frame9 and64 in the emote atlas are actually transparent; completed/initial emotes are not substituted with invented graphics
- All atlases reject unexpected dimensions/rotation, preserve returned UV bounds, use nearest filtering and apply floor(vertex+0.5) at drawing

The player andlamp cutscene frames, positions and offsets come from ActorActionState; the renderer does not run a second animation or action scheduler.
CutsceneCamera supplies source-tested400×240 center/offset values. Camera translation stays floating-point until final vertex snap.
The platform invokes physics updates at60Hz and one idle phase per render frame, capped at100ms with large suspension backlog discarded.

## Scope and validation

This section describes the opening house's world rendering and source layering.
The checked dialogue Blackbars resource is integrated; see
[NPC_DOORWAY_OCCLUSION.md](NPC_DOORWAY_OCCLUSION.md). Source battle presentation
and NDSP request handling are separate components described in
[BATTLE_RESIDENCY_CHECKPOINT.md](BATTLE_RESIDENCY_CHECKPOINT.md) and
[AUDIO_BACKEND.md](AUDIO_BACKEND.md).

General scene composition, full AnimationTree coverage and unreviewed shader
behaviors remain outside this bounded renderer. Audio audibility, CIA installation
and Old/New 3DS hardware validation are outstanding. Emulator display/input
checks do not establish physical-hardware acceptance.
